"""Sketchfab GLB -> Lanternfall .lfm figure (humans re-rigged on the native 28-bone biped rig; animals on the 13-bone quadruped rig).

  python3 tools/glb_to_lfm.py human <name> [--pose]      e.g. strong_knight   -> assets/people/<name>.lfm
  python3 tools/glb_to_lfm.py animal <name>              e.g. cow             -> assets/animals/<name>.lfm

Pipeline per model (all numpy / scipy / fast-simplification, no GPU):
 1. read GLB, bake rest pose, sample baseColor textures at every source vertex (mip-reduced, bilinear)
 2. quadric-decimate to a per-role triangle budget (fast_simplification), transfer colour from the nearest source vertices (filtered, not point sampled)
 3. fit to the donor rig (height, pelvis, shoulders) and compute 2-bone capsule skin weights against the donor skeleton
 4. bake cavity + AO proxies from mesh curvature, quantise to the 36 B vertex, and splice into the donor .lfm (rig + baked clips are reused)
"""
import sys, os, json, colorsys
import numpy as np
from scipy.spatial import cKDTree
import fast_simplification as fs

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lfm_io
from glb_lib import load_glb

ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, 'source_models')

# name: (file, role, tris, height m, flip(yaw 180), metal, rough, mat type, extra tint)
HUMANS = {
    'strong_knight': dict(f='strong_knight.glb', tris=16000, H=1.86, flip=False, metal=.45, rough=.5, ty=12),
    'dark_knight': dict(f='dark_knight.glb', tris=20000, H=1.92, flip=False, metal=.5, rough=.45, ty=12),
    'crimson_vanguard': dict(f='crimson_vanguard.glb', tris=22000, H=1.78, flip=False, metal=.3, rough=.55, ty=12),
    'village_head': dict(f='village_head.glb', tris=9000, H=1.74, flip=False, metal=0.0, rough=.8, ty=12),
    'farmer': dict(f='farmer.glb', tris=8000, H=1.72, flip=False, metal=0.0, rough=.85, ty=12),
    'valkyrie': dict(f='ironbound_valkyrie.glb', tris=22000, H=1.86, flip=False, metal=.4, rough=.5, ty=12),
    'obsidian_warden': dict(f='obsidian_warden_shadow_hunter.glb', tris=22000, H=2.0, flip=False, metal=.45, rough=.5, ty=12),
    'winged_sentinel': dict(f='winged_sentinel.glb', tris=24000, H=2.1, flip=False, metal=.4, rough=.5, ty=12),
    'female_warrior': dict(f='low_poly_female_warrior_character_3d_model.glb', tris=16000, H=1.74, flip=False, metal=.3, rough=.6, ty=12),
    'artorias': dict(f='knight_artorias_and_great_grey_wolf_sif.glb', tris=20000, H=2.05, flip=False, metal=.45, rough=.5, ty=12),
}


def lin2store(c):
    """linear rgb [0,1] -> stored u8 (shader squares the stored value => stored = sqrt(linear))."""
    return (np.sqrt(np.clip(c, 0, 1)) * 255 + .5).astype(np.uint8)


def decimate(P, T, tris):
    if len(T) <= tris:
        return P.copy(), T.copy()
    Pd, Td = fs.simplify(P.astype(np.float32), T.astype(np.int32), target_reduction=1 - tris / len(T), agg=6)
    return Pd.astype(np.float32), Td.astype(np.int32)


def clean(P, T):
    """drop unreferenced verts / degenerate tris, return compact arrays."""
    ok = (T[:, 0] != T[:, 1]) & (T[:, 1] != T[:, 2]) & (T[:, 0] != T[:, 2])
    T = T[ok]
    used = np.unique(T)
    remap = -np.ones(len(P), np.int64)
    remap[used] = np.arange(len(used))
    return P[used], remap[T].astype(np.int32), used


def vnormals(P, T):
    f = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    N = np.zeros_like(P, dtype=np.float64)
    for k in range(3):
        np.add.at(N, T[:, k], f)
    return (N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-12)).astype(np.float32)


def transfer_color(Psrc, Csrc, Pdst, k=6):
    tree = cKDTree(Psrc)
    d, i = tree.query(Pdst, k=k)
    w = 1.0 / (d + 1e-4)
    w /= w.sum(1, keepdims=True)
    return (Csrc[i] * w[:, :, None]).sum(1)


