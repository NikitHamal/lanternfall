// v2.4 procedural tree generator (included inside the anonymous namespace of xmesh.cpp, after xmesh_new.inc).
// Real branching (trunk flare + roots + recursive limbs with taper/curl) carrying alpha-tested LEAF CARDS (surf 12):
// every card is a quad whose fragment shader draws individual leaves (metal=0) or a fir spray (metal=1) and discards the rest.
// Card normals are ellipsoid normals of the canopy (soft volumetric shading); vertex alpha is a canopy AO (dark inside, bright rim).
struct TRng {
  uint32_t s;
  explicit TRng(uint32_t x) : s(x * 2654435761u + 0x9e3779b9u) { f(); f(); }
  float f() { s = s * 1664525u + 1013904223u; uint32_t t = s ^ (s >> 15); return ((t >> 4) & 0xffffff) / 16777216.f; }
  float r(float a, float c) { return a + (c - a) * f(); }
};
V3 rotAxis(V3 v, V3 k, float a) { float c = std::cos(a), s = std::sin(a); return v * c + cross(k, v) * s + k * (dot(k, v) * (1 - c)); }
V3 anyPerp(V3 d) { V3 r = std::fabs(d.y) < 0.9f ? V3{0, 1, 0} : V3{1, 0, 0}; return norm(cross(d, r)); }

// one double-sided leaf card: origin o, across axis u, along axis v (unit), size w x l; normal/ao given
void leafCard(MB& b, V3 o, V3 u, V3 v, float w, float l, V3 nrm, V3 col, float aoV, bool needle) {
  int base = (int)b.v->size(); V3 q[4] = {o - u * (w * 0.5f), o + u * (w * 0.5f), o + u * (w * 0.5f) + v * l, o - u * (w * 0.5f) + v * l}; float uu[4] = {0, 1, 1, 0}, vv[4] = {0, 0, 1, 1};
  for (int k = 0; k < 4; k++) vert(b, q[k], nrm, col, aoV, uu[k], vv[k], 12, 0.82f, needle ? 1.f : 0.f, 0.f);
  tri(b, base, base + 1, base + 2); tri(b, base, base + 2, base + 3); tri(b, base, base + 2, base + 1); tri(b, base, base + 3, base + 2);
}

