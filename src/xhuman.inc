// v2.8 human kit (included inside the anonymous namespace of xmesh.cpp, after xmesh_fauna.inc).
// Proportioned, clothed people (farmers, women, elders, children, merchants, guards, soldiers, knights per nation) built with the loft
// builder and skinned to the Polyfork 25-bone biped (T-pose bind, bone positions of polyfork/warden.lfm) so every baked clip and the
// procedural locomotion layer drive them.  Faces have eyes, brows, nose, mouth, ears; hair / beards / hats are partial shells.
enum { BN_HIPS = 1, BN_SPINE, BN_CHEST, BN_CHEST2, BN_NECK, BN_HEAD, BN_LCLAV, BN_LUP, BN_LFORE, BN_LHAND, BN_RCLAV, BN_RUP, BN_RFORE, BN_RHAND, BN_LTHIGH, BN_LSHIN, BN_LFOOT, BN_LTOE, BN_RTHIGH, BN_RSHIN, BN_RFOOT, BN_RTOE };
struct HSpec { uint32_t skin, hair, top, bot, boot, accent, tab, cape; int hairSt, hat, beard, armor, dress, apron, sleeve; float girth, shoulder; bool female; };
// hairSt 0 bald 1 short 2 long 3 bun 4 braid 5 mop | hat 0 none 1 straw 2 hood 3 cap 4 kettle 5 coif 6 bascinet 7 scarf 8 flat cap
// armor 0 cloth 1 gambeson 2 mail 3 plate | dress 0 none 1 skirt 2 long robe | sleeve 0 rolled 1 long
constexpr int XM_NHUMAN = 19;
inline float smx(float a, float b2, float x) { float t = clampf((x - a) / (b2 - a), 0.f, 1.f); return t * t * (3 - 2 * t); }
template <class F> void skinR(MB& b, size_t s, F f) {
  for (size_t i = s; i < b.v->size(); i++) { LV& o = (*b.v)[i]; int a = 1, c = 1; float w = 0; f(o.p[0], o.p[1], o.p[2], a, c, w); o.b[0] = (uint8_t)a; o.b[1] = (uint8_t)c; o.b[2] = (uint8_t)(clampf(w, 0.f, 1.f) * 255.f + 0.5f); }
}
template <class K, class F> void shellP(MB& b, V3 c, V3 r, int seg, int rings, const Sty& s, K keep, F shade, float jit, uint32_t seed) {
  int base = (int)b.v->size(); auto H = [&](int i, int j) { uint32_t h = (uint32_t)(i * 73856093) ^ (uint32_t)(j * 19349663) ^ seed * 83492791u; h = (h ^ (h >> 13)) * 1274126177u; return ((h >> 8) & 0xffff) / 65535.f; };
  for (int j = 0; j <= rings; j++) for (int i = 0; i <= seg; i++) { float ph = PI * j / rings, a = TAU * (i % seg) / seg; float jj = 1.f + (H(i % seg, j) - 0.5f) * jit; float x = std::sin(ph) * std::cos(a), y = std::cos(ph), z = std::sin(ph) * std::sin(a);
    V3 p{c.x + x * r.x * jj, c.y + y * r.y * jj, c.z + z * r.z * jj}; V3 n = norm(V3{x / r.x, y / r.y, z / r.z}); Shd sh = shade(p, n); vert(b, p, n, sh.col, sh.ao, i * 0.2f, j * 0.2f, s.surf, s.rough, s.metal, s.glow); }
  for (int j = 0; j < rings; j++) for (int i = 0; i < seg; i++) { int a = base + j * (seg + 1) + i, c2 = a + seg + 1; auto P = [&](int q) { const LV& o = (*b.v)[(size_t)q]; return V3{o.p[0], o.p[1], o.p[2]}; };
    V3 c0 = (P(a) + P(c2) + P(a + 1)) * (1.f / 3.f), c1 = (P(a + 1) + P(c2) + P(c2 + 1)) * (1.f / 3.f); if (keep(c0)) tri(b, a, c2, a + 1); if (keep(c1)) tri(b, a + 1, c2, c2 + 1); }
}
Sty hsty(int surf, float rough = 0.8f, float metal = 0.f) { return Sty{V3{1, 1, 1}, surf, rough, metal, 0.f, 1.f, 1.f}; }
auto hShade(uint32_t hex, float ao0, float ao1, float nz = 0.2f, float freq = 12.f) { V3 c = C(hex); return [=](float u, float, float, V3 p) -> Shd { float n = 1.f - nz * 0.5f + nz * vnz(p * freq); return Shd{c * n, ao0 + (ao1 - ao0) * u}; }; }
auto hShadeP(uint32_t hex, float ao, float nz, float freq) { V3 c = C(hex); return [=](V3 p, V3) -> Shd { float n = 1.f - nz * 0.5f + nz * vnz(p * freq); return Shd{c * n, ao}; }; }

