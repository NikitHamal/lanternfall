// v1.9 living world: fauna + ambient life. Included inside the anonymous namespace of xgame.cpp (after xkingdom.inc).
// Dogs, cats, hens, sheep, cows, horses (articulated procedural parts through xr::Dyn), songbirds + crows, butterflies, falling petals / leaves.
// Zero per-frame heap allocations: fixed pools, no string formatting except on events.
enum Sp { SP_DOG, SP_CAT, SP_HEN, SP_SHEEP, SP_COW, SP_HORSE, SP_DEER, SP_RABBIT, SP_N };
struct SpDef { const char* name; int body; float hipY, legLen, legW, legZ, legX, halfLen, rad, walk, run, flee, hp, wander, headU; int nLegs; uint32_t legCol; int meat; };
const SpDef SPD[SP_N] = {
  {"DOG", K_FA_DOG, 0.47f, 0.47f, 0.045f, 0.24f, 0.09f, 0.42f, 0.2f, 1.3f, 5.6f, 0.f, 30, 10, 0.78f, 4, 0xc4985c, 0},
  {"CAT", K_FA_CAT, 0.27f, 0.27f, 0.028f, 0.15f, 0.06f, 0.26f, 0.13f, 0.8f, 4.6f, 7.f, 12, 8, 0.75f, 4, 0xd8c8b0, 0},
  {"HEN", K_FA_HEN, 0.21f, 0.21f, 0.012f, 0.02f, 0.045f, 0.14f, 0.13f, 0.8f, 3.4f, 4.5f, 6, 5, 0.7f, 2, 0xd89a30, 2},
  {"SHEEP", K_FA_SHEEP, 0.52f, 0.52f, 0.04f, 0.33f, 0.15f, 0.45f, 0.36f, 1.0f, 4.8f, 9.f, 24, 14, 0.8f, 4, 0x3a3028, 8},
  {"COW", K_FA_COW, 1.0f, 1.0f, 0.075f, 0.62f, 0.28f, 0.85f, 0.52f, 0.9f, 3.9f, 5.f, 60, 12, 0.8f, 4, 0xe9e4da, 14},
  {"HORSE", K_FA_HORSE, 1.05f, 1.05f, 0.06f, 0.62f, 0.22f, 0.8f, 0.45f, 1.8f, 8.6f, 11.f, 70, 30, 0.8f, 4, 0xffffff, 10},
  {"DEER", K_FA_COW, 1.0f, 1.0f, 0.05f, 0.4f, 0.15f, 0.62f, 0.3f, 1.4f, 11.f, 24.f, 40, 22, 0.75f, 4, 0x8a5a36, 12},
  {"RABBIT", K_FA_CAT, 0.15f, 0.15f, 0.02f, 0.1f, 0.04f, 0.15f, 0.1f, 0.9f, 6.5f, 12.f, 4, 6, 0.7f, 4, 0x8a7a68, 3}};
struct Fauna { bool used = false, alive = true, pet = false; int sp = 0, state = 0; float x = 0, y = 0, z = 0, face = 0, sc = 1, hp = 10, maxHp = 10, homeX = 0, homeZ = 0, tx = 0, tz = 0, t = 0, spd = 0, ph = 0, hitT = 0, deadT = 0, fleeT = 0, biteCd = 0, tint[3] = {1, 1, 1}, ltint[3] = {1, 1, 1}, pitch = 0;
  float spdS = 0, cyc = 0, gDuty = 0.66f, gH = 0, gLift = 0, gOff[4] = {0.25f, 0.75f, 0.f, 0.5f}, bob = 0, pit = 0, look = 0, headP = 0, graze = 0, lie = 0, wag = 0, alert = 0; int act = 0; };