struct TreeGen {
  MB& b; TRng g; Sty bark; int maxDepth = 2; float up = 0.1f, curl = 0.4f, kidAng = 0.8f, lenDecay = 0.62f, radDecay = 0.6f, droop = 0.f;
  int kidsMid[4] = {1, 1, 0, 0}, kidsEnd[4] = {2, 2, 0, 0}, nseg[4] = {3, 3, 2, 2}, sides[4] = {5, 4, 3, 3};
  struct Tip { V3 p, d; float r; } tips[1200]; int nt = 0;
  TreeGen(MB& bb, uint32_t seed, const Sty& bk) : b(bb), g(seed), bark(bk) {}
  void limb(V3 p, V3 d, float L, float r0, int depth) {
    int NS = nseg[depth]; float r1 = r0 * 0.5f; V3 pp = p;
    for (int i = 0; i < NS; i++) {
      float ra = lerpf(r0, r1, (float)i / NS), rb = lerpf(r0, r1, (float)(i + 1) / NS);
      d = norm(d + V3{g.r(-1, 1), g.r(-0.6f, 0.7f), g.r(-1, 1)} * curl + V3{0, up - droop * (float)(i + 1) / NS, 0});
      V3 q = pp + d * (L / NS); tube(b, pp - d * 0.03f, q, ra, rb, sides[depth], bark); pp = q;
      bool mid = (i == NS / 2 && i < NS - 1);
      if (depth < maxDepth && (mid || i == NS - 1)) { int nk = mid ? kidsMid[depth] : kidsEnd[depth]; for (int k = 0; k < nk; k++) spawn(pp, d, L, rb, depth, k, nk); }
      else if (depth >= maxDepth && i == NS - 1 && nt < 1200) tips[nt++] = {pp, d, rb};
    }
  }
  void spawn(V3 p, V3 d, float L, float r, int depth, int k, int nk) {
    V3 pr = anyPerp(d); V3 ax = rotAxis(pr, d, g.r(0, TAU) + k * TAU / std::max(nk, 1));
    float ang = kidAng * g.r(0.7f, 1.35f); if (nk > 1 && k == 0) ang *= 0.35f;
    V3 nd = rotAxis(d, ax, ang); nd.y += up * 0.6f; limb(p, norm(nd), L * lenDecay * g.r(0.8f, 1.15f), std::max(r * radDecay, 0.012f), depth + 1);
  }
  void trunk(float h, float r0, float r1, float bend, int seg, int sd, float flare) {
    V3 p{0, -0.45f, 0}; V3 bd{g.r(-1, 1), 0, g.r(-1, 1)}; bd = norm(bd) * bend;
    tube(b, p, {0, 0.55f, 0}, r0 * flare, r0 * 1.02f, sd + 2, bark);
    p = {0, 0.5f, 0};
    for (int i = 1; i <= seg; i++) { float t = (float)i / seg; V3 q{bd.x * t * t, 0.5f + (h - 0.5f) * t, bd.z * t * t}; q.x += g.r(-0.05f, 0.05f); q.z += g.r(-0.05f, 0.05f);
      tube(b, p, q, lerpf(r0, r1, (float)(i - 1) / seg), lerpf(r0, r1, t), sd, bark); p = q; }
    for (int k = 0; k < 6; k++) { float a = k * 1.047f + g.r(-0.3f, 0.3f); V3 dir{std::cos(a), 0, std::sin(a)}; float rl = r0 * g.r(1.6f, 2.6f);
      V3 s0 = dir * (r0 * 0.7f) + V3{0, 0.9f, 0}, s1 = dir * (rl * 0.8f) + V3{0, 0.12f, 0}, s2 = dir * rl + V3{0, -0.35f, 0}; tube(b, s0, s1, r0 * 0.34f, r0 * 0.2f, 4, bark); tube(b, s1, s2, r0 * 0.2f, r0 * 0.06f, 4, bark); }
  }
  V3 trunkTop(float h, float bend) { (void)bend; return {0, h, 0}; }
};

// canopy leaves for broadleaf trees from the collected tips
void canopyCards(TreeGen& T, MB& b, int budget, float sz, V3 cA, V3 cB, uint32_t seed, float flat) {
  if (T.nt == 0) return; TRng g(seed); V3 c{0, 0, 0}; float ymin = 1e9f, ymax = -1e9f, rmax = 0.01f; for (int i = 0; i < T.nt; i++) { c = c + T.tips[i].p; ymin = std::min(ymin, T.tips[i].p.y); ymax = std::max(ymax, T.tips[i].p.y); }
  c = c * (1.f / T.nt); for (int i = 0; i < T.nt; i++) rmax = std::max(rmax, std::sqrt((T.tips[i].p.x - c.x) * (T.tips[i].p.x - c.x) + (T.tips[i].p.z - c.z) * (T.tips[i].p.z - c.z)));
  float ry = std::max((ymax - ymin) * 0.5f + sz * 0.8f, 1.2f), rx = rmax + sz * 0.8f; int per = std::max(1, budget / T.nt);
  for (int i = 0; i < T.nt; i++) for (int k = 0; k < per; k++) {
    V3 off{g.r(-1, 1), g.r(-0.7f, 0.9f), g.r(-1, 1)}; off = off * (sz * 0.75f); V3 pos = T.tips[i].p - T.tips[i].d * (sz * g.r(0.0f, 0.9f)) + off;
    V3 dd = pos - c; V3 e{dd.x / rx, dd.y / ry, dd.z / rx}; float fr = clampf(len(e), 0.f, 1.2f); V3 nrm = norm(V3{e.x, e.y * 0.8f + 0.28f, e.z});
    V3 face = norm(nrm * 0.55f + V3{g.r(-1, 1), g.r(-1, 1) * flat, g.r(-1, 1)} * 0.7f); V3 u = anyPerp(face); u = rotAxis(u, face, g.r(0, TAU)); V3 v = cross(face, u);
    float s = sz * g.r(0.8f, 1.25f); float aoV = clampf(0.34f + 0.7f * std::pow(clampf(fr, 0.f, 1.f), 0.9f) + 0.14f * clampf(e.y, -0.5f, 1.f), 0.3f, 1.f);
    V3 col = mix3(cA, cB, g.f()); leafCard(b, pos - u * (s * 0.0f) - v * (s * 0.5f), u, v, s, s, nrm, col, aoV, false);
  }
}