void hSpecOf(int vi, HSpec& s) {
  static const uint32_t SK[5] = {0xe3b592, 0xc78b62, 0x8d5a3b, 0xf0c8a8, 0xa86f48};
  static const HSpec V[10] = {
    /*0 farmer*/  {SK[1], 0x4a3322, 0xb9a98a, 0x5b4a38, 0x3a2a1c, 0x8a6a40, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1.05f, 1.f, false},
    /*1 woman blue*/ {SK[0], 0x2a1c14, 0x3d5f8c, 0x3d5f8c, 0x4a3426, 0xe8e0d0, 0, 0, 3, 0, 0, 0, 1, 1, 1, 0.9f, 0.92f, true},
    /*2 woman red*/ {SK[3], 0x8a4a22, 0x9c3c34, 0x9c3c34, 0x3a2a1c, 0xd8c8a0, 0, 0, 4, 7, 0, 0, 1, 0, 1, 0.9f, 0.92f, true},
    /*3 man green*/ {SK[0], 0x7a5a30, 0x4f7a42, 0x4a3e34, 0x3a2a1c, 0x6a4a28, 0, 0, 1, 8, 1, 0, 0, 0, 1, 1.f, 1.f, false},
    /*4 elder*/   {SK[3], 0xe2dfd8, 0x6e6e78, 0x6e6e78, 0x3a3028, 0x4a3a2a, 0, 0, 2, 0, 2, 0, 2, 0, 1, 0.95f, 0.95f, false},
    /*5 baker*/   {SK[0], 0x3a2a1c, 0xe6e2d6, 0x6a5a48, 0x3a2a1c, 0xf2eee4, 0, 0, 0, 8, 1, 0, 0, 1, 1, 1.15f, 1.05f, false},
    /*6 child*/   {SK[0], 0x7a4a24, 0x8a6a3c, 0x4f4a58, 0x3a2a1c, 0x6a4a28, 0, 0, 5, 0, 0, 0, 0, 0, 1, 0.85f, 0.9f, false},
    /*7 herbalist*/ {SK[4], 0xb87a40, 0x5a7a48, 0x5a7a48, 0x4a3426, 0xe8e0c8, 0, 0x3f5c34, 2, 2, 0, 0, 1, 1, 1, 0.9f, 0.92f, true},
    /*8 hunter*/  {SK[1], 0x4a3a28, 0x6a5238, 0x4a4034, 0x2e2218, 0x8a7048, 0, 0x4e3e2a, 1, 2, 1, 0, 0, 0, 1, 1.02f, 1.05f, false},
    /*9 merchant*/{SK[0], 0x2a2018, 0x6c3a6c, 0x3a3038, 0x2a1c14, 0xd8b048, 0, 0, 1, 8, 1, 0, 0, 0, 1, 1.12f, 1.f, false}};
  if (vi < 10) { s = V[vi]; return; }
  int k = vi - 10, nat = k / 3, tier = k % 3;
  static const uint32_t TAB[3] = {0x2f5aa8, 0xa8302a, 0x3a8a48}, ACC[3] = {0xe8d28a, 0x25201e, 0xd8c070};
  static const uint32_t HAIR[3] = {0x3a2a1c, 0x1e1812, 0x7a5a30};
  s = HSpec{SK[(nat + tier * 2) % 5], HAIR[(nat + tier) % 3], (uint32_t)(tier == 0 ? 0x8a7c5e : 0x7e848c), 0x4a4a52, 0x2e2218, ACC[nat], TAB[nat], TAB[nat], 1, tier == 0 ? 4 : tier == 1 ? 5 : 6, tier == 2 ? 0 : 1, tier == 0 ? 1 : tier == 1 ? 2 : 3, 0, 0, 1, tier == 2 ? 1.12f : 1.05f, tier == 2 ? 1.1f : 1.04f, false};
  if (tier == 2) s.beard = nat == 1 ? 1 : 0;
}

