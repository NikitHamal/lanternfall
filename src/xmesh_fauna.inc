// v2.6 anatomy-driven fauna (included inside the anonymous namespace of xmesh.cpp, after xtree.inc).
// Smooth lofted bodies (elliptical cross-sections along a Catmull-Rom spine) replace the old blob + stick animals.
// Parts are separate instanced meshes so xfauna.inc can animate them: body, neck+head (pivot at the withers), tail (pivot at the dock),
// two-bone legs (unit length, origin at the joint, hanging along -Y), saddle.  +Z forward, +Y up, origin on the ground under the body.
// Vertex colours carry countershading, points (dark legs, muzzle, mane, hooves) and cow patches; the instance tint multiplies them (coat colour).
// Vertex alpha = ambient occlusion; Sty.glow of surf 13 (fur) = dapple strength read by the fur branch of the prop shader.
struct Stn { float x, y, z, a, b; };   // station: centre, lateral half-width a, half-height b (perpendicular to the path)
struct Shd { V3 col; float ao; };
float hsh3(int x, int y, int z) { uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u; h = (h ^ (h >> 13)) * 1274126177u; h ^= h >> 16; return (h & 0xffff) / 65535.f; }
float vnz(V3 p) { int ix = (int)std::floor(p.x), iy = (int)std::floor(p.y), iz = (int)std::floor(p.z); float fx = p.x - ix, fy = p.y - iy, fz = p.z - iz; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
  auto L = [](float a, float b2, float t) { return a + (b2 - a) * t; };
  return L(L(L(hsh3(ix, iy, iz), hsh3(ix + 1, iy, iz), fx), L(hsh3(ix, iy + 1, iz), hsh3(ix + 1, iy + 1, iz), fx), fy), L(L(hsh3(ix, iy, iz + 1), hsh3(ix + 1, iy, iz + 1), fx), L(hsh3(ix, iy + 1, iz + 1), hsh3(ix + 1, iy + 1, iz + 1), fx), fy), fz); }
inline V3 hv(V3 a, V3 c) { return V3{a.x * c.x, a.y * c.y, a.z * c.z}; }
inline float crf(float p0, float p1, float p2, float p3, float t) { return 0.5f * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t); }
template <class F>
void loft(MB& b, const Stn* k, int n, int sub, int seg, const Sty& s, F shade) {
  int N = (n - 1) * sub + 1; std::vector<Stn> st((size_t)N); std::vector<V3> T((size_t)N), S((size_t)N), Vv((size_t)N), Cc((size_t)N);
  for (int i = 0; i < n - 1; i++) for (int j = 0; j < sub; j++) { float t = (float)j / sub; const Stn& p0 = k[i > 0 ? i - 1 : 0]; const Stn& p1 = k[i]; const Stn& p2 = k[i + 1]; const Stn& p3 = k[i + 2 < n ? i + 2 : n - 1]; Stn& o = st[(size_t)(i * sub + j)];
    o.x = crf(p0.x, p1.x, p2.x, p3.x, t); o.y = crf(p0.y, p1.y, p2.y, p3.y, t); o.z = crf(p0.z, p1.z, p2.z, p3.z, t); o.a = std::max(0.002f, crf(p0.a, p1.a, p2.a, p3.a, t)); o.b = std::max(0.002f, crf(p0.b, p1.b, p2.b, p3.b, t)); }
  st[(size_t)(N - 1)] = k[n - 1];
  for (int i = 0; i < N; i++) { Cc[(size_t)i] = V3{st[(size_t)i].x, st[(size_t)i].y, st[(size_t)i].z}; }
  for (int i = 0; i < N; i++) { V3 d = Cc[(size_t)std::min(N - 1, i + 1)] - Cc[(size_t)std::max(0, i - 1)]; float l = len(d); V3 t = l > 1e-6f ? d * (1.f / l) : V3{0, 0, 1}; T[(size_t)i] = t;
    V3 x{1, 0, 0}; V3 sv = x - t * dot(x, t); if (len(sv) < 1e-3f) sv = V3{0, 0, 1} - t * t.z; sv = norm(sv); S[(size_t)i] = sv; Vv[(size_t)i] = cross(t, sv); }
  int W = seg + 1; std::vector<V3> P((size_t)(N * W)), Nn((size_t)(N * W));
  for (int i = 0; i < N; i++) for (int q = 0; q < W; q++) { float th = TAU * (q % seg) / seg; P[(size_t)(i * W + q)] = Cc[(size_t)i] + S[(size_t)i] * (st[(size_t)i].a * std::cos(th)) + Vv[(size_t)i] * (st[(size_t)i].b * std::sin(th)); }
  for (int i = 0; i < N; i++) for (int q = 0; q < W; q++) { int qa = (q + seg - 1) % seg, qb = (q + 1) % seg; if (q == seg) { qa = seg - 1; qb = 1 % seg; }
    V3 dth = P[(size_t)(i * W + qb)] - P[(size_t)(i * W + qa)]; V3 dt = P[(size_t)(std::min(N - 1, i + 1) * W + q)] - P[(size_t)(std::max(0, i - 1) * W + q)]; V3 nn = cross(dth, dt); float l = len(nn); V3 out = P[(size_t)(i * W + q)] - Cc[(size_t)i];
    if (l < 1e-10f || len(out) < 1e-4f) nn = i == 0 ? T[(size_t)i] * -1.f : T[(size_t)i]; else { nn = nn * (1.f / l); if (dot(nn, out) < 0) nn = nn * -1.f; } Nn[(size_t)(i * W + q)] = nn; }
  int base = (int)b.v->size(); float run = 0;
  for (int i = 0; i < N; i++) { if (i > 0) run += len(Cc[(size_t)i] - Cc[(size_t)(i - 1)]); float u = (float)i / (N - 1);
    for (int q = 0; q < W; q++) { float th = TAU * (q % seg) / seg; Shd sh = shade(u, std::sin(th), std::cos(th), P[(size_t)(i * W + q)]); vert(b, P[(size_t)(i * W + q)], Nn[(size_t)(i * W + q)], sh.col, sh.ao, (float)q / seg, run, s.surf, s.rough, s.metal, s.glow); } }
  for (int i = 0; i < N - 1; i++) for (int q = 0; q < seg; q++) { int a00 = base + i * W + q, a01 = a00 + 1, a10 = a00 + W, a11 = a10 + 1; V3 g = cross(P[(size_t)(i * W + q + 1)] - P[(size_t)(i * W + q)], P[(size_t)((i + 1) * W + q)] - P[(size_t)(i * W + q)]);
    if (dot(g, Nn[(size_t)(i * W + q)]) >= 0) { tri(b, a00, a01, a10); tri(b, a01, a11, a10); } else { tri(b, a00, a10, a01); tri(b, a01, a10, a11); } }
}
Sty FURS(float rough = 0.84f, float dapple = 0.f) { return {V3{1, 1, 1}, 13, rough, 0.f, dapple, 1.f, 1.f}; }
Sty HOOF() { return {V3{1, 1, 1}, 11, 0.5f, 0.f, 0.f, 1.f, 1.f}; }
// coat colour with countershading (belly lighter, back darker) and underside ambient occlusion
Shd coat(V3 back, V3 belly, float sT, float aoLow = 0.55f) { float k = smooth(-0.75f, 0.55f, sT); return {belly + (back - belly) * k, aoLow + (1.f - aoLow) * smooth(-1.f, 0.35f, sT)}; }
V3 gry(float g) { return V3{g, g, g}; }
void eye(MB& b, float x, float y, float z, float r) { Sty e = {gry(0.04f), 11, 0.12f, 0.f, 0.f, 1.f, 1.f}; Sty hl = {gry(0.55f), 11, 0.1f, 0.f, 0.f, 1.f, 1.f}; blob(b, x, y, z, r, r * 1.05f, r * 0.9f, 8, 5, e, 5, 0.f); blob(b, x + (x > 0 ? 0.28f : -0.28f) * r, y + r * 0.35f, z + r * 0.35f, r * 0.28f, r * 0.28f, r * 0.2f, 5, 3, hl, 6, 0.f); }
void hairTuft(MB& b, V3 p0, V3 p1, V3 p2, float r0, float r1, const Sty& s) { tube(b, p0, p1, r0, (r0 + r1) * 0.55f, 5, s); tube(b, p1, p2, (r0 + r1) * 0.55f, r1, 5, s); }