void buildOak(MB& b, int variant) {
  Sty bk = sty(variant ? 0x3a2f27 : 0x4a3b2e, 9, 0.55f, 4.f); TreeGen T(b, 77 + variant * 13, bk); T.maxDepth = 2; T.up = 0.1f; T.curl = 0.5f; T.droop = 0.22f; T.kidAng = 0.85f;
  T.trunk(3.1f, 0.5f, 0.3f, 0.5f, 5, 8, 1.7f);
  for (int k = 0; k < 7; k++) { float a = k * 2.399f + T.g.r(-0.2f, 0.2f), hy = 1.9f + 1.5f * (k / 6.f) + (k == 0 ? -0.6f : 0.f), el = T.g.r(0.45f, 0.95f); if (k == 6) el = 1.2f;
    V3 d{std::cos(a) * std::cos(el), std::sin(el), std::sin(a) * std::cos(el)}; T.limb({0, hy, 0}, d, T.g.r(3.9f, 5.0f), 0.24f, 0); }
  canopyCards(T, b, 380, 1.7f, C(0x35602a), C(0x5f8f35), 5 + variant, 0.6f);
}
void buildPine(MB& b, int young) {
  Sty bk = sty(0x4a3626, 9, 0.5f, 4.f); TreeGen T(b, 31 + young * 7, bk); float H = young ? 8.f : 15.5f, y0 = young ? 1.3f : 2.4f; V3 cA = C(0x20401f), cB = C(0x3a5f2c);
  T.trunk(H, young ? 0.26f : 0.46f, 0.05f, 0.5f, young ? 4 : 6, 7, 1.5f);
  TRng g(901 + young); float step = young ? 0.62f : 0.86f; int w = 0;
  for (float y = y0; y < H - 0.5f; y += step, w++) {
    float f = (y - y0) / (H - y0); float L = (young ? 0.5f : 0.8f) + (young ? 1.9f : 3.5f) * std::pow(1.f - f, 0.95f); int n = f > 0.8f ? 3 : 5; float ph = w * 2.1f;
    for (int k = 0; k < n; k++) {
      float a = ph + k * TAU / n + g.r(-0.25f, 0.25f); V3 dir{std::cos(a), 0, std::sin(a)}; V3 side{-dir.z, 0, dir.x}; V3 s0{dir.x * 0.18f, y, dir.z * 0.18f};
      V3 p1 = s0 + dir * (L * 0.5f) + V3{0, -0.12f * L * 0.5f, 0}, p2 = p1 + dir * (L * 0.5f) + V3{0, -0.2f, 0}; tube(b, s0, p1, 0.07f + 0.02f * L, 0.04f, 4, bk); tube(b, p1, p2, 0.04f, 0.015f, 4, bk);
      V3 cn{0, y - 0.6f, 0};
      for (int c = 0; c < 3; c++) { float t = c == 0 ? 0.28f : c == 1 ? 0.55f : 0.78f; V3 o = s0 + (p2 - s0) * t; float cw = (0.8f + 0.3f * L) * (c == 2 ? 0.8f : 1.f), cl = L * (c == 2 ? 0.34f : 0.5f) + 0.5f;
        V3 vv = norm(dir + V3{0, -0.42f, 0}); V3 nr = norm(V3{o.x - cn.x, 0.55f, o.z - cn.z}); float aoV = clampf(0.42f + 0.62f * t + 0.2f * (1.f - f), 0.3f, 1.f);
        V3 col = mix3(cA, cB, g.f()); leafCard(b, o, side, vv, cw, cl, nr, col, aoV, true);
        if (c == 1) { V3 vv2 = norm(dir + V3{0, 0.1f, 0}); V3 u2 = norm(cross(vv2, V3{0, 1, 0})); (void)u2; leafCard(b, o + V3{0, 0.08f, 0}, side, vv2, cw * 0.9f, cl * 0.9f, nr, col * 1.08f, aoV, true); } }
    }
  }
  float tw = young ? 0.8f : 1.5f; V3 top{0, H, 0}; for (int k = 0; k < 2; k++) { float a = k * 1.5708f; V3 u{std::cos(a), 0, std::sin(a)}; leafCard(b, top - V3{0, tw * 1.1f, 0}, u, V3{0, 1, 0}, tw, tw * 1.7f, norm(V3{0, 1, 0}), C(0x2a5a30), 1.f, true); }
}
void buildBirchN(MB& b) {
  Sty bk = sty(0xe4ded0, 9, 0.7f, 4.f); TreeGen T(b, 211, bk); T.maxDepth = 2; T.up = 0.14f; T.curl = 0.4f; T.droop = 0.5f; T.kidAng = 0.7f; T.nseg[0] = 3; T.kidsMid[0] = 1; T.kidsEnd[0] = 2; T.kidsMid[1] = 0; T.kidsEnd[1] = 2;
  T.trunk(7.0f, 0.17f, 0.06f, 0.5f, 6, 6, 1.5f);
  for (int k = 0; k < 8; k++) { float a = k * 2.399f, hy = 2.6f + 3.8f * (k / 7.f), el = T.g.r(0.7f, 1.15f); V3 d{std::cos(a) * std::cos(el), std::sin(el), std::sin(a) * std::cos(el)}; T.limb({0, hy, 0}, d, T.g.r(1.9f, 2.7f), 0.06f, 0); }
  canopyCards(T, b, 260, 1.15f, C(0x7ba83a), C(0xa6c24c), 9, 0.9f);
  Sty dk = sty(0x201c1a, 9); for (int k = 0; k < 6; k++) { float y = 0.6f + k * 0.9f; cyl(b, 0, y, 0, 0.17f - 0.018f * k + 0.02f, 0.17f - 0.018f * k + 0.02f, 0.05f, 6, dk, nullptr, false); }
}
void buildCherryN(MB& b) {
  Sty bk = sty(0x2f2620, 9, 0.55f, 4.f); TreeGen T(b, 313, bk); T.maxDepth = 2; T.up = 0.05f; T.curl = 0.5f; T.droop = 0.3f; T.kidAng = 0.9f;
  T.trunk(2.3f, 0.34f, 0.22f, 0.5f, 4, 7, 1.7f);
  for (int k = 0; k < 6; k++) { float a = k * 2.399f, hy = 1.5f + 0.8f * (k / 5.f), el = T.g.r(0.4f, 0.9f); V3 d{std::cos(a) * std::cos(el), std::sin(el), std::sin(a) * std::cos(el)}; T.limb({0, hy, 0}, d, T.g.r(2.6f, 3.4f), 0.17f, 0); }
  canopyCards(T, b, 340, 1.3f, C(0xe8a6bd), C(0xf6cfdb), 13, 0.7f);
  canopyCards(T, b, 90, 1.0f, C(0xd0809f), C(0xe99ab5), 21, 0.7f);
}
