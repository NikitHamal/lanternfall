# LANTERNFALL asset pipeline, Polyfork -> Blender -> game (.lfm + preview .glb)
#   blender -b --factory-startup -P tools/blender_build_assets.py -- <key> [<key> ...]      (no keys = everything in the manifest)
# Per asset: import Polyfork GLB, extract world-space mesh + skin, re-palette to the game's colour/material vocabulary (per-vertex rough/metal/surf/grime),
# rebuild a game-named armature (root/hips/spine/.../wR/wL; quad + wisp rigs), resample Polyfork's own Idle/Walk/Run and AUTHOR the missing state clips
# (ready, attacks, skills, defend, dodge, hit_f/l/r, stag, down, five death variants) as real Blender Actions on that armature, bake AO + edge cavity
# (in the hang pose, ground contact included), then write the runtime .lfm (same LFM1 layout as v1.1.0) and a preview GLB with every clip.
import bpy, sys, os, json, math, struct, time
import numpy as np
from mathutils import Vector, Matrix, Quaternion
from mathutils.bvhtree import BVHTree

REPO = os.environ.get('LF_REPO', os.getcwd())
OUT = os.environ.get('LF_OUT', os.path.join(REPO, 'assets_build'))
LFM_DIR = os.environ.get('LF_LFM', os.path.join(REPO, 'assets', 'polyfork'))
for d in (OUT + '/glb', OUT + '/viewer', LFM_DIR): os.makedirs(d, exist_ok=True)
MAN = json.load(open(REPO + '/assets_raw/manifest.json'))['assets']
args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
KEYS = args or list(MAN)
FPS = 24.0

# ------------------------------------------------------------------ style spec (see ASSETS_STATUS.md)
def hex_lin(h):
    c = np.array([(h >> 16) & 255, (h >> 8) & 255, h & 255]) / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
SK, CL, QU, MA, LE, PL, FU, WO, ST, PE, GL, BO = range(12)   # T_SKIN .. T_BONE
# name: (hex, surf, rough, metal, grime)
PAL = {
 'mail': (0x70757a, MA, .55, .85, .5), 'plate': (0x8b8f93, PL, .45, .9, .4), 'iron': (0x34353a, PL, .55, .8, .6), 'brass': (0x8a6a34, PL, .45, .9, .3),
 'rust': (0x3d3e42, PL, .8, .5, 1.0), 'linen': (0x857a60, CL, .9, 0, .6), 'buff': (0x6e6148, QU, .9, 0, .7), 'murrey': (0x4a1e22, CL, .9, 0, .6),
 'bone': (0x6e6857, BO, .8, 0, .5), 'pale': (0x8f8a72, BO, .75, 0, .4), 'thorn': (0x6a5a40, BO, .8, 0, .6),
 'wool': (0x222a27, CL, .95, 0, .8), 'woolBrown': (0x3c3026, CL, .95, 0, .8), 'woolGrey': (0x44474a, CL, .95, 0, .8), 'black': (0x1a1a1c, CL, .9, 0, .6),
 'leather': (0x40291a, LE, .7, 0, .6), 'leatherDk': (0x271b13, LE, .62, 0, .5), 'leatherTan': (0x6a4a2c, LE, .7, 0, .6), 'waxed': (0x25221c, LE, .55, 0, .5),
 'skin': (0x6e5040, SK, .75, 0, .5), 'skinDk': (0x3a2a22, SK, .75, 0, .5), 'hair': (0x2a2018, FU, .9, 0, .5), 'hairGrey': (0x6a665e, FU, .9, 0, .5),
 'fur': (0x4c3b2c, FU, .95, 0, .8), 'furGrey': (0x5a5650, FU, .95, 0, .8), 'wood': (0x5e4a32, WO, .9, 0, .6), 'woodDk': (0x3a2c1e, WO, .9, 0, .7), 'rope': (0x6a5c40, CL, .92, 0, .6),
 'stone': (0x4a4a46, ST, .93, 0, 0), 'stone2': (0x45474a, ST, .93, 0, 0), 'lichen': (0x33332b, ST, .95, 0, 0),
 'peat': (0x2a221b, PE, .95, 0, .5), 'root': (0x40311f, PE, .9, 0, .5), 'turf': (0x26301a, PE, .95, 0, .5),
 'lantern': (0xffb45a, GL, .5, 0, 1.0), 'gloam': (0xbfffd0, GL, .5, 0, 1.0), 'rime': (0xa8d8ff, GL, .4, 0, 1.0), 'tonic': (0x7ab04a, GL, .5, 0, 1.0),
}
PALN = {k: (hex_lin(v[0]),) + v[1:] for k, v in PAL.items()}
GLOWS = ('lantern', 'gloam', 'rime', 'tonic')
STONE = ['stone', 'stone2', 'lichen', 'peat', 'turf']
THEME = {
 'warden': ['iron', 'plate', 'mail', 'rust', 'leatherDk', 'leather', 'woolGrey', 'murrey', 'brass', 'skinDk', 'black'],
 'reaver': ['iron', 'rust', 'skin', 'skinDk', 'leatherDk', 'leather', 'black', 'brass', 'woolGrey', 'stone', 'lantern'],
 'fletcher': ['wool', 'woolGrey', 'woolBrown', 'black', 'leatherDk', 'leather', 'waxed', 'rope', 'skinDk', 'murrey'],
 'lamplighter': ['murrey', 'wool', 'black', 'leatherDk', 'leatherTan', 'brass', 'skinDk', 'woolBrown', 'lantern'],
 'apothecary': ['woolBrown', 'leather', 'leatherTan', 'linen', 'buff', 'skin', 'skinDk', 'hairGrey', 'turf', 'rope', 'tonic'],
 'rimebinder': ['linen', 'woolGrey', 'pale', 'plate', 'hairGrey', 'murrey', 'skinDk', 'mail', 'rime'],
 'thornling': ['wood', 'woodDk', 'root', 'thorn', 'turf', 'peat', 'tonic'],
 'peatwight': ['peat', 'root', 'turf', 'woodDk', 'skinDk', 'leatherDk'],
 'gloamhound': ['furGrey', 'fur', 'black', 'woolGrey', 'bone', 'skinDk', 'gloam'],
 'gloamwisp': ['linen', 'woolGrey', 'pale', 'gloam'],
 'rustbound': ['rust', 'iron', 'mail', 'plate', 'brass', 'leatherDk', 'woolBrown', 'black'],
 'cairnback': STONE, 'barrowking': ['bone', 'pale', 'iron', 'rust', 'leatherDk', 'murrey', 'black'],
 'mossjaw': ['stone', 'lichen', 'peat', 'turf', 'skinDk', 'furGrey', 'bone'],
 'hollowstag': ['bone', 'pale', 'woodDk', 'wood', 'furGrey', 'fur', 'rime'],
 'emberhound': ['black', 'skinDk', 'iron', 'rust', 'lantern'], 'thornboar': ['fur', 'furGrey', 'thorn', 'turf', 'black', 'bone', 'rime'],
 'rootcolossus': ['wood', 'woodDk', 'root', 'peat', 'turf', 'lantern'],
 'w_arming_sword': ['plate', 'iron', 'brass', 'leatherDk', 'leather'], 'w_greatsword': ['iron', 'plate', 'rust', 'leatherDk'],
 'w_bow': ['wood', 'woodDk', 'leather', 'rope'], 'w_kite_shield': ['wood', 'woodDk', 'iron', 'plate', 'leather', 'murrey', 'brass'],
 'w_round_shield': ['wood', 'woodDk', 'iron', 'leather', 'murrey', 'brass'], 'w_staff': ['wood', 'woodDk', 'rime', 'brass'],
 'w_cleaver': ['iron', 'rust', 'woodDk', 'leatherDk'], 'w_lantern': ['iron', 'brass', 'lantern', 'leatherDk'],
 'p_cairn': STONE, 'p_headstone': STONE, 'p_grave_mound': ['peat', 'turf', 'root', 'stone'], 'p_rune_stone': STONE + ['rime'],
 'p_dead_tree': ['woodDk', 'wood', 'peat', 'root', 'turf'], 'p_twisted_oak': ['woodDk', 'wood', 'peat', 'root', 'turf'],
 'p_split_fence': ['woodDk', 'wood', 'peat', 'rope'], 'p_low_wall': STONE, 'p_field_wall': STONE, 'p_stone_well': STONE + ['woodDk', 'wood', 'rope'],
 'p_brazier': ['iron', 'brass', 'woodDk', 'rust', 'lantern'], 'p_wall_torch': ['iron', 'brass', 'woodDk', 'rust', 'lantern'], 'p_plank_bridge': ['woodDk', 'wood', 'rope', 'peat'],
}
BUILD = ['stone', 'stone2', 'lichen', 'woodDk', 'wood', 'leatherDk', 'woolBrown', 'buff', 'rope', 'iron', 'peat', 'lantern']
for k in MAN:
    if k.startswith('b_'): THEME[k] = BUILD
HERO = dict(scale=1.0, radius=0.4, hpY=1.8, height=2.55, death='fall', H=1.82)
CFG = {k: dict(HERO) for k in ('warden', 'reaver', 'fletcher', 'lamplighter', 'apothecary', 'rimebinder')}
CFG['lamplighter']['height'] = 1.88
for k, v in dict(
  thornling=(.62, .28, 1.05, 1.79, 'burst', 1.69), peatwight=(1.04, .42, 1.85, 1.73, 'burn', 1.78), gloamhound=(1.12, .40, 1.15, 1.06, 'shatter', 1.03),
  gloamwisp=(1.0, .35, 2.2, 1.82, 'gutter', 1.82), rustbound=(1.1, .42, 2.1, 1.86, 'shatter', 1.91), cairnback=(1.5, .75, 2.7, 1.81, 'shatter', 1.8),
  barrowking=(1.95, .85, 3.9, 1.95, 'shatter', 2.0), mossjaw=(2.35, 1.15, 4.3, 2.82, 'shatter', 1.83), hollowstag=(2.2, 1.0, 3.6, 1.99, 'shatter', 1.99),
  emberhound=(1.0, .4, .85, .85, 'burn', .85), thornboar=(1.0, .5, 1.1, 1.0, 'burst', 1.0), rootcolossus=(1.8, 1.0, 3.4, 2.0, 'shatter', 2.0)).items():
    CFG[k] = dict(scale=v[0], radius=v[1], hpY=v[2], height=v[3], death=v[4], H=v[5])
