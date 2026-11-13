"""Minimal numpy GLB reader used by the Lanternfall asset pipeline (tools/glb_to_lfm.py).

load_glb(path) -> dict(P float32 (n,3)  rest-pose positions (metres, +Y up, in glTF space)
                       C uint8   (n,3)  sRGB albedo sampled from baseColorTexture * factor * COLOR_0
                       T int32   (m,3)  triangles
                       J uint8 (n,4) / W float32 (n,4) when the source is skinned, else None
                       mat int32 (n,)   material index (for later material-type heuristics)
                       skin: dict(names, parent, head) when skinned)
Memory-conscious: textures are decoded one at a time and reduced to <= max_tex before sampling.
"""
import json
import struct

import numpy as np
from PIL import Image
import io

CT = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
NC = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def _quat_mat(q, t, s):
    x, y, z, w = q
    r = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    m = np.eye(4)
    m[:3, :3] = r * np.array(s)[None, :]
    m[:3, 3] = t
    return m


class Glb:
    def __init__(self, path):
        b = open(path, 'rb').read()
        L = struct.unpack('<I', b[12:16])[0]
        self.j = json.loads(b[20:20 + L])
        off = 20 + L
        self.bin = b''
        if off < len(b):
            bl = struct.unpack('<I', b[off:off + 4])[0]
            self.bin = memoryview(b)[off + 8:off + 8 + bl]
        self.raw = b

    def acc(self, i):
        a = self.j['accessors'][i]
        bv = self.j['bufferViews'][a['bufferView']] if 'bufferView' in a else None
        n = a['count']
        nc = NC[a['type']]
        dt = CT[a['componentType']]
        if bv is None:
            return np.zeros((n, nc), dt)
        o = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
        isz = np.dtype(dt).itemsize * nc
        st = bv.get('byteStride', isz)
        if st == isz:
            arr = np.frombuffer(self.bin, dtype=dt, count=n * nc, offset=o).reshape(n, nc)
        else:
            tmp = np.frombuffer(self.bin, dtype=np.uint8, count=(n - 1) * st + isz, offset=o)
            arr = np.lib.stride_tricks.as_strided(tmp, shape=(n, isz), strides=(st, 1)).copy().view(dt).reshape(n, nc)
        if a.get('normalized'):
            if dt == np.uint8:
                arr = arr / 255.0
            elif dt == np.uint16:
                arr = arr / 65535.0
            elif dt == np.int16:
                arr = np.maximum(arr / 32767.0, -1)
            elif dt == np.int8:
                arr = np.maximum(arr / 127.0, -1)
        return arr

    def image(self, idx, max_tex):
        im = self.j['images'][idx]
        if 'bufferView' in im:
            bv = self.j['bufferViews'][im['bufferView']]
            o = bv.get('byteOffset', 0)
            data = bytes(self.bin[o:o + bv['byteLength']])
        else:
            return None
        img = Image.open(io.BytesIO(data))
        img.draft('RGB', (max_tex, max_tex))
        img = img.convert('RGBA')
        while max(img.size) > max_tex:
            img = img.reduce(2)
        return np.asarray(img)

    def node_world(self):
        nodes = self.j['nodes']
        par = {}
        for i, n in enumerate(nodes):
            for c in n.get('children', []):
                par[c] = i
        loc = []
        for n in nodes:
            if 'matrix' in n:
                loc.append(np.array(n['matrix']).reshape(4, 4).T)
            else:
                loc.append(_quat_mat(n.get('rotation', [0, 0, 0, 1]), n.get('translation', [0, 0, 0]), n.get('scale', [1, 1, 1])))
        wm = [None] * len(nodes)

        def go(i):
            if wm[i] is None:
                wm[i] = loc[i] if i not in par else go(par[i]) @ loc[i]
            return wm[i]
        for i in range(len(nodes)):
            go(i)
        return wm, par


def bilinear(tex, uv):
    h, w = tex.shape[:2]
    u = (uv[:, 0] % 1.0) * w - 0.5
    v = (uv[:, 1] % 1.0) * h - 0.5
    x0 = np.floor(u).astype(np.int32)
    y0 = np.floor(v).astype(np.int32)
    fx = (u - x0)[:, None]
    fy = (v - y0)[:, None]
    x1 = (x0 + 1) % w
    y1 = (y0 + 1) % h
    x0 %= w
    y0 %= h
    t = tex.astype(np.float32)
    return (t[y0, x0] * (1 - fx) * (1 - fy) + t[y0, x1] * fx * (1 - fy) + t[y1, x0] * (1 - fx) * fy + t[y1, x1] * fx * fy)


