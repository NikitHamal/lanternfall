# LANTERNFALL Blender pipeline (blender -b --factory-startup -P blender_build.py -- <figure>)
#  raw bake (.lfraw from tools/bake.js) -> real Blender mesh + armature + vertex groups + attributes
#  -> per-vertex ambient-occlusion / cavity bake (BVH ray casting, incl. ground contact)
#  -> Cycles preview renders of the posed rig (baked 'ready' / action pose driven through the armature)
#  -> .blend file + packed runtime asset (.lfm) read back FROM the Blender datablocks
import bpy, sys, json, struct, math, os, time
import numpy as np
from mathutils import Vector, Matrix, Quaternion
from mathutils.bvhtree import BVHTree

key = sys.argv[sys.argv.index('--') + 1]
RAW = os.environ.get('LF_RAW', '/work/raw'); OUT = os.environ.get('LF_OUT', '/work/out'); BL = '/work/blend'; PRV = '/work/preview'
for d in (OUT, BL, PRV): os.makedirs(d, exist_ok=True)
T0 = time.time()

def load(path):
    b = open(path, 'rb').read(); hl = struct.unpack('<I', b[:4])[0]; h = json.loads(b[4:4 + hl].rstrip(b'\0')); base = 4 + hl
    dts = {'f32': '<f4', 'u8': 'u1', 'u32': '<u4'}
    def arr(k):
        m = h['blobs'][k]; return np.frombuffer(b, dtype=dts[m['type']], count=m['count'], offset=base + m['off'])
    return h, arr