DISCOL = {'burn': (1, .45, .1), 'burst': (.6, 1, .5), 'gutter': (.6, 1, .75), 'shatter': (1, .45, .1), 'fall': (1, .45, .1)}
SKILLS = {  # clip name -> generator kind (first entry is `attack`)
 'warden': [('attack', 'slash1'), ('bash', 'thrust_l'), ('guard', 'block')], 'reaver': [('attack', 'slash2'), ('cleave', 'sweep'), ('sunder', 'overhead')],
 'fletcher': [('attack', 'draw'), ('aimed', 'draw_long'), ('volley', 'draw_high')], 'lamplighter': [('attack', 'slash1'), ('kindle', 'raise1'), ('cinder', 'raise2')],
 'apothecary': [('attack', 'toss'), ('tonic', 'toss'), ('firepot', 'lob'), ('quick', 'raise2')], 'rimebinder': [('attack', 'cast'), ('lance', 'cast'), ('hoar', 'slam')],
 'thornling': [('attack', 'slash1')], 'peatwight': [('attack', 'overhead')], 'rustbound': [('attack', 'sweep')], 'cairnback': [('attack', 'overhead')],
 'barrowking': [('attack', 'slash2'), ('special', 'sweep')], 'mossjaw': [('attack', 'overhead'), ('special', 'slam')], 'rootcolossus': [('attack', 'overhead'), ('special', 'slam')],
 'gloamwisp': [('attack', 'wisp_cast')],
 'gloamhound': [('attack', 'bite')], 'hollowstag': [('attack', 'gore'), ('special', 'charge')], 'emberhound': [('attack', 'bite')], 'thornboar': [('attack', 'gore'), ('special', 'charge')],
}
HEROES = ('warden', 'reaver', 'fletcher', 'lamplighter', 'apothecary', 'rimebinder')
QUADS = ('gloamhound', 'hollowstag', 'emberhound', 'thornboar')

# ------------------------------------------------------------------ small math
C3 = np.array([[1, 0, 0], [0, 0, 1], [0, -1, 0]], float)    # blender (x,y,z) -> game (x, z, -y)
C4 = np.eye(4); C4[:3, :3] = C3
def to_game(v): return v @ C3.T
def from_game(v): return v @ C3          # inverse (orthonormal)
def rotm(rx, ry, rz):                    # degrees, three.js 'YXZ' order: Ry * Rx * Rz
    a, b, c = np.radians([rx, ry, rz]); ca, sa, cb, sb, cc, sc = math.cos(a), math.sin(a), math.cos(b), math.sin(b), math.cos(c), math.sin(c)
    Rx = np.array([[1, 0, 0], [0, ca, -sa], [0, sa, ca]]); Ry = np.array([[cb, 0, sb], [0, 1, 0], [-sb, 0, cb]]); Rz = np.array([[cc, -sc, 0], [sc, cc, 0], [0, 0, 1]])
    return Ry @ Rx @ Rz
def axis_rot(ax, ang):
    ax = np.asarray(ax, float); ax = ax / (np.linalg.norm(ax) + 1e-9); K = np.array([[0, -ax[2], ax[1]], [ax[2], 0, -ax[0]], [-ax[1], ax[0], 0]])
    return np.eye(3) + math.sin(ang) * K + (1 - math.cos(ang)) * K @ K
def m2q(R):
    q = Matrix(R.tolist()).to_quaternion(); return np.array([q.x, q.y, q.z, q.w])
def sstep(x): x = min(1.0, max(0.0, x)); return x * x * (3 - 2 * x)
def srgb_gamma(c): return np.sqrt(np.clip(c, 0, 1))

class Rig:
    def __init__(self, names, parent, head):
        self.names, self.parent, self.head = names, parent, np.asarray(head, float); self.nb = len(names); self.ix = {n: i for i, n in enumerate(names)}

def solve(rig, pose, rt=(0, 0, 0), free=None):
    """world-axis FK: each bone rotates about its posed head, applied after its parent's accumulated transform."""
    nb = rig.nb; A = [None] * nb; Q = np.zeros((nb, 4)); P = np.zeros((nb, 3))
    for b in range(nb):
        h = rig.head[b]; Ap = A[rig.parent[b]] if rig.parent[b] >= 0 else np.eye(4)
        p = (Ap @ np.append(h, 1))[:3]; R = rotm(*pose.get(b, (0, 0, 0)))
        M = np.eye(4); M[:3, :3] = R; M[:3, 3] = p - R @ p
        if b == 0: M[:3, 3] += np.asarray(rt)
        A[b] = M @ Ap
    for b in range(nb):
        Ab = A[b]
        if free is not None and b in free:
            off, ax, ang = free[b]; p = Ab[:3, 3] * 0 + (Ab @ np.append(rig.head[b], 1))[:3]; Rf = axis_rot(ax, ang)
            Mf = np.eye(4); Mf[:3, :3] = Rf; Mf[:3, 3] = p - Rf @ p + off; Ab = Mf @ Ab
        Q[b] = m2q(Ab[:3, :3]); P[b] = (Ab @ np.append(rig.head[b], 1))[:3]
    return Q, P

def fix_signs(Q):    # Q: (F, nb, 4) keep quaternions on one hemisphere over time so the runtime's linear blend never flips
    for f in range(1, len(Q)):
        d = (Q[f] * Q[f - 1]).sum(1) < 0; Q[f][d] *= -1
    return Q

# ------------------------------------------------------------------ pose library (absolute pose, unlisted roles fall back to DEF)
HUM_DEF = dict(lUp=(0, 0, -80), rUp=(0, 0, 80), lFore=(-10, 0, 0), rFore=(-10, 0, 0))
def mix(a, b, t):
    out = {}
    for k in set(a) | set(b):
        if k == 'rt': continue
        x = a.get(k, (0, 0, 0)); y = b.get(k, (0, 0, 0)); out[k] = tuple(x[i] + (y[i] - x[i]) * t for i in range(3))
    xa = a.get('rt', (0, 0, 0)); xb = b.get('rt', (0, 0, 0)); out['rt'] = tuple(xa[i] + (xb[i] - xa[i]) * t for i in range(3)); return out
def evalkeys(keys, u):
    if u <= keys[0][0]: return keys[0][1]
    for i in range(len(keys) - 1):
        if u <= keys[i + 1][0]:
            t = sstep((u - keys[i][0]) / max(1e-6, keys[i + 1][0] - keys[i][0])); return mix(keys[i][1], keys[i + 1][1], t)
    return keys[-1][1]
def W(base, **kw): d = dict(base); d.update(kw); return d
def L(d): return {k: v for k, v in d.items()}

H_READY = dict(rt=(0, -0.05, 0), spine=(6, 0, 0), chest=(4, 0, 0), neck=(-4, 0, 0), head=(-4, 0, 0), lThigh=(-14, 0, 3), lShin=(22, 0, 0), lFoot=(-8, 0, 0),
               rThigh=(10, 0, -3), rShin=(14, 0, 0), rFoot=(-4, 0, 0), lUp=(-35, 0, -62), lFore=(-55, 0, 0), rUp=(-30, 0, 62), rFore=(-60, 0, 0))
LUNGE = dict(lThigh=(-38, 0, 3), lShin=(35, 0, 0), rThigh=(18, 0, -3), rShin=(30, 0, 0))

