#include "xmesh.h"
#include "xworld.h"
#include <cmath>
#include <cstring>
namespace {
struct MB { std::vector<LV>* v; std::vector<uint16_t>* i; };
float ao(float y, float base, float h) { return base + (1.f - base) * smooth(-1.f, h, y); }
inline int8_t sn8(float x) { return (int8_t)std::lround(clampf(x, -1.f, 1.f) * 127.f); }
void vert(MB& b, V3 p, V3 n, V3 col, float a, float u, float vv, int surf, float rough, float metal, float glow) {
  LV o; o.p[0] = p.x; o.p[1] = p.y; o.p[2] = p.z; o.n[0] = sn8(n.x); o.n[1] = sn8(n.y); o.n[2] = sn8(n.z); o.pad = 0;
  o.c[0] = (uint8_t)(std::sqrt(clampf(col.x, 0, 1)) * 255.f); o.c[1] = (uint8_t)(std::sqrt(clampf(col.y, 0, 1)) * 255.f); o.c[2] = (uint8_t)(std::sqrt(clampf(col.z, 0, 1)) * 255.f); o.c[3] = (uint8_t)(clampf(a, 0, 1) * 255.f);
  o.uv[0] = u; o.uv[1] = vv; o.m[0] = (uint8_t)(rough * 255.f); o.m[1] = (uint8_t)(metal * 255.f); o.m[2] = (uint8_t)surf; o.m[3] = (uint8_t)(clampf(glow, 0, 2) * 127.5f); o.b[0] = 0; o.b[1] = 0; o.b[2] = 0; o.b[3] = 128; b.v->push_back(o);
}
void tri(MB& b, int a, int c, int d) { b.i->push_back((uint16_t)a); b.i->push_back((uint16_t)c); b.i->push_back((uint16_t)d); }
struct Sty { V3 col; int surf; float rough, metal, glow, aoBase, aoH; };
void quad(MB& b, V3 p0, V3 p1, V3 p2, V3 p3, const Sty& s, float uvs = 0.2f) {   // p0..p3 counter-clockwise seen from the normal side
  V3 n = norm(cross(p1 - p0, p3 - p0)); int base = (int)b.v->size(); V3 q[4] = {p0, p1, p2, p3}; float uu[4] = {0, 1, 1, 0}, vv[4] = {0, 0, 1, 1}; float w = len(p1 - p0) * uvs, h = len(p3 - p0) * uvs;
  for (int k = 0; k < 4; k++) vert(b, q[k], n, s.col, ao(q[k].y, s.aoBase, s.aoH), uu[k] * w, vv[k] * h, s.surf, s.rough, s.metal, s.glow); tri(b, base, base + 1, base + 2); tri(b, base, base + 2, base + 3);
}
void boxR(MB& b, float cx, float cy, float cz, float hx, float hy, float hz, float ry, const Sty& s) {
  float c = std::cos(ry), sn = std::sin(ry); auto P = [&](float x, float y, float z) { return V3{cx + x * c + z * sn, cy + y, cz - x * sn + z * c}; };
  quad(b, P(-hx, -hy, hz), P(hx, -hy, hz), P(hx, hy, hz), P(-hx, hy, hz), s); quad(b, P(hx, -hy, -hz), P(-hx, -hy, -hz), P(-hx, hy, -hz), P(hx, hy, -hz), s);
  quad(b, P(hx, -hy, hz), P(hx, -hy, -hz), P(hx, hy, -hz), P(hx, hy, hz), s); quad(b, P(-hx, -hy, -hz), P(-hx, -hy, hz), P(-hx, hy, hz), P(-hx, hy, -hz), s);
  quad(b, P(-hx, hy, hz), P(hx, hy, hz), P(hx, hy, -hz), P(-hx, hy, -hz), s); quad(b, P(-hx, -hy, -hz), P(hx, -hy, -hz), P(hx, -hy, hz), P(-hx, -hy, hz), s);
}
void box(MB& b, float cx, float cy, float cz, float hx, float hy, float hz, const Sty& s) { boxR(b, cx, cy, cz, hx, hy, hz, 0, s); }
void cyl(MB& b, float cx, float y0, float cz, float r0, float r1, float h, int seg, const Sty& s, const V3* topCol = nullptr, bool cap = true) {
  int base = (int)b.v->size(); float slope = (r0 - r1) / std::max(h, 1e-3f);
  for (int k = 0; k <= seg; k++) { float a = TAU * k / seg, ca = std::cos(a), sa = std::sin(a); V3 n = norm({ca, slope, sa}); V3 tc = topCol ? *topCol : s.col;
    vert(b, {cx + ca * r0, y0, cz + sa * r0}, n, s.col, ao(y0, s.aoBase, s.aoH), k * 0.35f, 0, s.surf, s.rough, s.metal, s.glow); vert(b, {cx + ca * r1, y0 + h, cz + sa * r1}, n, tc, ao(y0 + h, s.aoBase, s.aoH), k * 0.35f, h * 0.25f, s.surf, s.rough, s.metal, s.glow); }
  for (int k = 0; k < seg; k++) { int a = base + k * 2; tri(b, a, a + 1, a + 2); tri(b, a + 1, a + 3, a + 2); }
  if (cap && r1 > 0.05f) { int c0 = (int)b.v->size(); vert(b, {cx, y0 + h, cz}, {0, 1, 0}, topCol ? *topCol : s.col, 1, 0, 0, s.surf, s.rough, s.metal, s.glow); for (int k = 0; k < seg; k++) { float a = TAU * k / seg; vert(b, {cx + std::cos(a) * r1, y0 + h, cz + std::sin(a) * r1}, {0, 1, 0}, topCol ? *topCol : s.col, 1, 1, 1, s.surf, s.rough, s.metal, s.glow); } for (int k = 0; k < seg; k++) tri(b, c0, c0 + 1 + (k + 1) % seg, c0 + 1 + k); }
}
void blob(MB& b, float cx, float cy, float cz, float rx, float ry, float rz, int seg, int rings, const Sty& s, uint32_t seed, float jit, bool flat = false) {
  int base = (int)b.v->size(); auto H = [&](int i, int j) { uint32_t h = (uint32_t)(i * 73856093) ^ (uint32_t)(j * 19349663) ^ seed * 83492791u; h = (h ^ (h >> 13)) * 1274126177u; return ((h >> 8) & 0xffff) / 65535.f; };
  for (int j = 0; j <= rings; j++) for (int i = 0; i <= seg; i++) { float v = (float)j / rings, a = TAU * (i % seg) / seg, ph = PI * v; float jj = 1.f + (H(i % seg, j) - 0.5f) * jit; float x = std::sin(ph) * std::cos(a), y = std::cos(ph), z = std::sin(ph) * std::sin(a);
    vert(b, {cx + x * rx * jj, cy + y * ry * jj, cz + z * rz * jj}, norm({x / std::max(rx, 1e-3f), y / std::max(ry, 1e-3f), z / std::max(rz, 1e-3f)}), s.col * (0.8f + 0.4f * H(j, i % seg)), ao(cy + y * ry, s.aoBase, s.aoH), i * 0.2f, j * 0.2f, s.surf, s.rough, s.metal, s.glow); }
  for (int j = 0; j < rings; j++) for (int i = 0; i < seg; i++) { int a = base + j * (seg + 1) + i, c = a + seg + 1; tri(b, a, c, a + 1); tri(b, a + 1, c, c + 1); } (void)flat;
}
void roof(MB& b, float cx, float cy, float cz, float hl, float hw, float h, float over, const Sty& s, const Sty& gable) {
  float L = hl + over, W = hw + over; V3 a0{cx - L, cy, cz + W}, a1{cx + L, cy, cz + W}, r1{cx + L, cy + h, cz}, r0{cx - L, cy + h, cz}, b0{cx + L, cy, cz - W}, b1{cx - L, cy, cz - W};
  quad(b, a0, a1, r1, r0, s, 0.3f); quad(b, b0, b1, r0, r1, s, 0.3f);
  float gl = hl; V3 g0{cx + gl, cy, cz + hw}, g1{cx + gl, cy, cz - hw}, g2{cx + gl, cy + h * 0.92f, cz}; int base = (int)b.v->size(); V3 nn{1, 0, 0}; vert(b, g0, nn, gable.col, 0.8f, 0, 0, gable.surf, gable.rough, 0, 0); vert(b, g1, nn, gable.col, 0.8f, 1, 0, gable.surf, gable.rough, 0, 0); vert(b, g2, nn, gable.col, 0.9f, 0.5f, 1, gable.surf, gable.rough, 0, 0); tri(b, base, base + 2, base + 1);
  g0 = {cx - gl, cy, cz + hw}; g1 = {cx - gl, cy, cz - hw}; g2 = {cx - gl, cy + h * 0.92f, cz}; base = (int)b.v->size(); nn = {-1, 0, 0}; vert(b, g0, nn, gable.col, 0.8f, 0, 0, gable.surf, gable.rough, 0, 0); vert(b, g1, nn, gable.col, 0.8f, 1, 0, gable.surf, gable.rough, 0, 0); vert(b, g2, nn, gable.col, 0.9f, 0.5f, 1, gable.surf, gable.rough, 0, 0); tri(b, base, base + 1, base + 2); (void)b0; (void)b1;
}
Sty stone(uint32_t hex, float base = 0.55f) { return {C(hex), 8, 0.92f, 0, 0, base, 5.f}; }
Sty wood(uint32_t hex, float base = 0.7f) { return {C(hex), 7, 0.85f, 0, 0, base, 3.f}; }
Sty cloth(uint32_t hex) { return {C(hex), 1, 0.95f, 0, 0, 0.9f, 3.f}; }
Sty glowS(uint32_t hex, float g) { return {C(hex), 10, 0.6f, 0, g, 1, 3.f}; }
Sty metal(uint32_t hex) { return {C(hex), 5, 0.5f, 0.7f, 0, 0.8f, 3.f}; }
void merlonsLine(MB& b, float x0, float x1, float z, float y, int n, float hw, float hh, float hd, const Sty& s) { for (int k = 0; k < n; k++) { float x = x0 + (x1 - x0) * (k + 0.5f) / n; box(b, x, y + hh, z, hw, hh, hd, s); } }
void build(int kind, MB& b) {
  switch (kind) {
    case xw::K_WALL: { Sty s = stone(0x76726a), s2 = stone(0x6a665e, 0.7f); box(b, 0, 1.75f, 0, 2.55f, 4.25f, 1.2f, s); box(b, 0, 5.55f, 0.1f, 2.55f, 0.35f, 1.35f, s2); merlonsLine(b, -2.55f, 2.55f, 1.0f, 6.0f, 3, 0.55f, 0.55f, 0.28f, s); box(b, 0, 6.12f, -1.0f, 2.55f, 0.12f, 0.16f, s2);
      Sty slit = {C(0x15110e), 8, 1, 0, 0, 1, 3}; box(b, 0, 3.4f, 1.22f, 0.12f, 0.6f, 0.05f, slit); box(b, -1.7f, 1.1f, 1.21f, 0.5f, 0.12f, 0.06f, stone(0x5a564f, 0.6f)); box(b, 1.7f, 2.5f, 1.21f, 0.6f, 0.1f, 0.06f, stone(0x8a857a, 0.8f)); break; }
    case xw::K_WTOWER: { Sty s = stone(0x7a766d); cyl(b, 0, -2.5f, 0, 3.7f, 3.5f, 13.4f, 12, s); cyl(b, 0, 10.f, 0, 3.5f, 4.05f, 1.2f, 12, stone(0x6a665e, 0.8f)); Sty cr = stone(0x77736a, 0.8f);
      for (int k = 0; k < 10; k++) { float a = TAU * k / 10; boxR(b, std::cos(a) * 3.8f, 11.9f, std::sin(a) * 3.8f, 0.62f, 0.55f, 0.35f, a + PI / 2, cr); }
      Sty slit = {C(0x15110e), 8, 1, 0, 0, 1, 3}; for (int k = 0; k < 4; k++) { float a = TAU * k / 4 + 0.4f; boxR(b, std::cos(a) * 3.55f, 6.5f, std::sin(a) * 3.55f, 0.14f, 0.7f, 0.1f, a + PI / 2, slit); } break; }
    case xw::K_GATE: { Sty s = stone(0x76726a), s2 = stone(0x68645c, 0.75f); for (int sd = -1; sd <= 1; sd += 2) { float cx = sd * 5.0f; box(b, cx, 3.75f, 0, 2.5f, 6.25f, 2.7f, s); box(b, cx, 9.8f, 0, 2.8f, 0.4f, 3.0f, s2); merlonsLine(b, cx - 2.8f, cx + 2.8f, 2.75f, 10.2f, 4, 0.5f, 0.55f, 0.3f, s); merlonsLine(b, cx - 2.8f, cx + 2.8f, -2.75f, 10.2f, 4, 0.5f, 0.55f, 0.3f, s);
      box(b, cx + sd * 2.3f, 10.7f, 0, 0.3f, 0.55f, 2.4f, s); Sty slit = {C(0x15110e), 8, 1, 0, 0, 1, 3}; box(b, cx, 6.5f, 2.72f, 0.14f, 0.8f, 0.05f, slit); box(b, cx, 3.6f, 2.72f, 0.12f, 0.6f, 0.05f, slit); Sty win = glowS(0xffb45a, 1.0f); box(b, cx, 8.0f, 2.72f, 0.3f, 0.5f, 0.05f, win); }
    box(b, 0, 7.2f, 0, 2.6f, 2.0f, 2.6f, s); box(b, 0, 9.4f, 0, 2.8f, 0.4f, 3.0f, s2); merlonsLine(b, -2.6f, 2.6f, 2.75f, 9.8f, 3, 0.55f, 0.5f, 0.3f, s); merlonsLine(b, -2.6f, 2.6f, -2.75f, 9.8f, 3, 0.55f, 0.5f, 0.3f, s);
    box(b, -1.6f, 5.0f, 0, 0.3f, 0.35f, 2.3f, s2); box(b, 1.6f, 5.0f, 0, 0.3f, 0.35f, 2.3f, s2); box(b, 0, 5.5f, 0, 2.6f, 0.4f, 2.4f, s2);
    Sty ir = metal(0x2a2b2e); for (int k = -2; k <= 2; k++) box(b, k * 0.9f, 4.6f, 2.0f, 0.06f, 0.65f, 0.06f, ir); box(b, 0, 5.1f, 2.0f, 2.4f, 0.07f, 0.07f, ir);
    Sty w = wood(0x4a3320); boxR(b, -2.35f, 2.0f, 0.1f, 0.14f, 2.0f, 2.2f, 0, w); boxR(b, 2.35f, 2.0f, 0.1f, 0.14f, 2.0f, 2.2f, 0, w); box(b, -2.35f, 3.2f, 0.1f, 0.18f, 0.08f, 2.25f, metal(0x2a2b2e)); box(b, 2.35f, 3.2f, 0.1f, 0.18f, 0.08f, 2.25f, metal(0x2a2b2e));
    box(b, 0, -1.6f, 0, 7.5f, 1.5f, 2.5f, stone(0x5a564f, 0.5f)); break; }
    case xw::K_KEEP: { Sty s = stone(0x7b776e), s2 = stone(0x6b675f, 0.75f), rf = {C(0x3b434d), 5, 0.7f, 0.2f, 0, 0.8f, 6}; box(b, 0, 6.5f, 0, 8.f, 9.5f, 8.f, s); box(b, 0, 16.2f, 0, 8.4f, 0.5f, 8.4f, s2); merlonsLine(b, -8.4f, 8.4f, 8.5f, 16.7f, 7, 0.7f, 0.7f, 0.38f, s); merlonsLine(b, -8.4f, 8.4f, -8.5f, 16.7f, 7, 0.7f, 0.7f, 0.38f, s);
      for (int k = 0; k < 5; k++) { float z = -8.4f + 16.8f * (k + 0.5f) / 5; box(b, 8.5f, 17.4f, z, 0.38f, 0.7f, 0.7f, s); box(b, -8.5f, 17.4f, z, 0.38f, 0.7f, 0.7f, s); }
      box(b, 0, 21.f, 0, 4.4f, 3.5f, 4.4f, s); box(b, 0, 24.7f, 0, 4.7f, 0.4f, 4.7f, s2); cyl(b, 0, 25.1f, 0, 4.6f, 0.4f, 6.5f, 4, rf);
      for (int cu = -1; cu <= 1; cu += 2) for (int cv = -1; cv <= 1; cv += 2) { cyl(b, cu * 8.f, -2.5f, cv * 8.f, 2.7f, 2.5f, 21.f, 10, s); cyl(b, cu * 8.f, 18.0f, cv * 8.f, 2.5f, 3.1f, 1.f, 10, s2); cyl(b, cu * 8.f, 19.0f, cv * 8.f, 3.2f, 0.2f, 5.5f, 10, rf); }
      Sty door = wood(0x3b2a1a), dark = {C(0x120e0b), 8, 1, 0, 0, 1, 3}; box(b, 0, 2.3f, 8.05f, 1.7f, 2.3f, 0.15f, dark); box(b, 0, 2.3f, 8.12f, 1.5f, 2.1f, 0.1f, door); box(b, 0, 4.7f, 8.1f, 2.0f, 0.2f, 0.12f, s2); box(b, 0, 0.3f, 9.2f, 3.f, 0.3f, 1.2f, s2);
      Sty win = glowS(0xffb45a, 1.0f); for (int r = 0; r < 3; r++) for (int k = -1; k <= 1; k += 2) { box(b, k * 4.5f, 7.f + r * 3.1f, 8.05f, 0.35f, 0.7f, 0.06f, win); box(b, k * 4.5f, 7.f + r * 3.1f, -8.05f, 0.35f, 0.7f, 0.06f, win); box(b, 8.05f, 7.f + r * 3.1f, k * 4.5f, 0.06f, 0.7f, 0.35f, win); box(b, -8.05f, 7.f + r * 3.1f, k * 4.5f, 0.06f, 0.7f, 0.35f, win); } break; }
    case xw::K_HOUSE: { Sty pl = {C(0xaa9c7c), 1, 0.95f, 0, 0, 0.6f, 3.f}, tim = wood(0x3a2a1d), s = stone(0x6a665e, 0.55f), rf = {C(0x6d3a2c), 7, 0.9f, 0, 0, 0.75f, 5}, gb = {C(0xaa9c7c), 1, 0.95f, 0, 0, 0.8f, 3.f}; box(b, 0, -0.2f, 0, 4.05f, 0.7f, 3.05f, s); box(b, 0, 1.7f, 0, 3.95f, 1.4f, 2.95f, pl); box(b, 0, 3.95f, 0, 4.3f, 1.2f, 3.3f, pl); box(b, 0, 3.2f, 0, 4.35f, 0.12f, 3.35f, tim);
      for (int k = -3; k <= 3; k++) { box(b, k * 1.25f, 3.95f, 3.32f, 0.1f, 1.2f, 0.05f, tim); box(b, k * 1.25f, 3.95f, -3.32f, 0.1f, 1.2f, 0.05f, tim); } for (int k = -2; k <= 2; k++) box(b, k * 1.5f, 1.7f, 2.98f, 0.1f, 1.4f, 0.05f, tim);
      roof(b, 0, 5.1f, 0, 4.f, 3.3f, 3.3f, 0.6f, rf, gb); box(b, 2.4f, 7.2f, -1.2f, 0.45f, 1.5f, 0.45f, s); Sty door = wood(0x4a3320), win = glowS(0xffb45a, 1.0f); box(b, 0, 1.15f, 3.0f, 0.55f, 1.0f, 0.06f, door); box(b, -2.4f, 1.8f, 3.0f, 0.4f, 0.4f, 0.06f, win); box(b, 2.4f, 1.8f, 3.0f, 0.4f, 0.4f, 0.06f, win); box(b, -2.2f, 4.1f, 3.36f, 0.4f, 0.4f, 0.06f, win); box(b, 2.2f, 4.1f, 3.36f, 0.4f, 0.4f, 0.06f, win); break; }
    case xw::K_LONG: { Sty pl = {C(0x8e8268), 1, 0.95f, 0, 0, 0.6f, 3.f}, tim = wood(0x33261a), s = stone(0x6a665e, 0.55f), rf = {C(0x4a4036), 7, 0.9f, 0, 0, 0.75f, 5}; box(b, 0, -0.2f, 0, 6.05f, 0.7f, 3.55f, s); box(b, 0, 2.0f, 0, 5.95f, 1.8f, 3.45f, pl); for (int k = -4; k <= 4; k++) { box(b, k * 1.3f, 2.0f, 3.48f, 0.12f, 1.8f, 0.05f, tim); box(b, k * 1.3f, 2.0f, -3.48f, 0.12f, 1.8f, 0.05f, tim); } box(b, 0, 3.7f, 3.5f, 6.f, 0.1f, 0.06f, tim);
      roof(b, 0, 3.8f, 0, 6.f, 3.5f, 3.2f, 0.7f, rf, pl); box(b, -3.5f, 6.8f, 0, 0.4f, 1.2f, 0.4f, s); box(b, 3.5f, 6.8f, 0, 0.4f, 1.2f, 0.4f, s); Sty door = wood(0x3d2b1b), win = glowS(0xffb45a, 1.0f); box(b, -2.5f, 1.2f, 3.5f, 0.6f, 1.1f, 0.06f, door); box(b, 2.5f, 1.2f, 3.5f, 0.6f, 1.1f, 0.06f, door); for (int k = 0; k < 4; k++) box(b, -4.5f + k * 3.0f, 2.2f, 3.5f, 0.4f, 0.4f, 0.06f, win); break; }
    case xw::K_PALI: { Sty w = wood(0x4a3a28, 0.55f); for (int k = 0; k < 6; k++) { float x = -1.67f + k * 0.67f; float h = 3.6f + ((k * 7) % 3) * 0.35f; cyl(b, x, -0.6f, 0, 0.23f, 0.21f, h, 7, w, nullptr, false); cyl(b, x, -0.6f + h, 0, 0.21f, 0.02f, 0.7f, 7, w, nullptr, false); } box(b, 0, 1.2f, 0.25f, 2.0f, 0.1f, 0.06f, w); box(b, 0, 2.5f, 0.25f, 2.0f, 0.1f, 0.06f, w); break; }
    case xw::K_BRIDGE: { Sty w = wood(0x5a4630, 0.7f), w2 = wood(0x3d2d1d, 0.7f); box(b, 0, -0.15f, 0, 2.1f, 0.15f, 4.f, w); for (int k = -7; k <= 7; k++) box(b, 0, 0.0f, k * 0.55f, 2.0f, 0.04f, 0.24f, k % 2 ? w2 : w); for (int sd = -1; sd <= 1; sd += 2) { box(b, sd * 2.0f, 0.95f, 0, 0.07f, 0.06f, 4.f, w2); box(b, sd * 2.0f, 0.5f, 0, 0.06f, 0.05f, 4.f, w2); for (int k = -2; k <= 2; k++) cyl(b, sd * 2.0f, -0.1f, k * 1.9f, 0.09f, 0.08f, 1.1f, 6, w2, nullptr, false); for (int k = -1; k <= 1; k += 2) cyl(b, sd * 1.6f, -6.f, k * 3.2f, 0.22f, 0.2f, 6.f, 6, w2, nullptr, false); } box(b, 0, -0.55f, 0, 2.f, 0.18f, 4.f, w2); break; }
    case xw::K_TENT: { Sty c = {C(0xa59a7c), 1, 0.95f, 0, 0, 0.7f, 2.f}, c2 = {C(0x6b5a40), 1, 0.95f, 0, 0, 0.7f, 2.f}, dk = {C(0x120e0b), 1, 1, 0, 0, 1, 3}; V3 a0{-1.7f, 0, 2.f}, a1{1.7f, 0, 2.f}, r1{0, 2.4f, 2.f}, a2{-1.7f, 0, -2.f}, a3{1.7f, 0, -2.f}, r2{0, 2.4f, -2.f};
      quad(b, a0, r1, r2, a2, c, 0.3f); quad(b, a1, a3, r2, r1, c2, 0.3f); int base = (int)b.v->size(); V3 nn{0, 0, 1}; vert(b, a0, nn, c2.col, 1, 0, 0, 1, 0.9f, 0, 0); vert(b, a1, nn, c2.col, 1, 1, 0, 1, 0.9f, 0, 0); vert(b, r1, nn, c2.col, 1, 0.5f, 1, 1, 0.9f, 0, 0); tri(b, base, base + 1, base + 2);
      base = (int)b.v->size(); nn = {0, 0, 1}; vert(b, {-0.7f, 0, 2.02f}, nn, dk.col, 1, 0, 0, 1, 1, 0, 0); vert(b, {0.7f, 0, 2.02f}, nn, dk.col, 1, 1, 0, 1, 1, 0, 0); vert(b, {0, 1.6f, 2.02f}, nn, dk.col, 1, 0.5f, 1, 1, 1, 0, 0); tri(b, base, base + 1, base + 2); cyl(b, 0, 2.2f, 2.f, 0.06f, 0.05f, 0.9f, 5, wood(0x3d2d1d), nullptr, false); break; }
    case xw::K_HAY: { Sty h = {C(0xb39a4c), 6, 0.95f, 0, 0, 0.55f, 1.f}; cyl(b, 0, 0, 0, 1.25f, 1.45f, 0.9f, 10, h, nullptr, false); cyl(b, 0, 0.9f, 0, 1.45f, 0.9f, 0.9f, 10, h, nullptr, false); cyl(b, 0, 1.8f, 0, 0.9f, 0.05f, 0.9f, 10, h, nullptr, false); break; }
    case xw::K_MILL: { Sty s = stone(0x8d8576, 0.6f), w = wood(0x4d3a26), rf = {C(0x4a3b2c), 7, 0.9f, 0, 0, 0.8f, 5}, cl = cloth(0xd8ceb0); cyl(b, 0, -0.5f, 0, 2.9f, 1.9f, 7.5f, 10, s); cyl(b, 0, 7.0f, 0, 2.3f, 0.2f, 3.2f, 10, rf, nullptr, false); box(b, 0, 1.2f, 2.3f, 0.7f, 1.2f, 0.1f, w); Sty win = glowS(0xffb45a, 0.8f); box(b, 0, 4.3f, 2.1f, 0.3f, 0.45f, 0.08f, win);
      for (int k = 0; k < 4; k++) { float a = k * PI / 2 + 0.35f; float ca = std::cos(a), sa = std::sin(a); box(b, 0, 6.2f, 2.3f, 0.0f, 0.0f, 0.0f, w); boxR(b, 0, 6.2f, 2.5f, 0.16f, 0.16f, 0.16f, 0, w); float cx = -sa * 2.6f, cy = ca * 2.6f; (void)cx; (void)cy;
        V3 c0{0, 6.2f, 2.55f}; V3 dir{ca, sa, 0}; auto Q = [&](float along, float off, float z) { return V3{c0.x + dir.x * along - dir.y * off, c0.y + dir.y * along + dir.x * off, c0.z + z}; }; quad(b, Q(0.6f, -0.12f, 0), Q(8.2f, -0.12f, 0), Q(8.2f, 0.12f, 0), Q(0.6f, 0.12f, 0), w); quad(b, Q(2.0f, 0.12f, 0.02f), Q(8.0f, 0.12f, 0.02f), Q(8.0f, 1.6f, 0.02f), Q(2.0f, 1.6f, 0.02f), cl); quad(b, Q(2.0f, 1.6f, -0.02f), Q(8.0f, 1.6f, -0.02f), Q(8.0f, 0.12f, -0.02f), Q(2.0f, 0.12f, -0.02f), cl); } break; }
    case xw::K_BANNER: { Sty w = wood(0x3d2d1d), c = cloth(0xe8e0d0), g = cloth(0xf4d58a); cyl(b, 0, 0, 0, 0.14f, 0.12f, 6.f, 6, w, nullptr, false); box(b, 0, 5.7f, 0, 1.0f, 0.07f, 0.07f, w); quad(b, {-0.85f, 2.2f, 0.02f}, {0.85f, 2.2f, 0.02f}, {0.85f, 5.65f, 0.02f}, {-0.85f, 5.65f, 0.02f}, c, 0.3f); quad(b, {0.85f, 2.2f, -0.02f}, {-0.85f, 2.2f, -0.02f}, {-0.85f, 5.65f, -0.02f}, {0.85f, 5.65f, -0.02f}, c, 0.3f);
      quad(b, {-0.45f, 3.1f, 0.03f}, {0.45f, 3.1f, 0.03f}, {0.45f, 4.6f, 0.03f}, {-0.45f, 4.6f, 0.03f}, g, 0.3f); quad(b, {0.45f, 3.1f, -0.03f}, {-0.45f, 3.1f, -0.03f}, {-0.45f, 4.6f, -0.03f}, {0.45f, 4.6f, -0.03f}, g, 0.3f); break; }
    case xw::K_FLAGPOLE: { Sty w = wood(0x3d2d1d), c = cloth(0xe8e0d0); cyl(b, 0, 0, 0, 0.1f, 0.07f, 5.f, 6, w, nullptr, false); quad(b, {0, 4.9f, 0.02f}, {0, 3.9f, 0.02f}, {2.0f, 4.1f, 0.02f}, {2.0f, 4.7f, 0.02f}, c, 0.3f); quad(b, {0, 3.9f, -0.02f}, {0, 4.9f, -0.02f}, {2.0f, 4.7f, -0.02f}, {2.0f, 4.1f, -0.02f}, c, 0.3f); break; }
    case xw::K_BIGROCK: { Sty s = {C(0x7a7266), 8, 0.95f, 0, 0, 0.5f, 1.2f}; const int seg = 9; for (int ring = 0; ring < 3; ring++) { float h0 = ring == 0 ? 0.f : ring == 1 ? 0.55f : 0.9f, h1 = ring == 0 ? 0.55f : ring == 1 ? 0.9f : 1.0f; float r0 = ring == 0 ? 1.0f : ring == 1 ? 0.85f : 0.45f, r1 = ring == 0 ? 0.85f : ring == 1 ? 0.45f : 0.0f;
      for (int k = 0; k < seg; k++) { auto J = [&](int rr, int kk) { uint32_t h = (uint32_t)(rr * 92821 + kk * 68917 + 5); h = (h ^ (h >> 13)) * 1274126177u; return 0.78f + 0.44f * (((h >> 8) & 0xff) / 255.f); }; float a0 = TAU * k / seg, a1 = TAU * (k + 1) / seg; float j00 = J(ring, k), j01 = J(ring, k + 1), j10 = J(ring + 1, k), j11 = J(ring + 1, k + 1);
        V3 p0{std::cos(a0) * r0 * j00, h0 - 0.2f, std::sin(a0) * r0 * j00}, p1{std::cos(a1) * r0 * j01, h0 - 0.2f, std::sin(a1) * r0 * j01}, p2{std::cos(a1) * r1 * j11, h1, std::sin(a1) * r1 * j11}, p3{std::cos(a0) * r1 * j10, h1, std::sin(a0) * r1 * j10}; Sty ss = s; ss.col = s.col * (0.8f + 0.4f * ((k * 37 + ring * 11) % 7) / 7.f); quad(b, p1, p0, p3, p2, ss, 0.5f); } } break; }
    case XM_LOCONI: { Sty tr = {C(0x3a2a1c), 9, 0.9f, 0, 0, 0.6f, 3.f}; cyl(b, 0, 0, 0, 0.32f, 0.2f, 3.0f, 5, tr, nullptr, false); Sty g1 = {C(0x264a2c), 0, 0.9f, 0, 0, 0.55f, 9.f}; V3 top = C(0x2f5a34);
      cyl(b, 0, 1.6f, 0, 2.5f, 0.9f, 3.0f, 9, g1, &top, false); cyl(b, 0, 3.8f, 0, 2.0f, 0.7f, 2.9f, 9, g1, &top, false); cyl(b, 0, 6.0f, 0, 1.45f, 0.0f, 3.1f, 9, g1, &top, false); break; }
    case XM_LOBROAD: { Sty tr = {C(0x4a3626), 9, 0.9f, 0, 0, 0.6f, 3.f}; cyl(b, 0, 0, 0, 0.42f, 0.28f, 3.6f, 5, tr, nullptr, false); Sty lf = {C(0x3d6a2a), 0, 0.9f, 0, 0, 0.6f, 8.f}; blob(b, 0, 5.2f, 0, 3.1f, 2.4f, 3.1f, 8, 4, lf, 3, 0.25f); blob(b, 2.2f, 4.2f, 0.7f, 2.1f, 1.7f, 2.1f, 7, 3, lf, 5, 0.25f); blob(b, -1.9f, 4.4f, -1.0f, 2.2f, 1.7f, 2.2f, 7, 3, lf, 9, 0.25f); break; }
    default: break;
  }
}
}  // namespace
bool xmBuild(int kind, std::vector<LV>& v, std::vector<uint16_t>& idx) {
  v.clear(); idx.clear(); MB b{&v, &idx}; build(kind, b); return !v.empty();
}
