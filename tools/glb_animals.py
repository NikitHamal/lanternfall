"""Quadruped sculpts -> .lfm on the native quadruped rig (hollowstag / gloamhound donor: 21 bones, baked idle/walk/run clips reused)."""
import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lfm_io
from glb_lib import load_glb
from glb_to_lfm import decimate, clean, transfer_color, vnormals, curvature, fit_colors, lin2store, seg_dist, ROOT, SRC

ANIMALS = {
    'stag': dict(f='stag_realistic.glb', donor='hollowstag', tris=10000, metal=0.0, rough=.85, ty=12, tint=(1.0, .92, .8)),
    'husky': dict(f='realistic_3d_husky_dog_model.glb', donor='gloamhound', tris=9000, metal=0.0, rough=.85, ty=12, tint=(1, 1, 1), fallback=(.62, .58, .55)),
    'horse_bay': dict(f='balt_horse_12-13th_century.glb', donor='hollowstag', tris=14000, metal=0.0, rough=.8, ty=12, tint=(1, 1, 1)),
    'horse_black': dict(f='midnight_black_horse_3d_model_free.glb', donor='hollowstag', tris=16000, metal=0.0, rough=.7, ty=12, tint=(1, 1, 1)),
    'cow': dict(f='brown_cow_3d_model.glb', donor='gloamhound', tris=9000, metal=0.0, rough=.85, ty=12, tint=(1, 1, 1)),
    'sheep': dict(f='sheep.glb', donor='gloamhound', tris=6000, metal=0.0, rough=.95, ty=12, tint=(1, 1, 1)),
    'cat': dict(f='cat_in_motion_3d_model_free.glb', donor='gloamhound', tris=6000, metal=0.0, rough=.85, ty=12, tint=(1, 1, 1)),
    'tiger': dict(f='old_tiger_3d_model.glb', donor='gloamhound', tris=12000, metal=0.0, rough=.85, ty=12, tint=(1, 1, 1)),
}
RAD = {'hips': .24, 'spine': .22, 'chest': .24, 'neck': .12, 'head': .13, 'jaw': .08, 'tail1': .07, 'tail2': .05,
       'flU': .11, 'flL': .06, 'flP': .05, 'frU': .11, 'frL': .06, 'frP': .05, 'hlU': .12, 'hlL': .06, 'hlP': .05, 'hrU': .12, 'hrL': .06, 'hrP': .05}


