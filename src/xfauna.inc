// v1.9 living world: fauna + ambient life. Included inside the anonymous namespace of xgame.cpp (after xkingdom.inc).
// Dogs, cats, hens, sheep, cows, horses (articulated procedural parts through xr::Dyn), songbirds + crows, butterflies, falling petals / leaves.
// Zero per-frame heap allocations: fixed pools, no string formatting except on events.
enum Sp { SP_DOG, SP_CAT, SP_HEN, SP_SHEEP, SP_COW, SP_HORSE, SP_N };
struct SpDef { const char* name; int body; float hipY, legLen, legW, legZ, legX, halfLen, rad, walk, run, flee, hp, wander, headU; int nLegs; uint32_t legCol; int meat; };
const SpDef SPD[SP_N] = {
  {"DOG", K_FA_DOG, 0.47f, 0.47f, 0.045f, 0.24f, 0.09f, 0.42f, 0.2f, 1.3f, 5.6f, 0.f, 30, 10, 0.78f, 4, 0xc4985c, 0},
  {"CAT", K_FA_CAT, 0.27f, 0.27f, 0.028f, 0.15f, 0.06f, 0.26f, 0.13f, 0.8f, 4.6f, 7.f, 12, 8, 0.75f, 4, 0xd8c8b0, 0},
  {"HEN", K_FA_HEN, 0.21f, 0.21f, 0.012f, 0.02f, 0.045f, 0.14f, 0.13f, 0.8f, 3.4f, 4.5f, 6, 5, 0.7f, 2, 0xd89a30, 2},
  {"SHEEP", K_FA_SHEEP, 0.52f, 0.52f, 0.04f, 0.33f, 0.15f, 0.45f, 0.36f, 1.0f, 4.8f, 9.f, 24, 14, 0.8f, 4, 0x3a3028, 8},
  {"COW", K_FA_COW, 1.0f, 1.0f, 0.075f, 0.62f, 0.28f, 0.85f, 0.52f, 0.9f, 3.9f, 5.f, 60, 12, 0.8f, 4, 0xe9e4da, 14},
  {"HORSE", K_FA_HORSE, 1.05f, 1.05f, 0.06f, 0.62f, 0.22f, 0.8f, 0.45f, 1.8f, 8.6f, 11.f, 70, 30, 0.8f, 4, 0xffffff, 10}};