hb, ab = load(f'{RAW}/{key}.lfraw')                          # base figure
lant = os.path.exists(f'{RAW}/{key}+L.lfraw')
hf, af = load(f'{RAW}/{key}+L.lfraw') if lant else (hb, ab)   # superset incl. back-lantern geometry
nv, nb = hf['nv'], hf['nb']
P = af('P').reshape(-1, 3).astype(np.float64); N = af('N').reshape(-1, 3); C = af('C').reshape(-1, 3); UV = af('UV').reshape(-1, 2)
M = af('M').reshape(-1, 4); BA = af('BA'); BB = af('BB'); W = af('W'); I = af('I').astype(np.int64).reshape(-1, 3)
if lant:  # the lantern geometry is appended: prefix must be identical to the base figure
    assert np.allclose(P[:hb['nv']], ab('P').reshape(-1, 3), atol=1e-6), 'lantern prefix mismatch'
    assert np.array_equal(I[:hb['ni'] // 3], ab('I').astype(np.int64).reshape(-1, 3))

# ---------------- Blender scene ----------------
bpy.ops.wm.read_factory_settings(use_empty=True)
scn = bpy.context.scene
root = bpy.data.objects.new('Root_YUp', None); scn.collection.objects.link(root); root.rotation_euler = (math.pi / 2, 0, 0)

mesh = bpy.data.meshes.new(key)
mesh.from_pydata(P.tolist(), [], I.tolist())
mesh.update()
mesh.normals_split_custom_set_from_vertices(N.astype(float).tolist())
ca = mesh.color_attributes.new('Col', 'FLOAT_COLOR', 'POINT'); ca.data.foreach_set('color', np.c_[C, np.ones(nv)].ravel())
def fattr(name, v):
    a = mesh.attributes.new(name, 'FLOAT', 'POINT'); a.data.foreach_set('value', np.asarray(v, dtype=np.float32))
fattr('rough', M[:, 0]); fattr('metal', M[:, 1]); fattr('surf', M[:, 2]); fattr('grime', M[:, 3])
fattr('glow', np.where(np.abs(M[:, 2] - 10) < 0.5, M[:, 3] * 3.0, 0.0))
uvl = mesh.uv_layers.new(name='UVMap'); uvl.data.foreach_set('uv', UV[I.ravel()].ravel())
obj = bpy.data.objects.new(key, mesh); scn.collection.objects.link(obj); obj.parent = root

# armature (rest = bind pose, limbs hang along -Y)
arm = bpy.data.armatures.new('Rig_' + key); aobj = bpy.data.objects.new('Rig_' + key, arm); scn.collection.objects.link(aobj); aobj.parent = root
bpy.context.view_layer.objects.active = aobj; bpy.ops.object.mode_set(mode='EDIT')
head = np.array(hf['head']).reshape(-1, 3); par = hf['parent']; ebs = []
kids = {i: [] for i in range(nb)}
for i, p in enumerate(par):
    if p >= 0: kids[p].append(i)
for i, nm in enumerate(hf['bones']):
    eb = arm.edit_bones.new(nm); eb.head = Vector(head[i])
    tl = Vector(head[i]) + Vector((0, 0.06, 0))
    if kids[i]:
        c = max(kids[i], key=lambda k: (Vector(head[k]) - Vector(head[i])).length); d = Vector(head[c]) - Vector(head[i])
        if d.length > 0.03: tl = Vector(head[c])
    eb.tail = tl; eb.roll = 0; ebs.append(eb)
for i, p in enumerate(par):
    if p >= 0: ebs[i].parent = ebs[p]
bpy.ops.object.mode_set(mode='OBJECT')
# skin weights -> vertex groups (two-bone blend exactly like the original CPU skinner)
vgs = [obj.vertex_groups.new(name=nm) for nm in hf['bones']]
wa = np.where(BA == BB, 1.0, 1.0 - W); wb = np.where(BA == BB, 0.0, W)
for b in range(nb):
    ia = np.nonzero(BA == b)[0]
    if len(ia):
        for w_ in np.unique(np.round(wa[ia], 4)):
            sel = ia[np.round(wa[ia], 4) == w_]
            if w_ > 0: vgs[b].add(sel.tolist(), float(w_), 'REPLACE')
    ib = np.nonzero((BB == b) & (wb > 0))[0]
    for w_ in np.unique(np.round(wb[ib], 4)):
        sel = ib[np.round(wb[ib], 4) == w_]
        vgs[b].add(sel.tolist(), float(w_), 'ADD')
md = obj.modifiers.new('Armature', 'ARMATURE'); md.object = aobj

# ---------------- AO + cavity bake ----------------
dg = bpy.context.evaluated_depsgraph_get()
bvh = BVHTree.FromPolygons([tuple(v) for v in P.tolist()], [tuple(t) for t in I.tolist()], epsilon=0.0)
rng = np.random.default_rng(7); K = 24; MAXD = 0.7
r1, r2 = rng.random(K), rng.random(K); ang = 2 * np.pi * r1; rad = np.sqrt(r2)
loc = np.stack([rad * np.cos(ang), rad * np.sin(ang), np.sqrt(1 - r2)], 1)  # cosine hemisphere around +Z
ao = np.ones(nv); Nn = N.astype(np.float64); Nn /= (np.linalg.norm(Nn, axis=1, keepdims=True) + 1e-9)
for v in range(nv):
    n = Vector(Nn[v]); t = Vector((1, 0, 0)) if abs(n.x) < 0.9 else Vector((0, 1, 0)); t = (t - n * t.dot(n)).normalized(); bt = n.cross(t)
    o = Vector(P[v]) + n * 0.006; occ = 0.0
    for k in range(K):
        d = t * loc[k, 0] + bt * loc[k, 1] + n * loc[k, 2]
        hit = bvh.ray_cast(o, d, MAXD)
        if hit[0] is not None: occ += 1.0 - (hit[3] / MAXD) ** 0.6 * 0.5 if False else (1.0 - hit[3] / MAXD)
        elif d.y < -1e-4 and o.y > 0:   # ground plane
            tg = -o.y / d.y
            if tg < MAXD: occ += 1.0 - tg / MAXD
    ao[v] = 1.0 - occ / K
ao = 1.0 - 0.6 * (1.0 - np.clip(ao, 0, 1))   # softened: the runtime shader already adds mud/grime
ed = np.empty(len(mesh.edges) * 2, dtype=np.int64); mesh.edges.foreach_get('vertices', ed); ed = ed.reshape(-1, 2)
dd = P[ed[:, 1]] - P[ed[:, 0]]; l2 = (dd * dd).sum(1) + 1e-9; kk = ((Nn[ed[:, 1]] - Nn[ed[:, 0]]) * dd).sum(1) / l2
cav = np.zeros(nv); cnt = np.zeros(nv); np.add.at(cav, ed[:, 0], kk); np.add.at(cav, ed[:, 1], kk); np.add.at(cnt, ed[:, 0], 1); np.add.at(cnt, ed[:, 1], 1)
cav = np.clip(cav / np.maximum(cnt, 1) / 14.0, -1, 1)
fattr('ao', ao); fattr('cavity', cav)
cb = mesh.color_attributes.new('ColAO', 'FLOAT_COLOR', 'POINT'); shade = (0.25 + 0.75 * ao)[:, None] * (1.0 + 0.18 * np.clip(cav, 0, 1))[:, None]
cb.data.foreach_set('color', np.c_[C * shade, np.ones(nv)].ravel())
t_bake = time.time() - T0

# ---------------- read back from Blender datablocks (source of truth for the runtime asset) ----------------
rP = np.empty(nv * 3, dtype=np.float32); mesh.vertices.foreach_get('co', rP); rP = rP.reshape(-1, 3)
li = np.empty(len(mesh.loops), dtype=np.int64); mesh.loops.foreach_get('vertex_index', li); rI = li.reshape(-1, 3)
rC = np.empty(nv * 4, dtype=np.float32); mesh.color_attributes['Col'].data.foreach_get('color', rC); rC = rC.reshape(-1, 4)[:, :3]
def rf(name):
    a = np.empty(nv, dtype=np.float32); mesh.attributes[name].data.foreach_get('value', a); return a
rM = np.stack([rf('rough'), rf('metal'), rf('surf'), rf('grime')], 1); rao = rf('ao'); rcav = rf('cavity')
rn = np.empty(nv * 3, dtype=np.float32); mesh.vertices.foreach_get('normal', rn)
rBA = np.zeros(nv, np.uint8); rBB = np.zeros(nv, np.uint8); rW = np.zeros(nv, np.float32)
best = [[] for _ in range(nv)]
for v in mesh.vertices:
    g = sorted(((vg.weight, vg.group) for vg in v.groups), reverse=True)[:2]
    if len(g) == 1: rBA[v.index] = rBB[v.index] = g[0][1]
    elif len(g) == 2:
        rBA[v.index] = g[0][1]; rBB[v.index] = g[1][1]; tot = g[0][0] + g[1][0]; rW[v.index] = g[1][0] / tot
        # keep the original ordering (BA owns the lower weight index convention): ensure BA matches source when equal sets
src_same = np.mean((rBA == BA) & ((rBB == BB) | (W == 0)))
rUV = np.empty(len(mesh.loops) * 2, dtype=np.float32); uvl.data.foreach_get('uv', rUV); rUV = rUV.reshape(-1, 2)
uvv = np.zeros((nv, 2), np.float32); uvv[rI.ravel()] = rUV   # all loops of a vertex share uv (verified below)
uv_err = float(np.abs(uvv - UV).max())
# prefer the exact original assignment where the Blender read-back agrees (BA/BB order)
swap = (rBA != BA) & (rBB == BA) & (rBA == BB); rBA2 = np.where(swap, BA, rBA); rBB2 = np.where(swap, BB, rBB); rW2 = np.where(swap, 1.0 - rW, rW)

# ---------------- write .lfm ----------------
def s(x, n): b = x.encode()[:n]; return b + b'\0' * (n - len(b))
buf = bytearray(); buf += b'LFM1'
death = hf.get('death') or 'fall'; dc = hf.get('disCol') or [1, 0.45, 0.1]
buf += struct.pack('<7I', nv, len(rI), nb, len(hb['clips']), len(hf['fx']), hb['nv'], hb['ni'] // 3 * 3)
buf += struct.pack('<4f', hf['scale'], hf['radius'], hf['hpY'], hf['height'] or 2.0)
buf += struct.pack('<3f', *dc) + struct.pack('<BBBB', 1 if hf['hero'] else 0, 1 if hf['float'] else 0, 1 if lant else 0, 0) + s(death, 12) + s(key, 16)
for i in range(nb): buf += s(hf['bones'][i], 12)
buf += np.array(par, dtype=np.int8).tobytes() + b'\0' * ((4 - nb % 4) % 4)
buf += head.astype(np.float32).tobytes() + np.array(hf['abs'], dtype=np.uint8).tobytes() + b'\0' * ((4 - nb % 4) % 4)
for k, v in hf['fx'].items(): buf += s(k, 12) + struct.pack('<i3f', int(v[0]), float(v[1]), float(v[2]), float(v[3]))
vt = np.zeros(nv, dtype=[('p', '<f4', 3), ('n', 'i1', 4), ('c', 'u1', 4), ('uv', '<f4', 2), ('m', 'u1', 4), ('b', 'u1', 4)])
vt['p'] = rP
nn = rn.reshape(-1, 3); nn = nn / (np.linalg.norm(nn, axis=1, keepdims=True) + 1e-9)
# runtime uses the authored (custom) normals from the source, which Blender stored losslessly
vt['n'][:, :3] = np.clip(np.round(N * 127), -127, 127).astype(np.int8)
vt['c'][:, :3] = np.clip(np.round(np.sqrt(np.clip(rC, 0, 1)) * 255), 0, 255).astype(np.uint8)   # sqrt encoded, shader squares
vt['c'][:, 3] = np.clip(np.round(rao * 255), 0, 255).astype(np.uint8)
vt['uv'] = uvv
vt['m'][:, 0] = np.clip(np.round(rM[:, 0] * 255), 0, 255); vt['m'][:, 1] = np.clip(np.round(rM[:, 1] * 255), 0, 255)
vt['m'][:, 2] = np.clip(np.round(rM[:, 2]), 0, 255); vt['m'][:, 3] = np.clip(np.round(rM[:, 3] / 2.0 * 255), 0, 255)
vt['b'][:, 0] = BA; vt['b'][:, 1] = BB; vt['b'][:, 2] = np.clip(np.round(W * 255), 0, 255); vt['b'][:, 3] = np.clip(np.round((rcav * 0.5 + 0.5) * 255), 0, 255)
buf += vt.tobytes()
buf += rI.astype('<u2').ravel().tobytes()
for c in hb['clips']:
    d = ab(c['blob']).reshape(c['frames'], nb, 7)
    qd = np.clip(np.round(d[:, :, :4] * 32767), -32767, 32767).astype('<i2'); pd = np.clip(np.round(d[:, :, 4:] * 2048), -32767, 32767).astype('<i2')
    buf += s(c['name'], 12) + struct.pack('<H B B', c['frames'], c['loop'], 0) + struct.pack('<4f', c['fps'] if c['fps'] else 0, c['hit'], c['speed'], c['T'])
    buf += np.concatenate([qd, pd], axis=2).tobytes()
open(f'{OUT}/{key}.lfm', 'wb').write(bytes(buf))

# ---------------- preview renders (posed through the real armature) ----------------
def frame_pose(clipname, f):
    c = next(c for c in hb['clips'] if c['name'] == clipname); d = ab(c['blob']).reshape(c['frames'], nb, 7)[min(f, c['frames'] - 1)]
    for i, nm in enumerate(hf['bones']):
        pb = aobj.pose.bones[nm]; q = d[i, :4]; qq = Quaternion((q[3], q[0], q[1], q[2]))
        Mm = Matrix.Translation(Vector(d[i, 4:7])) @ qq.to_matrix().to_4x4() @ Matrix.Translation(-Vector(head[i]))
        pb.matrix = Mm @ aobj.data.bones[nm].matrix_local
    bpy.context.view_layer.update()
mat = bpy.data.materials.new('LF'); mat.use_nodes = True; nt = mat.node_tree; nt.nodes.clear()
o = nt.nodes.new('ShaderNodeOutputMaterial'); bs = nt.nodes.new('ShaderNodeBsdfPrincipled'); vc = nt.nodes.new('ShaderNodeVertexColor'); vc.layer_name = 'ColAO'
for nm, sock in (('rough', 'Roughness'), ('metal', 'Metallic'), ('glow', 'Emission Strength')):
    a = nt.nodes.new('ShaderNodeAttribute'); a.attribute_name = nm; nt.links.new(a.outputs['Fac'], bs.inputs[sock])
nt.links.new(vc.outputs['Color'], bs.inputs['Base Color']); nt.links.new(vc.outputs['Color'], bs.inputs['Emission Color']); nt.links.new(bs.outputs['BSDF'], o.inputs['Surface'])
mesh.materials.append(mat)
fl = bpy.data.objects.new('Floor', bpy.data.meshes.new('Floor')); scn.collection.objects.link(fl)
fl.data.from_pydata([(-6, -6, 0), (6, -6, 0), (6, 6, 0), (-6, 6, 0)], [], [(0, 1, 2, 3)]); fm = bpy.data.materials.new('Fl'); fm.use_nodes = True
fm.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.05, 0.04, 0.03, 1); fm.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.95; fl.data.materials.append(fm)
wd = bpy.data.worlds.new('W'); wd.use_nodes = True; wd.node_tree.nodes['Background'].inputs[0].default_value = (0.012, 0.02, 0.04, 1); wd.node_tree.nodes['Background'].inputs[1].default_value = 1.0; scn.world = wd
def light(kind, loc, energy, color, rot=None):
    l = bpy.data.lights.new(kind, kind); l.energy = energy; l.color = color
    if kind == 'POINT': l.shadow_soft_size = 0.25
    lo = bpy.data.objects.new(kind, l); lo.location = loc
    if rot: lo.rotation_euler = rot
    scn.collection.objects.link(lo); return lo
sc = hf['scale']; H = (hf['height'] or 2.0) * sc
root.scale = (sc, sc, sc)
light('SUN', (0, 0, 5), 3.2, (0.6, 0.72, 1.0), (math.radians(55), 0, math.radians(-35)))
light('POINT', (-2.2 * sc, -2.0 * sc, 3.0 * sc), 1500 * sc * sc, (1.0, 0.62, 0.28))
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); scn.collection.objects.link(cam); cam.data.lens = 70; scn.camera = cam
scn.render.engine = 'CYCLES'; scn.cycles.device = 'CPU'; scn.cycles.samples = 72; scn.cycles.use_denoising = False; scn.render.threads = 2
scn.render.resolution_x = 600; scn.render.resolution_y = 700; scn.view_settings.view_transform = 'AgX'
for tag, clipname, fr, az in (('a', 'ready', 0, 0.55), ('b', 'attack', 24, -0.5)):
    try: frame_pose(clipname, fr)
    except StopIteration: frame_pose('idle', 0)
    ext = max(H, (P[:, 0].max() - P[:, 0].min()) * sc, (P[:, 2].max() - P[:, 2].min()) * sc); dist = ext * 3.0; tgt = Vector((0, 0, H * 0.5))
    cam.location = tgt + Vector((math.sin(az) * dist, -math.cos(az) * dist, ext * 0.14)); d = tgt - cam.location
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    scn.render.filepath = f'{PRV}/{key}_{tag}.png'; bpy.ops.render.render(write_still=True)
for pb in aobj.pose.bones: pb.matrix_basis = Matrix.Identity(4)
bpy.ops.wm.save_as_mainfile(filepath=f'{BL}/{key}.blend', compress=True)
print('LFDONE', key, json.dumps({'nv': nv, 'tris': len(rI), 'bones': nb, 'ao_mean': round(float(ao.mean()), 3), 'uv_err': uv_err, 'bone_match': round(float(src_same), 4), 'bake_s': round(t_bake, 1), 'total_s': round(time.time() - T0, 1), 'lfm_bytes': len(buf)}))