constexpr int MAXF = 1800; Fauna FA[MAXF]; int nFA = 0;
// riding: the mounted horse is a full Fauna (same rig as the herds) that follows the player; mountT / dmT drive the mount and dismount hop
bool rideH = false; Fauna rideF; float rideX = 0, rideZ = 0, rideFace = 0, mountT = 1.f, dmT = 0.f, mountFx = 0, mountFz = 0; constexpr float RIDE_OFF = 0.66f; bool dbgClean = false; float dbgSpd = -1.f; int dbgTrack = -1; float dbgAng = 1.1f, dbgDist = 4.f, dbgHF = 0.9f;
#include "xfauna_rig.inc"
float riderLift() { if (K.mounted && rideH) { float e = smooth(0.f, 1.f, mountT); return RIDE_OFF * e + (mountT < 1.f ? 0.3f * std::sin(PI * mountT) : 0.f) + (mountT >= 1.f ? rideF.bob * 0.9f : 0.f); } if (!K.mounted && dmT > 0.f) return RIDE_OFF * smooth(0.f, 1.f, dmT / 0.45f); return 0.f; }
float riderPose() { if (K.mounted && rideH) return smooth(0.f, 1.f, mountT * 1.5f); if (!K.mounted && dmT > 0.f) return smooth(0.f, 1.f, dmT / 0.45f); return 1.f; }
struct Bird { float cx = 0, cz = 0, r = 10, alt = 12, ph = 0, spd = 7, dir = 1, gh = 0, sc = 1, tint[3] = {1, 1, 1}; bool crow = false; } BD[56]; int nBD = 0;
struct Bfly { float x = 0, y = 0, z = 0, ax = 0, az = 0, ph = 0, tint[3] = {1, 1, 1}; bool on = false; } BF[44];
struct Petal { float x = 0, y = 0, z = 0, ph = 0, fall = 1, spin = 1, sz = 1, tint[3] = {1, 1, 1}; bool on = false; } PT[120];
Rng frng(4242);
inline float fr01() { return frng(); }
void tintHex(float* t, uint32_t hex, float k = 1.f) { V3 c = C(hex); t[0] = c.x * k; t[1] = c.y * k; t[2] = c.z * k; }
Fauna* addFauna(int sp, float x, float z, bool anyGround = false) {
  if (nFA >= MAXF) return nullptr; if (!anyGround && !validSpot(x, z, 0.6f)) return nullptr; float cx = x, cz = z; collide(cx, cz, 0.7f); if (dist2(cx, cz, x, z) > 0.2f) return nullptr;
  Fauna& f = FA[nFA++]; f = Fauna(); f.used = true; f.sp = sp; f.x = f.homeX = f.tx = x; f.z = f.homeZ = f.tz = z; f.y = height(x, z); f.face = fr01() * TAU; f.ph = fr01() * TAU; f.t = fr01() * 4.f; f.sc = (0.9f + fr01() * 0.22f) * (sp == SP_DOG ? 0.95f + fr01() * 0.2f : 1.f);
  const SpDef& d = SPD[sp]; f.maxHp = f.hp = (float)d.hp; tintHex(f.ltint, d.legCol); f.tint[0] = f.tint[1] = f.tint[2] = 1.f;
  float r = fr01(); float* t = f.tint;
  static const uint32_t HC[7] = {0x8a4a22, 0x5a3418, 0x2a2420, 0xb8b4aa, 0xe8e4dc, 0xd8b070, 0x6a3a1c}, DC[6] = {0xc4985c, 0x2e2622, 0xe2d6c0, 0x8a5a36, 0x6c6660, 0xb87a3c}, CC[6] = {0xc9b79c, 0x2a2622, 0xd8803a, 0x8a8478, 0xe8e4dc, 0x6a5a4a}, KC[4] = {0xf0e8da, 0xb8683a, 0x3a2a24, 0xd8b070}, OC[4] = {0xf2eee6, 0xc08850, 0xe8e0d0, 0x8a5a38}, DE[4] = {0xa8703c, 0x946034, 0xb88450, 0x7a4e2c}, RC[4] = {0x8a7660, 0xb0a290, 0x6a5a4a, 0xe0d8cc};
  switch (sp) {
    case SP_HORSE: tintHex(t, HC[(int)(r * 6.99f)], 1.f + (fr01() - 0.5f) * 0.12f); break;
    case SP_DOG: tintHex(t, DC[(int)(r * 5.99f)], 1.05f); break;
    case SP_CAT: tintHex(t, CC[(int)(r * 5.99f)], 1.05f); break;
    case SP_HEN: tintHex(t, KC[(int)(r * 3.99f)], 1.05f); break;
    case SP_SHEEP: if (r < 0.12f) tintHex(t, 0x3a3430); else tintHex(t, 0xeee6d4, 1.f + (fr01() - 0.5f) * 0.1f); break;
    case SP_COW: tintHex(t, OC[(int)(r * 3.99f)], 1.f); break;
    case SP_DEER: tintHex(t, DE[(int)(r * 3.99f)], 1.1f); break;
    default: tintHex(t, RC[(int)(r * 3.99f)], 1.1f); break; }
  for (int i = 0; i < 3; i++) f.ltint[i] = t[i];
  return &f;
}
void faunaInit() {
  nFA = 0; nBD = 0; frng = Rng(4242);
  for (size_t i = 0; i < settles.size() && nFA < MAXF - 130; i++) { const Settle& s = settles[i]; if (s.type > ST_HAMLET) continue; int hens = s.type == ST_CAPITAL ? 6 : s.type == ST_VILLAGE ? 5 : 3, dogs = s.type == ST_CAPITAL ? 3 : s.type == ST_VILLAGE ? 2 : 1, cats = s.type == ST_CAPITAL ? 3 : s.type == ST_VILLAGE ? 2 : 1;
    int sheep = s.type == ST_CAPITAL ? 5 : s.type == ST_VILLAGE ? 4 : 3, cows = s.type == ST_CAPITAL ? 2 : s.type == ST_VILLAGE ? 3 : 2, horses = s.type == ST_CAPITAL ? 3 : s.type == ST_VILLAGE ? 2 : 1;
    auto scatter = [&](int sp, int n, float r0, float r1) { for (int k = 0, tries = 0; k < n && tries < n * 14; tries++) { float a = fr01() * TAU, r = r0 + (r1 - r0) * fr01(); if (addFauna(sp, s.x + std::cos(a) * r, s.z + std::sin(a) * r)) k++; } };
    scatter(SP_HEN, hens, 4.f, s.r * 0.75f); scatter(SP_DOG, dogs, 4.f, s.r * 0.8f); scatter(SP_CAT, cats, 4.f, s.r * 0.8f);
    scatter(SP_SHEEP, sheep, s.r * 0.95f, s.r * 1.5f + 14.f); scatter(SP_COW, cows, s.r * 1.0f, s.r * 1.6f + 16.f); scatter(SP_HORSE, horses, s.r * 0.7f, s.r * 1.4f + 14.f); }
  for (int g = 0, tries = 0; g < 30 && tries < 5000 && nFA < MAXF - 10; tries++) { float x = (fr01() * 2.f - 1.f) * (HALF - 150.f), z = (fr01() * 2.f - 1.f) * (HALF - 150.f); float ht = height(x, z); if (ht < 8.f || ht > 85.f || settleNear(x, z, 60) >= 0 || forestAt(x, z) < 0.35f || !validSpot(x, z, 0.5f)) continue;
    int n = 3 + (int)(fr01() * 4.f), got = 0; for (int k = 0, t2 = 0; k < n && t2 < n * 8; t2++) { float a = fr01() * TAU, r = 2.f + fr01() * 8.f; Fauna* f = addFauna(SP_DEER, x + std::cos(a) * r, z + std::sin(a) * r); if (f) { f->homeX = x; f->homeZ = z; k++; got++; } } if (got) g++; }
  for (int g = 0, tries = 0; g < 46 && tries < 6000 && nFA < MAXF - 6; tries++) { float x = (fr01() * 2.f - 1.f) * (HALF - 150.f), z = (fr01() * 2.f - 1.f) * (HALF - 150.f); float ht = height(x, z); if (ht < 4.f || ht > 70.f || forestAt(x, z) > 0.7f || waterAt(x, z) > -1e8f || !validSpot(x, z, 0.4f)) continue;
    int n = 2 + (int)(fr01() * 3.f), got = 0; for (int k = 0, t2 = 0; k < n && t2 < n * 8; t2++) { float a = fr01() * TAU, r = 1.f + fr01() * 5.f; Fauna* f = addFauna(SP_RABBIT, x + std::cos(a) * r, z + std::sin(a) * r); if (f) { f->homeX = x; f->homeZ = z; k++; got++; } } if (got) g++; }
  auto herd = [&](int sp, int n, int herds) { for (int h = 0, tries = 0; h < herds && tries < 1600 && nFA < MAXF - 6; tries++) { float x = (fr01() * 2.f - 1.f) * (HALF - 120.f), z = (fr01() * 2.f - 1.f) * (HALF - 120.f); float ht = height(x, z); if (ht < 3.f || ht > 46.f || settleNear(x, z, 50) >= 0 || !validSpot(x, z, 0.4f)) continue;
      int made = 0; for (int k = 0, t2 = 0; k < n && t2 < n * 10; t2++) { float a = fr01() * TAU, r = 2.f + fr01() * 9.f; Fauna* f = addFauna(sp, x + std::cos(a) * r, z + std::sin(a) * r); if (f) { f->homeX = x; f->homeZ = z; f->tx = f->x; f->tz = f->z; k++; made++; } } if (made) h++; } };
  herd(SP_SHEEP, 6, 34); herd(SP_COW, 4, 18); herd(SP_HORSE, 4, 26);
  for (int i = 0; i < 40; i++) { Bird& b = BD[nBD++]; b.cx = (fr01() * 2.f - 1.f) * (HALF - 60.f); b.cz = (fr01() * 2.f - 1.f) * (HALF - 60.f); b.r = 8.f + fr01() * 24.f; b.alt = 8.f + fr01() * 16.f; b.ph = fr01() * TAU; b.spd = 6.f + fr01() * 3.f; b.dir = fr01() < 0.5f ? 1.f : -1.f; b.sc = 1.f; b.crow = false;
    static const uint32_t BC[5] = {0x6a8ac8, 0xc8442a, 0x8a6a4a, 0xe8c040, 0x9a9a8e}; tintHex(b.tint, BC[i % 5]); b.gh = std::max(0.f, height(b.cx, b.cz)); }
  for (size_t i = 0, c = 0; i < settles.size() && c < 14; i++) { const Settle& s = settles[i]; if (s.type != ST_GRAVE && s.type != ST_RUINS && s.type != ST_CAMP) continue; for (int k = 0; k < 4 && nBD < 56; k++, c++) { Bird& b = BD[nBD++]; b.cx = s.x; b.cz = s.z; b.r = 14.f + fr01() * 14.f; b.alt = 11.f + fr01() * 10.f; b.ph = fr01() * TAU; b.spd = 7.f + fr01() * 2.f; b.dir = k % 2 ? 1.f : -1.f; b.sc = 2.2f; b.crow = true; tintHex(b.tint, 0x26262c); b.gh = std::max(0.f, height(b.cx, b.cz)); } }
  for (Bfly& b : BF) b.on = false; for (Petal& p : PT) p.on = false;
  { int cn[SP_N] = {0}; for (int i = 0; i < nFA; i++) cn[FA[i].sp]++; fprintf(stderr, "fauna: %d (dog %d cat %d hen %d sheep %d cow %d horse %d deer %d rabbit %d) birds %d\n", nFA, cn[0], cn[1], cn[2], cn[3], cn[4], cn[5], cn[6], cn[7], nBD); }
}
// ------------------------------------------------------------------ simulation
bool faunaStep(Fauna& f, float dt, float speed, float tx, float tz, float turn) {
  float dx = tx - f.x, dz = tz - f.z, d = std::sqrt(dx * dx + dz * dz); if (d < 0.06f) return true; float want = std::atan2(dx, dz); f.face += clampf(angDiff(f.face, want), -turn * dt, turn * dt);
  float st = std::min(d, speed * dt * f.sc); float nx = f.x + std::sin(f.face) * st, nz = f.z + std::cos(f.face) * st; float ox = nx, oz = nz; collideAny(nx, nz, 0.35f * f.sc); bool blocked = dist2(nx, nz, ox, oz) > 0.0004f;
  float dep; if (inWater(nx, nz, &dep) && dep > 0.35f) return false; if (badStep(nx, nz, f.y)) return false; f.x = nx; f.z = nz; f.spd = speed; return blocked ? false : d < 0.3f;
}
void faunaDmgText(float x, float y, float z, const char* txt, uint32_t col) { DmgTxt& d = DT[dtNext++ % 12]; d.x = x; d.y = y; d.z = z; d.t = 0; d.val = 0; d.zone = 0; d.kill = false; snprintf(d.txt, sizeof d.txt, "%s", txt); d.col = col; }
void faunaHurt(int i, float dmg, int zone, float fx, float fz, bool byPlayer) {
  Fauna& f = FA[i]; if (!f.alive) return; const SpDef& sd = SPD[f.sp]; dmg *= ZMUL[zone & 3]; f.hp -= dmg; f.hitT = 0.3f; f.fleeT = 7.f; f.state = 2; float dx = f.x - fx, dz = f.z - fz, l = std::sqrt(dx * dx + dz * dz) + 1e-4f; float nx = f.x + dx / l * 0.35f, nz = f.z + dz / l * 0.35f; collideAny(nx, nz, 0.3f); if (!badStep(nx, nz, f.y)) { f.x = nx; f.z = nz; }
  bool kill = f.hp <= 0; if (byPlayer) { static const char* ZT[4] = {"%d", "%d HEADSHOT", "%d LEG", "%d LEG"}; char b[20]; snprintf(b, sizeof b, ZT[zone & 3], (int)(dmg + 0.5f)); faunaDmgText(f.x, f.y + sd.hipY * f.sc * (zone == Z_HEAD ? 1.5f : 1.f), f.z, b, kill ? 0xff6a50 : zone == Z_HEAD ? 0xffd040 : 0xffffff); H.hitT = 0.3f; H.hitZone = zone; H.hitKill = kill; }
  if (!kill && f.sp == SP_DOG && f.pet) { f.state = 3; }
  if (kill) { f.alive = false; f.deadT = 0; f.pet = f.pet && false; audio::play(S_DIE, 0, 0.45f);
    if (byPlayer) { townOnAnimal(); if (sd.meat > 0) { H.food += sd.meat; char b[20]; snprintf(b, sizeof b, "+%d FOOD", sd.meat); faunaDmgText(f.x, f.y + 0.9f, f.z, b, 0xa8e07a); }
      for (int k = 0; k < MAXN; k++) { Npc& n = npcs[k]; if (!n.used || !n.alive || n.asleep || n.role != R_VILL || n.interior) continue; if (dist2(n.x, n.z, f.x, f.z) < 20.f * 20.f && (f.sp == SP_DOG || f.sp == SP_CAT || f.sp == SP_HORSE)) n.panic = std::max(n.panic, 6.f); } } }
}
void capsuleOf(const Fauna& f, V3& a, V3& b, float& r) { const SpDef& d = SPD[f.sp]; const Ana& A = ANA[f.sp]; float sn = std::sin(f.face), cs = std::cos(f.face); float yy = (A.bodyY - f.lie * (A.bodyY - A.lieY)) * f.sc;
  a = V3{f.x - sn * d.halfLen * f.sc, f.y + yy, f.z - cs * d.halfLen * f.sc}; b = V3{f.x + sn * d.halfLen * f.sc, f.y + yy, f.z + cs * d.halfLen * f.sc}; r = d.rad * f.sc; }