// ---------------------------------------------------------------- legs (unit length, origin at the top joint)
void legMesh(int kind, MB& b) {
  Sty fs = FURS(0.84f), hf = HOOF();
  auto shadeUp = [&](float u, float sT, float, V3) -> Shd { float dk = 1.f - 0.28f * smooth(0.5f, 1.f, u); Shd r = coat(gry(dk), gry(dk * 0.92f), sT, 0.5f); r.ao *= 0.7f + 0.3f * smooth(0.f, 0.35f, u); return r; };
  if (kind == xw::K_AN_UPF) { const Stn k[] = {{0, 0, 0, 0.52f, 0.5f}, {0, -0.14f, 0, 0.5f, 0.52f}, {0, -0.45f, 0, 0.36f, 0.4f}, {0, -0.82f, 0, 0.3f, 0.32f}, {0, -0.95f, 0, 0.32f, 0.34f}, {0, -1.0f, 0, 0.1f, 0.1f}}; loft(b, k, 6, 3, 9, fs, shadeUp); return; }
  if (kind == xw::K_AN_UPH) { const Stn k[] = {{0, 0, 0, 0.55f, 0.62f}, {0, -0.2f, 0, 0.52f, 0.66f}, {0, -0.55f, 0, 0.32f, 0.4f}, {0, -0.86f, 0, 0.24f, 0.3f}, {0, -0.96f, 0, 0.32f, 0.36f}, {0, -1.0f, 0, 0.1f, 0.1f}}; loft(b, k, 6, 3, 9, fs, shadeUp); return; }
  if (kind == xw::K_AN_LOH) {   // knee/hock -> cannon -> fetlock -> pastern -> hoof
    const Stn k[] = {{0, 0, 0, 0.3f, 0.32f}, {0, -0.07f, 0, 0.27f, 0.3f}, {0, -0.45f, 0, 0.2f, 0.2f}, {0, -0.78f, 0.f, 0.21f, 0.22f}, {0, -0.84f, 0.01f, 0.25f, 0.26f}, {0, -0.89f, 0.02f, 0.19f, 0.2f}, {0, -0.92f, 0.03f, 0.2f, 0.22f}};
    auto sh = [&](float u, float sT, float, V3) -> Shd { float dk = 0.62f - 0.4f * smooth(0.f, 0.3f, u); Shd r = coat(gry(dk), gry(dk * 0.9f), sT, 0.55f); r.col = r.col * (1.f + 0.2f * std::sin(u * 40.f) * 0.f); return r; };
    loft(b, k, 7, 3, 9, fs, sh);
    const Stn hfk[] = {{0, -0.915f, 0.03f, 0.2f, 0.22f}, {0, -0.945f, 0.045f, 0.25f, 0.29f}, {0, -0.985f, 0.062f, 0.3f, 0.35f}, {0, -1.0f, 0.065f, 0.27f, 0.32f}, {0, -1.0f, 0.065f, 0.02f, 0.02f}};
    loft(b, hfk, 5, 2, 10, hf, [&](float u, float sT, float, V3) -> Shd { return {gry(0.2f - 0.1f * u), 0.7f + 0.3f * std::max(0.f, sT)}; }); return; }
  if (kind == xw::K_AN_LOP) {   // paw
    const Stn k[] = {{0, 0, 0, 0.3f, 0.3f}, {0, -0.06f, 0, 0.27f, 0.28f}, {0, -0.55f, 0, 0.2f, 0.2f}, {0, -0.82f, 0.02f, 0.24f, 0.26f}, {0, -0.93f, 0.1f, 0.3f, 0.4f}, {0, -0.99f, 0.16f, 0.28f, 0.36f}, {0, -1.0f, 0.17f, 0.06f, 0.08f}};
    loft(b, k, 7, 3, 9, fs, [&](float u, float sT, float, V3) -> Shd { float lt = 0.9f + 0.2f * smooth(0.55f, 0.9f, u); Shd r = coat(gry(lt), gry(lt), sT, 0.6f); if (u > 0.9f && sT < -0.2f) { r.col = gry(0.13f); } return r; });
    Sty cl = {gry(0.08f), 11, 0.4f, 0.f, 0.f, 1.f, 1.f}; for (int c = -1; c <= 1; c++) blob(b, c * 0.1f, -0.97f, 0.34f, 0.04f, 0.05f, 0.06f, 5, 3, cl, 20 + c, 0.f); return; }
}
// ---------------------------------------------------------------- HORSE
void buildHorse(int kind, MB& b) {
  Sty fs = FURS(0.8f, 0.45f), dkS = FURS(0.9f), hoof = HOOF(); V3 dark = gry(0.1f);
  if (kind == xw::K_AN_BHORSE) {
    const Stn k[] = {{0, 1.18f, 0.9f, 0.04f, 0.06f}, {0, 1.2f, 0.82f, 0.14f, 0.2f}, {0, 1.2f, 0.7f, 0.22f, 0.32f}, {0, 1.2f, 0.5f, 0.27f, 0.39f}, {0, 1.2f, 0.2f, 0.29f, 0.38f}, {0, 1.24f, -0.12f, 0.29f, 0.32f}, {0, 1.3f, -0.42f, 0.3f, 0.3f}, {0, 1.3f, -0.68f, 0.27f, 0.3f}, {0, 1.25f, -0.86f, 0.2f, 0.25f}, {0, 1.2f, -0.96f, 0.07f, 0.1f}};
    loft(b, k, 10, 4, 14, fs, [&](float u, float sT, float cT, V3 p) -> Shd { Shd r = coat(gry(0.96f), gry(0.74f), sT, 0.5f); float flank = smooth(0.55f, 0.8f, u) * smooth(-0.1f, -0.8f, sT); r.col = r.col * (1.f - 0.1f * flank); r.col = r.col * (0.94f + 0.12f * vnz(p * 3.f)); (void)cT; return r; });
    blob(b, 0.0f, 1.04f, 0.25f, 0.2f, 0.07f, 0.45f, 8, 4, fs, 3, 0.04f);   // girth fullness (hidden mostly)
    return; }
  if (kind == xw::K_AN_HHORSE) {   // pivot at the withers base (0, 1.40, 0.60)
    const Stn nk[] = {{0, -0.02f, -0.06f, 0.16f, 0.2f}, {0, 0.22f, 0.2f, 0.145f, 0.2f}, {0, 0.46f, 0.4f, 0.12f, 0.17f}, {0, 0.64f, 0.55f, 0.1f, 0.135f}};
    loft(b, nk, 4, 4, 12, fs, [&](float, float sT, float, V3 p) -> Shd { Shd r = coat(gry(0.98f), gry(0.84f), sT, 0.55f); r.col = r.col * (0.94f + 0.12f * vnz(p * 3.f)); return r; });
    const Stn hk[] = {{0, 0.72f, 0.56f, 0.09f, 0.115f}, {0, 0.62f, 0.69f, 0.1f, 0.14f}, {0, 0.5f, 0.81f, 0.077f, 0.105f}, {0, 0.4f, 0.91f, 0.061f, 0.078f}, {0, 0.325f, 0.975f, 0.066f, 0.07f}, {0, 0.29f, 1.01f, 0.05f, 0.05f}, {0, 0.28f, 1.025f, 0.01f, 0.01f}};
    loft(b, hk, 7, 3, 12, fs, [&](float u, float sT, float cT, V3 p) -> Shd { Shd r = coat(gry(0.98f), gry(0.8f), sT, 0.6f); float mz = smooth(0.72f, 0.95f, u); r.col = r.col + (gry(0.28f) - r.col) * mz * 0.8f; r.col = r.col * (0.95f + 0.1f * vnz(p * 5.f)); (void)cT; return r; });
    blob(b, 0, 0.58f, 0.64f, 0.1f, 0.1f, 0.12f, 8, 4, fs, 4, 0.02f);   // jowl
    for (int sd = -1; sd <= 1; sd += 2) {
      eye(b, sd * 0.098f, 0.63f, 0.73f, 0.021f);
      Sty ear = FURS(0.9f); tube(b, {sd * 0.055f, 0.77f, 0.52f}, {sd * 0.07f, 0.96f, 0.545f}, 0.034f, 0.01f, 6, ear); tube(b, {sd * 0.058f, 0.8f, 0.52f}, {sd * 0.068f, 0.93f, 0.54f}, 0.02f, 0.006f, 4, dkS);
      blob(b, sd * 0.034f, 0.305f, 0.992f, 0.013f, 0.012f, 0.02f, 6, 3, HOOF(), 7 + sd, 0.f); }
    for (int i = 0; i < 17; i++) { float t = i / 16.f; V3 base{0.03f, 0.75f - 0.74f * t + 0.1f * (1.f - t) * 0.f, 0.545f - 0.5f * t}; base.y += 0.07f + 0.02f * (1.f - t); float len2 = 0.2f + 0.08f * std::sin(t * 3.f); V3 p1 = base + V3{0.05f, -0.07f, -0.015f}, p2 = base + V3{0.08f, -len2, -0.04f - 0.03f * t}; hairTuft(b, base, p1, p2, 0.026f, 0.006f, dkS); }
    hairTuft(b, {0, 0.8f, 0.6f}, {0, 0.84f, 0.67f}, {0.0f, 0.72f, 0.74f}, 0.03f, 0.008f, dkS); hairTuft(b, {0.02f, 0.79f, 0.58f}, {0.03f, 0.8f, 0.66f}, {0.04f, 0.68f, 0.7f}, 0.02f, 0.006f, dkS);
    return; }
  if (kind == xw::K_AN_THORSE) {   // pivot at the dock (0, 1.46, -0.88)
    const Stn dk[] = {{0, 0, 0.0f, 0.075f, 0.08f}, {0, -0.06f, -0.12f, 0.06f, 0.06f}, {0, -0.17f, -0.2f, 0.045f, 0.05f}, {0, -0.25f, -0.22f, 0.02f, 0.02f}};
    loft(b, dk, 4, 3, 8, fs, [&](float, float sT, float, V3) -> Shd { return coat(gry(0.9f), gry(0.7f), sT, 0.6f); });
    for (int i = 0; i < 14; i++) { float a = i * 2.4f; float sp = 0.035f * std::cos(a), sy = 0.02f * std::sin(a); float L = 0.82f + 0.12f * hsh3(i, 3, 7); V3 p0{sp * 0.4f, -0.08f + sy, -0.1f}, p1{sp, -0.35f, -0.25f - 0.03f * i / 14.f}, p2{sp * 1.7f, -L * 0.78f, -0.32f - 0.05f * hsh3(i, 5, 2)}, p3{sp * 2.2f, -L, -0.3f - 0.08f * hsh3(i, 9, 1)}; tube(b, p0, p1, 0.028f, 0.02f, 5, dkS); tube(b, p1, p2, 0.02f, 0.012f, 5, dkS); tube(b, p2, p3, 0.012f, 0.003f, 4, dkS); }
    return; }
  if (kind == xw::K_AN_HDEER || kind == xw::K_AN_BDEER) return;
}
// ---------------------------------------------------------------- COW
void buildCow(int kind, MB& b) {
  Sty fs = FURS(0.82f, 0.2f), dkS = FURS(0.9f);
  auto patch = [&](V3 p) -> float { float n = vnz(hv(p, V3{2.3f, 2.8f, 2.3f}) + V3{3.f, 1.f, 7.f}) * 0.7f + vnz(p * 5.5f) * 0.3f; return smooth(0.52f, 0.58f, n); };
  if (kind == xw::K_AN_BCOW) {
    const Stn k[] = {{0, 0.98f, 0.9f, 0.05f, 0.07f}, {0, 0.98f, 0.8f, 0.2f, 0.28f}, {0, 0.98f, 0.52f, 0.3f, 0.41f}, {0, 0.94f, 0.12f, 0.35f, 0.46f}, {0, 0.95f, -0.28f, 0.34f, 0.44f}, {0, 1.0f, -0.62f, 0.29f, 0.38f}, {0, 1.0f, -0.84f, 0.21f, 0.29f}, {0, 0.98f, -0.95f, 0.07f, 0.1f}};
    loft(b, k, 8, 5, 16, fs, [&](float u, float sT, float, V3 p) -> Shd { Shd r = coat(gry(1.f), gry(0.85f), sT, 0.5f); float pt = patch(p); r.col = r.col * (1.f - 0.88f * pt); if (u > 0.4f && u < 0.78f && sT < -0.6f) r.col = r.col + (V3{0.9f, 0.55f, 0.52f} - r.col) * 0.0f; return r; });
    Sty ud = FURS(0.7f); Sty pk = {V3{0.9f, 0.58f, 0.55f}, 13, 0.7f, 0.f, 0.f, 0.6f, 1.f};
    blob(b, 0, 0.56f, -0.56f, 0.15f, 0.13f, 0.16f, 9, 5, pk, 3, 0.03f); for (int i = 0; i < 4; i++) tube(b, {(i & 1 ? 0.06f : -0.06f), 0.5f, -0.5f - (i < 2 ? 0.f : 0.12f)}, {(i & 1 ? 0.06f : -0.06f), 0.4f, -0.5f - (i < 2 ? 0.f : 0.12f)}, 0.022f, 0.016f, 5, pk); (void)ud;
    blob(b, 0.0f, 1.47f, -0.45f, 0.2f, 0.05f, 0.3f, 7, 3, fs, 11, 0.0f);   // hip / back ridge
    return; }
  if (kind == xw::K_AN_HCOW) {   // pivot (0, 1.14, 0.74)
    const Stn nk[] = {{0, 0.02f, -0.08f, 0.22f, 0.27f}, {0, 0.07f, 0.18f, 0.2f, 0.25f}, {0, 0.1f, 0.4f, 0.16f, 0.2f}, {0, 0.09f, 0.52f, 0.13f, 0.17f}};
    loft(b, nk, 4, 4, 12, fs, [&](float, float sT, float, V3 p) -> Shd { Shd r = coat(gry(1.f), gry(0.88f), sT, 0.55f); r.col = r.col * (1.f - 0.88f * patch(p + V3{0.f, 1.14f, 0.74f})); return r; });
    const Stn hk[] = {{0, 0.08f, 0.5f, 0.13f, 0.17f}, {0, 0.03f, 0.66f, 0.14f, 0.19f}, {0, -0.08f, 0.8f, 0.115f, 0.15f}, {0, -0.2f, 0.91f, 0.1f, 0.12f}, {0, -0.27f, 0.98f, 0.11f, 0.1f}, {0, -0.3f, 1.01f, 0.1f, 0.08f}, {0, -0.305f, 1.02f, 0.02f, 0.02f}};
    loft(b, hk, 7, 3, 12, fs, [&](float u, float sT, float, V3 p) -> Shd { Shd r = coat(gry(1.f), gry(0.9f), sT, 0.6f); r.col = r.col * (1.f - 0.88f * patch(p + V3{0.f, 1.14f, 0.74f})); float mz = smooth(0.8f, 0.92f, u); r.col = r.col + (V3{0.86f, 0.55f, 0.52f} - r.col) * mz; return r; });
    blob(b, 0, -0.12f, 0.33f, 0.08f, 0.17f, 0.2f, 8, 4, fs, 5, 0.04f);   // dewlap
    for (int sd = -1; sd <= 1; sd += 2) { eye(b, sd * 0.12f, 0.1f, 0.74f, 0.022f);
      Sty ear = FURS(0.9f); tube(b, {sd * 0.1f, 0.17f, 0.56f}, {sd * 0.2f, 0.19f, 0.53f}, 0.04f, 0.02f, 6, ear); tube(b, {sd * 0.2f, 0.19f, 0.53f}, {sd * 0.28f, 0.17f, 0.52f}, 0.022f, 0.008f, 5, ear);
      Sty hn = {V3{0.92f, 0.88f, 0.76f}, 11, 0.45f, 0.f, 0.f, 0.9f, 1.f}; tube(b, {sd * 0.1f, 0.2f, 0.6f}, {sd * 0.2f, 0.26f, 0.6f}, 0.026f, 0.02f, 6, hn); tube(b, {sd * 0.2f, 0.26f, 0.6f}, {sd * 0.26f, 0.38f, 0.64f}, 0.02f, 0.006f, 6, hn);
      blob(b, sd * 0.034f, -0.285f, 1.01f, 0.014f, 0.012f, 0.02f, 6, 3, HOOF(), 7 + sd, 0.f); }
    return; }
  if (kind == xw::K_AN_TCOW) {   // pivot (0, 1.38, -0.9)
    const Stn tk[] = {{0, 0, 0, 0.04f, 0.04f}, {0, -0.12f, -0.07f, 0.03f, 0.03f}, {0, -0.55f, -0.1f, 0.022f, 0.022f}, {0, -0.82f, -0.09f, 0.016f, 0.016f}};
    loft(b, tk, 4, 4, 7, fs, [&](float, float, float, V3) -> Shd { return {gry(0.9f), 0.9f}; });
    for (int i = 0; i < 8; i++) { float a = i * 0.8f; tube(b, {0, -0.78f, -0.09f}, {0.03f * std::cos(a), -1.0f - 0.03f * hsh3(i, 1, 1), -0.1f + 0.03f * std::sin(a)}, 0.03f, 0.006f, 5, dkS); }
    return; }
}
// ---------------------------------------------------------------- SHEEP
void buildSheep(int kind, MB& b) {
  Sty wool = FURS(0.95f, 0.1f), faceS = FURS(0.9f);
  if (kind == xw::K_AN_BSHEEP) {
    const Stn k[] = {{0, 0.66f, 0.5f, 0.05f, 0.08f}, {0, 0.66f, 0.42f, 0.17f, 0.22f}, {0, 0.66f, 0.2f, 0.24f, 0.29f}, {0, 0.66f, -0.1f, 0.26f, 0.3f}, {0, 0.67f, -0.36f, 0.24f, 0.28f}, {0, 0.65f, -0.52f, 0.14f, 0.2f}, {0, 0.63f, -0.58f, 0.04f, 0.06f}};
    loft(b, k, 7, 4, 14, wool, [&](float, float sT, float, V3 p) -> Shd { Shd r = coat(gry(0.96f), gry(0.8f), sT, 0.5f); r.col = r.col * (0.88f + 0.2f * vnz(p * 6.f)); return r; });
    for (int i = 0; i < 70; i++) { float u = hsh3(i, 1, 9), a = hsh3(i, 2, 4) * TAU; float z = 0.42f - u * 0.9f; float ry = 0.27f - 0.05f * std::fabs(u - 0.4f), rx = 0.23f; float sy = std::sin(a), cy = std::cos(a); if (sy < -0.55f) continue; float r = 0.09f + 0.05f * hsh3(i, 3, 3);
      Sty w2 = wool; w2.col = gry(0.92f + 0.12f * hsh3(i, 6, 1)); blob(b, cy * rx * 0.95f, 0.66f + sy * ry * 0.95f, z, r, r * 0.9f, r, 6, 4, w2, 100 + i, 0.25f); }
    blob(b, 0, 0.7f, -0.58f, 0.09f, 0.09f, 0.08f, 6, 4, wool, 301, 0.2f);   // tail tuft
    return; }
  if (kind == xw::K_AN_HSHEEP) {   // pivot (0, 0.74, 0.4)
    const Stn nk[] = {{0, 0.0f, -0.05f, 0.11f, 0.13f}, {0, 0.07f, 0.12f, 0.09f, 0.11f}, {0, 0.12f, 0.2f, 0.08f, 0.1f}};
    loft(b, nk, 3, 4, 10, wool, [&](float, float sT, float, V3 p) -> Shd { Shd r = coat(gry(0.95f), gry(0.8f), sT, 0.6f); r.col = r.col * (0.9f + 0.15f * vnz(p * 8.f)); return r; });
    for (int i = 0; i < 10; i++) { float a = i * 0.7f; blob(b, 0.07f * std::cos(a), 0.12f + 0.06f * std::sin(a), 0.06f + 0.03f * hsh3(i, 2, 2), 0.075f, 0.075f, 0.075f, 6, 4, wool, 400 + i, 0.25f); }
    const Stn hk[] = {{0, 0.15f, 0.2f, 0.07f, 0.085f}, {0, 0.12f, 0.29f, 0.075f, 0.1f}, {0, 0.06f, 0.37f, 0.06f, 0.08f}, {0, 0.0f, 0.43f, 0.048f, 0.055f}, {0, -0.02f, 0.45f, 0.01f, 0.01f}};
    loft(b, hk, 5, 3, 10, faceS, [&](float u, float sT, float, V3) -> Shd { Shd r = coat(V3{0.2f, 0.17f, 0.15f}, V3{0.28f, 0.24f, 0.2f}, sT, 0.6f); float mz = smooth(0.8f, 0.95f, u); r.col = r.col + (V3{0.45f, 0.32f, 0.3f} - r.col) * mz * 0.5f; return r; });
    for (int sd = -1; sd <= 1; sd += 2) { eye(b, sd * 0.062f, 0.13f, 0.34f, 0.016f); Sty ear = faceS; ear.col = V3{0.2f, 0.17f, 0.15f}; tube(b, {sd * 0.06f, 0.18f, 0.22f}, {sd * 0.17f, 0.15f, 0.2f}, 0.03f, 0.012f, 6, ear); tube(b, {sd * 0.17f, 0.15f, 0.2f}, {sd * 0.22f, 0.11f, 0.19f}, 0.014f, 0.006f, 5, ear); }
    blob(b, 0, 0.19f, 0.2f, 0.09f, 0.06f, 0.1f, 6, 4, wool, 500, 0.3f);   // forelock wool
    return; }
}
// ---------------------------------------------------------------- DOG / CAT
void buildDog(int kind, MB& b) {
  Sty fs = FURS(0.8f, 0.2f), dkS = FURS(0.9f);
  if (kind == xw::K_AN_BDOG) {
    const Stn k[] = {{0, 0.54f, 0.48f, 0.04f, 0.06f}, {0, 0.55f, 0.4f, 0.1f, 0.15f}, {0, 0.55f, 0.22f, 0.14f, 0.2f}, {0, 0.56f, -0.02f, 0.125f, 0.17f}, {0, 0.57f, -0.22f, 0.13f, 0.16f}, {0, 0.55f, -0.36f, 0.11f, 0.13f}, {0, 0.53f, -0.44f, 0.04f, 0.06f}};
    loft(b, k, 7, 4, 12, fs, [&](float u, float sT, float cT, V3 p) -> Shd { Shd r = coat(gry(0.96f), gry(0.78f), sT, 0.5f); float sd = smooth(0.2f, 0.95f, std::fabs(cT)) * 0.f; r.col = r.col * (0.92f + 0.14f * vnz(p * 5.f)); float saddle = smooth(0.1f, 0.5f, sT) * smooth(0.12f, 0.35f, u) * (1.f - smooth(0.6f, 0.85f, u)); r.col = r.col * (1.f - 0.18f * saddle); (void)sd; return r; });
    return; }
  if (kind == xw::K_AN_HDOG) {   // pivot (0, 0.62, 0.38)
    const Stn nk[] = {{0, -0.02f, -0.06f, 0.09f, 0.11f}, {0, 0.06f, 0.07f, 0.075f, 0.1f}, {0, 0.12f, 0.15f, 0.065f, 0.085f}};
    loft(b, nk, 3, 4, 10, fs, [&](float, float sT, float, V3 p) -> Shd { Shd r = coat(gry(0.97f), gry(0.84f), sT, 0.6f); r.col = r.col * (0.94f + 0.1f * vnz(p * 6.f)); return r; });
    const Stn hk[] = {{0, 0.15f, 0.13f, 0.06f, 0.075f}, {0, 0.17f, 0.2f, 0.065f, 0.08f}, {0, 0.155f, 0.28f, 0.05f, 0.062f}, {0, 0.125f, 0.355f, 0.037f, 0.047f}, {0, 0.108f, 0.415f, 0.034f, 0.04f}, {0, 0.1f, 0.445f, 0.012f, 0.012f}};
    loft(b, hk, 6, 3, 10, fs, [&](float u, float sT, float, V3) -> Shd { Shd r = coat(gry(0.97f), gry(0.9f), sT, 0.6f); float mz = smooth(0.45f, 0.8f, u); r.col = r.col * (1.f - 0.3f * mz * smooth(-0.2f, 0.8f, sT)); return r; });
    blob(b, 0, 0.115f, 0.435f, 0.026f, 0.02f, 0.022f, 7, 4, HOOF(), 9, 0.f);   // nose
    for (int sd = -1; sd <= 1; sd += 2) { eye(b, sd * 0.052f, 0.185f, 0.285f, 0.014f);
      Sty ear = dkS; ear.col = gry(0.5f); tube(b, {sd * 0.04f, 0.2f, 0.16f}, {sd * 0.055f, 0.34f, 0.15f}, 0.036f, 0.006f, 6, ear); }
    return; }
  if (kind == xw::K_AN_TDOG) {   // pivot (0, 0.57, -0.42)
    const Stn tk[] = {{0, 0, 0, 0.03f, 0.03f}, {0, 0.05f, -0.1f, 0.032f, 0.032f}, {0, 0.14f, -0.17f, 0.03f, 0.03f}, {0, 0.26f, -0.2f, 0.014f, 0.014f}};
    loft(b, tk, 4, 4, 8, fs, [&](float u, float sT, float, V3) -> Shd { Shd r = coat(gry(0.96f), gry(0.8f), sT, 0.7f); if (u > 0.82f) r.col = gry(1.15f); return r; }); return; }
}
void buildCat(int kind, MB& b) {
  Sty fs = FURS(0.78f, 0.1f), dkS = FURS(0.9f);
  if (kind == xw::K_AN_BCAT) {
    const Stn k[] = {{0, 0.25f, 0.2f, 0.03f, 0.04f}, {0, 0.26f, 0.15f, 0.06f, 0.085f}, {0, 0.27f, 0.04f, 0.072f, 0.095f}, {0, 0.27f, -0.1f, 0.062f, 0.085f}, {0, 0.26f, -0.19f, 0.066f, 0.082f}, {0, 0.25f, -0.25f, 0.02f, 0.04f}};
    loft(b, k, 6, 4, 10, fs, [&](float u, float sT, float cT, V3 p) -> Shd { Shd r = coat(gry(0.96f), gry(0.82f), sT, 0.55f); float st = 0.5f + 0.5f * std::sin(u * 30.f); r.col = r.col * (1.f - 0.14f * st * smooth(0.f, 0.8f, sT) * (1.f - smooth(0.7f, 1.f, std::fabs(cT)))); r.col = r.col * (0.94f + 0.1f * vnz(p * 9.f)); return r; });
    return; }
  if (kind == xw::K_AN_HCAT) {   // pivot (0, 0.3, 0.17)
    const Stn nk[] = {{0, -0.02f, -0.04f, 0.055f, 0.07f}, {0, 0.045f, 0.05f, 0.05f, 0.06f}};
    loft(b, nk, 2, 3, 9, fs, [&](float, float sT, float, V3) -> Shd { return coat(gry(0.97f), gry(0.84f), sT, 0.6f); });
    const Stn hk[] = {{0, 0.085f, 0.06f, 0.05f, 0.055f}, {0, 0.1f, 0.1f, 0.058f, 0.058f}, {0, 0.092f, 0.155f, 0.042f, 0.04f}, {0, 0.082f, 0.19f, 0.025f, 0.025f}, {0, 0.078f, 0.205f, 0.008f, 0.008f}};
    loft(b, hk, 5, 3, 10, fs, [&](float u, float sT, float, V3) -> Shd { Shd r = coat(gry(0.97f), gry(0.9f), sT, 0.65f); if (u > 0.78f) r.col = r.col + (V3{0.9f, 0.55f, 0.55f} - r.col) * 0.5f; return r; });
    for (int sd = -1; sd <= 1; sd += 2) { eye(b, sd * 0.032f, 0.105f, 0.155f, 0.011f); Sty ear = dkS; ear.col = gry(0.9f); tube(b, {sd * 0.034f, 0.13f, 0.09f}, {sd * 0.05f, 0.205f, 0.085f}, 0.03f, 0.004f, 5, ear); tube(b, {sd * 0.034f, 0.135f, 0.095f}, {sd * 0.046f, 0.185f, 0.09f}, 0.016f, 0.003f, 5, HOOF()); }
    return; }
  if (kind == xw::K_AN_TCAT) {   // pivot (0, 0.26, -0.22)
    const Stn tk[] = {{0, 0, 0, 0.022f, 0.022f}, {0, -0.04f, -0.1f, 0.022f, 0.022f}, {0, 0.0f, -0.22f, 0.021f, 0.021f}, {0, 0.1f, -0.27f, 0.02f, 0.02f}, {0, 0.2f, -0.26f, 0.008f, 0.008f}};
    loft(b, tk, 5, 4, 8, fs, [&](float, float sT, float, V3) -> Shd { return coat(gry(0.96f), gry(0.84f), sT, 0.7f); }); return; }
}
// ---------------------------------------------------------------- DEER (stag), RABBIT, HEN
void buildDeer(int kind, MB& b) {
  Sty fs = FURS(0.8f, 0.3f), dkS = FURS(0.9f);
  if (kind == xw::K_AN_BDEER) {
    const Stn k[] = {{0, 0.98f, 0.62f, 0.04f, 0.06f}, {0, 1.0f, 0.55f, 0.11f, 0.17f}, {0, 1.0f, 0.38f, 0.16f, 0.24f}, {0, 0.98f, 0.1f, 0.18f, 0.26f}, {0, 1.0f, -0.18f, 0.17f, 0.23f}, {0, 1.03f, -0.42f, 0.17f, 0.22f}, {0, 1.0f, -0.58f, 0.12f, 0.17f}, {0, 0.97f, -0.65f, 0.04f, 0.07f}};
    loft(b, k, 8, 4, 12, fs, [&](float u, float sT, float cT, V3 p) -> Shd { Shd r = coat(gry(0.93f), gry(0.58f), sT, 0.5f); float rump = smooth(0.85f, 0.95f, u) * smooth(-0.3f, 0.6f, sT); r.col = r.col + (gry(1.2f) - r.col) * rump * 0.8f; r.col = r.col * (0.92f + 0.14f * vnz(p * 5.f)); (void)cT; return r; });
    blob(b, 0, 0.99f, -0.68f, 0.04f, 0.06f, 0.04f, 6, 3, fs, 3, 0.f);   // short tail
    return; }
  if (kind == xw::K_AN_HDEER) {   // pivot (0, 1.1, 0.52)
    const Stn nk[] = {{0, -0.02f, -0.06f, 0.115f, 0.15f}, {0, 0.2f, 0.12f, 0.085f, 0.12f}, {0, 0.42f, 0.27f, 0.07f, 0.095f}, {0, 0.56f, 0.38f, 0.065f, 0.082f}};
    loft(b, nk, 4, 4, 11, fs, [&](float, float sT, float, V3 p) -> Shd { Shd r = coat(gry(0.95f), gry(0.75f), sT, 0.55f); r.col = r.col * (0.94f + 0.12f * vnz(p * 5.f)); return r; });
    const Stn hk[] = {{0, 0.6f, 0.37f, 0.06f, 0.075f}, {0, 0.55f, 0.46f, 0.066f, 0.086f}, {0, 0.46f, 0.55f, 0.05f, 0.064f}, {0, 0.395f, 0.62f, 0.04f, 0.048f}, {0, 0.365f, 0.655f, 0.038f, 0.04f}, {0, 0.36f, 0.665f, 0.008f, 0.008f}};
    loft(b, hk, 6, 3, 10, fs, [&](float u, float sT, float, V3) -> Shd { Shd r = coat(gry(0.96f), gry(0.8f), sT, 0.6f); float mz = smooth(0.75f, 0.95f, u); r.col = r.col + (gry(0.2f) - r.col) * mz * 0.7f; return r; });
    for (int sd = -1; sd <= 1; sd += 2) { eye(b, sd * 0.062f, 0.55f, 0.48f, 0.017f); Sty ear = dkS; tube(b, {sd * 0.05f, 0.62f, 0.34f}, {sd * 0.17f, 0.7f, 0.31f}, 0.04f, 0.006f, 6, ear);
      Sty an = {V3{0.8f, 0.72f, 0.56f}, 11, 0.5f, 0.f, 0.f, 0.9f, 1.f};   // antlers: main beam + tines
      V3 s0{sd * 0.04f, 0.66f, 0.36f}, s1{sd * 0.09f, 0.82f, 0.3f}, s2{sd * 0.15f, 0.98f, 0.24f}, s3{sd * 0.2f, 1.14f, 0.28f}; tube(b, s0, s1, 0.017f, 0.014f, 5, an); tube(b, s1, s2, 0.014f, 0.011f, 5, an); tube(b, s2, s3, 0.011f, 0.005f, 5, an);
      tube(b, s1, {sd * 0.08f, 0.9f, 0.46f}, 0.01f, 0.004f, 4, an); tube(b, s2, {sd * 0.1f, 1.1f, 0.4f}, 0.009f, 0.003f, 4, an); tube(b, s2, {sd * 0.24f, 1.04f, 0.2f}, 0.009f, 0.003f, 4, an); tube(b, s3, {sd * 0.26f, 1.28f, 0.32f}, 0.006f, 0.002f, 4, an); }
    blob(b, 0, 0.365f, 0.652f, 0.02f, 0.016f, 0.02f, 6, 3, HOOF(), 9, 0.f);
    return; }
}
void buildRabbit(MB& b) {
  Sty fs = FURS(0.85f, 0.2f);
  const Stn k[] = {{0, 0.1f, 0.12f, 0.03f, 0.04f}, {0, 0.13f, 0.06f, 0.07f, 0.09f}, {0, 0.14f, -0.04f, 0.08f, 0.11f}, {0, 0.11f, -0.13f, 0.075f, 0.085f}, {0, 0.07f, -0.17f, 0.03f, 0.04f}};
  loft(b, k, 5, 4, 10, fs, [&](float, float sT, float, V3 p) -> Shd { Shd r = coat(gry(0.96f), gry(0.8f), sT, 0.55f); r.col = r.col * (0.9f + 0.18f * vnz(p * 14.f)); return r; });
  blob(b, 0, 0.18f, 0.14f, 0.044f, 0.05f, 0.06f, 8, 5, fs, 31, 0.f); blob(b, 0, 0.19f, 0.19f, 0.03f, 0.03f, 0.04f, 7, 4, fs, 32, 0.f);
  blob(b, 0, 0.16f, 0.2f, 0.012f, 0.01f, 0.012f, 5, 3, HOOF(), 33, 0.f);
  for (int sd = -1; sd <= 1; sd += 2) { eye(b, sd * 0.034f, 0.2f, 0.16f, 0.01f); tube(b, {sd * 0.02f, 0.22f, 0.12f}, {sd * 0.035f, 0.36f, 0.08f}, 0.02f, 0.012f, 6, fs); tube(b, {sd * 0.035f, 0.36f, 0.08f}, {sd * 0.04f, 0.4f, 0.05f}, 0.012f, 0.003f, 5, fs);
    blob(b, sd * 0.055f, 0.06f, -0.05f, 0.03f, 0.06f, 0.08f, 6, 4, fs, 40 + sd, 0.f); blob(b, sd * 0.04f, 0.02f, 0.1f, 0.016f, 0.02f, 0.05f, 6, 3, fs, 44 + sd, 0.f); blob(b, sd * 0.05f, 0.02f, -0.09f, 0.02f, 0.02f, 0.07f, 6, 3, fs, 47 + sd, 0.f); }
  Sty tail = fs; tail.col = gry(1.3f); blob(b, 0, 0.1f, -0.18f, 0.04f, 0.04f, 0.04f, 7, 4, tail, 35, 0.15f);
}
void buildHen(MB& b) {
  Sty fs = FURS(0.8f, 0.1f); Sty red = {V3{0.85f, 0.12f, 0.1f}, 11, 0.55f, 0.f, 0.f, 0.9f, 1.f}; Sty yel = {V3{1.f, 0.78f, 0.25f}, 11, 0.5f, 0.f, 0.f, 0.9f, 1.f};
  const Stn k[] = {{0, 0.3f, 0.14f, 0.03f, 0.04f}, {0, 0.3f, 0.08f, 0.075f, 0.09f}, {0, 0.28f, -0.02f, 0.1f, 0.11f}, {0, 0.27f, -0.12f, 0.085f, 0.095f}, {0, 0.27f, -0.2f, 0.04f, 0.05f}, {0, 0.28f, -0.23f, 0.01f, 0.01f}};
  loft(b, k, 6, 4, 10, fs, [&](float u, float sT, float cT, V3 p) -> Shd { Shd r = coat(gry(0.95f), gry(0.8f), sT, 0.55f); float feather = vnz(hv(p, V3{30.f, 22.f, 22.f})); r.col = r.col * (0.86f + 0.22f * feather); float wing = smooth(0.35f, 0.6f, std::fabs(cT)) * smooth(0.f, 0.5f, sT + 0.3f) * smooth(0.25f, 0.4f, u) * (1.f - smooth(0.7f, 0.85f, u)); r.col = r.col * (1.f - 0.18f * wing); return r; });
  const Stn nk[] = {{0, 0.32f, 0.11f, 0.05f, 0.07f}, {0, 0.4f, 0.14f, 0.037f, 0.05f}, {0, 0.46f, 0.15f, 0.03f, 0.035f}};
  loft(b, nk, 3, 3, 9, fs, [&](float u, float sT, float, V3 p) -> Shd { Shd r = coat(gry(0.97f), gry(0.9f), sT, 0.6f); r.col = r.col * (0.9f + 0.18f * vnz(p * 40.f)); (void)u; return r; });
  blob(b, 0, 0.5f, 0.155f, 0.034f, 0.036f, 0.04f, 7, 4, fs, 7, 0.f);
  blob(b, 0, 0.545f, 0.15f, 0.008f, 0.025f, 0.035f, 5, 3, red, 8, 0.1f); blob(b, 0, 0.535f, 0.165f, 0.008f, 0.018f, 0.02f, 5, 3, red, 9, 0.1f); blob(b, 0, 0.45f, 0.185f, 0.01f, 0.025f, 0.012f, 5, 3, red, 10, 0.1f);
  tube(b, {0, 0.495f, 0.19f}, {0, 0.485f, 0.235f}, 0.013f, 0.003f, 5, yel);
  eye(b, 0.03f, 0.505f, 0.17f, 0.008f); eye(b, -0.03f, 0.505f, 0.17f, 0.008f);
  for (int i = -2; i <= 2; i++) { Sty tf = fs; tf.col = gry(i % 2 ? 0.55f : 0.9f); tube(b, {i * 0.008f, 0.3f, -0.17f}, {i * 0.03f, 0.43f - 0.02f * std::fabs((float)i), -0.27f}, 0.022f, 0.012f, 5, tf); tube(b, {i * 0.03f, 0.43f - 0.02f * std::fabs((float)i), -0.27f}, {i * 0.045f, 0.4f - 0.03f * std::fabs((float)i), -0.34f}, 0.012f, 0.002f, 4, tf); }
}
// ---------------------------------------------------------------- SADDLE (horse): blanket, seat, pommel, cantle, girth, stirrups
void buildSaddle(MB& b) {
  Sty lea = {V3{0.36f, 0.2f, 0.1f}, 4, 0.55f, 0.f, 0.f, 0.8f, 1.f}, bl = {V3{0.62f, 0.14f, 0.12f}, 1, 0.95f, 0.f, 0.f, 0.8f, 1.f}, ir = {V3{0.55f, 0.56f, 0.58f}, 5, 0.4f, 0.8f, 0.f, 0.9f, 1.f};
  const Stn bk[] = {{0, 1.52f, 0.5f, 0.02f, 0.012f}, {0, 1.53f, 0.42f, 0.33f, 0.016f}, {0, 1.53f, 0.1f, 0.35f, 0.018f}, {0, 1.54f, -0.2f, 0.33f, 0.016f}, {0, 1.54f, -0.28f, 0.02f, 0.012f}};
  loft(b, bk, 5, 2, 12, lea, [&](float, float, float, V3) -> Shd { return {V3{0.6f, 0.13f, 0.1f}, 0.9f}; });   // blanket (flat oval, wraps by its width)
  for (int sd = -1; sd <= 1; sd += 2) { tube(b, {sd * 0.27f, 1.5f, 0.42f}, {sd * 0.31f, 1.2f, 0.4f}, 0.02f, 0.016f, 5, bl); tube(b, {sd * 0.27f, 1.5f, -0.1f}, {sd * 0.32f, 1.2f, -0.1f}, 0.02f, 0.016f, 5, bl); }
  const Stn seat[] = {{0, 1.58f, 0.3f, 0.07f, 0.05f}, {0, 1.585f, 0.18f, 0.15f, 0.045f}, {0, 1.58f, -0.02f, 0.17f, 0.04f}, {0, 1.6f, -0.2f, 0.15f, 0.06f}, {0, 1.62f, -0.26f, 0.06f, 0.05f}};
  loft(b, seat, 5, 3, 10, lea, [&](float, float, float, V3) -> Shd { return {V3{0.4f, 0.22f, 0.11f}, 0.9f}; });
  blob(b, 0, 1.64f, 0.3f, 0.045f, 0.06f, 0.04f, 7, 4, lea, 3, 0.f);   // pommel
  for (int sd = -1; sd <= 1; sd += 2) { tube(b, {sd * 0.17f, 1.55f, 0.08f}, {sd * 0.3f, 1.2f, 0.08f}, 0.018f, 0.014f, 5, lea);
    const Stn st[] = {{sd * 0.3f, 1.2f, 0.08f, 0.03f, 0.012f}, {sd * 0.3f, 1.14f, 0.08f, 0.075f, 0.012f}, {sd * 0.3f, 1.08f, 0.08f, 0.03f, 0.012f}}; loft(b, st, 3, 2, 8, ir, [&](float, float, float, V3) -> Shd { return {V3{0.6f, 0.6f, 0.62f}, 0.9f}; }); }
  const Stn gk[] = {{-0.29f, 1.2f, 0.38f, 0.012f, 0.04f}, {-0.2f, 0.95f, 0.38f, 0.012f, 0.04f}, {0.0f, 0.82f, 0.38f, 0.012f, 0.04f}, {0.2f, 0.95f, 0.38f, 0.012f, 0.04f}, {0.29f, 1.2f, 0.38f, 0.012f, 0.04f}};
  loft(b, gk, 5, 3, 6, lea, [&](float, float, float, V3) -> Shd { return {V3{0.3f, 0.17f, 0.09f}, 0.9f}; });
}
void buildAnimal(int kind, MB& b) {
  using namespace xw;
  switch (kind) {
    case K_AN_UPF: case K_AN_UPH: case K_AN_LOH: case K_AN_LOP: legMesh(kind, b); break;
    case K_AN_BHORSE: case K_AN_HHORSE: case K_AN_THORSE: buildHorse(kind, b); break;
    case K_AN_BCOW: case K_AN_HCOW: case K_AN_TCOW: buildCow(kind, b); break;
    case K_AN_BSHEEP: case K_AN_HSHEEP: buildSheep(kind, b); break;
    case K_AN_BDOG: case K_AN_HDOG: case K_AN_TDOG: buildDog(kind, b); break;
    case K_AN_BCAT: case K_AN_HCAT: case K_AN_TCAT: buildCat(kind, b); break;
    case K_AN_BDEER: case K_AN_HDEER: buildDeer(kind, b); break;
    case K_AN_BRABBIT: buildRabbit(b); break;
    case K_AN_BHEN: buildHen(b); break;
    case K_AN_SADDLE: buildSaddle(b); break;
    default: break;
  }
}