def curvature(P, T, N):
    """umbrella-operator curvature along the normal: >0 convex ridge, <0 concave crease. Returns value normalised to ~[-1,1]."""
    n = len(P)
    acc = np.zeros((n, 3)); cnt = np.zeros(n)
    for a, b in ((0, 1), (1, 2), (2, 0)):
        np.add.at(acc, T[:, a], P[T[:, b]]); np.add.at(cnt, T[:, a], 1)
        np.add.at(acc, T[:, b], P[T[:, a]]); np.add.at(cnt, T[:, b], 1)
    avg = acc / np.maximum(cnt, 1)[:, None]
    el = np.sqrt(((P[T[:, 0]] - P[T[:, 1]]) ** 2).sum(1)).mean()
    c = -((avg - P) * N).sum(1) / max(el, 1e-6)
    return np.clip(c * 1.6, -1, 1)


def fit_colors(col, sat=1.18, gain=1.0):
    """lift flat Sketchfab albedo slightly: saturation + keep dark metals from crushing in the HDR chain."""
    g = col.mean(1, keepdims=True)
    c = g + (col - g) * sat
    return np.clip(c * gain, 0, 1)


def hue_shift(col, mask, dh, ds=1.0, dv=1.0):
    out = col.copy()
    idx = np.where(mask)[0]
    for i in idx:
        h, s, v = colorsys.rgb_to_hsv(*np.clip(np.sqrt(col[i]), 0, 1))
        r, g, b = colorsys.hsv_to_rgb((h + dh) % 1, min(1, s * ds), min(1, v * dv))
        out[i] = np.array([r, g, b]) ** 2
    return out


# ---------------------------------------------------------------- humans
BN = ['root', 'hips', 'spine', 'chest', 'neck', 'head', 'lUp', 'lFore', 'lHand', 'rUp', 'rFore', 'rHand', 'lThigh', 'lShin', 'lFoot', 'rThigh', 'rShin', 'rFoot']


def human_skeleton(donor, P, H):
    """donor heads (arms down, 1.8 m) with arms re-fitted to the model silhouette."""
    h = donor['head'].astype(np.float64).copy()
    sc = H / 1.8
    # arm fit: hand x from the lateral extent of the lower-arm band
    band = (P[:, 1] > 0.40 * H) & (P[:, 1] < 0.52 * H)
    ax = np.abs(P[band, 0])
    tw = 0.135 * H
    outer = ax[ax > tw]
    hx = np.median(outer) if len(outer) > 30 else 0.2 * H
    hx = float(np.clip(hx, 0.15 * H, 0.30 * H))
    sx = 0.118 * H
    for sgn, (u, f, hd) in ((1, (6, 7, 8)), (-1, (9, 10, 11))):
        h[u] = [sgn * sx, 0.80 * H, 0.0]
        h[hd] = [sgn * hx, 0.485 * H, 0.0]
        h[f] = (h[u] + h[hd]) / 2 + [0, 0.012 * H, 0]
    return h


SEG = {  # bone: (head idx, tail idx or offset vector, radius/H)
    'hips': ('hips', 'spine', .105), 'spine': ('spine', 'chest', .095), 'chest': ('chest', 'neck', .105), 'neck': ('neck', 'head', .045), 'head': ('head', (0, .12, 0.01), .075),
    'lUp': ('lUp', 'lFore', .042), 'lFore': ('lFore', 'lHand', .036), 'lHand': ('lHand', (0, -.10, 0), .04),
    'rUp': ('rUp', 'rFore', .042), 'rFore': ('rFore', 'rHand', .036), 'rHand': ('rHand', (0, -.10, 0), .04),
    'lThigh': ('lThigh', 'lShin', .062), 'lShin': ('lShin', 'lFoot', .045), 'lFoot': ('lFoot', (0, -.09, .13), .05),
    'rThigh': ('rThigh', 'rShin', .062), 'rShin': ('rShin', 'rFoot', .045), 'rFoot': ('rFoot', (0, -.09, .13), .05),
}


def seg_dist(P, a, b):
    ab = b - a
    t = np.clip(((P - a) @ ab) / max(ab @ ab, 1e-9), 0, 1)
    c = a + t[:, None] * ab
    return np.linalg.norm(P - c, axis=1)


