// v1.9 living-world meshes (included inside the anonymous namespace of xmesh.cpp, after xmesh_in.inc).
// Flora: birch, cherry blossom, jungle giant, weeping willow, palm, fern, mushrooms, reeds, lily pads, rock spire, flowering shrub.
// Fauna (articulated parts: +Z forward, +Y up, origin on the ground): dog, cat, hen, sheep, cow, horse, songbird, generic leg, generic wing, petal.
Sty sty(uint32_t hex, int surf, float aoB = 0.7f, float aoH = 3.f) { return {C(hex), surf, 0.9f, 0.f, 0.f, aoB, aoH}; }
void tube(MB& b, V3 p0, V3 p1, float r0, float r1, int seg, const Sty& s) {   // tapered oriented tube (same winding as cyl)
  V3 d = p1 - p0; float L = len(d); if (L < 1e-4f) return; V3 w = d * (1.f / L); V3 ref = std::fabs(w.y) < 0.95f ? V3{0, 1, 0} : V3{1, 0, 0}; V3 u = norm(cross(w, ref)), v = cross(u, w);
  int base = (int)b.v->size();
  for (int k = 0; k <= seg; k++) { float a = TAU * k / seg; V3 n = u * std::cos(a) + v * std::sin(a); V3 q0 = p0 + n * r0, q1 = p1 + n * r1;
    vert(b, q0, n, s.col, ao(q0.y, s.aoBase, s.aoH), k * 0.35f, 0, s.surf, s.rough, s.metal, s.glow); vert(b, q1, n, s.col, ao(q1.y, s.aoBase, s.aoH), k * 0.35f, L * 0.25f, s.surf, s.rough, s.metal, s.glow); }
  for (int k = 0; k < seg; k++) { int a = base + k * 2; tri(b, a, a + 1, a + 2); tri(b, a + 1, a + 3, a + 2); }
}
void quad2(MB& b, V3 p0, V3 p1, V3 p2, V3 p3, const Sty& s) { quad(b, p0, p1, p2, p3, s, 0.4f); quad(b, p3, p2, p1, p0, s, 0.4f); }   // double sided
// curved leaf / frond / blade strip: leaves `base` heading from vertical by `lean` radians towards yaw, bending by `droop` radians over its length
void blade(MB& b, V3 base, float yaw, float length, float width, float lean, float droop, int segs, const Sty& s) {
  float hx = std::cos(yaw), hz = std::sin(yaw); V3 wd{-hz, 0, hx}; V3 p = base; float sl = length / segs; V3 a0 = p - wd * (width * 0.5f), a1 = p + wd * (width * 0.5f);
  for (int i = 1; i <= segs; i++) { float t = (float)i / segs, th = lean + droop * t * t; p = p + V3{std::sin(th) * hx * sl, std::cos(th) * sl, std::sin(th) * hz * sl}; float hw = width * 0.5f * (1.f - 0.88f * t * t);
    V3 b0 = p - wd * hw, b1 = p + wd * hw; quad2(b, a0, a1, b1, b0, s); a0 = b0; a1 = b1; }
}
void leafBlob(MB& b, float x, float y, float z, float rx, float ry, float rz, const Sty& s, uint32_t seed) { blob(b, x, y, z, rx, ry, rz, 8, 4, s, seed, 0.32f); }