void buildHuman(int vi, MB& b) {
  HSpec sp; hSpecOf(vi, sp); const float g = sp.girth, SW = sp.shoulder; const bool F = sp.female; const int ar = sp.armor;
  const Sty skinS = hsty(0, 0.58f), clothS = hsty(1, 0.92f), quiltS = hsty(2, 0.9f), mailS = hsty(3, 0.42f, 0.55f), leathS = hsty(4, 0.6f), plateS = hsty(5, 0.36f, 0.85f), furS = hsty(6, 0.85f), eyeS = hsty(11, 0.25f);
  V3 skinC = C(sp.skin); auto skinSh = [&](float u, float, float, V3 p) -> Shd { float n = 0.94f + 0.12f * vnz(p * 30.f); return Shd{skinC * n, 0.8f + 0.2f * u}; };
  auto kHead = [](float, float y, float, int& a, int& c, float& w) { a = BN_NECK; c = BN_HEAD; w = smx(1.47f, 1.56f, y); };
  auto kTorso = [](float, float y, float, int& a, int& c, float& w) { static const int bn[4] = {1, 2, 3, 4}; static const float yy[4] = {0.96f, 1.068f, 1.185f, 1.312f};
    if (y <= yy[0]) { a = c = 1; w = 0; return; } if (y >= yy[3]) { a = BN_CHEST2; c = BN_NECK; w = smx(1.38f, 1.5f, y); return; }
    for (int i = 0; i < 3; i++) if (y < yy[i + 1]) { a = bn[i]; c = bn[i + 1]; w = (y - yy[i]) / (yy[i + 1] - yy[i]); return; } };
  auto kArm = [](int sd) { return [=](float x, float, float, int& a, int& c, float& w) { float ax = std::fabs(x); int up = sd > 0 ? BN_LUP : BN_RUP; if (ax < 0.3f) { a = BN_CHEST2; c = up; w = smx(0.14f, 0.3f, ax); } else if (ax < 0.72f) { a = up; c = up + 1; w = smx(0.5f, 0.64f, ax); } else { a = up + 1; c = up + 2; w = smx(0.835f, 0.915f, ax); } }; };
  auto kLeg = [](int sd) { return [=](float, float y, float z, int& a, int& c, float& w) { int th = sd > 0 ? BN_LTHIGH : BN_RTHIGH;
    if (y > 0.6f) { a = BN_HIPS; c = th; w = smx(1.f, 0.8f, y); } else if (y > 0.44f) { a = th; c = th + 1; w = smx(0.6f, 0.44f, y); } else if (y > 0.2f) { a = th + 1; c = th + 1; w = 0; }
    else if (z > 0.1f) { a = th + 2; c = th + 3; w = smx(0.1f, 0.17f, z); } else { a = th + 1; c = th + 2; w = smx(0.2f, 0.08f, y); } }; };
  auto kSkirt = [](float x, float y, float, int& a, int& c, float& w) { if (y > 0.96f) { a = BN_CHEST2; c = BN_HIPS; w = smx(1.3f, 0.99f, y); if (y > 1.3f) { a = c = BN_CHEST2; w = 0; } return; } a = BN_HIPS; c = x > 0 ? BN_LTHIGH : BN_RTHIGH; w = smx(0.96f, 0.4f, y) * 0.55f; };
  auto kCape = [](float x, float y, float, int& a, int& c, float& w) { if (y > 1.3f) { a = c = BN_CHEST2; w = 0; } else if (y > 0.95f) { a = BN_CHEST2; c = BN_HIPS; w = smx(1.3f, 0.95f, y); } else { a = BN_HIPS; c = x > 0 ? BN_LTHIGH : BN_RTHIGH; w = smx(0.95f, 0.4f, y) * 0.45f; } };
  auto kHeadOnly = [](float, float, float, int& a, int& c, float& w) { a = c = BN_HEAD; w = 0; };
  auto kHair = [](float, float y, float, int& a, int& c, float& w) { a = BN_HEAD; c = BN_CHEST2; w = smx(1.58f, 1.36f, y); };
  // -------------------------------------------------------------------- head + face
  { size_t s0 = b.v->size(); const Stn k[7] = {{0, 1.545f, 0.03f, 0.045f, 0.055f}, {0, 1.566f, 0.028f, 0.062f, 0.075f}, {0, 1.6f, 0.018f, 0.078f, 0.09f}, {0, 1.65f, 0.008f, 0.086f, 0.098f}, {0, 1.7f, 0.f, 0.082f, 0.096f}, {0, 1.745f, -0.005f, 0.06f, 0.075f}, {0, 1.775f, -0.01f, 0.02f, 0.03f}};
    loft(b, k, 7, 3, 18, skinS, skinSh); skinR(b, s0, kHead);
    s0 = b.v->size(); const Stn nk[3] = {{0, 1.43f, 0.f, 0.064f, 0.066f}, {0, 1.51f, 0.008f, 0.052f, 0.056f}, {0, 1.575f, 0.012f, 0.05f, 0.054f}}; loft(b, nk, 3, 2, 12, skinS, skinSh); skinR(b, s0, kHead);
    s0 = b.v->size(); Sty nose = skinS; blob(b, 0, 1.633f, 0.1f, 0.014f, 0.026f, 0.024f, 8, 5, nose, 3, 0.f); blob(b, 0, 1.617f, 0.108f, 0.014f, 0.01f, 0.012f, 7, 4, nose, 4, 0.f);
    for (int sd = -1; sd <= 1; sd += 2) {
      blob(b, sd * 0.088f, 1.645f, 0.0f, 0.009f, 0.026f, 0.019f, 7, 5, nose, 11 + sd, 0.f);                                           // ear
      Sty ew = eyeS; ew.col = C(0xf0ece4); Sty ir = eyeS; ir.col = C(sp.hair == 0xe2dfd8 ? 0x4a5a6a : 0x2a1c12);
      blob(b, sd * 0.034f, 1.667f, 0.088f, 0.0135f, 0.0105f, 0.009f, 8, 5, ew, 5, 0.f); blob(b, sd * 0.034f, 1.667f, 0.0965f, 0.0072f, 0.0072f, 0.004f, 7, 4, ir, 6, 0.f);
      Sty br = furS; br.col = C(sp.hair); box(b, sd * 0.036f, 1.69f, 0.094f, 0.019f, 0.0035f, 0.006f, br);
    }
    Sty lip = skinS; lip.col = C(0xa05a50); box(b, 0, 1.598f, 0.097f, 0.02f, 0.0032f, 0.005f, lip); skinR(b, s0, kHeadOnly);
    // darken the whole face shell vertex colours a bit: none; the skin shader handles variation
  }
  // -------------------------------------------------------------------- torso / clothing
  uint32_t topC = sp.top; Sty ts = ar == 1 ? quiltS : ar == 2 ? mailS : ar == 3 ? plateS : clothS; if (ar == 2) topC = 0x8e949c; if (ar == 3) topC = 0x9ca2aa;
  auto torso = [&](const Sty& st, uint32_t col, float grow, float hem) {
    Stn k[7] = {{0, hem, 0, 0.19f * g, 0.125f * g}, {0, 0.92f, 0, 0.168f * g, 0.108f * g}, {0, 1.03f, 0, (0.158f - 0.018f * (F ? 1.f : 0.f)) * g, 0.102f * g}, {0, 1.15f, 0, (0.178f - 0.014f * (F ? 1.f : 0.f)) * g, (0.116f + (F ? 0.014f : 0.f)) * g},
      {0, 1.28f, 0, (0.198f - (F ? 0.02f : 0.f)) * g * SW, (0.122f + (F ? 0.012f : 0.f)) * g}, {0, 1.395f, 0, 0.205f * SW * g * (F ? 0.93f : 1.f), 0.108f * g}, {0, 1.468f, 0, 0.094f, 0.086f}};
    for (Stn& q : k) { q.a += grow; q.b += grow; } if (hem < 0.8f) { k[0].a = 0.205f * g + grow; k[0].b = 0.14f * g + grow; }
    size_t s0 = b.v->size(); loft(b, k, 7, 3, 18, st, hShade(col, ar == 3 ? 0.82f : 0.7f, 1.f, ar == 3 ? 0.08f : 0.22f, 18.f)); skinR(b, s0, kTorso); };
  if (sp.dress == 0 || ar > 0) torso(ts, topC, 0.f, ar == 2 ? 0.62f : ar == 3 ? 0.78f : 0.8f); else torso(clothS, sp.top, 0.f, 0.8f);
  if (ar == 3) {   // breastplate ridge + faulds
    size_t s0 = b.v->size(); Stn fk[4] = {{0, 0.7f, 0, 0.215f * g, 0.15f * g}, {0, 0.82f, 0, 0.2f * g, 0.13f * g}, {0, 0.9f, 0, 0.18f * g, 0.115f * g}, {0, 0.95f, 0, 0.17f * g, 0.11f * g}};
    loft(b, fk, 4, 2, 18, plateS, hShade(0x949aa2, 0.6f, 1.f, 0.08f, 10.f)); skinR(b, s0, kTorso);
    s0 = b.v->size(); Sty gor = plateS; gor.col = C(0x9ca2aa); Stn gk[3] = {{0, 1.42f, 0, 0.115f, 0.1f}, {0, 1.46f, 0, 0.105f, 0.095f}, {0, 1.5f, 0, 0.082f, 0.078f}}; loft(b, gk, 3, 2, 14, plateS, hShade(0x9ca2aa, 0.7f, 1.f, 0.08f, 10.f)); skinR(b, s0, kTorso); }
  if (ar == 2) { size_t s0 = b.v->size(); Stn mk[4] = {{0, 1.455f, 0, 0.098f, 0.09f}, {0, 1.5f, 0.004f, 0.082f, 0.082f}, {0, 1.55f, 0.01f, 0.07f, 0.074f}, {0, 1.575f, 0.012f, 0.066f, 0.07f}}; loft(b, mk, 4, 2, 14, mailS, hShade(0x8e949c, 0.7f, 1.f, 0.2f, 18.f)); skinR(b, s0, kHead); }
  // skirt / dress / robe
  if (sp.dress > 0 && ar == 0) {
    float hem = sp.dress == 2 ? 0.1f : 0.3f; size_t s0 = b.v->size(); float ra = sp.dress == 2 ? 0.27f : 0.38f, rb = sp.dress == 2 ? 0.24f : 0.34f;
    Stn dk[5] = {{0, 1.04f, 0, 0.162f * g, 0.108f * g}, {0, 0.95f, 0, 0.19f * g, 0.135f * g}, {0, 0.75f, 0, ra * 0.68f, rb * 0.66f}, {0, 0.5f, 0, ra * 0.88f, rb * 0.88f}, {0, hem, 0, ra, rb}};
    loft(b, dk, 5, 3, 22, clothS, hShade(sp.top, 0.55f, 1.f, 0.24f, 16.f)); skinR(b, s0, kSkirt);
    s0 = b.v->size(); Stn hk2[2] = {{0, hem + 0.012f, 0, ra * 1.005f, rb * 1.005f}, {0, hem - 0.01f, 0, ra * 1.02f, rb * 1.02f}}; loft(b, hk2, 2, 1, 22, clothS, hShade(sp.accent == 0xe8e0d0 ? 0x8a7a5a : 0x2a2018, 0.5f, 0.5f, 0.1f, 8.f)); skinR(b, s0, kSkirt);
  }
  // belt
  { size_t s0 = b.v->size(); float wa = 0.168f * g, wb = 0.108f * g; Stn bk[2] = {{0, 0.945f, 0, wa + 0.012f, wb + 0.012f}, {0, 0.99f, 0, wa + 0.012f, wb + 0.012f}}; Sty bs = leathS; loft(b, bk, 2, 1, 16, bs, hShade(vi >= 10 ? 0x3a2a1c : 0x4a3220, 0.8f, 0.8f, 0.1f, 20.f));
    Sty bu = metal(0xc8a850); box(b, 0, 0.968f, wb + 0.014f, 0.02f, 0.02f, 0.004f, bu); if (vi == 0 || vi == 8 || vi == 9 || vi == 7) { box(b, F ? 0.17f : 0.19f * g, 0.9f, 0.02f, 0.025f, 0.04f, 0.045f, leathS); }
    skinR(b, s0, kTorso); }
  // tabard (nation colour) + cape
  if (vi >= 10) {
    Sty tb = clothS; size_t s0 = b.v->size(); V3 tc = C(sp.tab); V3 ac = C(sp.accent);
    const float zf = 0.132f * g, zb = -0.132f * g; (void)tc;
    auto panel = [&](float zc, float nz) { Stn tk[5] = {{0, 1.4f, zc, 0.14f * g, 0.012f}, {0, 1.2f, zc + nz * 0.004f, 0.13f * g, 0.01f}, {0, 1.0f, zc + nz * 0.01f, 0.13f * g, 0.01f}, {0, 0.78f, zc + nz * 0.02f, 0.145f * g, 0.01f}, {0, 0.52f, zc + nz * 0.03f, 0.15f * g, 0.01f}};
      loft(b, tk, 5, 2, 8, tb, hShade(sp.tab, 0.65f, 1.f, 0.25f, 14.f)); };
    panel(zf, 1.f); panel(zb, -1.f); skinR(b, s0, kSkirt);
    s0 = b.v->size(); Sty em = clothS; em.col = ac; box(b, 0, 1.12f, zf + 0.014f, 0.022f, 0.2f, 0.004f, em); box(b, 0, 1.2f, zf + 0.014f, 0.085f, 0.022f, 0.004f, em); skinR(b, s0, kTorso);
    if (sp.cape && vi % 3 == 1) { s0 = b.v->size(); Stn ck[5] = {{0, 1.46f, -0.1f, 0.2f, 0.03f}, {0, 1.3f, -0.15f, 0.24f, 0.03f}, {0, 1.0f, -0.19f, 0.27f, 0.03f}, {0, 0.7f, -0.22f, 0.3f, 0.03f}, {0, 0.48f, -0.23f, 0.31f, 0.03f}}; loft(b, ck, 5, 3, 14, clothS, hShade(sp.cape, 0.55f, 1.f, 0.25f, 12.f)); skinR(b, s0, kCape); }
  }
  if (vi % 3 == 2 && vi >= 10) { size_t s0 = b.v->size(); Stn ck[5] = {{0, 1.46f, -0.11f, 0.205f, 0.028f}, {0, 1.3f, -0.16f, 0.25f, 0.03f}, {0, 1.0f, -0.2f, 0.285f, 0.03f}, {0, 0.7f, -0.23f, 0.31f, 0.03f}, {0, 0.42f, -0.24f, 0.33f, 0.03f}}; loft(b, ck, 5, 3, 14, clothS, hShade(sp.cape, 0.5f, 1.f, 0.25f, 12.f)); skinR(b, s0, kCape); }
  if (sp.cape && vi < 10) {   // hooded cloak on villagers: mantle over shoulders + back drape
    size_t s0 = b.v->size(); Stn ck[4] = {{0, 1.47f, 0.f, 0.21f, 0.13f}, {0, 1.34f, -0.02f, 0.245f, 0.15f}, {0, 1.1f, -0.06f, 0.26f, 0.17f}, {0, 0.86f, -0.1f, 0.28f, 0.19f}}; loft(b, ck, 4, 3, 18, clothS, hShade(sp.cape, 0.55f, 1.f, 0.26f, 12.f)); skinR(b, s0, kCape); }
  // -------------------------------------------------------------------- arms
  for (int sd = -1; sd <= 1; sd += 2) {
    auto az = [](float ax) { return 0.056f + (ax - 0.25f) / 0.625f * 0.065f; };
    const float ay = 1.436f; auto A = kArm(sd); float ag = std::sqrt(g);
    const bool longS = sp.sleeve == 1 || ar > 0; Sty us = ar == 1 ? quiltS : ar == 2 ? mailS : ar == 3 ? plateS : clothS; uint32_t uc = ar == 2 ? 0x8e949c : ar == 3 ? 0x9ca2aa : sp.top; if (ar == 1) uc = sp.top;
    size_t s0 = b.v->size(); Stn up[5] = {{sd * 0.18f, ay - 0.004f, az(0.18f), 0.074f * ag, 0.074f * ag}, {sd * 0.25f, ay, az(0.25f), 0.062f * ag, 0.062f * ag}, {sd * 0.36f, ay, az(0.36f), 0.054f * ag, 0.054f * ag}, {sd * 0.47f, ay, az(0.47f), 0.049f * ag, 0.049f * ag}, {sd * 0.58f, ay, az(0.58f), 0.047f * ag, 0.047f * ag}};
    loft(b, up, 5, 3, 12, us, hShade(uc, 0.7f, 1.f, ar == 3 ? 0.08f : 0.2f, 18.f)); skinR(b, s0, A);
    s0 = b.v->size(); Stn fo[4] = {{sd * 0.56f, ay, az(0.56f), 0.047f * ag, 0.047f * ag}, {sd * 0.68f, ay, az(0.68f), 0.041f * ag, 0.041f * ag}, {sd * 0.81f, ay, az(0.81f), 0.034f * ag, 0.034f * ag}, {sd * 0.875f, ay, az(0.875f), 0.033f * ag, 0.033f * ag}};
    if (longS) { Sty fs = ar == 3 ? plateS : ar == 2 ? mailS : ar == 1 ? quiltS : clothS; loft(b, fo, 4, 3, 12, fs, hShade(uc, 0.7f, 1.f, ar == 3 ? 0.08f : 0.2f, 18.f)); skinR(b, s0, A); }
    else { loft(b, fo, 4, 3, 12, skinS, skinSh); skinR(b, s0, A); }
    if (!longS) { s0 = b.v->size(); Stn rl[2] = {{sd * 0.52f, ay, az(0.52f), 0.052f * ag, 0.052f * ag}, {sd * 0.585f, ay, az(0.585f), 0.05f * ag, 0.05f * ag}}; loft(b, rl, 2, 1, 12, clothS, hShade(uc, 0.7f, 0.9f, 0.2f, 18.f)); skinR(b, s0, A); }
    // cuff + glove / hand
    bool glove = ar > 0 || vi == 8; s0 = b.v->size(); Sty hs = glove ? leathS : skinS; uint32_t hc = glove ? (ar == 3 ? 0x6a6e74 : 0x4a3a28) : sp.skin;
    Sty hsx = hs; hsx.col = C(hc); float hx = sd * 0.935f, hz = az(0.935f);
    blob(b, hx, ay - 0.002f, hz, 0.052f, 0.017f, 0.04f, 9, 5, hsx, 31 + sd, 0.f);                                      // palm
    for (int f = 0; f < 4; f++) { float fz = hz - 0.026f + f * 0.0175f; float fl = f == 1 || f == 2 ? 0.05f : 0.044f; V3 p0{sd * 0.965f, ay - 0.004f, fz}, p1{sd * (0.965f + fl), ay - 0.01f, fz + 0.001f}; tube(b, p0, p1, 0.008f, 0.006f, 5, hsx); }
    tube(b, V3{sd * 0.91f, ay - 0.006f, hz + 0.034f}, V3{sd * 0.96f, ay - 0.012f, hz + 0.06f}, 0.0095f, 0.0072f, 5, hsx);   // thumb
    skinR(b, s0, kArm(sd));
    s0 = b.v->size(); blob(b, sd * 0.245f, ay + 0.002f, az(0.245f), ar == 3 ? 0.095f : 0.068f * ag, ar == 3 ? 0.078f : 0.066f * ag, ar == 3 ? 0.085f : 0.07f * ag, 10, 6, ar == 3 ? plateS : ar == 2 ? mailS : ar == 1 ? quiltS : clothS, 41 + sd, 0.f);
    { Sty ps = ar == 3 ? plateS : ar == 2 ? mailS : ar == 1 ? quiltS : clothS; (void)ps; }
    skinR(b, s0, A);
    if (ar == 3) { s0 = b.v->size(); Sty vb = plateS; vb.col = C(0x9ca2aa); Stn vk[3] = {{sd * 0.6f, ay, az(0.6f), 0.056f, 0.056f}, {sd * 0.72f, ay, az(0.72f), 0.048f, 0.048f}, {sd * 0.86f, ay, az(0.86f), 0.04f, 0.04f}}; loft(b, vk, 3, 2, 10, plateS, hShade(0x9ca2aa, 0.7f, 1.f, 0.08f, 10.f)); skinR(b, s0, A); }
  }
  // -------------------------------------------------------------------- legs + boots
  for (int sd = -1; sd <= 1; sd += 2) {
    auto lx = [&](float y) { return sd * (0.105f + (y - 0.127f) / 0.815f * 0.025f); }; auto Lg = kLeg(sd); float lg = std::sqrt(g);
    bool plate = ar == 3, mail = ar == 2; uint32_t lc = plate ? 0x90969e : sp.bot; if (mail) lc = sp.bot;
    size_t s0 = b.v->size(); Stn lk[7] = {{lx(0.98f), 0.98f, 0.01f, 0.108f * lg, 0.114f * lg}, {lx(0.8f), 0.8f, 0.01f, 0.096f * lg, 0.1f * lg}, {lx(0.62f), 0.62f, 0.008f, 0.075f * lg, 0.08f * lg}, {lx(0.52f), 0.52f, 0.004f, 0.066f * lg, 0.07f * lg}, {lx(0.4f), 0.4f, 0.0f, 0.066f * lg, 0.072f * lg}, {lx(0.26f), 0.26f, 0.0f, 0.05f * lg, 0.054f * lg}, {lx(0.14f), 0.14f, 0.0f, 0.047f * lg, 0.05f * lg}};
    loft(b, lk, 7, 3, 14, plate ? clothS : clothS, hShade(lc, 0.6f, 0.9f, 0.2f, 16.f)); skinR(b, s0, Lg);
    if (sp.dress == 0 || ar > 0) {
      if (plate) { s0 = b.v->size(); Stn gk[4] = {{lx(0.5f), 0.5f, 0.012f, 0.072f, 0.076f}, {lx(0.38f), 0.38f, 0.004f, 0.072f, 0.078f}, {lx(0.22f), 0.22f, 0.0f, 0.056f, 0.06f}, {lx(0.14f), 0.14f, 0.0f, 0.05f, 0.054f}}; loft(b, gk, 4, 2, 12, plateS, hShade(0x9ca2aa, 0.7f, 1.f, 0.08f, 10.f)); skinR(b, s0, Lg);
        s0 = b.v->size(); blob(b, lx(0.52f), 0.52f, 0.05f, 0.058f, 0.05f, 0.05f, 9, 6, plateS, 51 + sd, 0.f); skinR(b, s0, Lg); }
    }
    // boot shaft
    s0 = b.v->size(); Stn bk[4] = {{lx(0.36f), 0.36f, 0.0f, 0.058f * lg, 0.062f * lg}, {lx(0.26f), 0.26f, 0.0f, 0.054f * lg, 0.058f * lg}, {lx(0.16f), 0.16f, 0.0f, 0.052f * lg, 0.056f * lg}, {lx(0.1f), 0.1f, 0.0f, 0.052f * lg, 0.058f * lg}};
    if (sp.dress != 2) { loft(b, bk, 4, 2, 12, leathS, hShade(sp.boot, 0.6f, 0.9f, 0.18f, 22.f)); skinR(b, s0, Lg); }
    s0 = b.v->size(); float fx = lx(0.1f); Stn fk[5] = {{fx, 0.08f, -0.075f, 0.05f * lg, 0.048f}, {fx, 0.075f, 0.0f, 0.054f * lg, 0.055f}, {fx, 0.058f, 0.1f, 0.052f * lg, 0.044f}, {fx, 0.04f, 0.18f, 0.042f * lg, 0.032f}, {fx, 0.032f, 0.222f, 0.022f * lg, 0.02f}};
    // path runs along +Z: S = x axis, V = +Y;  shade: dark sole where sT < -0.5
    V3 bc = C(sp.boot), sole = C(0x1a120c); loft(b, fk, 5, 3, 12, leathS, [&](float u, float sT, float, V3 p) -> Shd { float k2 = smx(-0.35f, -0.7f, sT); return Shd{(bc * (0.9f + 0.2f * vnz(p * 40.f))) * (1.f - k2) + sole * k2, 0.6f + 0.4f * u}; }); skinR(b, s0, Lg);
  }
  // -------------------------------------------------------------------- hair / beard
  { Sty hr = furS; uint32_t hcol = sp.hair; V3 hc = C(hcol) * 1.75f;
    auto hairShade = [&](V3 p, V3) -> Shd { float n = 0.75f + 0.5f * vnz(V3{p.x * 40.f, p.y * 6.f, p.z * 40.f}); return Shd{hc * n, 0.8f}; };
    size_t s0 = b.v->size();
    auto cap = [&](float rx, float ry, float rz, float cy, float cz, float hairlineF, float backY, uint32_t seed, float jit) {
      shellP(b, V3{0, cy, cz}, V3{rx, ry, rz}, 16, 9, hr, [&](V3 c) { if (std::fabs(c.x) > 0.072f && std::fabs(c.z) < 0.035f && c.y < 1.69f) return false; return c.z > 0.02f ? c.y > hairlineF : c.y > backY; }, hairShade, jit, seed); };
    switch (sp.hairSt) {
      case 1: cap(0.095f, 0.1f, 0.103f, 1.69f, -0.004f, 1.716f, 1.62f, 2, 0.08f); break;
      case 2: cap(0.096f, 0.104f, 0.105f, 1.69f, -0.006f, 1.716f, 1.6f, 3, 0.06f);
        shellP(b, V3{0, 1.52f, -0.062f}, V3{0.098f, 0.19f, 0.058f}, 14, 8, hr, [&](V3 c) { return c.z < -0.03f && c.y < 1.64f; }, hairShade, 0.06f, 5); break;
      case 3: cap(0.095f, 0.1f, 0.103f, 1.69f, -0.004f, 1.712f, 1.64f, 6, 0.06f); blob(b, 0, 1.725f, -0.09f, 0.048f, 0.045f, 0.045f, 9, 6, [&] { Sty s = hr; s.col = hc; return s; }(), 7, 0.1f); break;
      case 4: cap(0.095f, 0.1f, 0.103f, 1.69f, -0.004f, 1.712f, 1.6f, 8, 0.06f); { Sty s = hr; s.col = hc; V3 p[5] = {{0, 1.64f, -0.098f}, {0, 1.54f, -0.125f}, {0, 1.42f, -0.14f}, {0, 1.3f, -0.138f}, {0, 1.2f, -0.13f}}; for (int i = 0; i < 4; i++) tube(b, p[i], p[i + 1], 0.034f - i * 0.0035f, 0.03f - i * 0.0035f, 7, s); blob(b, 0, 1.19f, -0.13f, 0.022f, 0.03f, 0.022f, 7, 4, s, 9, 0.f); } break;
      case 5: cap(0.1f, 0.11f, 0.108f, 1.69f, 0.f, 1.704f, 1.62f, 10, 0.2f); break;
      default: break; }
    skinR(b, s0, kHeadOnly);
    if (sp.hairSt == 2 || sp.hairSt == 4) skinR(b, s0, kHair);
    if (sp.beard) { s0 = b.v->size(); V3 bcn = hc * (sp.beard == 2 ? 1.f : 0.55f);
      auto bsh = [&](V3 p, V3) -> Shd { float n = 0.75f + 0.5f * vnz(V3{p.x * 40.f, p.y * 8.f, p.z * 40.f}); return Shd{bcn * n, 0.8f}; };
      if (sp.beard == 1) { shellP(b, V3{0, 1.585f, 0.034f}, V3{0.082f, 0.06f, 0.078f}, 14, 7, hr, [&](V3 c) { return c.z > 0.0f && c.y < 1.612f && c.y > 1.55f && !(std::fabs(c.x) < 0.03f && c.y > 1.596f && c.z > 0.07f); }, bsh, 0.05f, 12); box(b, 0, 1.612f, 0.1f, 0.028f, 0.007f, 0.009f, [&] { Sty s = hr; s.col = bcn; return s; }()); }
      else { shellP(b, V3{0, 1.58f, 0.03f}, V3{0.088f, 0.095f, 0.085f}, 14, 8, hr, [&](V3 c) { return c.z > -0.01f && c.y < 1.628f; }, bsh, 0.08f, 13); shellP(b, V3{0, 1.47f, 0.07f}, V3{0.07f, 0.12f, 0.045f}, 12, 7, hr, [&](V3 c) { return c.z > 0.02f; }, bsh, 0.08f, 14); }
      skinR(b, s0, kHeadOnly); }
  }
  // -------------------------------------------------------------------- headwear
  { size_t s0 = b.v->size(); const float cy = 1.69f;
    auto domeS = [&](const Sty& st, uint32_t col, float rx, float ry, float rz, float cz, float cut, float nz, float fr, auto keep, uint32_t seed) { shellP(b, V3{0, cy, cz}, V3{rx, ry, rz}, 18, 10, st, keep, hShadeP(col, 0.85f, nz, fr), 0.f, seed); (void)cut; };
    switch (sp.hat) {
      case 1: { Stn hk[8] = {{0, 1.698f, 0.f, 0.02f, 0.02f}, {0, 1.704f, 0.f, 0.17f, 0.17f}, {0, 1.708f, 0.f, 0.262f, 0.262f}, {0, 1.713f, 0.f, 0.27f, 0.27f}, {0, 1.72f, 0.f, 0.21f, 0.21f}, {0, 1.74f, 0.f, 0.105f, 0.108f}, {0, 1.805f, 0.f, 0.098f, 0.1f}, {0, 1.815f, 0.f, 0.05f, 0.05f}};
        loft(b, hk, 8, 2, 22, hsty(7, 0.9f), hShade(0xc8a860, 0.7f, 1.f, 0.35f, 22.f)); Stn rb[2] = {{0, 1.742f, 0.f, 0.107f, 0.11f}, {0, 1.756f, 0.f, 0.107f, 0.11f}}; loft(b, rb, 2, 1, 20, clothS, hShade(0x7a3a2a, 0.8f, 0.8f, 0.1f, 8.f)); } break;
      case 2: { V3 hc2 = C(sp.cape ? sp.cape : sp.top);
        shellP(b, V3{0, 1.67f, -0.012f}, V3{0.112f, 0.125f, 0.126f}, 18, 10, clothS, [&](V3 c) { return !(c.z > 0.055f && std::fabs(c.x) < 0.082f && c.y < 1.745f && c.y > 1.57f); }, hShadeP(sp.cape ? sp.cape : sp.top, 0.7f, 0.25f, 16.f), 0.f, 21);
        Stn cw[4] = {{0, 1.37f, -0.01f, 0.22f, 0.16f}, {0, 1.47f, -0.02f, 0.15f, 0.13f}, {0, 1.55f, -0.025f, 0.118f, 0.116f}, {0, 1.6f, -0.02f, 0.114f, 0.116f}}; (void)hc2;
        loft(b, cw, 4, 3, 18, clothS, hShade(sp.cape ? sp.cape : sp.top, 0.5f, 1.f, 0.25f, 16.f)); } break;
      case 3: domeS(clothS, sp.top, 0.1f, 0.1f, 0.108f, -0.004f, 0.f, 0.2f, 16.f, [](V3 c) { return c.y > 1.715f; }, 22); break;
      case 4: { shellP(b, V3{0, 1.685f, 0}, V3{0.106f, 0.094f, 0.11f}, 20, 10, plateS, [](V3 c) { return c.y > 1.675f; }, hShadeP(0x8e949c, 0.85f, 0.1f, 9.f), 0.f, 23);
        Stn br[3] = {{0, 1.672f, 0.f, 0.108f, 0.112f}, {0, 1.68f, 0.f, 0.15f, 0.15f}, {0, 1.684f, 0.f, 0.152f, 0.152f}}; loft(b, br, 3, 1, 22, plateS, hShade(0x8e949c, 0.7f, 1.f, 0.1f, 9.f)); } break;
      case 5: { V3 mc = C(0x8e949c);
        shellP(b, V3{0, 1.655f, -0.004f}, V3{0.1f, 0.118f, 0.108f}, 18, 10, mailS, [](V3 c) { return !(c.z > 0.045f && std::fabs(c.x) < 0.074f && c.y < 1.748f && c.y > 1.585f); }, hShadeP(0x8e949c, 0.8f, 0.2f, 18.f), 0.f, 24); (void)mc;
        Stn cl[4] = {{0, 1.4f, 0.f, 0.17f, 0.12f}, {0, 1.48f, 0.f, 0.11f, 0.1f}, {0, 1.56f, 0.005f, 0.09f, 0.095f}, {0, 1.6f, 0.0f, 0.1f, 0.105f}}; loft(b, cl, 4, 2, 16, mailS, hShade(0x8e949c, 0.6f, 1.f, 0.2f, 18.f)); } break;
      case 6: { shellP(b, V3{0, 1.665f, -0.005f}, V3{0.102f, 0.13f, 0.108f}, 18, 10, plateS, [](V3 c) { return !(c.z > 0.04f && std::fabs(c.x) < 0.07f && c.y < 1.752f && c.y > 1.585f) && c.y > 1.58f; }, hShadeP(0x9ca2aa, 0.85f, 0.08f, 9.f), 0.f, 25);
        box(b, 0, 1.655f, 0.108f, 0.008f, 0.06f, 0.006f, plateS);   // nasal
        Stn cl[4] = {{0, 1.4f, 0.f, 0.17f, 0.125f}, {0, 1.48f, 0.f, 0.112f, 0.1f}, {0, 1.55f, 0.005f, 0.095f, 0.098f}, {0, 1.585f, 0.0f, 0.108f, 0.108f}}; loft(b, cl, 4, 2, 16, mailS, hShade(0x8e949c, 0.6f, 1.f, 0.2f, 18.f)); } break;
      case 7: { V3 sc = C(sp.accent); (void)sc; shellP(b, V3{0, 1.683f, -0.006f}, V3{0.103f, 0.105f, 0.109f}, 18, 10, clothS, [](V3 c) { return c.y > 1.688f - 0.06f * std::max(0.f, c.z) / 0.1f && !(c.z > 0.03f && c.y < 1.7f); }, hShadeP(sp.accent, 0.8f, 0.2f, 16.f), 0.f, 26); blob(b, 0, 1.69f, -0.115f, 0.04f, 0.04f, 0.035f, 8, 5, clothS, 27, 0.f); } break;
      case 8: { shellP(b, V3{0, 1.69f, -0.004f}, V3{0.1f, 0.082f, 0.108f}, 18, 9, clothS, [](V3 c) { return c.y > 1.69f; }, hShadeP(sp.top == 0xe6e2d6 ? 0xf0ece0 : sp.accent == 0xd8b048 ? 0x4a2a4a : sp.top, 0.8f, 0.2f, 16.f), 0.f, 28);
        Stn pk[2] = {{0, 1.703f, 0.115f, 0.085f, 0.006f}, {0, 1.704f, 0.14f, 0.075f, 0.005f}}; loft(b, pk, 2, 1, 10, clothS, hShade(sp.top == 0xe6e2d6 ? 0xf0ece0 : sp.accent == 0xd8b048 ? 0x4a2a4a : sp.top, 0.7f, 0.9f, 0.1f, 8.f)); } break;
      default: break; }
    skinR(b, s0, kHeadOnly); if (sp.hat == 2) { /* cowl part rides on neck/chest blend */ skinR(b, s0, kHeadOnly); }
    if (sp.hat == 5 || sp.hat == 6 || sp.hat == 2) skinR(b, s0, kHair);   // collars follow the head -> chest blend
    if (sp.hat >= 4 && sp.hat <= 6) { }
  }
}