def human_keys(kind):
    R = H_READY
    if kind in ('slash1', 'slash2'):
        two = kind == 'slash2'
        wind = W(R, spine=(8, -28, 0), chest=(0, -18, 0), rUp=(-125, 0, 72), rFore=(-70, 0, 0), lUp=(-110, 0, -75) if two else (-25, 0, -70), lFore=(-70, 0, 0) if two else (-55, 0, 0), rt=(0, -.08, -.05))
        hit = W(R, spine=(18, 32, 0), chest=(5, 20, 0), rUp=(-85, 40, 78), rFore=(-20, 0, 0), lUp=(-80, 30, -78) if two else (-20, 0, -75), lFore=(-30, 0, 0), head=(-6, -10, 0), rt=(0, -.12, .18), **LUNGE)
        return [(0, R), (.30, wind), (.44, hit), (.62, hit), (1, R)], .44
    if kind == 'overhead':
        wind = W(R, spine=(-16, 0, 0), chest=(-10, 0, 0), lUp=(-170, 0, -80), rUp=(-170, 0, 80), lFore=(-30, 0, 0), rFore=(-30, 0, 0), head=(-12, 0, 0), rt=(0, -.04, -.1))
        hit = W(R, spine=(34, 0, 0), chest=(14, 0, 0), lUp=(-45, 0, -80), rUp=(-45, 0, 80), lFore=(-10, 0, 0), rFore=(-10, 0, 0), head=(10, 0, 0), rt=(0, -.2, .2), **LUNGE)
        return [(0, R), (.34, wind), (.46, hit), (.7, hit), (1, R)], .46
    if kind in ('sweep', 'slam'):
        if kind == 'slam':
            wind = W(R, spine=(-14, 0, 0), lUp=(-165, 0, -80), rUp=(-165, 0, 80), lFore=(-25, 0, 0), rFore=(-25, 0, 0), rt=(0, .02, -.1))
            hit = W(R, spine=(46, 0, 0), chest=(16, 0, 0), lUp=(-25, 0, -80), rUp=(-25, 0, 80), lFore=(-5, 0, 0), rFore=(-5, 0, 0), head=(14, 0, 0), rt=(0, -.28, .25), **LUNGE)
            return [(0, R), (.38, wind), (.5, hit), (.74, hit), (1, R)], .5
        wind = W(R, spine=(6, -45, 0), chest=(0, -25, 0), lUp=(-85, -70, -80), rUp=(-85, -70, 80), lFore=(-20, 0, 0), rFore=(-20, 0, 0), rt=(0, -.08, -.05))
        hit = W(R, spine=(10, 50, 0), chest=(0, 30, 0), lUp=(-85, 80, -80), rUp=(-85, 80, 80), lFore=(-10, 0, 0), rFore=(-10, 0, 0), rt=(0, -.12, .15), **LUNGE)
        return [(0, R), (.34, wind), (.5, hit), (.66, hit), (1, R)], .5
    if kind == 'thrust_l':
        wind = W(R, spine=(8, 18, 0), lUp=(-50, 0, -72), lFore=(-110, 0, 0), rt=(0, -.08, -.08))
        hit = W(R, spine=(14, -8, 0), lUp=(-95, 0, -80), lFore=(-8, 0, 0), rt=(0, -.1, .22), **LUNGE)
        return [(0, R), (.3, wind), (.45, hit), (.62, hit), (1, R)], .45
    if kind == 'block':
        blk = W(R, spine=(12, 0, 0), lUp=(-80, 0, -75), lFore=(-100, 0, 0), rUp=(-45, 0, 70), rFore=(-70, 0, 0), rt=(0, -.12, .05), lThigh=(-30, 0, 3), lShin=(45, 0, 0))
        return [(0, R), (.2, blk), (.8, blk), (1, R)], .3
    if kind.startswith('draw'):
        hi = kind == 'draw_high'; hold = {'draw': .46, 'draw_long': .62, 'draw_high': .5}[kind]
        draw = W(R, lUp=(-(110 if hi else 88), 0, -80), lFore=(-4, 0, 0), rUp=(-(105 if hi else 85), 0, 76), rFore=(-105, 0, 0), spine=(-6 if hi else 4, 12, 0), head=(-(18 if hi else 2), 18, 0), rt=(0, -.08, 0))
        rel = W(draw, rUp=(-(100 if hi else 80), 0, 80), rFore=(-40, 0, 0), spine=(-6 if hi else 4, 8, 0), rt=(0, -.08, -.04))
        return [(0, R), (hold * .75, draw), (hold, draw), (hold + .04, rel), (.8, rel), (1, R)], hold + .04
    if kind in ('toss', 'lob'):
        big = kind == 'lob'
        wind = W(R, rUp=(40, 0, 62), rFore=(-30, 0, 0), spine=(-8, -15, 0), rt=(0, -.1, -.04))
        hit = W(R, rUp=(-(165 if big else 140), 0, 78), rFore=(-10, 0, 0), spine=(14, 12, 0), head=(-4, 0, 0), rt=(0, -.08, .14), **LUNGE)
        return [(0, R), (.3, wind), (.46, hit), (.65, hit), (1, R)], .46
    if kind in ('raise1', 'raise2'):
        one = kind == 'raise1'
        up = W(R, rUp=(-160, 0, 78), rFore=(-20, 0, 0), head=(-18, 0, 0), spine=(-6, 0, 0), lUp=(-30, 0, -70) if one else (-160, 0, -78), lFore=(-55, 0, 0) if one else (-20, 0, 0))
        if one: return [(0, R), (.4, up), (.7, up), (1, R)], .45
        hit = W(up, rUp=(-30, 0, 75), lUp=(-30, 0, -75), spine=(30, 0, 0), head=(12, 0, 0), rt=(0, -.2, .1))
        return [(0, R), (.35, up), (.5, up), (.6, hit), (.8, hit), (1, R)], .55
    if kind == 'cast':
        wind = W(R, lUp=(-45, 0, -75), rUp=(-45, 0, 75), lFore=(-110, 0, 0), rFore=(-110, 0, 0), spine=(4, 0, 0), rt=(0, -.08, -.04))
        hit = W(R, lUp=(-95, 0, -80), rUp=(-95, 0, 80), lFore=(-6, 0, 0), rFore=(-6, 0, 0), spine=(12, 0, 0), head=(-6, 0, 0), rt=(0, -.1, .12), **LUNGE)
        return [(0, R), (.3, wind), (.46, hit), (.7, hit), (1, R)], .46
    raise KeyError(kind)

def human_common():
    R = H_READY; C = {}
    rec = W(R, spine=(-22, 0, 0), chest=(-14, 0, 0), head=(-26, 0, 0), neck=(-8, 0, 0), rUp=(15, 0, 82), lUp=(15, 0, -82), rFore=(-20, 0, 0), lFore=(-20, 0, 0), rt=(0, -.04, -.14))
    C['stag'] = ([(0, R), (.18, rec), (.5, W(rec, spine=(-8, 0, 0), head=(-10, 0, 0), rt=(0, -.04, -.04))), (1, R)], 0)
    for nm, sgn in (('hit_f', 0), ('hit_l', -1), ('hit_r', 1)):
        d = W(rec, rt=(.12 * sgn, -.05, -.1 if sgn == 0 else -.03), spine=(-14, 0, 18 * sgn), chest=(-6, 0, 10 * sgn), head=(-16, 0, 18 * sgn))
        C[nm] = ([(0, R), (.2, d), (.6, W(d, spine=(-4, 0, 6 * sgn))), (1, R)], 0)
    C['defend'] = ([(0, R), (.2, W(R, spine=(12, 0, 0), lUp=(-95, 0, -78), lFore=(-110, 0, 0), rUp=(-90, 0, 78), rFore=(-110, 0, 0), rt=(0, -.12, .04), lThigh=(-30, 0, 3), lShin=(45, 0, 0))), (.8, W(R, spine=(12, 0, 0), lUp=(-95, 0, -78), lFore=(-110, 0, 0), rUp=(-90, 0, 78), rFore=(-110, 0, 0), rt=(0, -.12, .04), lThigh=(-30, 0, 3), lShin=(45, 0, 0))), (1, R)], .3)
    dg = W(R, rt=(-.55, -.22, -.08), spine=(12, 0, 18), chest=(0, 0, 10), lThigh=(-20, 0, 18), lShin=(40, 0, 0), rThigh=(10, 0, 4), rShin=(20, 0, 0), lUp=(-30, 0, -90), rUp=(-50, 0, 90), rFore=(-20, 0, 0))
    C['dodge'] = ([(0, R), (.28, dg), (.62, W(dg, rt=(-.5, -.06, 0))), (1, W(R, rt=(0, -.05, 0)))], 0)
    C['down'] = ([(0, R), (1, W(R, rt=(0, -.62, .05), spine=(26, 0, 0), chest=(14, 0, 0), neck=(20, 0, 0), head=(34, 0, 0), lThigh=(-78, 0, 8), lShin=(100, 0, 0), lFoot=(-20, 0, 0),
                              rThigh=(14, 0, -6), rShin=(120, 0, 0), rFoot=(30, 0, 0), lUp=(12, 0, -75), lFore=(-35, 0, 0), rUp=(8, 0, 70), rFore=(-30, 0, 0)))], 0)
    # deaths
    knee = W(rec, rt=(0, -.45, -.06), lThigh=(-50, 0, 6), lShin=(85, 0, 0), rThigh=(-40, 0, -6), rShin=(80, 0, 0), spine=(-10, 0, 0), head=(-10, 0, 0))
    C['dissolve'] = ([(0, R), (.18, rec), (.42, knee), (.9, W(knee, rt=(0, .02, 0), root=(-86, 0, 0), lUp=(-10, 0, -100), rUp=(-10, 0, 100))), (1, W(knee, rt=(0, .02, 0), root=(-86, 0, 0), lUp=(-10, 0, -100), rUp=(-10, 0, 100)))], 0)
    ba = W(R, spine=(32, 0, 0), chest=(10, 0, 0), head=(30, 0, 0), rUp=(-135, 0, 78), lUp=(-135, 0, -78), rFore=(-95, 0, 0), lFore=(-95, 0, 0), rt=(0, -.12, 0))
    bk = W(ba, rt=(0, -.55, .1), lThigh=(-80, 0, 6), lShin=(110, 0, 0), rThigh=(16, 0, -6), rShin=(130, 0, 0), spine=(42, 0, 0))
    C['burn'] = ([(0, R), (.25, ba), (.6, bk), (.95, W(bk, root=(72, 0, 0), rt=(0, .02, 0), rUp=(-150, 0, 60), lUp=(-150, 0, -60))), (1, W(bk, root=(72, 0, 0), rt=(0, .02, 0), rUp=(-150, 0, 60), lUp=(-150, 0, -60)))], 0)
    cr = W(R, rt=(0, -.2, 0), spine=(28, 0, 0), head=(20, 0, 0), lUp=(-30, 0, -75), rUp=(-30, 0, 75), lFore=(-100, 0, 0), rFore=(-100, 0, 0), lThigh=(-30, 0, 3), lShin=(60, 0, 0), rThigh=(-30, 0, -3), rShin=(60, 0, 0))
    po = W(R, rt=(0, .3, 0), spine=(-22, 0, 0), chest=(-14, 0, 0), head=(-34, 0, 0), lUp=(0, 0, -150), rUp=(0, 0, 150), lFore=(0, 0, 0), rFore=(0, 0, 0), lThigh=(-20, 0, 38), rThigh=(-20, 0, -38), lShin=(10, 0, 0), rShin=(10, 0, 0))
    C['burst'] = ([(0, R), (.2, cr), (.4, po), (.62, po), (1, W(R, rt=(0, -.12, 0), spine=(22, 0, 0), head=(24, 0, 0)))], 0)
    gu = W(R, rt=(0, -.78, .05), spine=(36, 0, 0), chest=(16, 0, 0), head=(48, 0, 0), lThigh=(-85, 0, 8), lShin=(125, 0, 0), rThigh=(-85, 0, -8), rShin=(125, 0, 0), lUp=(30, 0, -60), rUp=(30, 0, 60), lFore=(-20, 0, 0), rFore=(-20, 0, 0))
    C['gutter'] = ([(0, R), (.5, gu), (1, W(gu, rt=(0, -.98, .05)))], 0)
    C['shatter'] = ([(0, R), (.22, rec), (1, rec)], 0)
    return C