void buildFlora(int kind, MB& b) {
  switch (kind) {
    case xw::K_BIRCH: { Sty bk = sty(0xe8e2d4, 9), bd = sty(0x25211f, 9), l1 = sty(0x7aa83c, 0, 0.55f, 10.f), l2 = sty(0x9cbd4c, 0, 0.6f, 10.f);
      tube(b, {0, -0.3f, 0}, {0.14f, 3.4f, 0.05f}, 0.2f, 0.15f, 6, bk); tube(b, {0.14f, 3.4f, 0.05f}, {0.0f, 7.0f, -0.12f}, 0.15f, 0.06f, 6, bk);
      for (int k = 0; k < 8; k++) { float y = 0.5f + k * 0.78f; float ox = 0.14f * std::min(y, 3.4f) / 3.4f - (y > 3.4f ? 0.14f * (y - 3.4f) / 3.6f : 0.f); cyl(b, ox, y, 0.04f, 0.2f - 0.012f * y + 0.012f, 0.2f - 0.012f * y + 0.012f, 0.06f, 6, bd, nullptr, false); }
      tube(b, {0.1f, 3.9f, 0.05f}, {1.5f, 5.3f, 0.6f}, 0.09f, 0.04f, 5, bk); tube(b, {0.05f, 4.8f, 0.0f}, {-1.4f, 6.2f, -0.5f}, 0.08f, 0.04f, 5, bk); tube(b, {0.0f, 5.6f, 0.0f}, {0.5f, 7.2f, 1.2f}, 0.07f, 0.03f, 5, bk);
      leafBlob(b, 0.1f, 6.6f, 0, 1.55f, 1.9f, 1.55f, l1, 3); leafBlob(b, 1.6f, 5.6f, 0.6f, 1.3f, 1.4f, 1.3f, l2, 7); leafBlob(b, -1.5f, 6.1f, -0.5f, 1.35f, 1.5f, 1.35f, l1, 11); leafBlob(b, 0.5f, 7.5f, 1.1f, 1.1f, 1.2f, 1.1f, l2, 13); leafBlob(b, -0.2f, 8.0f, -0.3f, 1.05f, 1.2f, 1.05f, l1, 17); break; }
    case xw::K_CHERRY: { Sty bk = sty(0x4a3328, 9), p1 = sty(0xf3aac4, 0, 0.7f, 10.f), p2 = sty(0xf8c4d6, 0, 0.75f, 10.f), p3 = sty(0xe58cb0, 0, 0.7f, 10.f);
      tube(b, {0, -0.3f, 0}, {0.25f, 1.7f, 0.1f}, 0.36f, 0.27f, 6, bk); tube(b, {0.25f, 1.7f, 0.1f}, {0.6f, 2.9f, 0.25f}, 0.27f, 0.2f, 6, bk);
      tube(b, {0.6f, 2.9f, 0.25f}, {-1.4f, 4.2f, -0.4f}, 0.17f, 0.08f, 5, bk); tube(b, {0.6f, 2.9f, 0.25f}, {2.1f, 4.3f, 0.7f}, 0.17f, 0.08f, 5, bk); tube(b, {0.6f, 2.9f, 0.25f}, {0.4f, 4.8f, 1.7f}, 0.15f, 0.07f, 5, bk); tube(b, {0.55f, 3.0f, 0.2f}, {0.2f, 5.2f, -1.6f}, 0.14f, 0.07f, 5, bk);
      leafBlob(b, 0.5f, 5.2f, 0.2f, 2.5f, 1.5f, 2.5f, p1, 3); leafBlob(b, 2.2f, 4.6f, 0.7f, 1.7f, 1.2f, 1.7f, p2, 5); leafBlob(b, -1.6f, 4.7f, -0.4f, 1.9f, 1.25f, 1.9f, p3, 9); leafBlob(b, 0.4f, 5.4f, 1.8f, 1.7f, 1.2f, 1.7f, p2, 13); leafBlob(b, 0.2f, 5.7f, -1.7f, 1.7f, 1.2f, 1.7f, p1, 17); leafBlob(b, 0.6f, 6.4f, 0.3f, 1.5f, 0.9f, 1.5f, p2, 19);
      for (int k = 0; k < 6; k++) { float a = k * 1.05f + 0.3f; blade(b, {0.5f + std::cos(a) * 2.3f, 4.5f, 0.3f + std::sin(a) * 2.3f}, a, 0.9f, 0.14f, 2.7f, 0.2f, 2, p3); } break; }
    case xw::K_JUNGLE: { Sty bk = sty(0x5c4c3c, 9), l1 = sty(0x2f7a28, 0, 0.5f, 14.f), l2 = sty(0x42932f, 0, 0.55f, 14.f), vn = sty(0x3a5e2a, 0), fl = sty(0xe3662a, 0);
      tube(b, {0, -0.4f, 0}, {0.2f, 11.5f, 0.1f}, 0.78f, 0.42f, 7, bk);
      for (int k = 0; k < 5; k++) { float a = k * 1.2566f + 0.4f; tube(b, {std::cos(a) * 2.0f, -0.3f, std::sin(a) * 2.0f}, {std::cos(a) * 0.45f, 4.0f, std::sin(a) * 0.45f}, 0.4f, 0.14f, 5, bk); }
      leafBlob(b, 0.2f, 12.6f, 0.1f, 5.0f, 1.9f, 5.0f, l1, 3); leafBlob(b, 3.3f, 11.8f, 1.6f, 3.3f, 1.4f, 3.3f, l2, 5); leafBlob(b, -3.1f, 12.0f, -1.9f, 3.5f, 1.5f, 3.5f, l1, 9); leafBlob(b, 0.6f, 14.0f, -1.0f, 3.1f, 1.6f, 3.1f, l2, 13); leafBlob(b, -1.5f, 11.6f, 3.2f, 2.8f, 1.3f, 2.8f, l2, 15);
      for (int k = 0; k < 8; k++) { float a = k * 0.785f + 0.2f, r = 3.4f + (k % 3) * 0.5f; tube(b, {std::cos(a) * r, 11.8f, std::sin(a) * r}, {std::cos(a) * (r + 0.3f), 5.0f + (k % 4) * 1.2f, std::sin(a) * (r + 0.3f)}, 0.05f, 0.025f, 4, vn); }
      for (int k = 0; k < 3; k++) { float a = k * 2.1f + 1.f; blob(b, std::cos(a) * 0.7f, 5.2f + k * 1.8f, std::sin(a) * 0.7f, 0.22f, 0.18f, 0.22f, 6, 3, fl, 21 + k, 0.2f); } break; }
    case xw::K_WILLOW: { Sty bk = sty(0x4a3d2e, 9), l1 = sty(0x86b04a, 0, 0.6f, 8.f), st = sty(0x93bb55, 0, 0.55f, 4.f);
      tube(b, {0, -0.3f, 0}, {0.45f, 2.5f, 0.1f}, 0.52f, 0.36f, 7, bk); tube(b, {0.45f, 2.5f, 0.1f}, {-1.0f, 4.1f, 0.4f}, 0.26f, 0.12f, 6, bk); tube(b, {0.45f, 2.5f, 0.1f}, {1.7f, 4.0f, -0.3f}, 0.24f, 0.12f, 6, bk);
      leafBlob(b, 0.4f, 4.4f, 0.1f, 2.8f, 1.2f, 2.8f, l1, 3); leafBlob(b, -1.1f, 4.2f, 0.4f, 1.8f, 1.0f, 1.8f, l1, 7); leafBlob(b, 1.8f, 4.1f, -0.3f, 1.8f, 1.0f, 1.8f, l1, 9);
      for (int k = 0; k < 30; k++) { float a = k * 0.2094f + 0.07f * (k % 3), r = 1.6f + 1.4f * ((k * 7) % 5) / 4.f; float cx = 0.4f + std::cos(a) * r * 0.9f, cz = 0.1f + std::sin(a) * r * 0.9f; blade(b, {cx, 4.0f - 0.05f * r, cz}, a, 3.4f + 0.7f * ((k * 3) % 4) / 3.f, 0.26f, 2.85f, 0.3f, 3, st); } break; }
    case xw::K_PALM: { Sty bk = sty(0x8a6c48, 9), rg = sty(0x6a5238, 9), f1 = sty(0x4c9a32, 0, 0.6f, 9.f), f2 = sty(0x3a8628, 0, 0.55f, 9.f), co = sty(0x5a3c22, 0);
      V3 prev{0, -0.3f, 0}; for (int i = 1; i <= 6; i++) { float t = i / 6.f; V3 p{std::sin(t * 0.9f) * 1.15f, -0.3f + t * 7.9f, 0.f}; tube(b, prev, p, 0.3f - 0.14f * (i - 1) / 6.f, 0.3f - 0.14f * i / 6.f, 6, bk); if (i < 6) cyl(b, p.x, p.y - 0.04f, 0, 0.3f - 0.14f * i / 6.f + 0.02f, 0.3f - 0.14f * i / 6.f + 0.02f, 0.07f, 6, rg, nullptr, false); prev = p; }
      V3 top = prev; for (int k = 0; k < 11; k++) { float a = k * TAU / 11 + 0.2f; blade(b, top + V3{0, 0.05f, 0}, a, 3.4f, 0.62f, 0.55f + 0.25f * (k % 2), 1.55f, 5, k % 2 ? f1 : f2); }
      for (int k = 0; k < 3; k++) { float a = k * 2.1f; blob(b, top.x + std::cos(a) * 0.22f, top.y - 0.25f, std::sin(a) * 0.22f, 0.16f, 0.2f, 0.16f, 6, 3, co, 31 + k, 0.1f); } break; }
    case xw::K_FERN: { Sty g1 = sty(0x2f7a2a, 0, 0.55f, 1.f), g2 = sty(0x3f9238, 0, 0.6f, 1.f); for (int k = 0; k < 10; k++) blade(b, {0, 0.02f, 0}, k * 0.628f + 0.2f * (k % 3), 1.05f + 0.35f * (k % 3), 0.24f, 0.75f + 0.12f * (k % 4), 1.15f, 4, k % 2 ? g1 : g2); break; }
    case xw::K_MUSHROOM: { Sty st = sty(0xe8dcc0, 1, 0.8f, 1.f), red = sty(0xc4372a, 1, 0.75f, 1.f), brn = sty(0x8a5a36, 1, 0.75f, 1.f), wh = sty(0xf6f0e0, 1);
      for (int k = 0; k < 6; k++) { float a = k * 1.047f + 0.3f, r = k == 0 ? 0.f : 0.3f + 0.08f * (k % 3); float x = std::cos(a) * r, z = std::sin(a) * r, hh = 0.2f + 0.07f * (k % 3), cr = 0.15f + 0.04f * (k % 2); bool rd = k % 2 == 0;
        cyl(b, x, 0, z, 0.04f, 0.03f, hh, 6, st, nullptr, false); blob(b, x, hh, z, cr, cr * 0.55f, cr, 8, 3, rd ? red : brn, 40 + k, 0.1f);
        if (rd) for (int j = 0; j < 4; j++) { float aa = j * 1.57f + k; blob(b, x + std::cos(aa) * cr * 0.55f, hh + cr * 0.4f, z + std::sin(aa) * cr * 0.55f, 0.025f, 0.015f, 0.025f, 5, 2, wh, 50 + j, 0.f); } } break; }
    case xw::K_REEDS: { Sty r1 = sty(0x6d8a3a, 0, 0.6f, 1.5f), r2 = sty(0x82a048, 0, 0.6f, 1.5f), ct = sty(0x5a3a22, 1), sk = sty(0x7a8f48, 0);
      for (int k = 0; k < 18; k++) { float a = k * 2.4f, r = 0.1f + 0.55f * ((k * 13) % 7) / 6.f; blade(b, {std::cos(a) * r, 0, std::sin(a) * r}, a * 1.7f, 1.35f + 0.65f * ((k * 5) % 6) / 5.f, 0.06f, 0.07f + 0.3f * ((k * 3) % 4) / 3.f, 0.34f, 3, k % 2 ? r1 : r2); }
      for (int k = 0; k < 4; k++) { float a = k * 1.9f + 0.5f, r = 0.2f + 0.2f * (k % 2); float x = std::cos(a) * r, z = std::sin(a) * r, hh = 1.5f + 0.15f * k; tube(b, {x, 0, z}, {x + 0.04f, hh, z}, 0.012f, 0.01f, 4, sk); cyl(b, x + 0.04f, hh - 0.05f, z, 0.045f, 0.04f, 0.24f, 6, ct, nullptr, true); } break; }
    case xw::K_LILY: { Sty pad = sty(0x2f7a3a, 0, 0.8f, 1.f), pad2 = sty(0x3f8e44, 0, 0.8f, 1.f), pk = sty(0xf4b6cf, 1, 0.85f, 1.f), yl = sty(0xf2d24a, 1);
      const float px[3] = {0.f, 0.75f, -0.55f}, pz[3] = {0.f, 0.35f, 0.62f}, pr[3] = {0.55f, 0.42f, 0.38f}; for (int k = 0; k < 3; k++) cyl(b, px[k], 0.f, pz[k], pr[k], pr[k], 0.018f, 10, k % 2 ? pad2 : pad, nullptr, true);
      for (int k = 0; k < 8; k++) blade(b, {0.12f, 0.03f, 0.04f}, k * 0.785f, 0.24f, 0.11f, 0.85f, -0.4f, 2, pk); blob(b, 0.12f, 0.08f, 0.04f, 0.035f, 0.03f, 0.035f, 5, 2, yl, 61, 0.f); break; }
    case xw::K_SPIRE: { Sty r1 = {C(0x7d776c), 8, 0.92f, 0, 0, 0.5f, 6.f}, r2 = {C(0x8d887b), 8, 0.92f, 0, 0, 0.55f, 14.f}, r3 = {C(0x6b655b), 8, 0.92f, 0, 0, 0.5f, 10.f};
      cyl(b, 0, -1.f, 0, 1.9f, 1.25f, 5.6f, 5, r1, nullptr, true); cyl(b, 0.1f, 4.5f, 0.05f, 1.25f, 0.68f, 4.5f, 5, r2, nullptr, true); cyl(b, -0.05f, 9.0f, 0, 0.68f, 0.06f, 3.6f, 5, r3, nullptr, false);
      tube(b, {1.4f, -1.f, 0.3f}, {0.95f, 5.2f, 0.1f}, 0.85f, 0.2f, 5, r2); tube(b, {-1.3f, -1.f, -0.4f}, {-0.7f, 4.0f, -0.2f}, 0.8f, 0.18f, 5, r3); tube(b, {0.2f, -1.f, 1.5f}, {0.15f, 3.2f, 0.9f}, 0.7f, 0.15f, 5, r1); break; }
    case xw::K_FBUSH: { Sty lf = sty(0x3a6e2c, 0, 0.55f, 1.2f); blob(b, 0, 0.55f, 0, 0.98f, 0.62f, 0.98f, 9, 4, lf, 5, 0.25f);
      const uint32_t fc[4] = {0xf2a0c0, 0xf8f4ee, 0xf4d24a, 0xb48ae8}; for (int k = 0; k < 16; k++) { float a = k * 2.4f, el = 0.1f + 0.75f * ((k * 37) % 10) / 10.f; float x = std::cos(a) * std::cos(el) * 0.95f, y = 0.55f + std::sin(el) * 0.6f, z = std::sin(a) * std::cos(el) * 0.95f;
        Sty f = sty(fc[(k / 2) % 4], 1, 0.9f, 1.f); blob(b, x, y, z, 0.13f, 0.1f, 0.13f, 6, 3, f, 70 + k, 0.2f); } break; }
    default: break;
  }
}