def human_weights(P, heads, H):
    names = ['root', 'hips', 'spine', 'chest', 'neck', 'head', 'lUp', 'lFore', 'lHand', 'rUp', 'rFore', 'rHand', 'lThigh', 'lShin', 'lFoot', 'rThigh', 'rShin', 'rFoot']
    ix = {n: i for i, n in enumerate(names)}
    cost = np.full((len(P), len(names)), 1e9)
    for b, (hn, tn, r) in SEG.items():
        a = heads[ix[hn]]
        t = heads[ix[tn]] if isinstance(tn, str) else a + np.array(tn) * (H / 1.8)
        d = seg_dist(P, a, t) / (r * H)
        cost[:, ix[b]] = d
    # arm gate: vertices clearly outboard of the torso may only take arm bones; inboard ones may not take arm bones below the shoulder
    ax = np.abs(P[:, 0])
    arm_ids = [ix[n] for n in ('lUp', 'lFore', 'lHand', 'rUp', 'rFore', 'rHand')]
    side = P[:, 0] > 0
    for j in arm_ids:
        wrong = (side & (j in (ix['rUp'], ix['rFore'], ix['rHand']))) | (~side & (j in (ix['lUp'], ix['lFore'], ix['lHand'])))
        cost[:, j] = np.where(wrong, 1e9, cost[:, j])
    leg_ids = [ix[n] for n in ('lThigh', 'lShin', 'lFoot', 'rThigh', 'rShin', 'rFoot')]
    for j in leg_ids:
        right = j in (ix['rThigh'], ix['rShin'], ix['rFoot'])
        cost[:, j] = np.where((P[:, 0] > 0.02 * H) & right | (P[:, 0] < -0.02 * H) & (~right), cost[:, j] + 1.5, cost[:, j])
    # head crest etc above the neck are head; everything above neck joint -> head/neck only
    top = P[:, 1] > heads[ix['head']][1] - 0.02
    for j in range(len(names)):
        if names[j] not in ('head', 'neck'):
            cost[top, j] += 4
    order = np.argsort(cost, axis=1)[:, :2]
    ca = np.take_along_axis(cost, order[:, :1], 1)[:, 0]
    cb = np.take_along_axis(cost, order[:, 1:2], 1)[:, 0]
    wa = np.exp(-ca ** 2 * 1.2) + 1e-6
    wb = np.exp(-cb ** 2 * 1.2) * 0.9 + 1e-6
    # blend weight of the second bone (shader: mix(boneA, boneB, w))
    wB = wb / (wa + wb)
    wB = np.where(wB < 0.04, 0, wB)
    return order[:, 0].astype(np.uint8), order[:, 1].astype(np.uint8), wB.astype(np.float32)


def pose_preview(P, T, C, bA, bB, wB, heads, out):
    """quick CPU skinning check: raise the left arm, bend right knee, twist the spine."""
    from PIL import Image
    from glb_preview import splat, vnormals as vn
    def rot(axis, ang):
        axis = np.array(axis, float); axis /= np.linalg.norm(axis); K = np.array([[0, -axis[2], axis[1]], [axis[2], 0, -axis[0]], [-axis[1], axis[0], 0]])
        return np.eye(3) + np.sin(ang) * K + (1 - np.cos(ang)) * K @ K
    n = len(BN)
    R = [np.eye(3) for _ in range(n)]
    ix = {k: i for i, k in enumerate(BN)}
    R[ix['lUp']] = rot([0, 0, 1], -1.4)  # swing left arm outwards/up
    R[ix['rUp']] = rot([1, 0, 0], -1.2)  # right arm forward
    R[ix['rThigh']] = rot([1, 0, 0], -0.9); R[ix['rShin']] = rot([1, 0, 0], 1.1)
    R[ix['spine']] = rot([0, 1, 0], 0.5)
    par = {'hips': 'root', 'spine': 'hips', 'chest': 'spine', 'neck': 'chest', 'head': 'neck', 'lUp': 'chest', 'lFore': 'lUp', 'lHand': 'lFore', 'rUp': 'chest', 'rFore': 'rUp', 'rHand': 'rFore',
           'lThigh': 'hips', 'lShin': 'lThigh', 'lFoot': 'lShin', 'rThigh': 'hips', 'rShin': 'rThigh', 'rFoot': 'rShin'}
    Wm = {}; Tm = {}
    for b in BN:
        i = ix[b]
        if b == 'root':
            Wm[b] = np.eye(3); Tm[b] = np.zeros(3); continue
        p = par[b]
        Wm[b] = Wm[p] @ R[i]
        Tm[b] = Tm[p] + Wm[p] @ (heads[i] - heads[ix[p]]) + (heads[ix[p]] - Tm[p] * 0) * 0
    # world transform of bone i about its head: x' = Tm_head + Wm (x - head)
    Hh = {}
    for b in BN:
        i = ix[b]
        if b == 'root': Hh[b] = heads[i]; continue
        p = par[b]; Hh[b] = Hh[p] + Wm[p] @ (heads[i] - heads[ix[p]])
    def skin(bi):
        out = np.zeros_like(P)
        for i, b in enumerate(BN):
            m = bi == i
            if m.any(): out[m] = Hh[b] + (P[m] - heads[i]) @ Wm[b].T
        return out
    Pp = skin(bA) * (1 - wB[:, None]) + skin(bB) * wB[:, None]
    N = vn(Pp, T)
    ims = [splat(Pp, C, N, a, 360, 480) for a in (np.pi, np.pi * 0.6)]
    Image.fromarray(np.concatenate(ims, 1)).save(out)