QD = {}
def quad_poses():
    R = dict(rt=(0, -.04, 0), neck=(-8, 0, 0), head=(14, 0, 0), spine=(-3, 0, 0), tail1=(-28, 0, 0), tail2=(-10, 0, 0), flU=(-18, 0, 0), frU=(-18, 0, 0), flL=(26, 0, 0), frL=(26, 0, 0),
             hlU=(14, 0, 0), hrU=(14, 0, 0), hlL=(-22, 0, 0), hrL=(-22, 0, 0), hlK=(10, 0, 0), hrK=(10, 0, 0))
    C = {}
    def atk(kind):
        if kind == 'bite':
            wd = W(R, rt=(0, -.12, -.12), spine=(-12, 0, 0), neck=(-24, 0, 0), head=(-20, 0, 0), jaw=(0, 0, 0))
            ht = W(R, rt=(0, -.02, .5), spine=(14, 0, 0), neck=(18, 0, 0), head=(26, 0, 0), jaw=(38, 0, 0), flU=(-50, 0, 0), frU=(-50, 0, 0), hlU=(30, 0, 0), hrU=(30, 0, 0))
            return [(0, R), (.3, wd), (.48, ht), (.62, W(ht, jaw=(0, 0, 0))), (1, R)], .48
        if kind == 'gore':
            wd = W(R, rt=(0, -.1, -.2), spine=(-14, 0, 0), neck=(-22, 0, 0), head=(-26, 0, 0), flU=(30, 0, 0), frU=(30, 0, 0))
            ht = W(R, rt=(0, -.1, .55), spine=(10, 0, 0), neck=(30, 0, 0), head=(38, 0, 0), flU=(-60, 0, 0), frU=(-60, 0, 0), hlU=(34, 0, 0), hrU=(34, 0, 0))
            return [(0, R), (.34, wd), (.5, ht), (.68, ht), (1, R)], .5
        wd = W(R, rt=(0, .02, -.25), spine=(-34, 0, 0), neck=(-14, 0, 0), flU=(-90, 0, 0), frU=(-90, 0, 0), flL=(60, 0, 0), frL=(60, 0, 0), hlU=(30, 0, 0), hrU=(30, 0, 0), head=(10, 0, 0))
        ht = W(R, rt=(0, -.1, 1.3), spine=(18, 0, 0), neck=(28, 0, 0), head=(34, 0, 0), flU=(-60, 0, 0), frU=(-60, 0, 0), hlU=(-40, 0, 0), hrU=(-40, 0, 0))
        return [(0, R), (.4, wd), (.7, ht), (.85, W(ht, rt=(0, -.04, 1.45))), (1, R)], .7
    C['atk'] = atk
    rec = W(R, rt=(0, -.04, -.16), spine=(-18, 0, 0), neck=(-24, 0, 0), head=(-18, 0, 0), tail1=(-50, 0, 0))
    C['stag'] = ([(0, R), (.18, rec), (.5, W(rec, rt=(0, -.04, -.04), spine=(-6, 0, 0))), (1, R)], 0)
    for nm, s in (('hit_f', 0), ('hit_l', -1), ('hit_r', 1)):
        d = W(rec, rt=(.14 * s, -.04, -.08), spine=(-12, 0, 16 * s), neck=(-14, 0, 16 * s), head=(-12, 0, 18 * s))
        C[nm] = ([(0, R), (.2, d), (.6, W(d, spine=(-4, 0, 5 * s))), (1, R)], 0)
    bk = W(R, rt=(0, -.14, 0), spine=(8, 0, 0), neck=(10, 0, 0), head=(24, 0, 0), flU=(-30, 0, 0), frU=(-30, 0, 0), flL=(60, 0, 0), frL=(60, 0, 0), hlU=(30, 0, 0), hrU=(30, 0, 0))
    C['defend'] = ([(0, R), (.2, bk), (.8, bk), (1, R)], .3)
    dg = W(R, rt=(-.6, -.06, .05), spine=(0, 0, 18), neck=(-10, 0, 20), flU=(-40, 0, 12), frU=(-30, 0, 12), hlU=(30, 0, 12), hrU=(30, 0, 12))
    C['dodge'] = ([(0, R), (.3, dg), (.6, W(dg, rt=(-.55, -.02, 0))), (1, R)], 0)
    C['down'] = ([(0, R), (1, W(R, rt=(0, -.35, 0), spine=(10, 0, 0), neck=(30, 0, 0), head=(40, 0, 0), flU=(-60, 0, 10), frU=(-60, 0, -10), flL=(110, 0, 0), frL=(110, 0, 0), hlU=(50, 0, 10), hrU=(50, 0, -10), hlL=(-60, 0, 0), hrL=(-60, 0, 0)))], 0)
    C['dissolve'] = ([(0, R), (.18, rec), (.5, W(rec, rt=(0, -.2, 0), flU=(-30, 0, 20), frU=(-30, 0, -20), hlU=(30, 0, 20), hrU=(30, 0, -20))), (.9, W(rec, rt=(0, .02, 0), root=(0, 0, 90), neck=(20, 0, 0), head=(30, 0, 0))), (1, W(rec, rt=(0, .02, 0), root=(0, 0, 90), neck=(20, 0, 0), head=(30, 0, 0)))], 0)
    sp = W(R, rt=(0, -.5, 0), spine=(30, 0, 0), neck=(34, 0, 0), head=(40, 0, 0), flU=(-30, 0, 50), frU=(-30, 0, -50), hlU=(30, 0, 50), hrU=(30, 0, -50), tail1=(-70, 0, 0))
    C['burn'] = ([(0, R), (.3, W(R, rt=(0, -.1, 0), spine=(20, 0, 0), head=(40, 0, 0), neck=(30, 0, 0), tail1=(-80, 0, 0))), (.8, sp), (1, W(sp, rt=(0, -.6, 0)))], 0)
    C['burst'] = ([(0, R), (.2, W(R, rt=(0, -.2, 0), spine=(20, 0, 0), head=(26, 0, 0))), (.4, W(R, rt=(0, .35, 0), spine=(-20, 0, 0), neck=(-30, 0, 0), head=(-34, 0, 0), flU=(-40, 0, 60), frU=(-40, 0, -60), hlU=(40, 0, 60), hrU=(40, 0, -60), jaw=(35, 0, 0))), (.65, W(R, rt=(0, .3, 0), flU=(-40, 0, 60), frU=(-40, 0, -60), hlU=(40, 0, 60), hrU=(40, 0, -60))), (1, W(sp, rt=(0, -.3, 0)))], 0)
    C['gutter'] = ([(0, R), (.5, W(sp, rt=(0, -.55, 0))), (1, W(sp, rt=(0, -.75, 0)))], 0)
    C['shatter'] = ([(0, R), (.22, rec), (1, rec)], 0)
    return C, R

def wisp_poses():
    R = dict(rt=(0, 0, 0)); C = {}
    def sway(a): return dict(rt=(0, 0, 0), core=(a * 4, 0, a * 3), sh1=(a * 7, 0, -a * 5), sh2=(-a * 8, 0, a * 7), sh3=(a * 9, 0, -a * 8))
    C['sway'] = lambda ph, amp: W(sway(math.sin(ph) * amp), rt=(0, math.sin(ph * 2) * .04 * amp, 0))
    wd = dict(rt=(0, .08, -.12), core=(-14, 0, 0), sh1=(-18, 0, 0), sh2=(-16, 0, 0), sh3=(-12, 0, 0))
    ht = dict(rt=(0, -.02, .5), core=(16, 0, 0), sh1=(22, 0, 0), sh2=(24, 0, 0), sh3=(26, 0, 0))
    C['atk'] = ([(0, R), (.35, wd), (.5, ht), (.65, ht), (1, R)], .5)
    rec = dict(rt=(0, 0, -.2), core=(-20, 0, 0), sh1=(-28, 0, 6), sh2=(-30, 0, -8), sh3=(-30, 0, 10))
    C['stag'] = ([(0, R), (.2, rec), (1, R)], 0)
    for nm, s in (('hit_f', 0), ('hit_l', -1), ('hit_r', 1)):
        C[nm] = ([(0, R), (.2, dict(rt=(.18 * s, 0, -.1), core=(-10, 0, 18 * s), sh1=(-12, 0, 24 * s), sh2=(-12, 0, 20 * s), sh3=(-12, 0, 16 * s))), (1, R)], 0)
    C['defend'] = ([(0, R), (.25, dict(rt=(0, -.1, 0), core=(10, 0, 0), sh1=(24, 0, 0), sh2=(32, 0, 0), sh3=(40, 0, 0))), (.8, dict(rt=(0, -.1, 0), core=(10, 0, 0), sh1=(24, 0, 0), sh2=(32, 0, 0), sh3=(40, 0, 0))), (1, R)], .3)
    C['dodge'] = ([(0, R), (.3, dict(rt=(-.7, .05, 0), core=(0, 0, 14), sh1=(0, 0, 20), sh2=(0, 0, 26), sh3=(0, 0, 30))), (.6, dict(rt=(-.65, .05, 0))), (1, R)], 0)
    C['down'] = ([(0, R), (1, dict(rt=(0, -.4, 0), core=(20, 0, 0), sh1=(30, 0, 0), sh2=(40, 0, 0), sh3=(50, 0, 0)))], 0)
    C['dissolve'] = ([(0, R), (.3, rec), (1, dict(rt=(0, -.2, -.1), core=(-60, 0, 0), sh1=(-50, 0, 0), sh2=(-40, 0, 0), sh3=(-30, 0, 0)))], 0)
    C['burn'] = ([(0, R), (.4, dict(rt=(0, .1, 0), core=(0, 0, 14), sh1=(0, 0, -18), sh2=(0, 0, 22), sh3=(0, 0, -26))), (1, dict(rt=(0, -.5, 0), core=(30, 0, 0), sh1=(40, 0, 0), sh2=(50, 0, 0), sh3=(60, 0, 0)))], 0)
    C['burst'] = ([(0, R), (.25, dict(rt=(0, -.1, 0), core=(20, 0, 0))), (.5, dict(rt=(0, .3, 0), core=(-10, 0, 0), sh1=(0, 0, 40), sh2=(0, 0, -50), sh3=(0, 0, 60))), (1, dict(rt=(0, -.2, 0)))], 0)
    C['gutter'] = ([(0, R), (.5, dict(rt=(0, -.35, 0), core=(30, 0, 0), sh1=(40, 0, 0), sh2=(50, 0, 0), sh3=(60, 0, 0))), (1, dict(rt=(0, -.8, 0), core=(30, 0, 0), sh1=(40, 0, 0), sh2=(50, 0, 0), sh3=(60, 0, 0)))], 0)
    C['shatter'] = ([(0, R), (.22, rec), (1, rec)], 0)
    return C, R