void buildFauna(int kind, MB& b) {
  Sty fur, dk, lt, nose;
  switch (kind) {
    case xw::K_FA_LEG: { Sty lg = {V3{1, 1, 1}, 1, 0.9f, 0, 0, 0.85f, 1.f}, hf = {C(0x2e2824), 1, 0.9f, 0, 0, 0.7f, 1.f}; tube(b, {0, 0, 0}, {0, -0.93f, 0}, 1.f, 0.72f, 6, lg); box(b, 0, -0.96f, 0.1f, 0.8f, 0.045f, 0.9f, hf); break; }
    case xw::K_FA_WING: { Sty w = {V3{1, 1, 1}, 1, 0.8f, 0, 0, 0.95f, 1.f}; quad2(b, {0, 0, 0.12f}, {0.5f, 0, 0.14f}, {0.5f, 0, -0.14f}, {0, 0, -0.12f}, w); quad2(b, {0.5f, 0, 0.14f}, {1.f, 0, 0.0f}, {1.f, 0, -0.22f}, {0.5f, 0, -0.14f}, w); break; }
    case xw::K_PETAL: { Sty pt = {V3{1, 1, 1}, 1, 0.8f, 0, 0, 1.f, 1.f}; quad2(b, {-0.045f, 0, -0.07f}, {0.045f, 0, -0.07f}, {0.05f, 0.012f, 0.07f}, {-0.05f, 0.012f, 0.07f}, pt); break; }
    case xw::K_FA_DOG: { fur = sty(0xc4985c, 1, 0.75f, 1.f); dk = sty(0x5a3a22, 1, 0.8f, 1.f); lt = sty(0xe8d8b8, 1, 0.8f, 1.f); nose = sty(0x1a1412, 1);
      blob(b, 0, 0.55f, -0.02f, 0.16f, 0.17f, 0.36f, 8, 4, fur, 3, 0.08f); blob(b, 0, 0.57f, 0.2f, 0.18f, 0.2f, 0.2f, 7, 3, fur, 5, 0.08f); blob(b, 0, 0.45f, 0.02f, 0.1f, 0.07f, 0.28f, 6, 3, lt, 6, 0.05f);
      tube(b, {0, 0.64f, 0.3f}, {0, 0.76f, 0.46f}, 0.11f, 0.09f, 6, fur); blob(b, 0, 0.78f, 0.52f, 0.12f, 0.11f, 0.14f, 7, 4, fur, 7, 0.06f); box(b, 0, 0.74f, 0.68f, 0.05f, 0.045f, 0.08f, lt); box(b, 0, 0.765f, 0.77f, 0.026f, 0.02f, 0.016f, nose);
      boxR(b, -0.085f, 0.89f, 0.49f, 0.025f, 0.07f, 0.04f, 0.f, dk); boxR(b, 0.085f, 0.89f, 0.49f, 0.025f, 0.07f, 0.04f, 0.f, dk); tube(b, {0, 0.62f, -0.34f}, {0, 0.84f, -0.56f}, 0.045f, 0.022f, 5, fur); break; }
    case xw::K_FA_CAT: { fur = sty(0xd8c8b0, 1, 0.75f, 1.f); dk = sty(0x4a4038, 1, 0.8f, 1.f); lt = sty(0xf2ead8, 1, 0.8f, 1.f);
      blob(b, 0, 0.3f, 0, 0.1f, 0.1f, 0.22f, 8, 4, fur, 3, 0.06f); blob(b, 0, 0.33f, 0.14f, 0.11f, 0.12f, 0.12f, 7, 3, fur, 5, 0.06f); blob(b, 0, 0.42f, 0.28f, 0.085f, 0.08f, 0.085f, 7, 3, fur, 7, 0.04f); box(b, 0, 0.395f, 0.35f, 0.03f, 0.025f, 0.03f, lt);
      boxR(b, -0.055f, 0.5f, 0.27f, 0.02f, 0.035f, 0.01f, 0.f, dk); boxR(b, 0.055f, 0.5f, 0.27f, 0.02f, 0.035f, 0.01f, 0.f, dk); for (int k = 0; k < 3; k++) box(b, 0, 0.4f, -0.02f + k * 0.06f, 0.104f, 0.012f, 0.012f, dk);
      tube(b, {0, 0.32f, -0.2f}, {0, 0.38f, -0.34f}, 0.032f, 0.026f, 5, fur); tube(b, {0, 0.38f, -0.34f}, {0, 0.6f, -0.38f}, 0.026f, 0.018f, 5, fur); break; }
    case xw::K_FA_HEN: { fur = sty(0xf0e8da, 1, 0.8f, 1.f); dk = sty(0xb8452a, 1, 0.85f, 1.f); lt = sty(0xe0a030, 1, 0.8f, 1.f); nose = sty(0x8a5a36, 1, 0.8f, 1.f);
      blob(b, 0, 0.26f, 0, 0.1f, 0.11f, 0.15f, 8, 4, fur, 3, 0.1f); blob(b, 0, 0.36f, 0.12f, 0.06f, 0.1f, 0.06f, 6, 3, fur, 5, 0.06f); blob(b, 0, 0.46f, 0.15f, 0.05f, 0.05f, 0.055f, 6, 3, fur, 7, 0.04f);
      box(b, 0, 0.52f, 0.15f, 0.01f, 0.03f, 0.04f, dk); box(b, 0, 0.44f, 0.2f, 0.012f, 0.012f, 0.03f, lt); box(b, 0, 0.4f, 0.185f, 0.008f, 0.025f, 0.01f, dk);
      for (int k = -1; k <= 1; k++) boxR(b, k * 0.035f, 0.38f, -0.2f, 0.012f, 0.1f, 0.07f, k * 0.25f, k == 0 ? nose : fur); break; }
    case xw::K_FA_SHEEP: { fur = sty(0xece6d6, 1, 0.8f, 3.f); dk = sty(0x3a3028, 1, 0.8f, 1.f);
      blob(b, 0, 0.72f, 0, 0.36f, 0.31f, 0.5f, 9, 5, fur, 3, 0.26f); blob(b, 0.18f, 0.84f, -0.15f, 0.2f, 0.16f, 0.22f, 7, 3, fur, 5, 0.25f); blob(b, -0.18f, 0.84f, 0.12f, 0.2f, 0.16f, 0.22f, 7, 3, fur, 7, 0.25f); blob(b, 0, 0.8f, -0.4f, 0.25f, 0.22f, 0.2f, 7, 3, fur, 9, 0.25f);
      blob(b, 0, 0.82f, 0.46f, 0.2f, 0.2f, 0.18f, 7, 3, fur, 11, 0.22f); tube(b, {0, 0.76f, 0.42f}, {0, 0.84f, 0.6f}, 0.09f, 0.08f, 6, dk); blob(b, 0, 0.82f, 0.68f, 0.09f, 0.1f, 0.13f, 7, 3, dk, 13, 0.05f);
      boxR(b, -0.14f, 0.88f, 0.62f, 0.07f, 0.02f, 0.04f, -0.4f, dk); boxR(b, 0.14f, 0.88f, 0.62f, 0.07f, 0.02f, 0.04f, 0.4f, dk); break; }
    case xw::K_FA_COW: { fur = sty(0xe9e4da, 1, 0.8f, 3.f); dk = sty(0x2a2624, 1, 0.8f, 2.f); lt = sty(0xe6a8a0, 1, 0.85f, 1.f); nose = sty(0xd8d0b8, 1);
      blob(b, 0, 1.18f, 0, 0.5f, 0.44f, 0.98f, 10, 5, fur, 3, 0.06f); blob(b, 0.28f, 1.3f, 0.2f, 0.26f, 0.2f, 0.42f, 7, 3, dk, 5, 0.12f); blob(b, -0.3f, 1.22f, -0.45f, 0.24f, 0.22f, 0.36f, 7, 3, dk, 7, 0.12f); blob(b, 0.1f, 1.45f, -0.6f, 0.3f, 0.12f, 0.3f, 7, 3, dk, 9, 0.12f);
      blob(b, 0, 0.92f, -0.55f, 0.18f, 0.14f, 0.18f, 6, 3, lt, 11, 0.05f); tube(b, {0, 1.3f, 0.8f}, {0, 1.3f, 1.12f}, 0.28f, 0.22f, 7, fur); blob(b, 0, 1.28f, 1.22f, 0.21f, 0.2f, 0.28f, 8, 4, fur, 13, 0.05f); box(b, 0, 1.2f, 1.45f, 0.16f, 0.1f, 0.1f, lt); box(b, -0.08f, 1.22f, 1.55f, 0.02f, 0.02f, 0.02f, nose);
      boxR(b, -0.25f, 1.38f, 1.12f, 0.1f, 0.03f, 0.05f, -0.2f, fur); boxR(b, 0.25f, 1.38f, 1.12f, 0.1f, 0.03f, 0.05f, 0.2f, fur); tube(b, {-0.14f, 1.46f, 1.14f}, {-0.3f, 1.62f, 1.1f}, 0.03f, 0.012f, 5, nose); tube(b, {0.14f, 1.46f, 1.14f}, {0.3f, 1.62f, 1.1f}, 0.03f, 0.012f, 5, nose);
      tube(b, {0, 1.42f, -0.95f}, {0.02f, 0.72f, -1.08f}, 0.045f, 0.03f, 5, fur); blob(b, 0.02f, 0.66f, -1.08f, 0.06f, 0.1f, 0.06f, 5, 3, dk, 15, 0.1f); break; }
    case xw::K_FA_HORSE: { fur = sty(0xffffff, 1, 0.8f, 3.f); dk = sty(0x2a2018, 1, 0.8f, 2.f); lt = sty(0xf0e6d0, 1, 0.8f, 1.f);
      blob(b, 0, 1.3f, 0, 0.37f, 0.42f, 0.88f, 10, 5, fur, 3, 0.04f); blob(b, 0, 1.38f, 0.62f, 0.34f, 0.4f, 0.32f, 8, 4, fur, 5, 0.04f); blob(b, 0, 1.3f, -0.6f, 0.36f, 0.4f, 0.32f, 8, 4, fur, 7, 0.04f);
      tube(b, {0, 1.5f, 0.72f}, {0, 2.12f, 1.12f}, 0.24f, 0.14f, 7, fur); blob(b, 0, 2.12f, 1.14f, 0.14f, 0.17f, 0.17f, 7, 3, fur, 9, 0.04f); tube(b, {0, 2.12f, 1.2f}, {0, 1.78f, 1.5f}, 0.13f, 0.1f, 7, fur); box(b, 0, 1.76f, 1.52f, 0.09f, 0.08f, 0.07f, lt);
      boxR(b, -0.07f, 2.34f, 1.08f, 0.025f, 0.07f, 0.04f, 0.f, fur); boxR(b, 0.07f, 2.34f, 1.08f, 0.025f, 0.07f, 0.04f, 0.f, fur);
      for (int k = 0; k < 6; k++) box(b, 0, 2.18f - k * 0.1f, 0.9f - k * 0.08f - 0.0f, 0.03f, 0.1f, 0.05f, dk); box(b, 0, 2.32f, 1.08f, 0.03f, 0.06f, 0.04f, dk);
      tube(b, {0, 1.5f, -0.86f}, {0, 0.9f, -1.14f}, 0.07f, 0.12f, 6, dk); blob(b, 0, 0.78f, -1.16f, 0.1f, 0.2f, 0.08f, 6, 3, dk, 11, 0.2f); break; }
    case xw::K_FA_BIRD: { fur = sty(0xffffff, 1, 0.8f, 1.f); lt = sty(0xe8963a, 1, 0.85f, 1.f); dk = sty(0xb89870, 1, 0.8f, 1.f);
      blob(b, 0, 0, 0, 0.05f, 0.045f, 0.09f, 7, 3, fur, 3, 0.05f); blob(b, 0, 0.03f, 0.08f, 0.036f, 0.036f, 0.036f, 6, 3, fur, 5, 0.04f); box(b, 0, 0.025f, 0.125f, 0.008f, 0.008f, 0.02f, lt); boxR(b, 0, -0.005f, -0.14f, 0.03f, 0.004f, 0.065f, 0.f, dk); break; }
    default: break;
  }
}