bool faunaSegHit(const Fauna& f, V3 p0, V3 p1, float pad, int& zone) {
  V3 a, b; float r; capsuleOf(f, a, b, r); V3 ab = b - a; float L2 = dot(ab, ab) + 1e-6f; const Ana& A = ANA[f.sp]; float sn = std::sin(f.face), cs = std::cos(f.face);
  float hdY = A.hdY * (1.f - 0.55f * f.graze - 0.3f * f.lie) * f.sc; V3 hc{f.x + sn * A.hdZ * f.sc, f.y + hdY, f.z + cs * A.hdZ * f.sc}; float hr = A.hdR * f.sc;
  for (int s = 0; s <= 4; s++) { V3 q = p0 + (p1 - p0) * (s / 4.f);
    if (A.head >= 0 && len(q - hc) < hr + pad) { zone = Z_HEAD; return true; }
    float u = clampf(dot(q - a, ab) / L2, 0, 1); V3 c = a + ab * u; if (len(q - c) < r + pad) { if (q.y < f.y + A.fy * 0.45f * f.sc) zone = Z_LEG; else zone = Z_TORSO; return true; } }
  return false;
}
bool faunaArrow(Arr& a, V3 p0, V3 p1) {
  if (CI.in) return false; for (int i = 0; i < nFA; i++) { Fauna& f = FA[i]; if (!f.used || !f.alive || std::fabs(f.x - p1.x) > 4.f || std::fabs(f.z - p1.z) > 4.f) continue; int z = Z_TORSO; if (faunaSegHit(f, p0, p1, 0.04f, z)) { a.stuck = true; a.npc = -1; a.p = p1; a.life = 1.4f; a.v = V3{}; audio::play(S_ARROWHIT, 0, 0.6f); faunaHurt(i, a.dmg, z, p0.x, p0.z, true); return true; } }
  return false;
}
bool faunaMelee(float dmg, float reach, float ox, float oy, float oz, float fx, float fz) {
  if (CI.in) return false; bool hit = false; for (int i = 0; i < nFA; i++) { Fauna& f = FA[i]; if (!f.used || !f.alive) continue; float dx = f.x - ox, dz = f.z - oz, d = std::sqrt(dx * dx + dz * dz); if (d > reach + f.sc * 0.5f || (dx * fx + dz * fz) / (d + 1e-4f) < 0.1f) continue;
    int zn = Z_TORSO; float pit = clampf(P.pitch * 0.9f, -0.3f, 0.4f); V3 dir{fx * std::cos(pit), std::sin(pit), fz * std::cos(pit)}; bool got = false; for (int k = -1; k <= 1 && !got; k++) { float yw = std::atan2(fx, fz) + k * 0.3f; V3 dd{std::sin(yw) * std::cos(pit), std::sin(pit), std::cos(yw) * std::cos(pit)}; int z2 = Z_TORSO; if (faunaSegHit(f, V3{ox, oy, oz}, V3{ox, oy, oz} + dd * (reach + 0.8f), 0.12f, z2)) { got = true; zn = z2; } } (void)dir;
    faunaHurt(i, dmg, zn, ox, oz, true); hit = true; }
  return hit;
}
int faunaNear(float r) { int best = -1; float bd = r * r; for (int i = 0; i < nFA; i++) { const Fauna& f = FA[i]; if (!f.used || !f.alive) continue; float d = dist2(f.x, f.z, P.x, P.z); if (d < bd) { bd = d; best = i; } } return best; }
void faunaPrompt() {
  if (CI.in || K.mounted) return; int i = faunaNear(3.2f); if (i < 0) return; const Fauna& f = FA[i]; if (f.sp >= SP_DEER) return; static const char* PR[SP_N] = {"E  BEFRIEND THE DOG", "E  PET THE CAT", "E  SHOO THE HEN", "E  PET THE SHEEP", "E  MILK THE COW", "E  MOUNT THE HORSE", "", ""};
  snprintf(H.prompt, sizeof H.prompt, "%s", f.sp == SP_DOG && f.pet ? "E  PRAISE YOUR DOG" : PR[f.sp]);
}
bool faunaInteract() {
  if (CI.in || K.mounted) return false; for (const Npc& n : npcs) if (n.used && n.alive && n.role < R_MON && !n.interior && dist2(n.x, n.z, P.x, P.z) < 2.6f * 2.6f) return false;
  int i = faunaNear(3.2f); if (i < 0) return false; Fauna& f = FA[i]; if (f.sp >= SP_DEER) return false;
  if (f.sp == SP_DOG) { if (!f.pet) { for (int k = 0; k < nFA; k++) FA[k].pet = false; f.pet = true; f.state = 0; banner("A NEW COMPANION", "YOUR DOG WILL FOLLOW YOU AND BITE MONSTERS", 3.5f); } else say("DOG", "Woof! Woof! (it wags its tail)"); f.wag = 3.f; }
  else if (f.sp == SP_CAT) say("CAT", "Mrrow. (purrs and rubs against your leg)"); else if (f.sp == SP_HEN) { say("HEN", "Bawk bawk BAWK!"); f.fleeT = 3.f; f.state = 2; }
  else if (f.sp == SP_SHEEP) { say("SHEEP", "Baaaa. (it leans into your hand)"); f.act = 0; }
  else if (f.sp == SP_COW) { if (f.biteCd > 0.f) say("COW", "Mooo. (she has nothing left to give just now)"); else { f.biteCd = 45.f; H.food += 3; say("COW", "Mooo. (fresh milk: +3 food)"); } }
  else if (!P.swim && !P.fly) { rideH = true; K.mounted = true; rideF = f; rideF.used = true; rideF.sc = 1.f; rideF.act = 0; rideF.lie = 0; rideF.graze = 0; mountT = 0.f; dmT = 0.f; mountFx = P.x; mountFz = P.z; rideX = f.x; rideZ = f.z; rideFace = f.face; f.used = false; P.speed = 0; banner("MOUNTED", "K TO DISMOUNT - GALLOP, SHOOT, CAST", 2.6f); }
  audio::play(S_CLICK, 0, 0.4f); return true;
}
void faunaDismount() {
  rideH = false; dmT = 0.45f; mountT = 1.f; Fauna* slot = nullptr; for (int i = 0; i < nFA; i++) if (!FA[i].used) { slot = &FA[i]; break; } if (!slot && nFA < MAXF) slot = &FA[nFA++]; if (!slot) return;
  *slot = rideF; slot->used = true; slot->alive = true; slot->x = slot->homeX = slot->tx = rideX; slot->z = slot->homeZ = slot->tz = rideZ; slot->y = height(rideX, rideZ); slot->face = rideFace; slot->state = 0; slot->fleeT = 0; slot->t = 4.f; slot->pet = false; slot->sc = 1.f;
  float sx = std::cos(rideFace) * 1.45f, sz = -std::sin(rideFace) * 1.45f; float nx = P.x + sx, nz = P.z + sz; collideAny(nx, nz, 0.5f); if (!badStep(nx, nz, P.y)) { P.x = nx; P.z = nz; } else { nx = P.x - sx; nz = P.z - sz; collideAny(nx, nz, 0.5f); if (!badStep(nx, nz, P.y)) { P.x = nx; P.z = nz; } }
}
void faunaUpdate(float dt) {
  if (dmT > 0.f) dmT = std::max(0.f, dmT - dt);
  if (rideH) { if (!K.mounted) faunaDismount(); else if (!CI.in) {
      if (mountT < 1.f) { mountT = std::min(1.f, mountT + dt / 0.95f); float e = smooth(0.f, 1.f, mountT); P.x = mountFx + (rideF.x - mountFx) * e; P.z = mountFz + (rideF.z - mountFz) * e; P.face += angDiff(P.face, rideF.face) * std::min(1.f, dt * 5.f); P.speed = 0; rideF.y = height(rideF.x, rideF.z); gaitUpdate(rideF, dt, 0.f); }
      else { rideF.x = P.x; rideF.z = P.z; rideF.y = P.y; rideF.face = P.face; gaitUpdate(rideF, dt, dbgSpd >= 0.f ? dbgSpd : P.speed); rideF.headP += (-0.05f - rideF.headP) * std::min(1.f, dt * 3.f); rideF.look += (0.f - rideF.look) * std::min(1.f, dt * 3.f); }
      rideX = rideF.x; rideZ = rideF.z; rideFace = rideF.face; } }
  if (CI.in) return; bool night = clockH > 20.5f || clockH < 5.2f;
  Npc* mon = nullptr;
  for (int i = 0; i < nFA; i++) { Fauna& f = FA[i]; if (!f.used) continue; float pd2 = dist2(f.x, f.z, P.x, P.z);
    if (!f.alive) { f.deadT += dt; if (f.deadT > 45.f) f.used = false; continue; }
    if (f.pet && pd2 > 70.f * 70.f) { float a = P.yaw + PI + (fr01() - 0.5f) * 1.2f; f.x = P.x + std::sin(a) * 4.f; f.z = P.z + std::cos(a) * 4.f; f.y = height(f.x, f.z); pd2 = 16.f; }
    if (pd2 > 190.f * 190.f && !f.pet) continue;
    const SpDef& sd = SPD[f.sp]; f.hitT = std::max(0.f, f.hitT - dt); f.t -= dt; f.biteCd -= dt; f.wag = std::max(0.f, f.wag - dt); float pd = std::sqrt(pd2); float cy = height(f.x, f.z); f.y += (cy - f.y) * std::min(1.f, dt * 12.f);
    float spdNow = 0; bool moving = false; bool wild = f.sp == SP_DEER || f.sp == SP_RABBIT; bool fleeing = false; float rr = 0.f;
    if (f.pet) {   // companion dog: follow, guard, bite
      mon = nullptr; float md = 18.f * 18.f; for (int k = 0; k < nMonL; k++) { Npc& m = npcs[monList[k]]; if (!m.alive || m.role != R_MON || m.interior) continue; float d = dist2(m.x, m.z, P.x, P.z); if (d < md) { md = d; mon = &m; } }
      if (mon) { float d = std::sqrt(dist2(mon->x, mon->z, f.x, f.z)); if (d > 1.7f) { faunaStep(f, dt, sd.run, mon->x, mon->z, 9.f); spdNow = sd.run; moving = true; } else { f.face += clampf(angDiff(f.face, std::atan2(mon->x - f.x, mon->z - f.z)), -9.f * dt, 9.f * dt); if (f.biteCd <= 0) { f.biteCd = 1.1f; hurtNpc(*mon, 7.f, f.x, f.z, false, Z_LEG); audio::play(S_ARROWHIT, 0, 0.25f); } } }
      else if (pd > 3.4f) { float sp = pd > 9.f ? sd.run : pd > 5.f ? 3.4f : sd.walk * 1.4f; faunaStep(f, dt, sp, P.x, P.z, 7.f); spdNow = sp; moving = true; f.act = 0; }
      else { f.face += clampf(angDiff(f.face, std::atan2(P.x - f.x, P.z - f.z)), -3.f * dt, 3.f * dt); f.wag = std::max(f.wag, 0.5f); }
    } else {
      float threat = 0, ax = 0, az = 0; if (sd.flee > 0 && P.deadT <= 0) { bool sprintNow = P.speed > 5.2f || K.mounted; bool armed = B.draw > 0 || P.atkT >= 0; bool still = P.speed < 1.3f;
        if (wild) { rr = armed ? sd.flee * 0.9f : sprintNow ? sd.flee * 0.75f : still ? sd.flee * 0.16f : sd.flee * 0.42f; if (f.sp == SP_RABBIT) rr *= 0.8f; }
        else if (armed) rr = sd.flee * 0.9f; else if (sprintNow) rr = sd.flee * 0.55f; else rr = (f.sp == SP_HEN ? 1.5f : f.sp == SP_CAT ? 1.4f : f.sp == SP_SHEEP ? 0.6f : 0.f);   // calm animals: walk up to them, they only bolt if you charge, shoot or swing
        if (f.hp < f.maxHp * 0.99f) rr = std::max(rr, sd.flee); if (pd < rr) { threat = 1; ax = f.x - P.x; az = f.z - P.z; } }
      if (!threat) for (int k = 0; k < nMonL; k++) { const Npc& m = npcs[monList[k]]; if (!m.alive || m.role != R_MON || m.interior) continue; float d = dist2(m.x, m.z, f.x, f.z); if (d < 14.f * 14.f) { threat = 1; ax = f.x - m.x; az = f.z - m.z; break; } }
      if (threat > 0 || f.fleeT > 0) { fleeing = true; if (threat > 0) { f.fleeT = std::max(f.fleeT, wild ? 3.5f : 1.2f); float l = std::sqrt(ax * ax + az * az) + 1e-4f; float fd = wild ? 38.f : 14.f; f.tx = f.x + ax / l * fd + (fr01() - 0.5f) * 5.f; f.tz = f.z + az / l * fd + (fr01() - 0.5f) * 5.f; f.state = 2; f.act = 0; } f.fleeT -= dt;
        float sp = sd.run * (f.hp < f.maxHp * 0.99f ? 0.9f : 1.f); if (f.sp == SP_RABBIT && std::fmod(tAbs * 3.f + f.ph, 2.f) < 0.3f) { f.tx += (fr01() - 0.5f) * 6.f; } if (!faunaStep(f, dt, sp, f.tx, f.tz, wild ? 10.f : 8.f)) { f.tx = f.x + (fr01() - 0.5f) * 16.f; f.tz = f.z + (fr01() - 0.5f) * 16.f; } spdNow = sp; moving = true; if (f.fleeT <= 0) { f.state = 0; f.t = 1.f + fr01() * 3.f; } }
      else if (f.state == 1) { f.act = 0; if (faunaStep(f, dt, sd.walk, f.tx, f.tz, 3.f) || f.t < -9.f) { f.state = 0; f.t = 2.f + fr01() * 6.f; } else { spdNow = sd.walk; moving = true; } if (spdNow == 0 && f.state == 1) { f.state = 0; f.t = 1.f; } }
      else if ((f.sp == SP_HORSE || f.sp == SP_DOG) && f.state == 0 && pd < 20.f && pd > 2.0f && P.speed < 4.4f && P.deadT <= 0 && !K.mounted) { float sp = pd > 9.f ? sd.walk * 1.2f : sd.walk * 0.8f; faunaStep(f, dt, sp, P.x, P.z, 3.f); spdNow = sp; moving = true; f.homeX = f.x; f.homeZ = f.z; f.act = 0; }   // curious: come over to be mounted / petted
      else if ((f.sp == SP_HORSE || f.sp == SP_DOG) && f.state == 0 && pd <= 2.0f && !K.mounted) { f.face += clampf(angDiff(f.face, std::atan2(P.x - f.x, P.z - f.z)), -2.5f * dt, 2.5f * dt); f.homeX = f.x; f.homeZ = f.z; f.act = 0; if (f.sp == SP_DOG) f.wag = std::max(f.wag, 0.6f); }
      else if (wild && pd < std::max(rr, sd.flee * 0.16f) * 2.6f + 4.f && P.deadT <= 0) { f.alert = 1.f; f.act = 0; f.face += clampf(angDiff(f.face, std::atan2(P.x - f.x, P.z - f.z)), -2.2f * dt, 2.2f * dt); }   // alert: freeze, stare, ears up
      else { if (f.t <= 0) { float r = fr01(); float lieP = (f.sp == SP_CAT ? 0.4f : f.sp == SP_COW || f.sp == SP_SHEEP ? 0.2f : f.sp == SP_DEER || f.sp == SP_DOG ? 0.14f : f.sp == SP_HORSE ? 0.04f : 0.f) * (night ? 2.6f : 1.f); bool grazer = f.sp == SP_SHEEP || f.sp == SP_COW || f.sp == SP_HORSE || f.sp == SP_DEER || f.sp == SP_RABBIT || f.sp == SP_HEN;
          f.act = 0; float dur = 2.f + fr01() * 5.f; if (grazer && r < (f.sp == SP_HEN ? 0.6f : 0.5f)) { f.act = 1; dur = f.sp == SP_HEN ? 2.f + fr01() * 3.f : 6.f + fr01() * 10.f; } else if (r < 0.5f + lieP && lieP > 0.f && pd > 4.f) { f.act = 2; dur = 12.f + fr01() * 26.f; }
          else if (r < 0.88f) { float a = fr01() * TAU, rd = 2.f + fr01() * sd.wander; f.tx = f.homeX + std::cos(a) * rd; f.tz = f.homeZ + std::sin(a) * rd; f.state = 1; dur = 0.f; } f.t = dur; }
        if (f.act == 2 && pd < 3.2f) { f.act = 0; f.t = 1.f; } }
      if (!wild || (pd > std::max(rr, sd.flee * 0.16f) * 2.6f + 4.f)) f.alert = 0.f;
    }
    // head / body state: look at the player, graze, lie down, alert
    { bool still = !moving && !fleeing; bool watch = still && f.act != 2 && P.deadT <= 0 && pd < (wild ? 40.f : (f.sp == SP_DOG || f.sp == SP_HORSE) ? 16.f : 11.f);
      float lookT = 0.f, headT = 0.f; if (watch) { float d = angDiff(f.face, std::atan2(P.x - f.x, P.z - f.z)); lookT = clampf(d, -0.95f, 0.95f); if (std::fabs(d) > 1.f && f.act == 0 && f.alert > 0.f) f.face += clampf(d - clampf(d, -1.f, 1.f), -1.f, 1.f) * dt * 1.5f; headT = f.alert > 0.f ? (f.sp == SP_DEER ? -0.22f : -0.1f) : (pd < 6.f ? -0.05f : 0.f); if (f.act == 1 && pd > 7.f) lookT *= 0.3f; }
      else if (still && f.act != 2) lookT = 0.28f * std::sin(tAbs * 0.35f + f.ph * 3.f);
      f.look += (lookT - f.look) * std::min(1.f, dt * (f.alert > 0.f ? 6.f : 2.5f)); f.headP += (headT - f.headP) * std::min(1.f, dt * 3.f);
      float gT = (still && f.act == 1 && !(watch && f.alert > 0.f)) ? 1.f : 0.f, lT = (still && f.act == 2) ? 1.f : 0.f; f.graze += (gT - f.graze) * std::min(1.f, dt * 2.2f); f.lie += (lT - f.lie) * std::min(1.f, dt * 1.1f); if (moving) { f.graze = std::max(0.f, f.graze - dt * 3.f); f.lie = 0.f; } }
    f.spd = spdNow; gaitUpdate(f, dt, spdNow); if (moving) f.ph += dt * 0.2f; else f.ph += dt * 0.02f; }
  // butterflies: stay close to the player over flowery ground, by day
  if (clockH > 6.f && clockH < 18.6f && !CI.in) { for (int i = 0; i < 44; i++) { Bfly& b = BF[i]; if (b.on && dist2(b.ax, b.az, P.x, P.z) > 46.f * 46.f) b.on = false;
      if (!b.on) { for (int tr = 0; tr < 5; tr++) { float a = fr01() * TAU, r = 5.f + fr01() * 36.f; float x = P.x + std::cos(a) * r, z = P.z + std::sin(a) * r; if (std::fabs(x) > HALF - 8 || std::fabs(z) > HALF - 8) continue; float h = height(x, z); if (h < 1.5f || h > 70.f || waterAt(x, z) > -1e8f) continue;
          if (!flowery(x, z)) continue; b.ax = x; b.az = z; b.ph = fr01() * TAU; b.on = true; static const uint32_t BC[6] = {0xf08a2a, 0xf2dc3a, 0x5a9af0, 0xf4f4ee, 0xe86aa8, 0xa07af0}; tintHex(b.tint, BC[(int)(fr01() * 5.99f)], 1.15f); break; } }
      if (b.on) { float t = tAbs * 0.6f + b.ph; b.x = b.ax + std::sin(t * 0.7f) * 2.6f + std::sin(t * 1.9f) * 0.6f; b.z = b.az + std::cos(t * 0.55f) * 2.6f + std::cos(t * 1.7f) * 0.6f; b.y = height(b.x, b.z) + 0.55f + 0.35f * std::sin(t * 1.3f) + 0.15f * std::sin(tAbs * 3.f + b.ph); } } }
  else for (Bfly& b : BF) b.on = false;
  // falling petals / leaves around the camera
  for (Petal& p : PT) { if (p.on) { p.ph += dt; p.y -= p.fall * dt; p.x += (std::sin(p.ph * 0.9f) * 0.5f + 0.45f) * dt; p.z += (std::cos(p.ph * 0.7f) * 0.45f + 0.2f) * dt; if (p.y < height(p.x, p.z) + 0.04f || dist2(p.x, p.z, P.x, P.z) > 30.f * 30.f) p.on = false; }
    if (!p.on) { float a = fr01() * TAU, r = 4.5f + fr01() * 24.f; float x = P.x + std::cos(a) * r, z = P.z + std::sin(a) * r; if (std::fabs(x) > HALF - 8 || std::fabs(z) > HALF - 8) continue; float h = height(x, z); if (h < 1.f || h > 80.f) continue;
      float zc, zj, zb, za; zones(x, z, h, zc, zj, zb, za); float fm = forestAt(x, z); float q = fr01(); p.sz = 1.f;
      if (zc > 0.3f && fm > 0.25f && q < 0.9f) tintHex(p.tint, q < 0.45f ? 0xf6b4cc : q < 0.75f ? 0xfbd2e0 : 0xee8fb4, 1.1f);
      else if (za > 0.4f && fm > 0.3f && q < 0.6f) { tintHex(p.tint, q < 0.2f ? 0xd8742a : q < 0.4f ? 0xc8402a : 0xe0a030); p.sz = 1.3f; }
      else if (zj > 0.4f && q < 0.25f) { tintHex(p.tint, 0x4a9a38); p.sz = 1.5f; } else if (fm > 0.55f && q < 0.15f) { tintHex(p.tint, q < 0.07f ? 0x8aa840 : 0xd4a838); p.sz = 1.2f; } else continue;
      p.x = x; p.z = z; p.y = h + 2.f + fr01() * 9.f; p.ph = fr01() * TAU; p.fall = 0.45f + fr01() * 0.5f; p.spin = 1.f + fr01() * 3.f; p.on = true; } }
}
int appendFaunaDyn(xr::Dyn* out, int k, int cap) {
  if (CI.in) return k; auto put = [&](int kind, const M4& m, float r, float g, float b) { if (k >= cap) return; xr::Dyn& o = out[k++]; o.kind = kind; o.m = m; o.mode = 0.f; o.tint[0] = r; o.tint[1] = g; o.tint[2] = b; };
  if (rideH && K.mounted && P.view != 0) k = emitAnimal(out, k, cap, rideF, true, tAbs);
  for (int pass = 0; pass < 2; pass++) for (int i = 0; i < nFA; i++) { const Fauna& f = FA[i]; if (!f.used) continue; float d2 = dist2(f.x, f.z, P.x, P.z); float lim = (SPD[f.sp].legLen < 0.3f ? 70.f : 125.f); float nearR = 48.f; if (pass == 0 ? d2 > nearR * nearR : (d2 <= nearR * nearR || d2 > lim * lim)) continue; if (k > cap - 20) break; k = emitAnimal(out, k, cap, f, false, tAbs); }
  // birds
  if (clockH > 5.6f && clockH < 19.4f) for (int i = 0; i < nBD; i++) { const Bird& b = BD[i]; if (k > cap - 40) break; float ang = b.dir * tAbs * b.spd / b.r + b.ph; float x = b.cx + std::cos(ang) * b.r, z = b.cz + std::sin(ang) * b.r; if (dist2(x, z, P.x, P.z) > 240.f * 240.f) continue;
    float y = b.gh + b.alt + std::sin(tAbs * 0.45f + b.ph) * 1.8f; float face = std::atan2(-b.dir * std::sin(ang), b.dir * std::cos(ang)); float flap = std::sin(tAbs * (b.crow ? 9.f : 17.f) + b.ph) * 0.75f * (std::sin(tAbs * 0.8f + b.ph * 2.f) > -0.3f ? 1.f : 0.2f);
    M4 base = mul(translate(x, y, z), mul(rotY(face), mul(rotZ(-b.dir * 0.28f), scale4(b.sc, b.sc, b.sc)))); put(K_FA_BIRD, base, b.tint[0], b.tint[1], b.tint[2]); float ws = 0.16f * b.sc;
    put(K_FA_WING, mul(base, mul(rotZ(flap + 0.1f), scale4(ws, ws, ws * 1.2f))), b.tint[0] * 0.85f, b.tint[1] * 0.85f, b.tint[2] * 0.85f); put(K_FA_WING, mul(base, mul(rotZ(-flap - 0.1f), mul(rotY(PI), scale4(ws, ws, ws * 1.2f)))), b.tint[0] * 0.85f, b.tint[1] * 0.85f, b.tint[2] * 0.85f); }
  // butterflies
  for (int i = 0; i < 44; i++) { const Bfly& b = BF[i]; if (!b.on || k > cap - 6) continue; float t = tAbs * 0.6f + b.ph; float face = std::atan2(std::cos(t * 0.7f) * 0.7f, -std::sin(t * 0.55f) * 0.55f); float flap = std::sin(tAbs * 20.f + b.ph * 3.f) * 0.95f + 0.25f; M4 base = mul(translate(b.x, b.y, b.z), rotY(face)); float ws = 0.085f;
    put(K_FA_WING, mul(base, mul(rotZ(flap), scale4(ws, ws, ws))), b.tint[0], b.tint[1], b.tint[2]); put(K_FA_WING, mul(base, mul(rotZ(-flap), mul(rotY(PI), scale4(ws, ws, ws)))), b.tint[0], b.tint[1], b.tint[2]); }
  // petals + leaves
  for (const Petal& p : PT) { if (!p.on || k >= cap) continue; float a = p.ph * p.spin; M4 m = mul(translate(p.x, p.y, p.z), mul(rotY(a), mul(rotX(0.9f * std::sin(p.ph * 2.3f)), mul(rotZ(0.7f * std::cos(p.ph * 1.7f)), scale4(p.sz, p.sz, p.sz))))); put(K_PETAL, m, p.tint[0], p.tint[1], p.tint[2]); }
  return k;
}