def free_motion(rig, u, seed=3):
    """independent rigid shards (not inherited down the chain): ballistic offsets + tumble. u in [0,1] over the clip."""
    rng = np.random.default_rng(seed); out = {}; t = max(0.0, (u - .22) / .78) * 1.6; g = 9.0
    for b in range(1, rig.nb):
        h = rig.head[b]; d = np.array([h[0], 0, h[2]]) + rng.normal(0, .15, 3) * [1, 0, 1]; d = d / (np.linalg.norm(d) + 1e-6)
        v = np.array([d[0] * rng.uniform(.8, 2.2), rng.uniform(1.0, 3.0), d[2] * rng.uniform(.8, 2.2)])
        off = v * t + np.array([0, -.5 * g * t * t, 0])
        if h[1] + off[1] < 0.06: off[1] = 0.06 - h[1] + (0.06 * abs(math.sin(t * 9)) * math.exp(-t * 3))
        ax = rng.normal(0, 1, 3); out[b] = (off, ax, t * rng.uniform(2.5, 7.0))
    return out

# ------------------------------------------------------------------ scene helpers
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for a in list(bpy.data.actions): bpy.data.actions.remove(a)

def lin_colour(mat):
    if mat and mat.use_nodes:
        for n in mat.node_tree.nodes:
            if n.type == 'BSDF_PRINCIPLED':
                c = n.inputs['Base Color'].default_value; e = n.inputs['Emission Strength'].default_value if 'Emission Strength' in n.inputs else 0
                return np.array([c[0], c[1], c[2]]), e
    return np.array([.5, .5, .5]), 0

def extract(meshes, skinned, bidx):
    Pc, Cc, Fn, BA, BB, BW, EM = [], [], [], [], [], [], []
    for o in meshes:
        me = o.data; me.calc_loop_triangles(); nvv = len(me.vertices)
        M = np.array(o.matrix_world); co = np.empty(nvv * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3) @ M[:3, :3].T + M[:3, 3]
        lt = me.loop_triangles; nt = len(lt); tv = np.empty(nt * 3, np.int64); tl = np.empty(nt * 3, np.int64); tp = np.empty(nt, np.int64)
        lt.foreach_get('vertices', tv); lt.foreach_get('loops', tl); lt.foreach_get('polygon_index', tp)
        pm = np.empty(len(me.polygons), np.int64); me.polygons.foreach_get('material_index', pm); mats = [lin_colour(m) for m in me.materials] or [(np.array([.5, .5, .5]), 0)]
        col = None
        if len(me.color_attributes):
            ca = me.color_attributes[0]; buf = np.empty(len(ca.data) * 4); ca.data.foreach_get('color', buf); buf = buf.reshape(-1, 4)[:, :3]
            col = buf[tl] if ca.domain == 'CORNER' else buf[tv]
        if col is None: col = np.array([mats[min(pm[p], len(mats) - 1)][0] for p in tp]).repeat(3, 0)
        em = np.array([mats[min(pm[p], len(mats) - 1)][1] for p in tp]).repeat(3)
        Pc.append(co[tv]); Cc.append(col); EM.append(em)
        if skinned:
            ba = np.zeros(nvv, np.int64); bb = np.zeros(nvv, np.int64); bw = np.zeros(nvv)
            for v in me.vertices:
                g = sorted(((x.weight, o.vertex_groups[x.group].name) for x in v.groups if o.vertex_groups[x.group].name in bidx), reverse=True)[:2]
                if not g: continue
                ba[v.index] = bidx[g[0][1]]; bb[v.index] = bidx[g[-1][1]]; tot = sum(x[0] for x in g); bw[v.index] = (g[1][0] / tot) if len(g) == 2 and tot > 0 else 0
            BA.append(ba[tv]); BB.append(bb[tv]); BW.append(bw[tv])
    P = np.concatenate(Pc); Cc = np.concatenate(Cc); EM = np.concatenate(EM)
    return P, Cc, EM, (np.concatenate(BA), np.concatenate(BB), np.concatenate(BW)) if skinned else None

def smooth_normals(P, ang=40.0):
    nt = len(P) // 3; T = P.reshape(nt, 3, 3); fn = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); ar = np.linalg.norm(fn, axis=1) + 1e-12; fnu = fn / ar[:, None]
    key = np.round(P / 1e-4).astype(np.int64); _, gid = np.unique(key, axis=0, return_inverse=True); gid = gid.ravel(); fid = np.repeat(np.arange(nt), 3)
    order = np.argsort(gid, kind='stable'); gs = gid[order]; starts = np.r_[0, np.nonzero(np.diff(gs))[0] + 1, len(gs)]
    N = np.zeros((len(P), 3)); cth = math.cos(math.radians(ang))
    for a, b in zip(starts[:-1], starts[1:]):
        idx = order[a:b]; fs = fid[idx]; fns = fnu[fs]; wts = ar[fs][:, None]
        for i in idx:
            f = fid[i]; m = (fns @ fnu[f]) > cth; s = (fns[m] * wts[m]).sum(0); N[i] = s / (np.linalg.norm(s) + 1e-12)
    return N, fnu, gid

def repalette(key, C, EM):
    th = THEME.get(key) or STONE; pal = np.array([PALN[n][0] for n in th]); gl = [i for i, n in enumerate(th) if n in GLOWS]
    uq, inv = np.unique(np.round(C, 3), axis=0, return_inverse=True); inv = inv.ravel()
    em_u = np.zeros(len(uq)); np.maximum.at(em_u, inv, EM)
    out = np.zeros_like(uq); surf = np.zeros(len(uq)); rough = np.zeros(len(uq)); met = np.zeros(len(uq)); gri = np.zeros(len(uq))
    lum = lambda c: float(c @ np.array([.2126, .7152, .0722])) + 1e-4
    for i, c in enumerate(uq):
        mx, mn = c.max(), c.min(); sat = (mx - mn) / (mx + 1e-6); lc = lum(c)
        if gl and ((key in CFG and sat > .55 and mx > .35) or em_u[i] > .05 or key == 'gloamwisp'): j = gl[0] if len(gl) == 1 else gl[int(np.argmin([np.abs(c - pal[g]).sum() for g in gl]))]
        else:
            cand = [k for k in range(len(th)) if k not in gl] or list(range(len(th)))
            d = [np.abs(np.sqrt(c) - np.sqrt(pal[k])).sum() + 1.5 * abs(math.log(lum(pal[k]) / lc)) for k in cand]; j = cand[int(np.argmin(d))]
        nm = th[j]; pc, sf, ro, me_, gr = PALN[nm]
        if nm in GLOWS: out[i] = pc
        else:
            desat = c + (lc - c) * .6; k = float(np.clip((lc / lum(pc)) ** .6, .55, 1.7)); out[i] = np.clip(pc * k * .85 + desat * .15, 0, 1) * (1 if sf != ST else 1)
        surf[i], rough[i], met[i], gri[i] = sf, ro, me_, gr
    return out[inv], surf[inv], rough[inv], met[inv], gri[inv]

def skin(Q, Pp, head, BAi, BBi, BWt, V):
    """numpy two-bone skinning identical to the runtime's: v' = R (v - head) + pos"""
    x, y, z, w = Q[:, 0], Q[:, 1], Q[:, 2], Q[:, 3]
    R = np.stack([1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), 2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
                  2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)], 1).reshape(-1, 3, 3)
    va = np.einsum('nij,nj->ni', R[BAi], V - head[BAi]) + Pp[BAi]; vb = np.einsum('nij,nj->ni', R[BBi], V - head[BBi]) + Pp[BBi]
    return va * (1 - BWt)[:, None] + vb * BWt[:, None]

def bake_ao(V, N, tri_idx, H):
    K = 14; MAXD = .7 * max(.5, H / 1.8); rng = np.random.default_rng(7)
    bvh = BVHTree.FromPolygons([tuple(v) for v in V.tolist()], [tuple(t) for t in tri_idx.tolist()], epsilon=0.0)
    r1, r2 = rng.random(K), rng.random(K); ang = 2 * np.pi * r1; rad = np.sqrt(r2); loc = np.stack([rad * np.cos(ang), rad * np.sin(ang), np.sqrt(1 - r2)], 1)
    ao = np.ones(len(V)); Nn = N / (np.linalg.norm(N, axis=1, keepdims=True) + 1e-9)
    for v in range(len(V)):
        n = Vector(Nn[v]); t = Vector((1, 0, 0)) if abs(n.x) < .9 else Vector((0, 1, 0)); t = (t - n * t.dot(n)).normalized(); bt = n.cross(t); o = Vector(V[v]) + n * .006; occ = 0.0
        for k in range(K):
            d = t * loc[k, 0] + bt * loc[k, 1] + n * loc[k, 2]; hit = bvh.ray_cast(o, d, MAXD)
            if hit[0] is not None: occ += 1.0 - hit[3] / MAXD
            elif d.y < -1e-4 and o.y > 0:
                tg = -o.y / d.y
                if tg < MAXD: occ += 1.0 - tg / MAXD
        ao[v] = 1.0 - occ / K
    return 1.0 - .6 * (1.0 - np.clip(ao, 0, 1))

