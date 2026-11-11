#include "meshes.h"
#include <cstdio>
Mesh gProp[NPK][5]; Mesh gShape[4]; Figure gHero[NCLS], gMon[NMON];
namespace {
struct MB {
  std::vector<Vtx> v; std::vector<uint32_t> idx; V3 col{0.5f, 0.5f, 0.5f}; float rough = 0.9f, metal = 0, glow = 0;
  void mat(V3 c, float r, float m, float g) { col = c; rough = r; metal = m; glow = g; }
  void pushV(V3 p) { Vtx x{}; x.p[0] = p.x; x.p[1] = p.y; x.p[2] = p.z; x.c[0] = col.x; x.c[1] = col.y; x.c[2] = col.z; x.m[0] = rough; x.m[1] = metal; x.m[2] = glow; v.push_back(x); }
  void finish(size_t v0, size_t i0, const M4& xf) {
    std::vector<V3> nn(v.size() - v0, V3{});
    for (size_t i = i0; i + 2 < idx.size(); i += 3) { Vtx &a = v[idx[i]], &b = v[idx[i + 1]], &c = v[idx[i + 2]]; V3 pa{a.p[0], a.p[1], a.p[2]}, pb{b.p[0], b.p[1], b.p[2]}, pc{c.p[0], c.p[1], c.p[2]}; V3 n = cross(pb - pa, pc - pa);
      for (int k = 0; k < 3; k++) { V3& q = nn[idx[i + k] - v0]; q = q + n; } }
    for (size_t i = v0; i < v.size(); i++) { V3 n = norm(nn[i - v0]); V3 p{v[i].p[0], v[i].p[1], v[i].p[2]}; p = xformPoint(xf, p); V3 nr{xf.m[0] * n.x + xf.m[4] * n.y + xf.m[8] * n.z, xf.m[1] * n.x + xf.m[5] * n.y + xf.m[9] * n.z, xf.m[2] * n.x + xf.m[6] * n.y + xf.m[10] * n.z}; nr = norm(nr);
      v[i].p[0] = p.x; v[i].p[1] = p.y; v[i].p[2] = p.z; v[i].n[0] = nr.x; v[i].n[1] = nr.y; v[i].n[2] = nr.z; }
  }
  template <class F> void grid(int nu, int nv, F fn, const M4& xf, bool capTop = false, bool capBot = false) {  // wraps in u
    size_t v0 = v.size(), i0 = idx.size();
    for (int j = 0; j <= nv; j++) for (int i = 0; i < nu; i++) pushV(fn((float)i / nu, (float)j / nv));
    for (int j = 0; j < nv; j++) for (int i = 0; i < nu; i++) { uint32_t a = v0 + j * nu + i, b = v0 + j * nu + (i + 1) % nu, c = v0 + (j + 1) * nu + i, d = v0 + (j + 1) * nu + (i + 1) % nu; idx.insert(idx.end(), {a, c, b, b, c, d}); }
    if (capTop) { V3 s{}; for (int i = 0; i < nu; i++) { Vtx& q = v[v0 + nv * nu + i]; s = s + V3{q.p[0], q.p[1], q.p[2]}; } s = s * (1.f / nu); uint32_t ci = v.size(); pushV(s); for (int i = 0; i < nu; i++) idx.insert(idx.end(), {ci, (uint32_t)(v0 + nv * nu + (i + 1) % nu), (uint32_t)(v0 + nv * nu + i)}); }
    if (capBot) { V3 s{}; for (int i = 0; i < nu; i++) { Vtx& q = v[v0 + i]; s = s + V3{q.p[0], q.p[1], q.p[2]}; } s = s * (1.f / nu); uint32_t ci = v.size(); pushV(s); for (int i = 0; i < nu; i++) idx.insert(idx.end(), {ci, (uint32_t)(v0 + i), (uint32_t)(v0 + (i + 1) % nu)}); }
    finish(v0, i0, xf);
  }
  void ellipsoid(float rx, float ry, float rz, const M4& xf, int nu = 12, int nv = 8) { grid(nu, nv, [&](float u, float w) { float a = u * TAU, b = (w - 0.5f) * PI, cb = std::cos(b); return V3{std::cos(a) * cb * rx, std::sin(b) * ry, std::sin(a) * cb * rz}; }, xf); }
  void box(float sx, float sy, float sz, const M4& xf) {
    size_t v0 = v.size(), i0 = idx.size(); (void)i0; static const float N[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int f = 0; f < 6; f++) { V3 n{N[f][0], N[f][1], N[f][2]}, t = std::fabs(n.y) > 0.5f ? V3{1, 0, 0} : V3{0, 1, 0}; V3 b = cross(n, t); t = cross(b, n); uint32_t base = v.size();
      for (int k = 0; k < 4; k++) { float a = (k & 1) ? 1.f : -1.f, c = (k & 2) ? 1.f : -1.f; V3 p = (n + t * a + b * c); p = {p.x * sx * 0.5f, p.y * sy * 0.5f, p.z * sz * 0.5f}; pushV(p); Vtx& q = v.back(); q.n[0] = n.x; q.n[1] = n.y; q.n[2] = n.z; }
      V3 e1{v[base + 1].p[0] - v[base].p[0], v[base + 1].p[1] - v[base].p[1], v[base + 1].p[2] - v[base].p[2]}, e2{v[base + 2].p[0] - v[base].p[0], v[base + 2].p[1] - v[base].p[1], v[base + 2].p[2] - v[base].p[2]}; bool flip = dot(cross(e1, e2), n) < 0;
      if (!flip) idx.insert(idx.end(), {base, base + 1, base + 2, base + 1, base + 3, base + 2}); else idx.insert(idx.end(), {base, base + 2, base + 1, base + 1, base + 2, base + 3}); }
    for (size_t i = v0; i < v.size(); i++) { V3 p{v[i].p[0], v[i].p[1], v[i].p[2]}, n{v[i].n[0], v[i].n[1], v[i].n[2]}; p = xformPoint(xf, p); V3 nr{xf.m[0] * n.x + xf.m[4] * n.y + xf.m[8] * n.z, xf.m[1] * n.x + xf.m[5] * n.y + xf.m[9] * n.z, xf.m[2] * n.x + xf.m[6] * n.y + xf.m[10] * n.z}; nr = norm(nr);
      v[i].p[0] = p.x; v[i].p[1] = p.y; v[i].p[2] = p.z; v[i].n[0] = nr.x; v[i].n[1] = nr.y; v[i].n[2] = nr.z; }
  }
  void lathe(const std::vector<std::pair<float, float>>& pr, int seg, const M4& xf) { int n = pr.size() - 1; grid(seg, n, [&](float u, float w) { auto& q = pr[(int)std::round(w * n)]; float a = u * TAU; return V3{std::cos(a) * q.first, q.second, std::sin(a) * q.first}; }, xf, pr.back().first > 0.001f, pr.front().first > 0.001f); }
  template <class RF> void tube(const std::vector<V3>& pts, RF rad, int seg, const M4& xf) {
    int n = pts.size() - 1; grid(seg, n, [&](float u, float w) { int k = (int)std::round(w * n); V3 T = norm(pts[std::min(k + 1, n)] - pts[std::max(k - 1, 0)]); V3 ax = std::fabs(T.y) < 0.9f ? V3{0, 1, 0} : V3{1, 0, 0}; V3 N = norm(cross(T, ax)), B = cross(T, N); float a = u * TAU, r = rad(w);
      return pts[k] + N * (std::cos(a) * r) + B * (std::sin(a) * r); }, xf, true, true); }
  struct Ring { float y, rx, rz, x, z, sq; };
  void loft(const std::vector<Ring>& rg, int seg, const M4& xf, bool cap0, bool cap1) { int n = rg.size() - 1; grid(seg, n, [&](float u, float w) { const Ring& r = rg[(int)std::round(w * n)]; float a = u * TAU, c = std::cos(a), s = std::sin(a), e = 2.f / r.sq;
      return V3{r.x + (c < 0 ? -1.f : 1.f) * std::pow(std::fabs(c), e) * r.rx, r.y, r.z + (s < 0 ? -1.f : 1.f) * std::pow(std::fabs(s), e) * r.rz}; }, xf, cap1, cap0); }
};
V3 rgb(float s) { return V3{s, s, s}; }
M4 TR(float x, float y, float z, float rx = 0, float ry = 0, float rz = 0) { return mul(translate(x, y, z), mul(rotY(ry), mul(rotX(rx), rotZ(rz)))); }
void gRock(MB& g, uint32_t seed, float sx, float sy, float sz) {
  Rng r(seed); float ox = r() * 50, oy = r() * 50; g.mat(C(0x4a4a46), 0.92f, 0, 0);
  g.grid(14, 9, [&](float u, float v) { float a = u * TAU, b = (v - 0.5f) * PI, cb = std::cos(b), x = std::cos(a) * cb, y = std::sin(b), z = std::sin(a) * cb;
    float d = 0.72f + 0.5f * fbm3(x * 1.3f + ox, y * 1.3f + oy, z * 1.3f, 3) + 0.12f * std::round(vnoise3(x * 2 + oy, y * 2, z * 2 + ox) * 2);
    return V3{x * d * sx, std::max(-0.25f * sy, y * d * sy), z * d * sz}; }, ident());
}
void gStanding(MB& g, uint32_t seed) {
  Rng r(seed); float h = 2.0f + r() * 1.9f, rx = 0.34f + r() * 0.22f, rz = 0.17f + r() * 0.12f; g.mat(C(0x45474a), 0.93f, 0, 0);
  std::vector<MB::Ring> rings; int n = 6; for (int i = 0; i <= n; i++) { float t = (float)i / n; rings.push_back({-0.4f + t * (h + 0.4f), rx * (1 - 0.35f * t * t) * (0.9f + r() * 0.2f), rz * (1 - 0.3f * t) * (0.9f + r() * 0.2f), (r() - 0.5f) * 0.08f, (r() - 0.5f) * 0.05f, 3.2f}); }
  rings.push_back({h + 0.06f, rx * 0.3f, rz * 0.35f, 0.04f, 0, 2.5f}); g.loft(rings, 12, TR(0, 0, 0, (r() - 0.5f) * 0.22f, r() * TAU, (r() - 0.5f) * 0.3f), false, true);
}
void gTree(MB& g, uint32_t seed) {
  Rng r(seed); g.mat(C(0x2a221b), 0.95f, 0, 0);
  struct Br { static void go(MB& g, Rng& r, V3 p, V3 d, float len, float rad, int depth) {
    std::vector<V3> pts{p}; V3 q = p, dir = d; int n = 4;
    for (int i = 0; i < n; i++) { dir.x += 0.22f + (r() - 0.5f) * 0.5f; dir.y += (r() - 0.35f) * 0.35f; dir.z += (r() - 0.5f) * 0.5f; dir = norm(dir); q = q + dir * (len / n); pts.push_back(q); }
    g.tube(pts, [&](float t) { return rad * (1 - t * 0.6f); }, depth > 1 ? 8 : 5, ident());
    if (depth > 0) { int kids = depth > 2 ? 3 : 2 + (r() < 0.4f ? 1 : 0); for (int k = 0; k < kids; k++) { V3 at = pts[std::min(n, 1 + (int)(r() * 3) + (k == 0 ? 1 : 0))]; V3 nd = norm(V3{dir.x + (r() - 0.5f) * 1.6f, dir.y + (r() - 0.2f) * 0.9f, dir.z + (r() - 0.5f) * 1.6f}); go(g, r, at, nd, len * (0.55f + r() * 0.2f), rad * (k == 0 ? 0.62f : 0.45f), depth - 1); } } } };
  Br::go(g, r, {0, -0.3f, 0}, {0.05f, 1, 0}, 2.6f + r() * 1.6f, 0.16f + r() * 0.1f, 4);
}
void gWall(MB& g, uint32_t seed) {
  Rng r(seed); float L = 4.2f, broke = r() < 0.5f ? r() * L : -9;
  for (int c = 0; c < 5; c++) { float x = -L / 2 + (c % 2) * 0.15f;
    while (x < L / 2) { float w = 0.26f + r() * 0.26f, h = 0.15f + r() * 0.07f, d = 0.3f + r() * 0.14f; bool gap = std::fabs(x + L / 2 - broke) < 0.5f + c * 0.22f;
      if (!(gap && c > 0) && !(c == 4 && r() < 0.25f)) { float s = 0.35f + r() * 0.3f; g.mat(rgb(s * 0.2f + 0.03f), 0.93f, 0, 0); g.ellipsoid(w * 0.58f, h * 0.62f, d * 0.55f, TR(x + w / 2, c * 0.2f + h * 0.4f + (r() - 0.5f) * 0.02f, (r() - 0.5f) * 0.08f, (r() - 0.5f) * 0.14f, (r() - 0.5f) * 0.3f, (r() - 0.5f) * 0.12f), 8, 5); }
      x += w + 0.015f; } }
}
void gCairn(MB& g, uint32_t seed) {
  Rng r(seed); float y = 0; int n = 5 + (int)(r() * 4);
  for (int i = 0; i < n; i++) { float s = 0.42f * (1 - (float)i / n * 0.72f) * (0.85f + r() * 0.3f), h = 0.1f + r() * 0.07f, c = 0.16f + r() * 0.12f; g.mat(V3{c, c, c * 0.98f}, 0.93f, 0, 0);
    g.ellipsoid(s, h * 0.8f, s * 0.7f, TR((r() - 0.5f) * 0.06f, y + h * 0.5f, (r() - 0.5f) * 0.06f, (r() - 0.5f) * 0.12f, r() * TAU, (r() - 0.5f) * 0.12f), 9, 5); y += h * 0.9f; }
}
void gFence(MB& g, uint32_t seed) {
  Rng r(seed); g.mat(C(0x3a3027), 0.9f, 0, 0); V3 tops[3];
  for (int i = 0; i < 3; i++) { float x = (i - 1) * 1.9f + (r() - 0.5f) * 0.2f, lean = (r() - 0.5f) * 0.35f, lz = (r() - 0.5f) * 0.3f, h = 1.0f + r() * 0.3f;
    g.tube({{x, -0.3f, 0}, {x + lean * 0.5f, h * 0.5f, lz * 0.5f}, {x + lean, h, lz}}, [](float t) { return 0.055f - t * 0.012f; }, 6, ident()); tops[i] = {x + lean * 0.8f, h * 0.78f, lz * 0.8f}; }
  for (int i = 0; i < 2; i++) if (r() < 0.75f) { V3 a = tops[i], b = tops[i + 1]; g.tube({a, {(a.x + b.x) / 2, (a.y + b.y) / 2 - 0.03f, (a.z + b.z) / 2 + 0.03f}, b}, [](float) { return 0.032f; }, 5, ident()); }
}
void gWaylight(MB& g) {
  V3 iron = C(0x1d1c1b); g.mat(C(0x47474a), 0.93f, 0, 0);
  g.loft({{-0.2f, 0.52f, 0.52f, 0, 0, 5}, {0.16f, 0.5f, 0.5f, 0, 0, 5}, {0.2f, 0.4f, 0.4f, 0, 0, 5}, {0.44f, 0.36f, 0.36f, 0, 0, 5}}, 12, ident(), false, true);
  g.mat(iron, 0.62f, 0.85f, 0);
  g.lathe({{0.09f, 0.44f}, {0.1f, 0.5f}, {0.06f, 0.56f}, {0.05f, 1.2f}, {0.07f, 1.24f}, {0.07f, 1.3f}, {0.045f, 1.34f}, {0.04f, 2.9f}, {0.06f, 2.94f}, {0.06f, 3.0f}, {0.035f, 3.04f}, {0.03f, 3.5f}, {0.0f, 3.62f}}, 8, ident());
  g.tube({{0, 3.3f, 0}, {0.12f, 3.62f, 0}, {0.42f, 3.78f, 0}, {0.7f, 3.62f, 0}, {0.74f, 3.38f, 0}}, [](float) { return 0.022f; }, 6, ident());
  g.tube({{0, 2.95f, 0}, {0.25f, 3.2f, 0}, {0.5f, 3.6f, 0}}, [](float) { return 0.014f; }, 5, ident());
  M4 t = translate(0.74f, 2.92f, 0);
  g.lathe({{0.0f, 0.46f}, {0.03f, 0.44f}, {0.05f, 0.4f}, {0.17f, 0.33f}, {0.18f, 0.31f}, {0.14f, 0.31f}}, 6, t); g.lathe({{0.14f, 0.0f}, {0.16f, 0.0f}, {0.16f, 0.03f}, {0.1f, 0.03f}, {0.05f, -0.03f}, {0.0f, -0.04f}}, 6, t);
  for (int i = 0; i < 6; i++) { float a = i / 6.f * TAU; g.tube({{std::cos(a) * 0.145f, 0.02f, std::sin(a) * 0.145f}, {std::cos(a) * 0.145f, 0.32f, std::sin(a) * 0.145f}}, [](float) { return 0.011f; }, 4, t); }
  g.mat(C(0xffb45a), 0.4f, 0, 1.0f); g.lathe({{0.0f, 0.04f}, {0.125f, 0.04f}, {0.125f, 0.3f}, {0.0f, 0.3f}}, 6, t);
}
void upload(Mesh& m, MB& g, int cap) {
  glGenVertexArrays(1, &m.vao); glBindVertexArray(m.vao); glGenBuffers(1, &m.vbo); glBindBuffer(GL_ARRAY_BUFFER, m.vbo); glBufferData(GL_ARRAY_BUFFER, g.v.size() * sizeof(Vtx), g.v.data(), GL_STATIC_DRAW);
  glGenBuffers(1, &m.ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, g.idx.size() * 4, g.idx.data(), GL_STATIC_DRAW); m.nIdx = (int)g.idx.size();
  for (int i = 0; i < 4; i++) { glEnableVertexAttribArray(i); glVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)(intptr_t)(i * 12)); }
  m.cap = cap; glGenBuffers(1, &m.inst); glBindBuffer(GL_ARRAY_BUFFER, m.inst); glBufferData(GL_ARRAY_BUFFER, cap * sizeof(Inst), nullptr, GL_DYNAMIC_DRAW);
  for (int i = 0; i < 6; i++) { glEnableVertexAttribArray(4 + i); glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Inst), (void*)(intptr_t)(i * 16)); glVertexAttribDivisor(4 + i, 1); }
  glBindVertexArray(0);
}
// ---- character parts ----
struct FB { Figure* f; V3 (*unused)(); };
Figure* cur; 
void P(int shape, int bone, float x, float y, float z, float sx, float sy, float sz, uint32_t hex, float rough = 0.8f, float metal = 0, float glow = 0, float rx = 0, float rz = 0) { cur->parts.push_back({(uint8_t)shape, (uint8_t)bone, x, y, z, sx, sy, sz, rx, rz, C(hex), rough, metal, glow}); }
void pivots(Figure& f) { float (*pv)[3] = f.pivot; auto s = [&](int b, float x, float y, float z) { pv[b][0] = x; pv[b][1] = y; pv[b][2] = z; };
  s(BN_ROOT, 0, 0.9f, 0); s(BN_HEAD, 0, 1.52f, 0); s(BN_LARM, -0.29f, 1.42f, 0); s(BN_RARM, 0.29f, 1.42f, 0); s(BN_LLEG, -0.11f, 0.9f, 0); s(BN_RLEG, 0.11f, 0.9f, 0); s(BN_LLEG2, -0.11f, 0.9f, 0); s(BN_RLEG2, 0.11f, 0.9f, 0); s(BN_TAIL, 0, 1.0f, -0.4f); s(BN_JAW, 0, 1.5f, 0.2f); }