def build(name):
    cfg = ANIMALS[name]
    g = load_glb(os.path.join(SRC, cfg['f']), 1024)
    P, T, Cl = g['P'].astype(np.float64), g['T'], g['Cl']
    donor = lfm_io.read(os.path.join(ROOT, 'assets', cfg['donor'] + '.lfm'))
    dv = donor['v']['p'][:donor['bnv']]
    # forward axis: the longest horizontal axis, head end = the end whose top vertices are higher
    ext = P.max(0) - P.min(0)
    ax = 0 if ext[0] > ext[2] else 2
    if ax == 0:  # rotate so length runs along z
        P = np.stack([P[:, 2], P[:, 1], -P[:, 0]], 1)
    z = P[:, 2]; zl, zh = np.percentile(z, [2, 98])
    def topy(m): return np.percentile(P[m, 1], 90) if m.sum() > 10 else 0
    if topy(z > zh - 0.15 * (zh - zl)) < topy(z < zl + 0.15 * (zh - zl)):
        P[:, 0] *= -1; P[:, 2] *= -1
    z = P[:, 2]; zl, zh = np.percentile(z, [2, 98])
    P[:, 1] -= np.percentile(P[:, 1], 0.1)
    dl, dh = np.percentile(dv[:, 2], [2, 98])
    s = (dh - dl) / (zh - zl)
    P *= s
    z = P[:, 2]; P[:, 2] += (dl + dh) / 2 - (np.percentile(z, 2) + np.percentile(z, 98)) / 2
    P[:, 0] -= np.median(P[:, 0])
    # weld split vertices (UV / normal seams) so the quadric decimator sees one closed surface, not shards
    q = np.round(P / 0.0005).astype(np.int64)
    _, first, inv = np.unique(q, axis=0, return_index=True, return_inverse=True)
    Pw = P[first]; Tw = inv.reshape(-1)[T]; Tw = Tw[(Tw[:, 0] != Tw[:, 1]) & (Tw[:, 1] != Tw[:, 2]) & (Tw[:, 0] != Tw[:, 2])]
    Pd, Td = decimate(Pw.astype(np.float32), Tw.astype(np.int32), cfg['tris'])
    Pd, Td, used = clean(Pd, Td)
    Cd = transfer_color(P, Cl, Pd)
    if g['Cl'].std() < 1e-3 and 'fallback' in cfg: Cd = np.tile(np.array(cfg['fallback']) ** 2.2, (len(Pd), 1))
    Cd = fit_colors(Cd) * np.array(cfg['tint'])
    p90 = np.percentile(Cd.mean(1), 90); Cd = np.clip(Cd * float(np.clip(0.34 / max(p90, 1e-3), .6, 3.0)), 0, 1)
    N = vnormals(Pd, Td); cur = curvature(Pd, Td, N)
    names, par, heads = donor['names'], donor['parent'], donor['head'].astype(np.float64)
    nb = len(names)
    cost = np.full((len(Pd), nb), 1e9)
    for b, n in enumerate(names):
        if n not in RAD: continue
        kids = [i for i in range(nb) if par[i] == b and names[i] in RAD]
        a = heads[b]
        t = heads[kids[0]] if kids else a + np.array([0, -0.06, 0.0]) if n.endswith('P') else (heads[kids[0]] if kids else a + np.array([0, 0, -0.2 if n.startswith('tail') else 0.12]))
        cost[:, b] = seg_dist(Pd.astype(np.float64), a, t) / RAD[n]
    pd_ = Pd.astype(np.float64)
    for b, n in enumerate(names):
        if len(n) == 3 and n[0] in 'fh' and n[1] in 'lr':
            left = n[1] == 'l'
            wrong = (pd_[:, 0] < -0.01) if left else (pd_[:, 0] > 0.01)
            cost[wrong, b] += 2.0
    order = np.argsort(cost, 1)[:, :2]
    ca = np.take_along_axis(cost, order[:, :1], 1)[:, 0]; cb = np.take_along_axis(cost, order[:, 1:2], 1)[:, 0]
    wa = np.exp(-ca ** 2 * 1.2) + 1e-6; wb = np.exp(-cb ** 2 * 1.2) * .9 + 1e-6
    wB = wb / (wa + wb); wB = np.where(wB < .04, 0, wB)
    v = np.zeros(len(Pd), lfm_io.VDT)
    v['p'] = Pd; v['n'][:, :3] = np.clip(N * 127, -127, 127).astype(np.int8)
    v['c'][:, :3] = lin2store(Cd); v['c'][:, 3] = (np.clip(1 + np.minimum(cur, 0) * .55, .45, 1) * 255).astype(np.uint8)
    v['m'][:, 0] = int(cfg['rough'] * 255); v['m'][:, 1] = int(cfg['metal'] * 255); v['m'][:, 2] = cfg['ty']
    v['b'][:, 0] = order[:, 0]; v['b'][:, 1] = order[:, 1]; v['b'][:, 2] = (wB * 255 + .5).astype(np.uint8); v['b'][:, 3] = ((np.clip(cur, -1, 1) * .5 + .5) * 255).astype(np.uint8)
    out = os.path.join(ROOT, 'assets', 'animals'); os.makedirs(out, exist_ok=True)
    lfm_io.write_like(donor, v, Td.astype(np.uint16), name, os.path.join(out, name + '.lfm'), height=float(Pd[:, 1].max()))
    print(f'{name}: {len(g["P"])}v -> {len(Pd)}v/{len(Td)}t  {os.path.getsize(os.path.join(out, name + ".lfm")) / 1e6:.2f} MB  scale {s:.3f}')


if __name__ == '__main__':
    for n in (sys.argv[1:] or list(ANIMALS)):
        build(n)