def cavity(P, N, gid):
    nt = len(P) // 3; g = gid; ng = g.max() + 1; pos = np.zeros((ng, 3)); pos[g] = P; nrm = np.zeros((ng, 3)); np.add.at(nrm, g, N); nrm /= np.linalg.norm(nrm, axis=1, keepdims=True) + 1e-9
    sm = np.zeros((ng, 3)); cnt = np.zeros(ng); ln = np.zeros(ng); gt = g.reshape(nt, 3)
    for a, b in ((0, 1), (1, 2), (2, 0)):
        ia, ib = gt[:, a], gt[:, b]; d = pos[ib] - pos[ia]; l = np.linalg.norm(d, axis=1)
        for i, j in ((ia, ib), (ib, ia)): np.add.at(sm, i, pos[j]); np.add.at(cnt, i, 1); np.add.at(ln, i, l)
    lap = pos - sm / np.maximum(cnt, 1)[:, None]; cv = (lap * nrm).sum(1) / (ln / np.maximum(cnt, 1) + 1e-6)
    return np.clip(cv * 1.2, -1, 1)[g]

# ------------------------------------------------------------------ rig naming
def gname_h(n):
    m = {'Hips': 'hips', 'Spine': 'spine', 'Spine1': 'chest', 'Spine2': 'chest2', 'Neck': 'neck', 'Head': 'head', 'LeftShoulder': 'lClav', 'LeftArm': 'lUp', 'LeftForeArm': 'lFore', 'LeftHand': 'lHand',
         'RightShoulder': 'rClav', 'RightArm': 'rUp', 'RightForeArm': 'rFore', 'RightHand': 'rHand', 'LeftUpLeg': 'lThigh', 'LeftLeg': 'lShin', 'LeftFoot': 'lFoot', 'LeftToeBase': 'lToe',
         'RightUpLeg': 'rThigh', 'RightLeg': 'rShin', 'RightFoot': 'rFoot', 'RightToeBase': 'rToe'}
    return m.get(n, n[:12])
def gname_q(n):
    s = n
    for a, b in (('LeftFront', 'fl'), ('RightFront', 'fr'), ('LeftHind', 'hl'), ('RightHind', 'hr'), ('UpLeg', 'U'), ('Elbow', 'E'), ('Hock', 'K'), ('Foot', 'P'), ('Leg', 'L'), ('LeftEar', 'lEar'), ('RightEar', 'rEar'),
                 ('TailTip', 'tail2'), ('Tail', 'tail1'), ('Chest', 'chest'), ('Hips', 'hips'), ('Spine', 'spine'), ('Neck', 'neck'), ('Head', 'head'), ('Jaw', 'jaw')):
        if s == n or True: s = s.replace(a, b)
    return s[:12]

def build_rig_from_import(arm, kind, s, dz):
    names, parent, head, src = ['root'], [-1], [np.zeros(3)], [None]
    gn = gname_h if kind == 'human' else gname_q
    pbs = list(arm.data.bones); base = {b.name: i + 1 for i, b in enumerate(pbs)}
    for b in pbs:
        names.append(gn(b.name)); parent.append(base[b.parent.name] if b.parent else 0)
        hw = np.array(arm.matrix_world @ b.head_local); head.append(to_game(s * hw + np.array([0, 0, dz]))); src.append(b.name)
    if kind == 'human':
        for nm, hand in (('wR', 'rHand'), ('wL', 'lHand')):
            if hand in names: names.append(nm); parent.append(names.index(hand)); head.append(head[names.index(hand)].copy()); src.append(None)
    return Rig(names, parent, np.array(head)), src

def sample_imported(arm, rig, src, s, dz, act, frames):
    scn = bpy.context.scene; arm.data.pose_position = 'POSE'; arm.animation_data_create(); arm.animation_data.action = act; nb = rig.nb
    Qs = np.zeros((len(frames), nb, 4)); Ps = np.zeros((len(frames), nb, 3)); Mw = np.array(arm.matrix_world)
    for fi, f in enumerate(frames):
        scn.frame_set(f); bpy.context.view_layer.update()
        for b in range(nb):
            if src[b] is None:
                if b == 0: Qs[fi, b] = [0, 0, 0, 1]; Ps[fi, b] = rig.head[b]
                continue
            pb = arm.pose.bones[src[b]]; Sw = np.array(arm.matrix_world @ pb.matrix) @ np.linalg.inv(Mw @ np.array(pb.bone.matrix_local))
            Qs[fi, b] = m2q(C3 @ Sw[:3, :3] @ C3.T); hb = np.array(arm.matrix_world @ pb.bone.head_local); pw = (Sw @ np.append(hb, 1))[:3]
            Ps[fi, b] = to_game(s * pw + np.array([0, 0, dz]))
    for b in range(nb):   # sockets copy their hand
        if src[b] is None and b > 0: Qs[:, b] = Qs[:, rig.parent[b]]; Ps[:, b] = Ps[:, rig.parent[b]]
    arm.animation_data.action = None
    return fix_signs(Qs), Ps

def author(rig, keys, frames, loop, hit_u=0, extra=None):
    Qs = np.zeros((frames, rig.nb, 4)); Ps = np.zeros((frames, rig.nb, 3))
    for f in range(frames):
        u = f / frames if loop else f / (frames - 1); pose = evalkeys(keys, u) if isinstance(keys, list) else keys(u)
        pd = {rig.ix[k]: v for k, v in pose.items() if k != 'rt' and k in rig.ix}
        free = free_motion(rig, u) if extra == 'free' else None
        Qs[f], Ps[f] = solve(rig, pd, pose.get('rt', (0, 0, 0)), free)
    return fix_signs(Qs), Ps

def with_def(keys, base):
    out = []
    for u, p in keys:
        q = dict(base); q.update(p); out.append((u, q))
    return out