def s2l(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def load_glb(path, max_tex=1024):
    g = Glb(path)
    j = g.j
    wm, par = g.node_world()
    nodes = j['nodes']
    skins = j.get('skins', [])
    P, C, T, J, W, M = [], [], [], [], [], []
    base = 0
    cache = {}
    skinned = False
    skin_info = None
    # joint rest data for skinned meshes
    for ni, n in enumerate(nodes):
        if 'mesh' not in n:
            continue
        mesh = j['meshes'][n['mesh']]
        sk = skins[n['skin']] if 'skin' in n else None
        if sk is not None:
            ibm = g.acc(sk['inverseBindMatrices']).reshape(-1, 4, 4).transpose(0, 2, 1) if 'inverseBindMatrices' in sk else None
            jm = np.stack([wm[jn] for jn in sk['joints']])
            skm = jm @ ibm if ibm is not None else jm
        for prim in mesh['primitives']:
            if prim.get('mode', 4) != 4 or 'indices' not in prim:
                continue
            at = prim['attributes']
            pos = g.acc(at['POSITION']).astype(np.float64)
            n0 = len(pos)
            mi = prim.get('material', 0)
            mat = j['materials'][mi] if j.get('materials') else {}
            pbr = mat.get('pbrMetallicRoughness', {})
            fac = np.array(pbr.get('baseColorFactor', [1, 1, 1, 1]), np.float32)
            col = np.tile(fac[:3], (n0, 1))
            bt = pbr.get('baseColorTexture')
            sg = mat.get('extensions', {}).get('KHR_materials_pbrSpecularGlossiness')
            if sg is not None:   # spec/gloss workflow: diffuse == albedo
                fac = np.array(sg.get('diffuseFactor', [1, 1, 1, 1]), np.float32)
                col = np.tile(fac[:3], (n0, 1)); bt = sg.get('diffuseTexture')
            if bt is not None and 'TEXCOORD_%d' % bt.get('texCoord', 0) in at:
                tex = j['textures'][bt['index']]
                src = tex.get('source')
                if src is None:
                    ext = tex.get('extensions', {})
                    src = next((e['source'] for e in ext.values() if 'source' in e), None)
                if src is not None:
                    if src not in cache:
                        cache[src] = g.image(src, max_tex)
                    img = cache[src]
                    if img is not None:
                        uv = g.acc(at['TEXCOORD_%d' % bt.get('texCoord', 0)]).astype(np.float64)
                        s = bilinear(img, uv)[:, :3] / 255.0
                        col = s2l(s) * fac[:3]  # linear
            if 'COLOR_0' in at:
                vc = g.acc(at['COLOR_0'])[:, :3].astype(np.float32)
                col = col * vc
            if sk is not None and 'JOINTS_0' in at:
                jj = g.acc(at['JOINTS_0']).astype(np.int64)
                ww = g.acc(at['WEIGHTS_0']).astype(np.float64)
                ww = ww / np.maximum(ww.sum(1, keepdims=True), 1e-8)
                ph = np.concatenate([pos, np.ones((n0, 1))], 1)
                out = np.zeros((n0, 3))
                for k in range(4):
                    mk = skm[jj[:, k]]  # (n,4,4)
                    out += ww[:, k:k + 1] * np.einsum('nij,nj->ni', mk, ph)[:, :3]
                pos = out
                skinned = True
                J.append(jj.astype(np.uint8))
                W.append(ww.astype(np.float32))
            else:
                ph = np.concatenate([pos, np.ones((n0, 1))], 1)
                pos = (ph @ wm[ni].T)[:, :3]
                if sk is None:
                    J.append(np.zeros((n0, 4), np.uint8))
                    W.append(np.zeros((n0, 4), np.float32))
            ind = g.acc(prim['indices']).astype(np.int64).reshape(-1, 3)
            P.append(pos.astype(np.float32))
            C.append((np.clip(col, 0, 1)).astype(np.float32))
            T.append((ind + base).astype(np.int32))
            M.append(np.full(n0, mi, np.int32))
            base += n0
    out = dict(P=np.concatenate(P), Cl=np.concatenate(C), T=np.concatenate(T), mat=np.concatenate(M))
    out['skinned'] = skinned
    if skinned:
        out['J'] = np.concatenate(J)
        out['W'] = np.concatenate(W)
        sk = skins[0]
        names = [nodes[jn].get('name', 'j%d' % i) for i, jn in enumerate(sk['joints'])]
        heads = np.array([wm[jn][:3, 3] for jn in sk['joints']])
        pidx = []
        for jn in sk['joints']:
            p = par.get(jn, -1)
            pidx.append(sk['joints'].index(p) if p in sk['joints'] else -1)
        out['skin'] = dict(names=names, parent=pidx, head=heads)
    out['n_src'] = (len(out['P']), len(out['T']))
    return out
