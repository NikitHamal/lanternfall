"""Render a quick 3-view preview (front / side / back, point-splat z-buffer) of a GLB: python3 tools/glb_preview.py in.glb out.png"""
import sys, os, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from glb_lib import load_glb
from PIL import Image
import fast_simplification as fs
from scipy.spatial import cKDTree

def splat(P, C, N, yaw, W=360, H=480, up=1):
    c, s = np.cos(yaw), np.sin(yaw)
    x = P[:, 0] * c + P[:, 2] * s; z = -P[:, 0] * s + P[:, 2] * c; y = P[:, 1]
    nx = N[:, 0] * c + N[:, 2] * s; nz = -N[:, 0] * s + N[:, 2] * c; ny = N[:, 1]
    lo = np.array([x.min(), y.min()]); hi = np.array([x.max(), y.max()])
    sc = min((W - 20) / max(hi[0] - lo[0], 1e-6), (H - 20) / max(hi[1] - lo[1], 1e-6))
    px = ((x - (lo[0] + hi[0]) / 2) * sc + W / 2).astype(int); py = (H / 2 - (y - (lo[1] + hi[1]) / 2) * sc).astype(int)
    sh = np.clip(0.35 + 0.65 * np.clip(nx * -0.4 + ny * 0.6 + nz * -0.7, 0, 1), 0, 1)
    img = np.full((H, W, 3), 40, np.uint8); zb = np.full((H, W), 1e9)
    order = np.argsort(-z)
    col = np.clip((C * sh[:, None]) ** (1 / 2.2), 0, 1) * 255
    for dx in (0, 1):
        for dy in (0, 1):
            xx = np.clip(px + dx, 0, W - 1); yy = np.clip(py + dy, 0, H - 1)
            img[yy[order], xx[order]] = col[order].astype(np.uint8)
    return img

def vnormals(P, T):
    n = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]]); N = np.zeros_like(P)
    for k in range(3): np.add.at(N, T[:, k], n)
    return N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)

if __name__ == '__main__':
    g = load_glb(sys.argv[1], 1024); P, T, C = g['P'], g['T'], g['Cl']
    print(sys.argv[1], 'verts', len(P), 'tris', len(T), 'bbox', P.min(0).round(2), P.max(0).round(2), 'skinned', g['skinned'])
    tgt = min(len(T), 60000)
    if len(T) > tgt:
        Pd, Td = fs.simplify(P.astype(np.float32), T.astype(np.int32), target_reduction=1 - tgt / len(T))
        d, i = cKDTree(P).query(Pd, k=1); Cd = C[i]
    else: Pd, Td, Cd = P, T, C
    N = vnormals(Pd, Td)
    ims = [splat(Pd, Cd, N, a) for a in (0, np.pi / 2, np.pi)]
    Image.fromarray(np.concatenate(ims, 1)).save(sys.argv[2])
