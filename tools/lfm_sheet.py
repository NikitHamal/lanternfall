# QA contact sheet straight from the runtime .lfm (reads the file exactly like src/figure.cpp, skins with the runtime's two-bone formula, numpy z-buffer raster).
#   blender -b --factory-startup -P tools/lfm_sheet.py -- <out.png> <file.lfm> [clip@t ...]      (t in 0..1; default = a standard set)
# It validates the shipped bytes (not the Blender scene), so a pass here means the game would see the same pose.
import sys, struct, zlib, math, numpy as np
a = sys.argv[sys.argv.index('--') + 1:]; OUT, PATH, REQ = a[0], a[1], a[2:]

def read_lfm(path):
    b = open(path, 'rb').read(); assert b[:4] == b'LFM1'; nv, nt, nb, nc, nfx, _, _ = struct.unpack_from('<7I', b, 4); o = 32
    scale, radius, hpY, height = struct.unpack_from('<4f', b, o); o = 32 + 16 + 12 + 4 + 12 + 16; names = []
    for i in range(nb): names.append(b[o:o + 12].split(b'\0')[0].decode()); o += 12
    par = np.frombuffer(b, np.int8, nb, o); o += (nb + 3) // 4 * 4; head = np.frombuffer(b, '<f4', nb * 3, o).reshape(nb, 3); o += nb * 12 + (nb + 3) // 4 * 4; o += nfx * 28
    vt = np.frombuffer(b, dtype=[('p', '<f4', 3), ('n', 'i1', 4), ('c', 'u1', 4), ('uv', '<f4', 2), ('m', 'u1', 4), ('b', 'u1', 4)], count=nv, offset=o); o += nv * 36
    idx = np.frombuffer(b, '<u2', nt * 3, o).reshape(-1, 3); o += nt * 6; clips = {}
    for i in range(nc):
        nm = b[o:o + 12].split(b'\0')[0].decode(); fr, lp = struct.unpack_from('<HB', b, o + 12); fps, hit, sp, T = struct.unpack_from('<4f', b, o + 16); o += 32
        d = np.frombuffer(b, '<i2', fr * nb * 7, o).reshape(fr, nb, 7); o += fr * nb * 14
        clips[nm] = dict(frames=fr, loop=lp, hit=hit, speed=sp, T=T, q=d[:, :, :4] / 32767.0, p=d[:, :, 4:] / 2048.0)
    return dict(nv=nv, nt=nt, nb=nb, names=names, par=par, head=head, vt=vt, idx=idx, clips=clips, scale=scale, key=path.split('/')[-1][:-4])

def skin(m, c, f):
    q = c['q'][f]; pp = c['p'][f]; x, y, z, w = q[:, 0], q[:, 1], q[:, 2], q[:, 3]
    R = np.stack([1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), 2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w), 2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)], 1).reshape(-1, 3, 3)
    vt = m['vt']; V = vt['p'].astype(float); A = vt['b'][:, 0].astype(int); B = vt['b'][:, 1].astype(int); Wt = vt['b'][:, 2] / 255.0; h = m['head']
    va = np.einsum('nij,nj->ni', R[A], V - h[A]) + pp[A]; vb = np.einsum('nij,nj->ni', R[B], V - h[B]) + pp[B]
    return va * (1 - Wt)[:, None] + vb * Wt[:, None]