void human(uint32_t skin, uint32_t shirt, uint32_t pants, uint32_t boots, float w = 1.f) {
  P(SH_CYL, BN_LLEG, -0.11f, 0.5f, 0, 0.075f, 0.82f, 0.075f, pants); P(SH_CYL, BN_RLEG, 0.11f, 0.5f, 0, 0.075f, 0.82f, 0.075f, pants);
  P(SH_BOX, BN_LLEG, -0.11f, 0.1f, 0.04f, 0.13f, 0.18f, 0.26f, boots, 0.7f); P(SH_BOX, BN_RLEG, 0.11f, 0.1f, 0.04f, 0.13f, 0.18f, 0.26f, boots, 0.7f);
  P(SH_BOX, BN_ROOT, 0, 1.2f, 0, 0.44f * w, 0.56f, 0.26f * w, shirt); P(SH_BOX, BN_ROOT, 0, 0.88f, 0, 0.4f * w, 0.14f, 0.24f * w, pants, 0.8f);
  P(SH_CYL, BN_LARM, -0.29f * w, 1.17f, 0, 0.065f, 0.52f, 0.065f, shirt); P(SH_CYL, BN_RARM, 0.29f * w, 1.17f, 0, 0.065f, 0.52f, 0.065f, shirt);
  P(SH_SPHERE, BN_LARM, -0.29f * w, 0.9f, 0.01f, 0.06f, 0.06f, 0.06f, skin); P(SH_SPHERE, BN_RARM, 0.29f * w, 0.9f, 0.01f, 0.06f, 0.06f, 0.06f, skin);
  P(SH_SPHERE, BN_HEAD, 0, 1.67f, 0.01f, 0.115f, 0.135f, 0.125f, skin, 0.7f); P(SH_CYL, BN_HEAD, 0, 1.53f, 0, 0.05f, 0.08f, 0.05f, skin);
}
void buildHeroes() {
  for (int c = 0; c < NCLS; c++) { cur = &gHero[c]; pivots(*cur); }
  cur = &gHero[WARDEN]; human(0xc9a27e, 0x6b2a24, 0x2a2622, 0x241c16, 1.1f);
  P(SH_SPHERE, BN_HEAD, 0, 1.7f, 0, 0.145f, 0.14f, 0.15f, 0x7e8388, 0.45f, 0.9f); P(SH_BOX, BN_HEAD, 0, 1.62f, 0.12f, 0.03f, 0.1f, 0.03f, 0x1a1a1c, 0.6f, 0.8f);
  P(SH_SPHERE, BN_LARM, -0.31f, 1.43f, 0, 0.14f, 0.1f, 0.13f, 0x7e8388, 0.45f, 0.9f); P(SH_SPHERE, BN_RARM, 0.31f, 1.43f, 0, 0.14f, 0.1f, 0.13f, 0x7e8388, 0.45f, 0.9f);
  P(SH_BOX, BN_ROOT, 0, 1.18f, 0.14f, 0.34f, 0.5f, 0.03f, 0x8a2e26, 0.8f); P(SH_BOX, BN_ROOT, 0, 1.3f, 0, 0.5f, 0.3f, 0.3f, 0x6e7378, 0.5f, 0.85f);
  P(SH_BOX, BN_LARM, -0.4f, 1.0f, 0.12f, 0.06f, 0.62f, 0.44f, 0x5d6368, 0.5f, 0.85f); P(SH_SPHERE, BN_LARM, -0.44f, 1.0f, 0.12f, 0.06f, 0.1f, 0.1f, 0xb9893a, 0.4f, 0.9f);
  P(SH_BOX, BN_RARM, 0.3f, 1.3f, 0.14f, 0.05f, 0.9f, 0.025f, 0xb6bcc2, 0.3f, 1.0f); P(SH_BOX, BN_RARM, 0.3f, 0.88f, 0.14f, 0.2f, 0.04f, 0.05f, 0x6b5a2a, 0.5f, 0.8f);
  cur = &gHero[REAVER]; human(0xb98a68, 0x3a2e28, 0x241f1c, 0x1c1713, 1.15f);
  P(SH_SPHERE, BN_HEAD, 0, 1.68f, 0, 0.12f, 0.13f, 0.13f, 0xb98a68, 0.7f); P(SH_CONE, BN_HEAD, 0, 1.52f, 0.08f, 0.1f, 0.18f, 0.08f, 0x3a2a1c, 0.9f, 0, 0, PI);
  P(SH_SPHERE, BN_LARM, -0.32f, 1.44f, 0, 0.17f, 0.12f, 0.15f, 0x555a5e, 0.5f, 0.85f); P(SH_SPHERE, BN_RARM, 0.32f, 1.44f, 0, 0.17f, 0.12f, 0.15f, 0x555a5e, 0.5f, 0.85f);
  P(SH_BOX, BN_ROOT, 0, 1.25f, 0, 0.5f, 0.4f, 0.3f, 0x4a4f54, 0.55f, 0.8f); P(SH_BOX, BN_ROOT, 0, 0.96f, 0, 0.46f, 0.1f, 0.28f, 0x2a1a12, 0.9f);
  P(SH_BOX, BN_RARM, 0.3f, 1.6f, 0.12f, 0.14f, 1.5f, 0.035f, 0x9aa0a6, 0.3f, 1.0f); P(SH_BOX, BN_RARM, 0.3f, 0.86f, 0.12f, 0.34f, 0.05f, 0.06f, 0x3a2a1a, 0.6f, 0.6f); P(SH_BOX, BN_LARM, -0.29f, 0.88f, 0.14f, 0.06f, 0.06f, 0.2f, 0x3a2a1a, 0.7f);
  cur = &gHero[FLETCHER]; human(0xc29a78, 0x34462a, 0x2c2a20, 0x2a2118, 0.9f);
  P(SH_SPHERE, BN_HEAD, 0, 1.7f, -0.02f, 0.15f, 0.15f, 0.15f, 0x2f4126, 0.9f); P(SH_CONE, BN_HEAD, 0, 1.78f, -0.1f, 0.14f, 0.3f, 0.14f, 0x2f4126, 0.9f, 0, 0, -0.5f);
  P(SH_BOX, BN_ROOT, 0, 1.0f, -0.16f, 0.46f, 0.9f, 0.04f, 0x263320, 0.95f); P(SH_BOX, BN_ROOT, 0.14f, 1.35f, -0.22f, 0.12f, 0.5f, 0.1f, 0x4a3320, 0.85f); P(SH_CONE, BN_ROOT, 0.14f, 1.68f, -0.22f, 0.07f, 0.12f, 0.07f, 0xb6a98a, 0.6f);
  P(SH_CYL, BN_LARM, -0.3f, 1.2f, 0.28f, 0.022f, 1.1f, 0.022f, 0x5a3d22, 0.6f, 0, 0, 0.15f); P(SH_CYL, BN_LARM, -0.3f, 1.62f, 0.2f, 0.02f, 0.45f, 0.02f, 0x5a3d22, 0.6f, 0, 0, -0.5f); P(SH_CYL, BN_LARM, -0.3f, 0.8f, 0.2f, 0.02f, 0.45f, 0.02f, 0x5a3d22, 0.6f, 0, 0, 0.5f);
  cur = &gHero[LAMPLIGHTER]; human(0xc9a27e, 0x5a3a24, 0x2a2420, 0x231a14, 1.0f);
  P(SH_CYL, BN_ROOT, 0, 0.62f, 0, 0.27f, 0.95f, 0.22f, 0x6a4a2c, 0.85f); P(SH_CYL, BN_HEAD, 0, 1.78f, 0, 0.21f, 0.03f, 0.21f, 0x2a2018, 0.8f); P(SH_CYL, BN_HEAD, 0, 1.84f, 0, 0.12f, 0.12f, 0.12f, 0x2a2018, 0.8f);
  P(SH_CYL, BN_RARM, 0.33f, 1.1f, 0.12f, 0.02f, 2.4f, 0.02f, 0x2b2118, 0.7f, 0.2f); P(SH_BOX, BN_RARM, 0.33f, 2.22f, 0.12f, 0.15f, 0.04f, 0.15f, 0x1d1c1b, 0.5f, 0.8f);
  P(SH_SPHERE, BN_RARM, 0.33f, 2.02f, 0.12f, 0.1f, 0.12f, 0.1f, 0xffb45a, 0.4f, 0, 6.0f); P(SH_CONE, BN_RARM, 0.33f, 2.32f, 0.12f, 0.1f, 0.1f, 0.1f, 0x1d1c1b, 0.5f, 0.8f);
  cur->lanX = 0.33f; cur->lanY = 2.02f; cur->lanZ = 0.12f;
  cur = &gHero[APOTHECARY]; human(0xc9a27e, 0x5f4a38, 0x35302a, 0x2a2018, 1.05f);
  P(SH_BOX, BN_ROOT, 0, 1.0f, 0.14f, 0.4f, 0.75f, 0.03f, 0x9a8a68, 0.9f); P(SH_BOX, BN_ROOT, 0, 0.92f, 0, 0.5f, 0.07f, 0.34f, 0x3a2a1c, 0.7f); P(SH_BOX, BN_ROOT, -0.34f, 0.85f, -0.1f, 0.18f, 0.28f, 0.16f, 0x5a4026, 0.85f);
  for (int i = 0; i < 4; i++) P(SH_SPHERE, BN_ROOT, -0.18f + i * 0.12f, 0.9f, 0.18f, 0.045f, 0.055f, 0.045f, i % 2 ? 0x6fe08a : 0xffb45a, 0.2f, 0, 1.2f);
  P(SH_CYL, BN_HEAD, 0, 1.79f, 0, 0.16f, 0.03f, 0.16f, 0x4a3a28, 0.8f); P(SH_CYL, BN_HEAD, 0, 1.85f, 0, 0.1f, 0.1f, 0.1f, 0x4a3a28, 0.8f); P(SH_BOX, BN_HEAD, 0, 1.73f, 0.12f, 0.2f, 0.05f, 0.03f, 0x8aa0a0, 0.2f, 0.9f);
  cur = &gHero[RIMEBINDER]; human(0xd9c2ae, 0x6c88a8, 0x4a607c, 0x2a303a, 0.95f);
  P(SH_CYL, BN_ROOT, 0, 0.6f, 0, 0.26f, 0.95f, 0.22f, 0x7e9abc, 0.8f); P(SH_SPHERE, BN_HEAD, 0, 1.72f, -0.03f, 0.16f, 0.17f, 0.16f, 0x5d7898, 0.85f); P(SH_CONE, BN_HEAD, 0, 1.9f, -0.12f, 0.12f, 0.22f, 0.12f, 0x5d7898, 0.85f, 0, 0, -0.6f);
  P(SH_CYL, BN_RARM, 0.33f, 1.15f, 0.1f, 0.022f, 2.3f, 0.022f, 0x4a3a2a, 0.7f); P(SH_CONE, BN_RARM, 0.33f, 2.42f, 0.1f, 0.07f, 0.35f, 0.07f, 0xaee4ff, 0.2f, 0, 4.0f);
}
void buildMonsters() {
  for (int c = 0; c < NMON; c++) { cur = &gMon[c]; pivots(*cur); }
  auto legs = [&](uint32_t col, float len, float r) { P(SH_CYL, BN_LLEG, -0.12f, len / 2, 0, r, len, r, col, 0.9f); P(SH_CYL, BN_RLEG, 0.12f, len / 2, 0, r, len, r, col, 0.9f); };
  cur = &gMon[THORNLING]; cur->pivot[BN_LLEG][1] = cur->pivot[BN_RLEG][1] = 0.6f; legs(0x3a2a18, 0.6f, 0.04f);
  P(SH_SPHERE, BN_ROOT, 0, 0.95f, 0, 0.3f, 0.36f, 0.28f, 0x3a2a1a, 0.95f); P(SH_SPHERE, BN_HEAD, 0, 1.45f, 0.05f, 0.2f, 0.18f, 0.2f, 0x4a3a22, 0.95f); P(SH_CYL, BN_LARM, -0.4f, 1.0f, 0.05f, 0.045f, 0.7f, 0.045f, 0x3a2a18, 0.9f, 0, 0, 0.1f); P(SH_CYL, BN_RARM, 0.4f, 1.0f, 0.05f, 0.045f, 0.7f, 0.045f, 0x3a2a18, 0.9f, 0, 0, -0.1f);
  P(SH_SPHERE, BN_HEAD, -0.08f, 1.48f, 0.23f, 0.035f, 0.035f, 0.035f, 0xa0ff70, 0.3f, 0, 5); P(SH_SPHERE, BN_HEAD, 0.08f, 1.48f, 0.23f, 0.035f, 0.035f, 0.035f, 0xa0ff70, 0.3f, 0, 5);
  for (int i = 0; i < 7; i++) { float a = i * 1.1f; P(SH_CONE, BN_ROOT, std::sin(a) * 0.2f, 1.0f + (i % 3) * 0.15f, -0.2f - std::cos(a) * 0.05f, 0.04f, 0.28f, 0.04f, 0x5a4a28, 0.9f, 0, 0, -0.9f + i * 0.1f, std::sin(a) * 0.6f); }
  cur = &gMon[PEATWIGHT]; legs(0x2a2016, 0.7f, 0.11f); P(SH_SPHERE, BN_ROOT, 0, 1.15f, 0, 0.5f, 0.62f, 0.42f, 0x2b2218, 1.0f); P(SH_SPHERE, BN_HEAD, 0, 1.78f, 0.12f, 0.22f, 0.2f, 0.22f, 0x33281c, 1.0f);
  P(SH_CYL, BN_LARM, -0.55f, 0.9f, 0.05f, 0.1f, 1.1f, 0.1f, 0x2b2218, 1.0f); P(SH_CYL, BN_RARM, 0.55f, 0.9f, 0.05f, 0.1f, 1.1f, 0.1f, 0x2b2218, 1.0f); P(SH_SPHERE, BN_LARM, -0.55f, 0.3f, 0.05f, 0.14f, 0.12f, 0.14f, 0x1f180f, 1.0f); P(SH_SPHERE, BN_RARM, 0.55f, 0.3f, 0.05f, 0.14f, 0.12f, 0.14f, 0x1f180f, 1.0f);
  P(SH_SPHERE, BN_HEAD, -0.08f, 1.8f, 0.32f, 0.04f, 0.04f, 0.04f, 0xff7a20, 0.3f, 0, 6); P(SH_SPHERE, BN_HEAD, 0.08f, 1.8f, 0.32f, 0.04f, 0.04f, 0.04f, 0xff7a20, 0.3f, 0, 6); P(SH_SPHERE, BN_ROOT, 0.2f, 1.5f, 0.3f, 0.12f, 0.08f, 0.08f, 0x4a5a22, 0.95f);
  cur = &gMon[GLOAMHOUND]; for (int b : {BN_LLEG, BN_RLEG}) { cur->pivot[b][1] = 0.75f; cur->pivot[b][2] = 0.35f; } for (int b : {BN_LLEG2, BN_RLEG2}) { cur->pivot[b][1] = 0.75f; cur->pivot[b][2] = -0.35f; }
  P(SH_SPHERE, BN_ROOT, 0, 0.82f, 0, 0.24f, 0.26f, 0.62f, 0x2c2a26, 0.95f); P(SH_SPHERE, BN_HEAD, 0, 0.98f, 0.62f, 0.17f, 0.17f, 0.2f, 0x34302a, 0.95f); P(SH_BOX, BN_JAW, 0, 0.9f, 0.84f, 0.13f, 0.09f, 0.22f, 0x2a2824, 0.95f);
  cur->pivot[BN_HEAD][1] = 0.9f; cur->pivot[BN_HEAD][2] = 0.4f;
  P(SH_SPHERE, BN_HEAD, -0.08f, 1.03f, 0.74f, 0.03f, 0.03f, 0.03f, 0x9aff80, 0.3f, 0, 6); P(SH_SPHERE, BN_HEAD, 0.08f, 1.03f, 0.74f, 0.03f, 0.03f, 0.03f, 0x9aff80, 0.3f, 0, 6);
  P(SH_CONE, BN_HEAD, -0.09f, 1.18f, 0.55f, 0.04f, 0.14f, 0.04f, 0x2c2a26, 0.9f); P(SH_CONE, BN_HEAD, 0.09f, 1.18f, 0.55f, 0.04f, 0.14f, 0.04f, 0x2c2a26, 0.9f);
  P(SH_CYL, BN_LLEG, -0.13f, 0.38f, 0.35f, 0.05f, 0.75f, 0.05f, 0x2a2824, 0.9f); P(SH_CYL, BN_RLEG, 0.13f, 0.38f, 0.35f, 0.05f, 0.75f, 0.05f, 0x2a2824, 0.9f); P(SH_CYL, BN_LLEG2, -0.13f, 0.38f, -0.35f, 0.055f, 0.75f, 0.055f, 0x2a2824, 0.9f); P(SH_CYL, BN_RLEG2, 0.13f, 0.38f, -0.35f, 0.055f, 0.75f, 0.055f, 0x2a2824, 0.9f);
  P(SH_CONE, BN_TAIL, 0, 0.95f, -0.8f, 0.06f, 0.5f, 0.06f, 0x2c2a26, 0.9f, 0, 0, -1.2f);
  cur = &gMon[GLOAMWISP]; P(SH_SPHERE, BN_ROOT, 0, 1.2f, 0, 0.22f, 0.26f, 0.22f, 0x6aff9a, 0.3f, 0, 3.5f); P(SH_CONE, BN_ROOT, 0, 0.8f, 0, 0.28f, 0.9f, 0.28f, 0x14302a, 0.9f, 0, 0.3f, PI); P(SH_SPHERE, BN_HEAD, 0, 1.5f, 0.05f, 0.14f, 0.14f, 0.14f, 0x14302a, 0.9f);
  P(SH_SPHERE, BN_HEAD, -0.05f, 1.52f, 0.17f, 0.025f, 0.025f, 0.025f, 0xd0ffe0, 0.3f, 0, 8); P(SH_SPHERE, BN_HEAD, 0.05f, 1.52f, 0.17f, 0.025f, 0.025f, 0.025f, 0xd0ffe0, 0.3f, 0, 8);
  cur = &gMon[RUSTBOUND]; human(0x6a4a30, 0x7a4426, 0x4a3020, 0x3a2a1a, 1.3f); P(SH_SPHERE, BN_HEAD, 0, 1.7f, 0, 0.16f, 0.17f, 0.16f, 0x7a4a2a, 0.9f, 0.6f); P(SH_SPHERE, BN_HEAD, -0.06f, 1.69f, 0.14f, 0.03f, 0.03f, 0.03f, 0x9aff80, 0.3f, 0, 6); P(SH_SPHERE, BN_HEAD, 0.06f, 1.69f, 0.14f, 0.03f, 0.03f, 0.03f, 0x9aff80, 0.3f, 0, 6);
  P(SH_SPHERE, BN_LARM, -0.34f, 1.44f, 0, 0.16f, 0.11f, 0.14f, 0x8a4a22, 0.9f, 0.6f); P(SH_SPHERE, BN_RARM, 0.34f, 1.44f, 0, 0.16f, 0.11f, 0.14f, 0x8a4a22, 0.9f, 0.6f); P(SH_BOX, BN_RARM, 0.34f, 1.15f, 0.15f, 0.08f, 1.2f, 0.04f, 0x6a4a34, 0.9f, 0.5f); P(SH_BOX, BN_LARM, -0.42f, 1.0f, 0.12f, 0.07f, 0.7f, 0.5f, 0x7a4a2a, 0.9f, 0.6f);
  cur = &gMon[CAIRNBACK]; legs(0x4a4a46, 0.7f, 0.14f); P(SH_SPHERE, BN_ROOT, 0, 1.15f, 0, 0.55f, 0.55f, 0.45f, 0x4e4e4a, 0.95f); P(SH_SPHERE, BN_ROOT, 0.1f, 1.7f, -0.05f, 0.36f, 0.3f, 0.34f, 0x5a5a54, 0.95f); P(SH_SPHERE, BN_HEAD, 0, 1.95f, 0.1f, 0.2f, 0.17f, 0.2f, 0x454540, 0.95f);
  P(SH_BOX, BN_LARM, -0.65f, 0.9f, 0.05f, 0.28f, 0.9f, 0.28f, 0x4a4a46, 0.95f); P(SH_BOX, BN_RARM, 0.65f, 0.9f, 0.05f, 0.28f, 0.9f, 0.28f, 0x4a4a46, 0.95f); P(SH_SPHERE, BN_ROOT, -0.2f, 1.3f, 0.4f, 0.2f, 0.12f, 0.1f, 0x4a5a2a, 1.0f); P(SH_SPHERE, BN_ROOT, 0.25f, 1.9f, 0.2f, 0.18f, 0.1f, 0.12f, 0x3a4a22, 1.0f);
  P(SH_SPHERE, BN_HEAD, -0.07f, 1.97f, 0.25f, 0.035f, 0.025f, 0.03f, 0xff8a30, 0.3f, 0, 7); P(SH_SPHERE, BN_HEAD, 0.07f, 1.97f, 0.25f, 0.035f, 0.025f, 0.03f, 0xff8a30, 0.3f, 0, 7); P(SH_BOX, BN_ROOT, 0, 1.2f, 0.44f, 0.18f, 0.04f, 0.02f, 0xff8a30, 0.3f, 0, 4);
  cur = &gMon[BARROWKING]; human(0x7a8a7a, 0x2e4a4a, 0x28322e, 0x22201c, 1.3f); P(SH_SPHERE, BN_HEAD, 0, 1.7f, 0, 0.16f, 0.17f, 0.16f, 0x8a9a8a, 0.8f); P(SH_CYL, BN_HEAD, 0, 1.88f, 0, 0.17f, 0.12f, 0.17f, 0xa07a3a, 0.5f, 0.8f);
  for (int i = 0; i < 5; i++) { float a = i / 5.f * TAU; P(SH_CONE, BN_HEAD, std::cos(a) * 0.15f, 2.0f, std::sin(a) * 0.15f, 0.04f, 0.14f, 0.04f, 0xa07a3a, 0.5f, 0.8f); }
  P(SH_SPHERE, BN_HEAD, -0.06f, 1.7f, 0.14f, 0.03f, 0.03f, 0.03f, 0x9aff9a, 0.3f, 0, 7); P(SH_SPHERE, BN_HEAD, 0.06f, 1.7f, 0.14f, 0.03f, 0.03f, 0.03f, 0x9aff9a, 0.3f, 0, 7);
  P(SH_BOX, BN_ROOT, 0, 1.0f, -0.18f, 0.7f, 1.2f, 0.05f, 0x1c3a3a, 0.95f); P(SH_SPHERE, BN_LARM, -0.36f, 1.46f, 0, 0.19f, 0.13f, 0.17f, 0x7a5030, 0.8f, 0.6f); P(SH_SPHERE, BN_RARM, 0.36f, 1.46f, 0, 0.19f, 0.13f, 0.17f, 0x7a5030, 0.8f, 0.6f);
  P(SH_BOX, BN_RARM, 0.34f, 1.5f, 0.15f, 0.16f, 1.7f, 0.04f, 0x8a5a38, 0.8f, 0.6f); P(SH_BOX, BN_ROOT, 0, 1.3f, 0.05f, 0.56f, 0.4f, 0.32f, 0x7a5030, 0.8f, 0.6f);
  cur = &gMon[MOSSJAW]; legs(0x4a4a44, 0.7f, 0.17f); P(SH_SPHERE, BN_ROOT, 0, 1.25f, 0, 0.65f, 0.6f, 0.5f, 0x4a4a44, 0.95f); P(SH_SPHERE, BN_ROOT, 0, 1.8f, -0.1f, 0.5f, 0.2f, 0.4f, 0x4a6a2a, 1.0f); P(SH_SPHERE, BN_HEAD, 0, 1.95f, 0.3f, 0.34f, 0.28f, 0.34f, 0x504f48, 0.95f); P(SH_BOX, BN_JAW, 0, 1.68f, 0.4f, 0.4f, 0.14f, 0.4f, 0x45443e, 0.95f);
  for (int i = 0; i < 5; i++) { P(SH_CONE, BN_JAW, -0.16f + i * 0.08f, 1.8f, 0.58f, 0.025f, 0.12f, 0.025f, 0xd0cdb8, 0.6f, 0, 0, 0); } cur->pivot[BN_JAW][1] = 1.75f; cur->pivot[BN_JAW][2] = 0.2f; cur->pivot[BN_HEAD][1] = 1.8f;
  P(SH_SPHERE, BN_HEAD, -0.13f, 2.05f, 0.58f, 0.04f, 0.03f, 0.03f, 0xffa040, 0.3f, 0, 8); P(SH_SPHERE, BN_HEAD, 0.13f, 2.05f, 0.58f, 0.04f, 0.03f, 0.03f, 0xffa040, 0.3f, 0, 8);
  P(SH_BOX, BN_LARM, -0.8f, 0.85f, 0.05f, 0.34f, 1.1f, 0.34f, 0x4a4a44, 0.95f); P(SH_BOX, BN_RARM, 0.8f, 0.85f, 0.05f, 0.34f, 1.1f, 0.34f, 0x4a4a44, 0.95f); P(SH_SPHERE, BN_LARM, -0.8f, 0.2f, 0.05f, 0.24f, 0.2f, 0.24f, 0x555550, 0.95f); P(SH_SPHERE, BN_RARM, 0.8f, 0.2f, 0.05f, 0.24f, 0.2f, 0.24f, 0x555550, 0.95f);
  cur = &gMon[HOLLOWSTAG]; for (int b : {BN_LLEG, BN_RLEG}) { cur->pivot[b][1] = 1.0f; cur->pivot[b][2] = 0.4f; } for (int b : {BN_LLEG2, BN_RLEG2}) { cur->pivot[b][1] = 1.0f; cur->pivot[b][2] = -0.45f; } cur->pivot[BN_HEAD][1] = 1.25f; cur->pivot[BN_HEAD][2] = 0.5f;
  P(SH_SPHERE, BN_ROOT, 0, 1.05f, 0, 0.3f, 0.34f, 0.7f, 0x6a5a44, 0.9f); P(SH_CYL, BN_ROOT, 0, 1.35f, 0.5f, 0.12f, 0.55f, 0.12f, 0x6a5a44, 0.9f, 0, 0, 0.5f); P(SH_SPHERE, BN_HEAD, 0, 1.55f, 0.85f, 0.14f, 0.16f, 0.26f, 0xd8d0b8, 0.7f);
  P(SH_SPHERE, BN_HEAD, -0.09f, 1.62f, 0.95f, 0.035f, 0.035f, 0.035f, 0xe0f4ff, 0.3f, 0, 8); P(SH_SPHERE, BN_HEAD, 0.09f, 1.62f, 0.95f, 0.035f, 0.035f, 0.035f, 0xe0f4ff, 0.3f, 0, 8);
  for (int s : {-1, 1}) { P(SH_CYL, BN_HEAD, s * 0.12f, 1.95f, 0.8f, 0.02f, 0.7f, 0.02f, 0x4a3a2a, 0.8f, 0, 0, -0.3f, -s * 0.5f); P(SH_CYL, BN_HEAD, s * 0.35f, 2.4f, 0.75f, 0.015f, 0.5f, 0.015f, 0x4a3a2a, 0.8f, 0, 0, -0.2f, -s * 0.9f); P(SH_CYL, BN_HEAD, s * 0.28f, 2.2f, 0.9f, 0.012f, 0.4f, 0.012f, 0x4a3a2a, 0.8f, 0, 0, 0.5f, -s * 0.3f); P(SH_CYL, BN_HEAD, s * 0.5f, 2.6f, 0.8f, 0.012f, 0.35f, 0.012f, 0x4a3a2a, 0.8f, 0, 0, -0.1f, s * 0.2f); }
  P(SH_CYL, BN_LLEG, -0.16f, 0.5f, 0.4f, 0.06f, 1.0f, 0.06f, 0x5a4a38, 0.9f); P(SH_CYL, BN_RLEG, 0.16f, 0.5f, 0.4f, 0.06f, 1.0f, 0.06f, 0x5a4a38, 0.9f); P(SH_CYL, BN_LLEG2, -0.16f, 0.5f, -0.45f, 0.07f, 1.0f, 0.07f, 0x5a4a38, 0.9f); P(SH_CYL, BN_RLEG2, 0.16f, 0.5f, -0.45f, 0.07f, 1.0f, 0.07f, 0x5a4a38, 0.9f);
  P(SH_CONE, BN_TAIL, 0, 1.1f, -0.85f, 0.08f, 0.35f, 0.08f, 0xd8d0b8, 0.8f, 0, 0, -1.0f);
}
}  // namespace
void meshUpload(Mesh& m, const Inst* d, int n) { m.count = std::min(n, m.cap); if (!m.count) return; glBindBuffer(GL_ARRAY_BUFFER, m.inst); glBufferSubData(GL_ARRAY_BUFFER, 0, m.count * sizeof(Inst), d); }
void meshDraw(const Mesh& m) { if (!m.count) return; glBindVertexArray(m.vao); glDrawElementsInstanced(GL_TRIANGLES, m.nIdx, GL_UNSIGNED_INT, nullptr, m.count); }
void buildMeshes() {
  for (int k = 0; k < NPK; k++) for (int v = 0; v < PROP_VARIANTS[k]; v++) { MB g; uint32_t s = k * 131 + v * 17 + 11;
    switch (k) { case PK_ROCK: gRock(g, 11 + v * 19, 0.5f, 0.36f, 0.42f); break; case PK_STAND: gStanding(g, 31 + v * 23); break; case PK_TREE: gTree(g, 51 + v * 29); break; case PK_CAIRN: gCairn(g, 71 + v * 31); break; case PK_WALL: gWall(g, 91 + v * 37); break; case PK_FENCE: gFence(g, 111 + v * 41); break; default: gWaylight(g); }
    (void)s; upload(gProp[k][v], g, k == PK_WAY ? 8 : 256); }
  { MB g; g.mat(rgb(1), 1, 1, 1); g.ellipsoid(1, 1, 1, ident(), 14, 10); upload(gShape[SH_SPHERE], g, 1024); }
  { MB g; g.mat(rgb(1), 1, 1, 1); g.box(1, 1, 1, ident()); upload(gShape[SH_BOX], g, 1024); }
  { MB g; g.mat(rgb(1), 1, 1, 1); g.lathe({{0.0f, -0.5f}, {1, -0.5f}, {1, 0.5f}, {0.0f, 0.5f}}, 10, ident()); upload(gShape[SH_CYL], g, 1024); }
  { MB g; g.mat(rgb(1), 1, 1, 1); g.lathe({{0.0f, -0.5f}, {1, -0.5f}, {0.0f, 0.5f}}, 10, ident()); upload(gShape[SH_CONE], g, 1024); }
  buildHeroes(); buildMonsters();
}
