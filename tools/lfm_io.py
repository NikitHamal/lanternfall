"""Read / patch Lanternfall .lfm figures (see src/figure.cpp loadFig). Vertex = 36 B: pos f32x3, nrm i8x3(+pad), col u8x4 (rgb, ao), uv f32x2, mat u8x4 (rough, metal, type, rustK), bn u8x4 (boneA, boneB, weightB*255, cavity)."""
import struct, numpy as np

VDT = np.dtype([('p', '<f4', 3), ('n', 'i1', 4), ('c', 'u1', 4), ('uv', '<f4', 2), ('m', 'u1', 4), ('b', 'u1', 4)])
assert VDT.itemsize == 36

def read(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'LFM1'
    nv, ntri, nb, ncl, nfx, bnv, bni = struct.unpack('<7I', b[4:32])
    scale, radius, hpY, height = struct.unpack('<4f', b[32:48]); dis = struct.unpack('<3f', b[48:60])
    hero, fly, lant = b[60], b[61], b[62]
    death = b[64:76].split(b'\0')[0].decode(); key = b[76:92].split(b'\0')[0].decode()
    o = 92
    names = [b[o + i * 12:o + i * 12 + 12].split(b'\0')[0].decode() for i in range(nb)]; o += nb * 12
    parent = [struct.unpack('b', b[o + i:o + i + 1])[0] for i in range(nb)]; o += (nb + 3) // 4 * 4
    head = np.frombuffer(b, '<f4', nb * 3, o).reshape(nb, 3).copy(); o += nb * 12 + (nb + 3) // 4 * 4
    o += nfx * 28
    vo = o
    v = np.frombuffer(b, VDT, nv, vo).copy(); io = vo + nv * 36
    idx = np.frombuffer(b, '<u2', ntri * 3, io).reshape(-1, 3).copy()
    return dict(raw=b, nv=nv, ntri=ntri, nb=nb, scale=scale, radius=radius, hpY=hpY, height=height, names=names, parent=parent, head=head,
                v=v, idx=idx, vo=vo, io=io, bnv=bnv, bni=bni, lant=lant, key=key, nclips=ncl, nfx=nfx, hero=hero)

def write_like(donor, v, idx, key, out, height=None, hpY=None, radius=None, scale=None):
    b = bytearray(donor['raw'][:donor['vo']])
    struct.pack_into('<7I', b, 4, len(v), len(idx), donor['nb'], donor['nclips'], donor['nfx'], len(v), len(idx) * 3)
    if scale is not None: struct.pack_into('<f', b, 32, scale)
    if height is not None: struct.pack_into('<f', b, 44, height)
    if hpY is not None: struct.pack_into('<f', b, 40, hpY)
    if radius is not None: struct.pack_into('<f', b, 36, radius)
    b[62] = 0
    k = key.encode()[:15].ljust(16, b'\0'); b[76:92] = k
    tail = donor['raw'][donor['io'] + donor['ntri'] * 6:]
    open(out, 'wb').write(bytes(b) + v.tobytes() + idx.astype('<u2').tobytes() + tail)