def raster(m, V, W_, H_, yaw=35.0, cx=0.0, cy=0.9, sc=95.0, bg=(0.07, 0.09, 0.12)):
    vt = m['vt']; col = (vt['c'][:, :3] / 255.0) ** 2 * (0.25 + 0.75 * vt['c'][:, 3:4] / 255.0); img = np.zeros((H_, W_, 3)); img[:] = bg; zb = np.full((H_, W_), 1e9)
    for yy in range(H_ // 8 * 7, H_): img[yy] = np.array(bg) * (0.6 + 0.6 * (yy - H_ // 8 * 7) / (H_ // 8))   # ground
    a = math.radians(yaw); ca, sa = math.cos(a), math.sin(a); X = V[:, 0] * ca + V[:, 2] * sa; Z = -V[:, 0] * sa + V[:, 2] * ca
    sx = W_ / 2 + (X - cx) * sc; sy = H_ * .88 - (V[:, 1] - 0.0) * sc; zz = Z; L = np.array([.4, .8, -.45]); L /= np.linalg.norm(L)
    Nn = vt['n'][:, :3] / 127.0; Nx = Nn[:, 0] * ca + Nn[:, 2] * sa; Nz = -Nn[:, 0] * sa + Nn[:, 2] * ca; lam = np.clip(Nx * L[0] + Nn[:, 1] * L[1] + Nz * L[2], 0, 1) * .75 + .35
    shade = np.clip(col * lam[:, None] * 1.9 + np.where(vt['m'][:, 2:3] == 10, 0.5, 0) * col, 0, 1) ** (1 / 2.2)
    for t in m['idx']:
        xs = sx[t]; ys = sy[t]; x0, x1 = int(max(0, math.floor(xs.min()))), int(min(W_ - 1, math.ceil(xs.max()))); y0, y1 = int(max(0, math.floor(ys.min()))), int(min(H_ - 1, math.ceil(ys.max())))
        if x1 < x0 or y1 < y0: continue
        d = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
        if abs(d) < 1e-9: continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
        l0 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / d; l1 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / d; l2 = 1 - l0 - l1
        ins = (l0 >= -1e-4) & (l1 >= -1e-4) & (l2 >= -1e-4)
        if not ins.any(): continue
        z = l0 * zz[t[0]] + l1 * zz[t[1]] + l2 * zz[t[2]]; sub = zb[y0:y1 + 1, x0:x1 + 1]; ok = ins & (z < sub)
        if not ok.any(): continue
        sub[ok] = z[ok]; cc = shade[t[0]][None, None] * l0[..., None] + shade[t[1]][None, None] * l1[..., None] + shade[t[2]][None, None] * l2[..., None]; im = img[y0:y1 + 1, x0:x1 + 1]; im[ok] = cc[ok]
    return img

def png(path, img):
    a8 = (np.clip(img, 0, 1) * 255).astype(np.uint8); h, w, _ = a8.shape; raw = b''.join(b'\0' + a8[y].tobytes() for y in range(h))
    def ch(t, d): c = struct.pack('>I', len(d)) + t + d; return c + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw, 6)) + ch(b'IEND', b''))

m = read_lfm(PATH); cl = m['clips']; req = REQ
if not cl:   # static mesh: identity pose
    cl = m['clips'] = {'static': dict(frames=1, loop=0, hit=0, speed=0, T=1, q=np.tile([0, 0, 0, 1.0], (1, m['nb'], 1)), p=m['head'][None].astype(float))}
    req = req or ['static@0']
if not req:
    pri = ['idle', 'ready', 'walk', 'run', 'attack', 'special', 'bash', 'cleave', 'sunder', 'aimed', 'volley', 'kindle', 'cinder', 'tonic', 'firepot', 'hoar', 'lance', 'guard', 'defend', 'dodge', 'stag', 'hit_l', 'down', 'shatter', 'burn', 'burst', 'gutter']
    req = []
    for n in pri:
        if n in cl: req.append(f'{n}@{cl[n]["hit"] if cl[n]["hit"] > .01 else (.5 if n in ("walk", "run", "stag", "defend", "dodge", "shatter", "burn", "burst", "gutter") else (1.0 if n == "down" else 0.0))}')
    req = req[:12]
V0 = skin(m, cl['ready'] if 'ready' in cl else list(cl.values())[0], 0); hmax = max(1.0, float(V0[:, 1].max()))
TW, TH = (190, 260) if len(cl) > 1 else (560, 360); cols = min(6, len(req)); rows = (len(req) + cols - 1) // cols; sheet = np.zeros((rows * TH, cols * TW, 3)); sc = min(TH * .74 / hmax, TW * .8 / max(1.0, float(np.ptp(V0[:, 0])), float(np.ptp(V0[:, 2]))) if len(cl) == 1 else 1e9)
for i, r in enumerate(req):
    n, t = r.split('@'); c = cl[n]; f = int(round(float(t) * (c['frames'] - 1))); V = skin(m, c, f); img = raster(m, V, TW, TH, cx=0.0, sc=sc)
    sheet[(i // cols) * TH:(i // cols + 1) * TH, (i % cols) * TW:(i % cols + 1) * TW] = img; sheet[(i // cols) * TH, (i % cols) * TW:(i % cols + 1) * TW] = .3
    print(f'tile {i}: {n}@{t} frame {f}/{c["frames"]} bbox y[{V[:, 1].min():.2f},{V[:, 1].max():.2f}] x[{V[:, 0].min():.2f},{V[:, 0].max():.2f}]')
png(OUT, sheet); print('wrote', OUT, sheet.shape, 'clips:', ' '.join(f'{k}:{v["frames"]}' for k, v in cl.items()))