# ------------------------------------------------------------------ main per-asset build
def build(key):
    t0 = time.time(); info = MAN[key]; cat = info['category']; reset()
    bpy.ops.import_scene.gltf(filepath=REPO + '/' + info['glb'])
    arm = next((o for o in bpy.data.objects if o.type == 'ARMATURE'), None)
    meshes = [o for o in bpy.data.objects if o.type == 'MESH' and len(o.data.polygons) and o.data.materials]
    character = key in CFG
    kind = 'human' if key in HEROES or (character and key not in QUADS and key != 'gloamwisp') else ('quad' if key in QUADS else 'wisp')
    if key == 'gloamwisp': meshes = [o for o in meshes if o.name.startswith('ghost-1')] or meshes[:1]
    skinned = bool(arm) and kind != 'wisp'
    if skinned: meshes = [o for o in meshes if len(o.vertex_groups)]
    if arm: arm.data.pose_position = 'REST'
    bidx = {b.name: i + 1 for i, b in enumerate(arm.data.bones)} if skinned else {}
    P, Cc, EM, W3 = extract(meshes, skinned, bidx)
    zmin, zmax = P[:, 2].min(), P[:, 2].max()
    if character:
        s = CFG[key]['H'] / (zmax - zmin); dz = -zmin * s + (0.3 if kind == 'wisp' else 0.0)
    else: s = 1.0; dz = -zmin
    V = to_game(P * s + np.array([0, 0, dz])); nt = len(V) // 3
    if kind == 'wisp': V[:, 0] -= (V[:, 0].max() + V[:, 0].min()) / 2; V[:, 2] -= (V[:, 2].max() + V[:, 2].min()) / 2
    Nsm, fnu, gid = smooth_normals(V)
    Cp, surf, rough, metal, grime = repalette(key, Cc, EM)
    U = np.where((np.abs(fnu[:, 0]) > np.abs(fnu[:, 1])) & (np.abs(fnu[:, 0]) > np.abs(fnu[:, 2])), 0, np.where(np.abs(fnu[:, 1]) > np.abs(fnu[:, 2]), 1, 2)).repeat(3)
    UV = np.where((U == 0)[:, None], V[:, [2, 1]], np.where((U == 1)[:, None], V[:, [0, 2]], V[:, [0, 1]]))
    tris = np.arange(len(V)).reshape(-1, 3)
    clips = []; rig = None; fx = []
    if character:
        if kind == 'wisp':
            Hh = V[:, 1].max(); ys = [0.0, .25 * Hh + .3, .5 * Hh + .3, .72 * Hh + .3, .9 * Hh + .3]; nm = ['root', 'core', 'sh1', 'sh2', 'sh3']; hd = np.array([[0, y, 0] for y in ys]); hd[0, 1] = 0
            rig = Rig(nm, [-1, 0, 1, 2, 3], hd); y = V[:, 1]; BAi = np.zeros(len(V), int); BBi = np.zeros(len(V), int); BWt = np.zeros(len(V))
            for i in range(len(V)):
                j = int(np.clip(np.searchsorted(ys, y[i]) - 1, 1, 3)); t = float(np.clip((y[i] - ys[j]) / (ys[j + 1] - ys[j]), 0, 1)); BAi[i] = j; BBi[i] = j + 1; BWt[i] = t
            BAi[y < ys[1]] = 1; BBi[y < ys[1]] = 1; BWt[y < ys[1]] = 0
        else:
            rig, src = build_rig_from_import(arm, kind, s, dz); BAi, BBi, BWt = W3
            BAi = BAi.copy(); BBi = BBi.copy(); BWt = BWt.copy()
        nb = rig.nb
        # --- clips
        def add(name, Q, Pp, loop, T, hit=0.0, speed=0.0, fps=0.0): clips.append(dict(name=name, Q=Q, P=Pp, loop=loop, T=T, hit=hit, speed=speed, fps=fps))
        if kind == 'wisp':
            Cw, R0 = wisp_poses()
            for nme, ph, amp, T, sp in (('idle', 1, 1.0, 3.0, 0), ('ready', 1, 1.6, 2.0, 0), ('walk', 1, 1.2, 1.2, 1.9), ('run', 1, 2.0, .8, 3.4)):
                f = 36 if nme in ('idle', 'ready') else 28
                Q, Pp = author(rig, lambda u, a=amp, nm=nme: Cw['sway'](u * 2 * math.pi, a) if nm != 'run' else W(Cw['sway'](u * 2 * math.pi, a), core=(10, 0, 0)), f, True)
                add(nme, Q, Pp, 1, T, 0, sp)
            for nme in ('atk', 'stag', 'hit_f', 'hit_l', 'hit_r', 'defend', 'dodge', 'down', 'dissolve', 'burn', 'burst', 'gutter', 'shatter'):
                keys, hit = Cw[nme]; fr = 20 if nme == 'stag' else (25 if nme == 'down' else (40 if nme in ('atk', 'hit_f', 'hit_l', 'hit_r', 'defend', 'dodge') else 48))
                Q, Pp = author(rig, keys, fr, False, extra='free' if nme == 'shatter' else None)
                add({'atk': 'attack'}.get(nme, nme), Q, Pp, 0, {'stag': .5, 'down': 1.3}.get(nme, 1.0 if fr == 40 else 2.0), hit)
        elif kind == 'quad':
            Cq, R0 = quad_poses()
            for nme in ('idle', 'walk', 'run'):
                act = next((a for a in sorted(bpy.data.actions, key=lambda a: a.name) if a.name.lower().startswith(nme)), None)
                if not act: continue
                f0, f1 = int(act.frame_range[0]), int(act.frame_range[1]); frs = list(range(f0, f1))
                Q, Pp = sample_imported(arm, rig, src, s, dz, act, frs); T = len(frs) / FPS; sp = 0.0
                if nme != 'idle':
                    fo = rig.ix.get('hlP') or rig.ix.get('hlK'); z = Pp[:, fo, 2]; stride = (z.max() - z.min()) / (0.6 if nme == 'walk' else 0.42); sp = stride / T
                add(nme, Q, Pp, 1, T, 0, sp)
            Q, Pp = author(rig, [(0, R0), (.5, mix(R0, W(R0, neck=(-14, 0, 0), head=(20, 0, 0), tail1=(-34, 0, 0)), 1)), (1, R0)], 48, True); add('ready', Q, Pp, 1, 2.4)
            for nme, kind_ in SKILLS.get(key, [('attack', 'bite')]):
                keys, hit = Cq['atk'](kind_); Q, Pp = author(rig, keys, 40, False); add(nme, Q, Pp, 0, 1.0, hit)
            for nme in ('stag', 'hit_f', 'hit_l', 'hit_r', 'defend', 'dodge', 'down', 'dissolve', 'burn', 'burst', 'gutter', 'shatter'):
                keys, hit = Cq[nme]; fr = 20 if nme == 'stag' else (25 if nme == 'down' else (40 if nme in ('hit_f', 'hit_l', 'hit_r', 'defend', 'dodge') else 48))
                Q, Pp = author(rig, keys, fr, False, extra='free' if nme == 'shatter' else None); add(nme, Q, Pp, 0, {'stag': .5, 'down': 1.3}.get(nme, 1.0 if fr == 40 else 2.0), hit)
        else:
            Ch = human_common()
            for nme in ('idle', 'walk', 'run'):
                act = next((a for a in sorted(bpy.data.actions, key=lambda a: a.name) if a.name.lower().startswith(nme)), None)
                f0, f1 = int(act.frame_range[0]), int(act.frame_range[1]); frs = list(range(f0, f1))
                Q, Pp = sample_imported(arm, rig, src, s, dz, act, frs); T = len(frs) / FPS; sp = 0.0
                if nme != 'idle':
                    z = Pp[:, rig.ix['lFoot'], 2]; stride = (z.max() - z.min()) / (0.6 if nme == 'walk' else 0.42); sp = stride / T
                add(nme, Q, Pp, 1, T, 0, sp)
            Q, Pp = author(rig, with_def([(0, H_READY), (.5, W(H_READY, spine=(8, 0, 0), chest=(6, 0, 0))), (1, H_READY)], HUM_DEF), 48, True); add('ready', Q, Pp, 1, 2.4)
            for nme, kind_ in SKILLS.get(key, [('attack', 'slash1')]):
                keys, hit = human_keys(kind_); Q, Pp = author(rig, with_def(keys, HUM_DEF), 40, False); add(nme, Q, Pp, 0, 1.0, hit)
            for nme in ('stag', 'hit_f', 'hit_l', 'hit_r', 'defend', 'dodge', 'down', 'dissolve', 'burn', 'burst', 'gutter', 'shatter'):
                keys, hit = Ch[nme]; fr = 20 if nme == 'stag' else (25 if nme == 'down' else (40 if nme in ('hit_f', 'hit_l', 'hit_r', 'defend', 'dodge') else 48))
                Q, Pp = author(rig, with_def(keys, HUM_DEF), fr, False, extra='free' if nme == 'shatter' else None); add(nme, Q, Pp, 0, {'stag': .5, 'down': 1.3}.get(nme, 1.0 if fr == 40 else 2.0), hit)
        # ao bake in the hang / ready pose
        R_Q, R_P = (clips[[c['name'] for c in clips].index('ready')]['Q'][0], clips[[c['name'] for c in clips].index('ready')]['P'][0])
        if kind == 'human':
            Q0, P0 = solve(rig, {rig.ix[k]: v for k, v in dict(HUM_DEF).items() if k in rig.ix}); Vp = skin(Q0, P0, rig.head, BAi, BBi, BWt, V)
        else: Vp = skin(R_Q, R_P, rig.head, BAi, BBi, BWt, V)
        Vp = Vp.copy(); Vp[:, 1] = Vp[:, 1] - min(0, Vp[:, 1].min())
        # fx
        if kind == 'human':
            rh = rig.head[rig.ix['rHand']] if 'rHand' in rig.ix else rig.head[-1]; lf = rig.head[rig.ix['lHand']]; sc = 1.0
            fx = [('tip', 'wR', rh + np.array([0, 0, .88])), ('base', 'wR', rh + np.array([0, 0, .06])), ('off', 'lHand', lf + np.array([0, 0, .1]))]
        else:
            hb = rig.head[rig.ix['head']] if 'head' in rig.ix else rig.head[-1]; top = rig.head[-1] if kind == 'wisp' else hb
            fx = [('tip', 'head' if kind == 'quad' else 'sh3', top + np.array([0, .05, .3 if kind == 'quad' else .1])), ('head', 'head' if kind == 'quad' else 'sh3', top)]
    else:
        Vp = V.copy(); rig = Rig(['root'], [-1], np.zeros((1, 3))); BAi = BBi = np.zeros(len(V), int); BWt = np.zeros(len(V))
    ao = bake_ao(Vp, Nsm, tris, CFG[key]['H'] if character else max(1.8, V[:, 1].max()))
    cav = cavity(V, Nsm, gid)
    t_bake = time.time() - t0
    write_lfm(key, V, Nsm, Cp, ao, UV, rough, metal, surf, grime, cav, tris, rig, BAi, BBi, BWt, clips, fx, character)
    export_glb(key, V, Nsm, Cp, ao, cav, tris, rig, BAi, BBi, BWt, clips, character, V)
    if not character: write_col(key, V)
    print(f'[{key}] verts={len(V)} tris={nt} bones={rig.nb} clips={len(clips)} bake={t_bake:.1f}s total={time.time() - t0:.1f}s', flush=True)
    return dict(key=key, verts=len(V), tris=nt, bones=rig.nb, clips=[c['name'] for c in clips])

# ------------------------------------------------------------------ writers
def s12(x, n): b = x.encode()[:n]; return b + b'\0' * (n - len(b))
LFM_ORDER = ['idle', 'ready', 'walk', 'run', 'attack', 'bash', 'cleave', 'aimed', 'kindle', 'tonic', 'guard', 'sunder', 'volley', 'cinder', 'firepot', 'lance', 'hoar', 'quick', 'special',
             'stag', 'down', 'defend', 'dodge', 'hit_f', 'hit_l', 'hit_r', 'shatter', 'dissolve', 'burn', 'burst', 'gutter']