def drop_outliers(P, T, Cl, H):
    """remove stray pieces (props authored at another scale, floating FX cards) far outside the body box"""
    bad = (P[:, 1] > 1.35 * H) | (P[:, 1] < -0.1 * H) | (np.abs(P[:, 0]) > 0.9 * H) | (np.abs(P[:, 2]) > 0.9 * H)
    if not bad.any(): return P, T, Cl
    keep = ~bad[T].any(1)
    return P, T[keep], Cl


def arms_down(P, H):
    """T/A-pose normalisation: rotate each arm (in the XY plane about the shoulder) towards ~10 deg off vertical when the sculpt holds it > 28 deg out.
    Torso vertices are untouched (weight is a function of position along the arm axis and distance from it)."""
    for sgn in (1, -1):
        sh = np.array([sgn * 0.118 * H, 0.80 * H])
        m = (np.sign(P[:, 0]) == sgn) & (P[:, 1] > 0.25 * H) & (P[:, 1] < 0.92 * H) & (np.abs(P[:, 0]) > 0.19 * H)
        if m.sum() < 50: continue
        far = np.argsort(-np.abs(P[m, 0]))[:max(30, m.sum() // 30)]
        tip = P[m][far][:, :2].mean(0)
        d = tip - sh; L = np.linalg.norm(d)
        ang = np.degrees(np.arctan2(abs(d[0]), -d[1]))
        if ang < 28 or L < 0.2 * H: continue
        da = np.radians(ang - 10.0) * -sgn * np.sign(1)  # rotate so the hand swings down (towards the body)
        dh = d / L
        rel = P[:, :2] - sh
        t = (rel @ dh) / L
        lat = np.abs(rel[:, 0] * dh[1] - rel[:, 1] * dh[0])
        w = np.clip((t - 0.12) / 0.25, 0, 1); w = w * w * (3 - 2 * w)
        w *= np.clip(1 - (lat - 0.09 * H) / (0.07 * H), 0, 1) * (t > 0)
        a = da * w
        # signs: left arm (x>0) rotates clockwise seen from +z when moving down
        c, sn = np.cos(a), np.sin(a)
        x = rel[:, 0] * c - rel[:, 1] * sn; y = rel[:, 0] * sn + rel[:, 1] * c
        P[:, 0] = sh[0] + x; P[:, 1] = sh[1] + y
        print(f'  arms {"L" if sgn > 0 else "R"}: {ang:.0f} deg -> 10')


def build_human(name, pose=False):
    cfg = HUMANS[name]
    g = load_glb(os.path.join(SRC, cfg['f']), 1024)
    P, T, Cl = g['P'].astype(np.float64), g['T'], g['Cl']
    ref = np.percentile(P[:, 1], 95) - np.percentile(P[:, 1], 1)   # stray props authored at another scale would wreck the height fit
    bad = (P[:, 1] > np.percentile(P[:, 1], 1) + 1.5 * ref) | (np.abs(P[:, 0]) > 1.5 * ref) | (np.abs(P[:, 2]) > 1.5 * ref)
    if bad.any(): T = T[~bad[T].any(1)]
    if name == 'artorias':  # file contains Artorias (skin 0) and Sif the wolf: keep the biped (taller than wide) cluster
        pass
    # normalise: feet on y=0, centre x/z on the torso, scale to H
    Pu = P[np.unique(T)]   # height fit from vertices that survive the stray-geometry cut (farmer.glb carries props ~40 m away)
    lo, hi = np.percentile(Pu[:, 1], .05), np.percentile(Pu[:, 1], 99.95)
    P[:, 1] -= lo
    # exclude stray weapons/wings from the height estimate via percentile
    Hm = hi - lo
    s = cfg['H'] / Hm
    P *= s
    tor = (P[:, 1] > .45 * cfg['H']) & (P[:, 1] < .7 * cfg['H'])
    P[:, 0] -= np.median(P[tor, 0]); P[:, 2] -= np.median(P[tor, 2]) - 0.0
    if cfg['flip']:
        P[:, 0] *= -1; P[:, 2] *= -1
    P, T, Cl = drop_outliers(P, T, Cl, cfg['H'])
    arms_down(P, cfg['H'])
    Pd, Td = decimate(P.astype(np.float32), T, cfg['tris'])
    Pd, Td, used = clean(Pd, Td)
    Cd = transfer_color(P, Cl, Pd)
    Cd = fit_colors(Cd)
    lum = Cd.mean(1); p90 = np.percentile(lum, 90)
    gain = float(np.clip(0.30 / max(p90, 1e-3), 0.55, 3.5))   # auto exposure: very dark PBR albedo is lifted, pure-white placeholder albedo is pulled to steel
    Cd = np.clip(Cd * gain, 0, 1)
    N = vnormals(Pd, Td)
    cur = curvature(Pd, Td, N)
    donor = lfm_io.read(os.path.join(ROOT, 'assets', 'warden.lfm'))
    heads = human_skeleton(donor, Pd, cfg['H'])
    bA, bB, wB = human_weights(Pd.astype(np.float64), heads, cfg['H'])
    nv = len(Pd)
    v = np.zeros(nv, lfm_io.VDT)
    # donor frame = 1.8 m figure: the rig is used at donor scale, so shrink to 1.8 and let FigAsset.scale restore the size
    k = 1.8 / cfg['H']
    v['p'] = Pd * k
    v['n'][:, :3] = np.clip(N * 127, -127, 127).astype(np.int8)
    v['c'][:, :3] = lin2store(Cd)
    v['c'][:, 3] = (np.clip(1 + np.minimum(cur, 0) * .55, .45, 1) * 255).astype(np.uint8)
    v['uv'] = Pd[:, [0, 2]] * 0.0
    v['m'][:, 0] = int(cfg['rough'] * 255); v['m'][:, 1] = int(cfg['metal'] * 255); v['m'][:, 2] = cfg['ty']; v['m'][:, 3] = 0
    v['b'][:, 0] = bA; v['b'][:, 1] = bB; v['b'][:, 2] = (wB * 255 + .5).astype(np.uint8); v['b'][:, 3] = ((np.clip(cur, -1, 1) * .5 + .5) * 255).astype(np.uint8)
    out = os.path.join(ROOT, 'assets', 'people'); os.makedirs(out, exist_ok=True)
    key = name[:15]
    lfm_io.write_like(donor, v, Td.astype(np.uint16), key, os.path.join(out, name + '.lfm'), height=1.8, hpY=1.8, scale=cfg['H'] / 1.8)
    sz = os.path.getsize(os.path.join(out, name + '.lfm'))
    print(f'{name}: gain {gain:.2f} src {len(g["P"])}v/{len(g["T"])}t -> {nv}v/{len(Td)}t, {sz / 1e6:.2f} MB, hand x {abs(heads[8][0]):.2f}, scale {cfg["H"] / 1.8:.3f}')
    if pose:
        pose_preview(v['p'].astype(np.float64), Td, Cd, bA, bB, wB, heads * k, os.path.join('/tmp/agentA_cache', name + '_pose.png'))
    return cfg['H'] / 1.8


if __name__ == '__main__':
    kind, name = sys.argv[1], sys.argv[2]
    if kind == 'human':
        build_human(name, '--pose' in sys.argv)
    elif kind == 'animal':
        import glb_animals
        glb_animals.build(name)