struct Fauna { bool used = false, alive = true, pet = false; int sp = 0, state = 0; float x = 0, y = 0, z = 0, face = 0, sc = 1, hp = 10, maxHp = 10, homeX = 0, homeZ = 0, tx = 0, tz = 0, t = 0, spd = 0, ph = 0, hitT = 0, deadT = 0, fleeT = 0, biteCd = 0, tint[3] = {1, 1, 1}, ltint[3] = {1, 1, 1}, pitch = 0; };
constexpr int MAXF = 560; Fauna FA[MAXF]; int nFA = 0;
bool rideH = false; float rideTint[3] = {1, 1, 1}, rideLeg[3] = {1, 1, 1}, ridePh = 0, rideX = 0, rideZ = 0, rideFace = 0;   // riding a procedural horse (E on a horse)
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
  float r = fr01();
  if (sp == SP_HORSE) { static const uint32_t HC[6] = {0x8a4a22, 0x5a3418, 0x2a2420, 0xb8b4aa, 0xe8e4dc, 0xd8b070}; uint32_t c = HC[(int)(r * 5.99f)]; tintHex(f.tint, c); tintHex(f.ltint, c); }
  else if (sp == SP_DOG) { if (r < 0.25f) { f.tint[0] = f.tint[1] = f.tint[2] = 0.35f; tintHex(f.ltint, 0xc4985c, 0.35f); } else if (r < 0.5f) { f.tint[0] = 1.5f; f.tint[1] = 1.45f; f.tint[2] = 1.35f; f.ltint[0] *= 1.5f; f.ltint[1] *= 1.45f; f.ltint[2] *= 1.35f; } }
  else if (sp == SP_CAT) { if (r < 0.3f) { f.tint[0] = f.tint[1] = f.tint[2] = 0.25f; tintHex(f.ltint, 0xd8c8b0, 0.25f); } else if (r < 0.6f) { f.tint[0] = 1.3f; f.tint[1] = 0.85f; f.tint[2] = 0.55f; f.ltint[0] *= 1.3f; f.ltint[1] *= 0.85f; f.ltint[2] *= 0.55f; } }
  else if (sp == SP_HEN) { if (r < 0.4f) { f.tint[0] = 0.9f; f.tint[1] = 0.6f; f.tint[2] = 0.45f; } else if (r < 0.55f) { f.tint[0] = f.tint[1] = f.tint[2] = 0.4f; } }
  else if (sp == SP_SHEEP) { if (r < 0.12f) f.tint[0] = f.tint[1] = f.tint[2] = 0.35f; }
  else if (sp == SP_COW) { if (r < 0.3f) { f.tint[0] = 1.1f; f.tint[1] = 0.8f; f.tint[2] = 0.55f; f.ltint[0] = f.tint[0]; f.ltint[1] = f.tint[1]; f.ltint[2] = f.tint[2]; } }
  return &f;
}
void faunaInit() {
  nFA = 0; nBD = 0; frng = Rng(4242);
  for (size_t i = 0; i < settles.size() && nFA < MAXF - 130; i++) { const Settle& s = settles[i]; if (s.type > ST_HAMLET) continue; int hens = s.type == ST_CAPITAL ? 6 : s.type == ST_VILLAGE ? 5 : 3, dogs = s.type == ST_CAPITAL ? 3 : s.type == ST_VILLAGE ? 2 : 1, cats = s.type == ST_CAPITAL ? 3 : s.type == ST_VILLAGE ? 2 : 1;
    int sheep = s.type == ST_CAPITAL ? 5 : s.type == ST_VILLAGE ? 4 : 3, cows = s.type == ST_CAPITAL ? 2 : s.type == ST_VILLAGE ? 3 : 2, horses = s.type == ST_CAPITAL ? 3 : s.type == ST_VILLAGE ? 2 : 1;
    auto scatter = [&](int sp, int n, float r0, float r1) { for (int k = 0, tries = 0; k < n && tries < n * 14; tries++) { float a = fr01() * TAU, r = r0 + (r1 - r0) * fr01(); if (addFauna(sp, s.x + std::cos(a) * r, s.z + std::sin(a) * r)) k++; } };
    scatter(SP_HEN, hens, 4.f, s.r * 0.75f); scatter(SP_DOG, dogs, 4.f, s.r * 0.8f); scatter(SP_CAT, cats, 4.f, s.r * 0.8f);
    scatter(SP_SHEEP, sheep, s.r * 0.95f, s.r * 1.5f + 14.f); scatter(SP_COW, cows, s.r * 1.0f, s.r * 1.6f + 16.f); scatter(SP_HORSE, horses, s.r * 0.7f, s.r * 1.4f + 14.f); }
  auto herd = [&](int sp, int n, int herds) { for (int h = 0, tries = 0; h < herds && tries < 400 && nFA < MAXF - 6; tries++) { float x = (fr01() * 2.f - 1.f) * (HALF - 120.f), z = (fr01() * 2.f - 1.f) * (HALF - 120.f); float ht = height(x, z); if (ht < 3.f || ht > 46.f || settleNear(x, z, 50) >= 0 || !validSpot(x, z, 0.4f)) continue;
      int made = 0; for (int k = 0, t2 = 0; k < n && t2 < n * 10; t2++) { float a = fr01() * TAU, r = 2.f + fr01() * 9.f; Fauna* f = addFauna(sp, x + std::cos(a) * r, z + std::sin(a) * r); if (f) { f->homeX = x; f->homeZ = z; f->tx = f->x; f->tz = f->z; k++; made++; } } if (made) h++; } };
  herd(SP_SHEEP, 5, 12); herd(SP_COW, 4, 6); herd(SP_HORSE, 3, 9);
  for (int i = 0; i < 40; i++) { Bird& b = BD[nBD++]; b.cx = (fr01() * 2.f - 1.f) * (HALF - 60.f); b.cz = (fr01() * 2.f - 1.f) * (HALF - 60.f); b.r = 8.f + fr01() * 24.f; b.alt = 8.f + fr01() * 16.f; b.ph = fr01() * TAU; b.spd = 6.f + fr01() * 3.f; b.dir = fr01() < 0.5f ? 1.f : -1.f; b.sc = 1.f; b.crow = false;
    static const uint32_t BC[5] = {0x6a8ac8, 0xc8442a, 0x8a6a4a, 0xe8c040, 0x9a9a8e}; tintHex(b.tint, BC[i % 5]); b.gh = std::max(0.f, height(b.cx, b.cz)); }
  for (size_t i = 0, c = 0; i < settles.size() && c < 14; i++) { const Settle& s = settles[i]; if (s.type != ST_GRAVE && s.type != ST_RUINS && s.type != ST_CAMP) continue; for (int k = 0; k < 4 && nBD < 56; k++, c++) { Bird& b = BD[nBD++]; b.cx = s.x; b.cz = s.z; b.r = 14.f + fr01() * 14.f; b.alt = 11.f + fr01() * 10.f; b.ph = fr01() * TAU; b.spd = 7.f + fr01() * 2.f; b.dir = k % 2 ? 1.f : -1.f; b.sc = 2.2f; b.crow = true; tintHex(b.tint, 0x26262c); b.gh = std::max(0.f, height(b.cx, b.cz)); } }
  for (Bfly& b : BF) b.on = false; for (Petal& p : PT) p.on = false;
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
void capsuleOf(const Fauna& f, V3& a, V3& b, float& r) { const SpDef& d = SPD[f.sp]; float sn = std::sin(f.face), cs = std::cos(f.face); float yy = d.hipY * f.sc * (d.nLegs == 2 ? 1.f : 1.05f) + (d.nLegs == 2 ? 0.f : 0.08f * f.sc) + (f.sp == SP_COW ? 0.18f * f.sc : f.sp == SP_HORSE ? 0.28f * f.sc : f.sp == SP_SHEEP ? 0.2f * f.sc : 0.f);
  a = V3{f.x - sn * d.halfLen * f.sc, f.y + yy, f.z - cs * d.halfLen * f.sc}; b = V3{f.x + sn * d.halfLen * f.sc, f.y + yy, f.z + cs * d.halfLen * f.sc}; r = d.rad * f.sc; }
bool faunaSegHit(const Fauna& f, V3 p0, V3 p1, float pad, int& zone) {
  V3 a, b; float r; capsuleOf(f, a, b, r); V3 ab = b - a; float L2 = dot(ab, ab) + 1e-6f;
  for (int s = 0; s <= 4; s++) { V3 q = p0 + (p1 - p0) * (s / 4.f); float u = clampf(dot(q - a, ab) / L2, 0, 1); V3 c = a + ab * u; if (len(q - c) < r + pad) { const SpDef& d = SPD[f.sp]; if (u > SPD[f.sp].headU && q.y > f.y + d.hipY * f.sc * 0.9f) zone = Z_HEAD; else if (q.y < f.y + d.hipY * f.sc * 0.5f) zone = Z_LEG; else zone = Z_TORSO; return true; } }
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
  if (CI.in || K.mounted) return; int i = faunaNear(3.0f); if (i < 0) return; const Fauna& f = FA[i]; static const char* PR[SP_N] = {"E  BEFRIEND THE DOG", "E  PET THE CAT", "E  SHOO THE HEN", "E  PET THE SHEEP", "E  PET THE COW", "E  MOUNT THE HORSE"};
  snprintf(H.prompt, sizeof H.prompt, "%s", f.sp == SP_DOG && f.pet ? "E  PRAISE YOUR DOG" : PR[f.sp]);
}
bool faunaInteract() {
  if (CI.in || K.mounted) return false; for (const Npc& n : npcs) if (n.used && n.alive && n.role < R_MON && !n.interior && dist2(n.x, n.z, P.x, P.z) < 2.6f * 2.6f) return false;
  int i = faunaNear(3.0f); if (i < 0) return false; Fauna& f = FA[i];
  if (f.sp == SP_DOG) { if (!f.pet) { for (int k = 0; k < nFA; k++) FA[k].pet = false; f.pet = true; f.state = 0; banner("A NEW COMPANION", "YOUR DOG WILL FOLLOW YOU AND BITE MONSTERS", 3.5f); } else say("DOG", "Woof! Woof! (it wags its tail)"); }
  else if (f.sp == SP_CAT) say("CAT", "Mrrow. (purrs and rubs against your leg)"); else if (f.sp == SP_HEN) { say("HEN", "Bawk bawk BAWK!"); f.fleeT = 3.f; f.state = 2; }
  else if (f.sp == SP_SHEEP) say("SHEEP", "Baaaa."); else if (f.sp == SP_COW) say("COW", "Mooo."); else if (!P.swim && !P.fly) { rideH = true; K.mounted = true; for (int k = 0; k < 3; k++) { rideTint[k] = f.tint[k]; rideLeg[k] = f.ltint[k]; } rideX = f.x; rideZ = f.z; f.used = false; P.x = f.x; P.z = f.z; banner("MOUNTED", "K TO DISMOUNT - GALLOP, SHOOT, CAST", 2.6f); }
  audio::play(S_CLICK, 0, 0.4f); return true;
}
void faunaDismount() {
  rideH = false; Fauna* slot = nullptr; for (int i = 0; i < nFA; i++) if (!FA[i].used) { slot = &FA[i]; break; } if (!slot && nFA < MAXF) slot = &FA[nFA++]; if (!slot) return;
  float x = rideX - std::sin(rideFace) * 1.6f, z = rideZ - std::cos(rideFace) * 1.6f; collideAny(x, z, 0.6f); *slot = Fauna(); slot->used = true; slot->sp = SP_HORSE; slot->x = slot->homeX = slot->tx = x; slot->z = slot->homeZ = slot->tz = z; slot->y = height(x, z); slot->face = rideFace; slot->sc = 1.f; slot->maxHp = slot->hp = (float)SPD[SP_HORSE].hp; slot->t = 3.f;
  for (int k = 0; k < 3; k++) { slot->tint[k] = rideTint[k]; slot->ltint[k] = rideLeg[k]; } }
void faunaUpdate(float dt) {
  if (rideH) { if (!K.mounted) faunaDismount(); else if (!CI.in) { ridePh += dt * (P.speed / 1.05f) * 1.9f; rideX = P.x; rideZ = P.z; rideFace = P.face; } }
  if (CI.in) return; bool day = clockH > 5.4f && clockH < 19.6f; (void)day;
  Npc* mon = nullptr;
  for (int i = 0; i < nFA; i++) { Fauna& f = FA[i]; if (!f.used) continue; float pd2 = dist2(f.x, f.z, P.x, P.z);
    if (!f.alive) { f.deadT += dt; if (f.deadT > 45.f) f.used = false; continue; }
    if (f.pet && pd2 > 70.f * 70.f) { float a = P.yaw + PI + (fr01() - 0.5f) * 1.2f; f.x = P.x + std::sin(a) * 4.f; f.z = P.z + std::cos(a) * 4.f; f.y = height(f.x, f.z); pd2 = 16.f; }
    if (pd2 > 190.f * 190.f && !f.pet) continue;
    const SpDef& sd = SPD[f.sp]; f.hitT = std::max(0.f, f.hitT - dt); f.t -= dt; f.biteCd -= dt; float pd = std::sqrt(pd2); float cy = height(f.x, f.z); f.y += (cy - f.y) * std::min(1.f, dt * 12.f);
    float spdNow = 0; bool moving = false;
    if (f.pet) {   // companion dog: follow, guard, bite
      mon = nullptr; float md = 18.f * 18.f; for (int k = 0; k < nMonL; k++) { Npc& m = npcs[monList[k]]; if (!m.alive || m.role != R_MON || m.interior) continue; float d = dist2(m.x, m.z, P.x, P.z); if (d < md) { md = d; mon = &m; } }
      if (mon) { float d = std::sqrt(dist2(mon->x, mon->z, f.x, f.z)); if (d > 1.7f) { faunaStep(f, dt, sd.run, mon->x, mon->z, 9.f); spdNow = sd.run; moving = true; } else { f.face += clampf(angDiff(f.face, std::atan2(mon->x - f.x, mon->z - f.z)), -9.f * dt, 9.f * dt); if (f.biteCd <= 0) { f.biteCd = 1.1f; hurtNpc(*mon, 7.f, f.x, f.z, false, Z_LEG); audio::play(S_ARROWHIT, 0, 0.25f); } } }
      else if (pd > 3.4f) { float sp = pd > 9.f ? sd.run : pd > 5.f ? 3.4f : sd.walk * 1.4f; faunaStep(f, dt, sp, P.x, P.z, 7.f); spdNow = sp; moving = true; }
      else { f.face += clampf(angDiff(f.face, std::atan2(P.x - f.x, P.z - f.z)), -3.f * dt, 3.f * dt); }
    } else {
      float threat = 0, ax = 0, az = 0; if (sd.flee > 0 && P.deadT <= 0) { float rr; { bool sprintNow = P.speed > 5.2f || K.mounted; bool armed = B.draw > 0 || P.atkT >= 0;   // calm animals: walk up to them, they only bolt if you charge, shoot or swing
        if (armed) rr = sd.flee * 0.9f; else if (sprintNow) rr = sd.flee * 0.55f; else rr = (f.sp == SP_HEN ? 1.5f : f.sp == SP_CAT ? 1.4f : f.sp == SP_SHEEP ? 1.2f : 0.f); if (f.hp < f.maxHp * 0.99f) rr = std::max(rr, sd.flee); } if (pd < rr) { threat = 1; ax = f.x - P.x; az = f.z - P.z; } }
      if (!threat) for (int k = 0; k < nMonL; k++) { const Npc& m = npcs[monList[k]]; if (!m.alive || m.role != R_MON || m.interior) continue; float d = dist2(m.x, m.z, f.x, f.z); if (d < 14.f * 14.f) { threat = 1; ax = f.x - m.x; az = f.z - m.z; break; } }
      if (threat > 0 || f.fleeT > 0) { if (threat > 0) { f.fleeT = std::max(f.fleeT, 1.2f); float l = std::sqrt(ax * ax + az * az) + 1e-4f; f.tx = f.x + ax / l * 14.f + (fr01() - 0.5f) * 5.f; f.tz = f.z + az / l * 14.f + (fr01() - 0.5f) * 5.f; f.state = 2; } f.fleeT -= dt;
        float sp = sd.run * (f.hp < f.maxHp * 0.99f ? 0.9f : 1.f); if (!faunaStep(f, dt, sp, f.tx, f.tz, 8.f)) { f.tx = f.x + (fr01() - 0.5f) * 16.f; f.tz = f.z + (fr01() - 0.5f) * 16.f; } spdNow = sp; moving = true; if (f.fleeT <= 0) { f.state = 0; f.t = 1.f + fr01() * 3.f; } }
      else if (f.state == 1) { if (faunaStep(f, dt, sd.walk, f.tx, f.tz, 3.f) || f.t < -9.f) { f.state = 0; f.t = 2.f + fr01() * 6.f; } else { spdNow = sd.walk; moving = true; } if (spdNow == 0 && f.state == 1) { f.state = 0; f.t = 1.f; } }
      else if ((f.sp == SP_HORSE || f.sp == SP_DOG) && f.state == 0 && pd < 20.f && pd > 2.0f && P.speed < 4.4f && P.deadT <= 0 && !K.mounted) { float sp = pd > 9.f ? sd.walk * 1.2f : sd.walk * 0.8f; faunaStep(f, dt, sp, P.x, P.z, 3.f); spdNow = sp; moving = true; f.homeX = f.x; f.homeZ = f.z; }   // curious: come over to be mounted / petted
      else if ((f.sp == SP_HORSE || f.sp == SP_DOG) && f.state == 0 && pd <= 2.0f && !K.mounted) { f.face += clampf(angDiff(f.face, std::atan2(P.x - f.x, P.z - f.z)), -2.5f * dt, 2.5f * dt); f.homeX = f.x; f.homeZ = f.z; }
      else { if (f.t <= 0) { float a = fr01() * TAU, r = 2.f + fr01() * sd.wander; f.tx = f.homeX + std::cos(a) * r; f.tz = f.homeZ + std::sin(a) * r; if (fr01() < 0.65f) { f.state = 1; f.t = 0.f; } else f.t = 2.f + fr01() * 6.f; } }
    }
    f.spd = spdNow; if (moving) f.ph += dt * (spdNow / std::max(0.2f, sd.legLen)) * (f.sp == SP_HEN ? 3.2f : 1.9f); else f.ph += dt * 1.6f;
    float wantPitch = 0.f; if (!moving && f.state == 0) { if (f.sp == SP_SHEEP || f.sp == SP_COW || f.sp == SP_HORSE) wantPitch = 0.3f + 0.04f * std::sin(tAbs * 1.4f + f.ph); else if (f.sp == SP_HEN) wantPitch = 0.45f * std::max(0.f, std::sin(tAbs * 5.f + f.ph)); } else if (moving && f.sp == SP_HORSE && spdNow > 4.f) wantPitch = -0.05f * std::sin(f.ph);
    f.pitch += (wantPitch - f.pitch) * std::min(1.f, dt * 6.f); }
  // butterflies: stay close to the player over flowery ground, by day
  if (clockH > 6.f && clockH < 18.6f && !CI.in) { for (int i = 0; i < 44; i++) { Bfly& b = BF[i]; if (b.on && dist2(b.ax, b.az, P.x, P.z) > 46.f * 46.f) b.on = false;
      if (!b.on) { for (int tr = 0; tr < 5; tr++) { float a = fr01() * TAU, r = 5.f + fr01() * 36.f; float x = P.x + std::cos(a) * r, z = P.z + std::sin(a) * r; if (std::fabs(x) > HALF - 8 || std::fabs(z) > HALF - 8) continue; float h = height(x, z); if (h < 1.5f || h > 70.f || waterAt(x, z) > -1e8f) continue;
          int gi = std::min(GS - 1, std::max(0, (int)((z + HALF) / CS))) * GS + std::min(GS - 1, std::max(0, (int)((x + HALF) / CS))); if (infoB[gi * 4] < 55 || infoA[gi * 4 + 3] < 90) continue; b.ax = x; b.az = z; b.ph = fr01() * TAU; b.on = true; static const uint32_t BC[6] = {0xf08a2a, 0xf2dc3a, 0x5a9af0, 0xf4f4ee, 0xe86aa8, 0xa07af0}; tintHex(b.tint, BC[(int)(fr01() * 5.99f)], 1.15f); break; } }
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
  if (rideH && K.mounted && P.view != 0 && k < cap - 8) { const SpDef& sd = SPD[SP_HORSE]; float sp = P.speed, amp = clampf(sp / sd.walk, 0.f, 1.f) * (sp > sd.walk * 2.2f ? 0.85f : 0.55f); float bob = std::fabs(std::sin(ridePh)) * 0.03f * std::min(1.f, sp / 3.f); const float rs = 1.0f;
    M4 base = mul(translate(P.x, P.y + bob, P.z), mul(rotY(P.face), scale4(rs, rs, rs))); float pit = sp > 4.f ? -0.05f * std::sin(ridePh) : 0.f; M4 bm = mul(base, mul(translate(0, sd.hipY, 0), mul(rotX(pit), translate(0, -sd.hipY, 0)))); put(K_FA_HORSE, bm, rideTint[0], rideTint[1], rideTint[2]);
    for (int l = 0; l < 4; l++) { float side = (l & 1) ? 1.f : -1.f, zz = l < 2 ? sd.legZ : -sd.legZ; float phase = (l == 0 || l == 3) ? ridePh : ridePh + PI; M4 lm = mul(base, mul(translate(side * sd.legX, sd.hipY, zz), mul(rotX(std::sin(phase) * amp), scale4(sd.legW, sd.legLen, sd.legW)))); put(K_FA_LEG, lm, rideLeg[0], rideLeg[1], rideLeg[2]); } }
  for (int i = 0; i < nFA; i++) { const Fauna& f = FA[i]; if (!f.used) continue; float d2 = dist2(f.x, f.z, P.x, P.z); if (d2 > 150.f * 150.f || k > cap - 8) continue; const SpDef& sd = SPD[f.sp]; float hit = f.hitT > 0 ? f.hitT * 2.4f : 0.f;
    float tr = f.tint[0] * (1.f + hit), tg = f.tint[1] * (1.f - hit * 0.3f), tb = f.tint[2] * (1.f - hit * 0.3f), lr = f.ltint[0] * (1.f + hit), lg = f.ltint[1] * (1.f - hit * 0.3f), lb = f.ltint[2] * (1.f - hit * 0.3f);
    float dead = f.alive ? 0.f : smooth(0.f, 0.45f, f.deadT), sink = f.alive ? 0.f : smooth(36.f, 45.f, f.deadT) * 0.6f; float bob = f.alive && f.spd > 0.2f ? std::fabs(std::sin(f.ph)) * 0.025f * f.sc * std::min(1.f, f.spd / 3.f) : 0.f;
    bool curl = f.alive && f.sp == SP_CAT && f.state == 0 && f.spd < 0.1f && f.fleeT <= 0 && f.t > 1.2f; float fold = curl ? 0.28f : 1.f; float sq = curl ? 0.78f : 1.f;
    M4 base = mul(translate(f.x, f.y + bob - sink, f.z), mul(rotY(f.face), mul(rotZ(dead * 1.5708f), scale4(f.sc, f.sc, f.sc)))); if (dead > 0) base = mul(translate(0, 0, 0), base);
    if (!f.alive) { float lift = sd.hipY * f.sc * 0.5f * dead; base = mul(translate(0, lift * 0.0f, 0), base); base.m[13] += sd.hipY * f.sc * 0.34f * dead; }
    M4 pit = mul(translate(0, sd.hipY, 0), mul(rotX(f.pitch), translate(0, -sd.hipY, 0))); M4 bm = mul(base, mul(pit, scale4(1.f, sq, 1.f))); put(sd.body, bm, tr, tg, tb);
    int nl = sd.nLegs; for (int l = 0; l < nl; l++) { float side = (l & 1) ? 1.f : -1.f; float zz = nl == 2 ? sd.legZ : (l < 2 ? sd.legZ : -sd.legZ); float phase = (l == 0 || l == 3 || nl == 2) ? f.ph : f.ph + PI; if (nl == 2 && l == 1) phase = f.ph + PI;
      float amp = f.alive ? clampf(f.spd / std::max(0.5f, sd.walk), 0.f, 1.f) * (f.spd > sd.walk * 1.8f ? 0.85f : 0.55f) : 0.15f; float sw = std::sin(phase) * amp; if (!f.alive) sw = (l & 1) ? 0.35f : -0.25f;
      M4 lm = mul(base, mul(translate(side * sd.legX * (nl == 2 ? 1.f : 1.f), sd.hipY * sq, zz), mul(rotX(sw), scale4(sd.legW, sd.legLen * fold, sd.legW)))); put(K_FA_LEG, lm, lr, lg, lb); } }
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