def write_lfm(key, V, N, C, ao, UV, rough, metal, surf, grime, cav, tris, rig, BA, BB, BW, clips, fx, character):
    cfg = CFG.get(key, dict(scale=1.0, radius=max(abs(V[:, 0]).max(), abs(V[:, 2]).max()), hpY=V[:, 1].max(), height=V[:, 1].max(), death='fall'))
    own = [c for c in clips if c['name'] not in ('dissolve', 'burn', 'burst', 'gutter', 'shatter') or c['name'] == cfg['death']]
    sel = sorted(own, key=lambda c: LFM_ORDER.index(c['name']) if c['name'] in LFM_ORDER else 99)[:16]
    nv, nt, nb = len(V), len(tris), rig.nb; buf = bytearray(b'LFM1')
    buf += struct.pack('<7I', nv, nt, nb, len(sel), len(fx), nv, nt * 3) + struct.pack('<4f', cfg['scale'], cfg['radius'], cfg['hpY'], cfg['height'])
    buf += struct.pack('<3f', *DISCOL.get(cfg['death'], (1, .45, .1))) + struct.pack('<BBBB', 1 if key in HEROES else 0, 1 if key == 'gloamwisp' else 0, 0, 0) + s12(cfg['death'], 12) + s12(key, 16)
    for n in rig.names: buf += s12(n, 12)
    buf += np.array(rig.parent, np.int8).tobytes() + b'\0' * ((4 - nb % 4) % 4)
    buf += rig.head.astype('<f4').tobytes() + np.zeros(nb, np.uint8).tobytes() + b'\0' * ((4 - nb % 4) % 4)
    for nm, bn, p in fx: buf += s12(nm, 12) + struct.pack('<i3f', rig.ix[bn], *[float(x) for x in p])
    vt = np.zeros(nv, dtype=[('p', '<f4', 3), ('n', 'i1', 4), ('c', 'u1', 4), ('uv', '<f4', 2), ('m', 'u1', 4), ('b', 'u1', 4)])
    vt['p'] = V; vt['n'][:, :3] = np.clip(np.round(N * 127), -127, 127).astype(np.int8)
    vt['c'][:, :3] = np.clip(np.round(srgb_gamma(C) * 255), 0, 255).astype(np.uint8); vt['c'][:, 3] = np.clip(np.round(ao * 255), 0, 255).astype(np.uint8); vt['uv'] = UV
    vt['m'][:, 0] = np.clip(np.round(rough * 255), 0, 255); vt['m'][:, 1] = np.clip(np.round(metal * 255), 0, 255); vt['m'][:, 2] = surf.astype(np.uint8); vt['m'][:, 3] = np.clip(np.round(grime / 2.0 * 255), 0, 255)
    vt['b'][:, 0] = BA; vt['b'][:, 1] = BB; vt['b'][:, 2] = np.clip(np.round(BW * 255), 0, 255); vt['b'][:, 3] = np.clip(np.round((cav * .5 + .5) * 255), 0, 255)
    buf += vt.tobytes() + tris.astype('<u2').ravel().tobytes()
    for c in sel:
        Q = c['Q']; Pp = c['P']; fr = len(Q); buf += s12(c['name'], 12) + struct.pack('<HBB', fr, c['loop'], 0) + struct.pack('<4f', c['fps'], c['hit'], c['speed'], c['T'])
        qd = np.clip(np.round(Q * 32767), -32767, 32767).astype('<i2'); pd = np.clip(np.round(Pp * 2048), -32767, 32767).astype('<i2'); buf += np.concatenate([qd, pd], axis=2).tobytes()
    open(f'{LFM_DIR}/{key}.lfm', 'wb').write(bytes(buf))

def write_col(key, V):
    mn, mx = V.min(0), V.max(0); xz = V[:, [0, 2]]; pts = sorted(set(map(tuple, np.round(xz, 2).tolist())))
    def cross(o, a, b): return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo = []; up = []
    for p in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0: lo.pop()
        lo.append(p)
    for p in reversed(pts):
        while len(up) >= 2 and cross(up[-2], up[-1], p) <= 0: up.pop()
        up.append(p)
    hull = lo[:-1] + up[:-1]
    while len(hull) > 12:   # drop the least significant vertex
        areas = [abs(cross(hull[i - 1], hull[i], hull[(i + 1) % len(hull)])) for i in range(len(hull))]; hull.pop(int(np.argmin(areas)))
    json.dump(dict(key=key, aabb=[mn.tolist(), mx.tolist()], radius=float(np.sqrt(((xz.max(0) - xz.min(0)) ** 2).sum()) / 2), height=float(mx[1]), footprint=[list(p) for p in hull],
                   note='collision proxy: AABB + circle + <=12-pt convex footprint, game space (Y up, origin on ground)'), open(f'{LFM_DIR}/{key}.col.json', 'w'))

def export_glb(key, V, N, C, ao, cav, tris, rig, BA, BB, BW, clips, character, Vg):
    reset(); scn = bpy.context.scene; scn.render.fps = int(FPS)
    Vb = from_game(V); Nb = from_game(N)
    me = bpy.data.meshes.new(key); me.from_pydata(Vb.tolist(), [], tris.tolist()); me.update()
    me.polygons.foreach_set('use_smooth', [True] * len(me.polygons)); me.normals_split_custom_set_from_vertices(Nb.tolist())
    ca = me.color_attributes.new('Col', 'FLOAT_COLOR', 'POINT'); shade = (.25 + .75 * ao)[:, None] * (1.0 + .18 * np.clip(cav, 0, 1))[:, None]
    ca.data.foreach_set('color', np.c_[np.clip(C * shade, 0, 1), np.ones(len(V))].ravel())
    mat = bpy.data.materials.new('LF_' + key); mat.use_nodes = True; nt_ = mat.node_tree; bs = nt_.nodes['Principled BSDF']; vc = nt_.nodes.new('ShaderNodeVertexColor'); vc.layer_name = 'Col'
    nt_.links.new(vc.outputs['Color'], bs.inputs['Base Color']); bs.inputs['Roughness'].default_value = .85; me.materials.append(mat)
    obj = bpy.data.objects.new(key, me); scn.collection.objects.link(obj)
    if character:
        ad = bpy.data.armatures.new('Rig_' + key); ao_ = bpy.data.objects.new('Rig_' + key, ad); scn.collection.objects.link(ao_)
        bpy.context.view_layer.objects.active = ao_; bpy.ops.object.mode_set(mode='EDIT'); ebs = []
        kids = {i: [] for i in range(rig.nb)}
        for i, p in enumerate(rig.parent):
            if p >= 0: kids[p].append(i)
        for i, nm in enumerate(rig.names):
            eb = ad.edit_bones.new(nm); hb = from_game(rig.head[i]); eb.head = Vector(hb); tl = hb + np.array([0, 0, .08])
            if kids[i]:
                c = max(kids[i], key=lambda k: np.linalg.norm(rig.head[k] - rig.head[i])); d = from_game(rig.head[c]) - hb
                if np.linalg.norm(d) > .03: tl = hb + d
            eb.tail = Vector(tl); ebs.append(eb)
        for i, p in enumerate(rig.parent):
            if p >= 0: ebs[i].parent = ebs[p]
        bpy.ops.object.mode_set(mode='OBJECT')
        vgs = [obj.vertex_groups.new(name=n) for n in rig.names]
        for b in range(rig.nb):
            ia = np.nonzero(BA == b)[0]; wa = 1 - BW[ia]
            for w_ in np.unique(np.round(wa, 3)):
                sel = ia[np.round(wa, 3) == w_]
                if w_ > 0: vgs[b].add(sel.tolist(), float(w_), 'REPLACE')
            ib = np.nonzero((BB == b) & (BW > 0))[0]
            for w_ in np.unique(np.round(BW[ib], 3)): vgs[b].add(ib[np.round(BW[ib], 3) == w_].tolist(), float(w_), 'ADD')
        obj.parent = ao_; md = obj.modifiers.new('Armature', 'ARMATURE'); md.object = ao_
        Rm = [np.array(ad.bones[n].matrix_local) for n in rig.names]; ao_.animation_data_create()
        for c in clips:
            act = bpy.data.actions.new(c['name']); Q, Pp = c['Q'], c['P']; nf = len(Q); tmult = (c['T'] * FPS) / (nf if c['loop'] else max(1, nf - 1))
            frames = np.arange(nf) * tmult
            # S_game -> blender: S_b = C^-1 S C ; pose basis B_b = R_b^-1 S_p^-1 S_b R_b
            Sb = np.zeros((nf, rig.nb, 4, 4))
            for b in range(rig.nb):
                x, y, z, w = Q[:, b, 0], Q[:, b, 1], Q[:, b, 2], Q[:, b, 3]
                Rg = np.stack([1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), 2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w), 2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)], 1).reshape(-1, 3, 3)
                Rb = C3.T @ Rg @ C3; tb = from_game(Pp[:, b]) - np.einsum('nij,j->ni', Rb, from_game(rig.head[b]))
                Sb[:, b, :3, :3] = Rb; Sb[:, b, :3, 3] = tb; Sb[:, b, 3, 3] = 1
            for b, nm in enumerate(rig.names):
                Rb_ = Rm[b]; Ri = np.linalg.inv(Rb_); loc = np.zeros((nf, 3)); rq = np.zeros((nf, 4))
                for f in range(nf):
                    S = Sb[f, b] if rig.parent[b] < 0 else np.linalg.inv(Sb[f, rig.parent[b]]) @ Sb[f, b]
                    Bm = Ri @ S @ Rb_; q = Matrix(Bm[:3, :3].tolist()).to_quaternion(); rq[f] = [q.w, q.x, q.y, q.z]; loc[f] = Bm[:3, 3]
                for ch in range(1, nf):
                    if rq[ch] @ rq[ch - 1] < 0: rq[ch] *= -1
                for dp, arr, n in (('location', loc, 3), ('rotation_quaternion', rq, 4)):
                    for k in range(n):
                        fc = act.fcurves.new(data_path=f'pose.bones["{nm}"].{dp}', index=k, action_group=nm); fc.keyframe_points.add(nf)
                        co = np.empty(nf * 2); co[0::2] = frames; co[1::2] = arr[:, k]; fc.keyframe_points.foreach_set('co', co)
                        for kp in fc.keyframe_points: kp.interpolation = 'LINEAR'
            tr = ao_.animation_data.nla_tracks.new(); tr.name = c['name']; st = tr.strips.new(c['name'], 0, act)
        ao_.animation_data.action = None
    mode = 'NLA_TRACKS' if character else 'ACTIVE_ACTIONS'
    kw = dict(filepath=f'{OUT}/glb/{key}.glb', export_format='GLB', export_animations=bool(character), export_apply=False)
    if character: kw['export_animation_mode'] = 'NLA_TRACKS'
    bpy.ops.object.select_all(action='SELECT')
    try: bpy.ops.export_scene.gltf(**kw, export_vertex_color='ACTIVE')
    except TypeError: bpy.ops.export_scene.gltf(**kw)

results = []
for k in KEYS:
    try: results.append(build(k))
    except Exception as e:
        import traceback; traceback.print_exc(); print(f'[{k}] FAILED {e}', flush=True)
json.dump(results, open(f'{OUT}/build_{"_".join(KEYS)[:40]}.json', 'w'))
