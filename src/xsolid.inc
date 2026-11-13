// v2.8 solid creatures: the player cannot walk through people, monsters or animals, and creatures keep a body-width apart
// (included in xgame.cpp after xanim.inc; static pools only, nothing allocates per frame).
static const float FA_RAD[SP_N] = {0.32f, 0.2f, 0.16f, 0.46f, 0.68f, 0.62f, 0.46f, 0.16f};
inline float npcRad(const Npc& n) { float r = 0.34f * n.sc; if (n.role >= R_MON) r = std::max(0.46f, 0.4f * n.sc); if (n.boss) r *= 1.5f; return r; }
inline float faRad(const Fauna& f) { return FA_RAD[f.sp] * f.sc; }
// push (x,z) out of every live creature within reach; creatures give way a little so crowds flow
bool creaturePush(float& x, float& z, float r, float y, bool shove) {
  bool hit = false;
  for (int i = 0; i < MAXN; i++) { Npc& n = npcs[i]; if (!n.used || !n.alive || n.asleep) continue; if (n.interior != CI.in) continue; float dx = x - n.x, dz = z - n.z; float rr = r + npcRad(n); if (dx * dx + dz * dz >= rr * rr) continue; if (std::fabs(n.y - y) > 1.5f) continue;
    float d = std::sqrt(dx * dx + dz * dz); if (d < 1e-3f) { dx = std::sin(P.face); dz = std::cos(P.face); d = 1.f; } float pen = rr - d; x += dx / d * pen * (shove && !n.boss ? 0.7f : 1.f); z += dz / d * pen * (shove && !n.boss ? 0.7f : 1.f); if (shove && !n.boss) { n.x -= dx / d * pen * 0.3f; n.z -= dz / d * pen * 0.3f; } hit = true; }
  if (!CI.in) for (int i = 0; i < nFA; i++) { Fauna& f = FA[i]; if (!f.used || !f.alive) continue; float dx = x - f.x, dz = z - f.z; float rr = r + faRad(f); if (dx * dx + dz * dz >= rr * rr) continue; if (std::fabs(f.y - y) > 1.6f) continue;
    float d = std::sqrt(dx * dx + dz * dz); if (d < 1e-3f) { dx = 1; dz = 0; d = 1.f; } float pen = rr - d; x += dx / d * pen * (shove ? 0.55f : 1.f); z += dz / d * pen * (shove ? 0.55f : 1.f); if (shove) { f.x -= dx / d * pen * 0.45f; f.z -= dz / d * pen * 0.45f; } hit = true; }
  return hit;
}
// creature <-> creature separation for everything near the player (cheap pair loop over a capped list)
void solidStep(float dt) {
  struct CR { float* x; float* z; float r; float y; bool heavy; }; static CR L[200]; int n = 0; const float R2 = 48.f * 48.f; (void)dt;
  for (int i = 0; i < MAXN && n < 200; i++) { Npc& m = npcs[i]; if (!m.used || !m.alive || m.asleep || m.interior != CI.in) continue; if (dist2(m.x, m.z, P.x, P.z) > R2) continue; L[n++] = {&m.x, &m.z, npcRad(m), m.y, m.boss || m.onWall}; }
  if (!CI.in) for (int i = 0; i < nFA && n < 200; i++) { Fauna& f = FA[i]; if (!f.used || !f.alive) continue; if (dist2(f.x, f.z, P.x, P.z) > R2) continue; L[n++] = {&f.x, &f.z, faRad(f), f.y, false}; }
  for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) { float dx = *L[b].x - *L[a].x, dz = *L[b].z - *L[a].z, rr = L[a].r + L[b].r; if (dx * dx + dz * dz >= rr * rr) continue; if (std::fabs(L[a].y - L[b].y) > 1.5f) continue;
    float d = std::sqrt(dx * dx + dz * dz); if (d < 1e-3f) { dx = (float)((a * 7 + b) % 5) - 2.f + 0.37f; dz = 1.f; d = std::sqrt(dx * dx + dz * dz); } float pen = std::min((rr - d) * 0.5f, 0.06f); float ka = L[a].heavy ? 0.f : (L[b].heavy ? 1.f : 0.5f), kb = L[b].heavy ? 0.f : (L[a].heavy ? 1.f : 0.5f); float ux = dx / d * pen * 2.f, uz = dz / d * pen * 2.f;
    *L[a].x -= ux * ka; *L[a].z -= uz * ka; *L[b].x += ux * kb; *L[b].z += uz * kb; }
}
// ---- v3.2 dynamic collision self-test: `--xcmd bumpN[@deg]` sprints the player straight at the nearest NPC (0), animal (1), cottage (2),
// gate tower (3), barn (4), chapel (5), wall segment (6) from 7 m away and prints how close the body got (LF_BUMP lines). Debug only, no per-frame cost when off.
int dbgBump = -1, dbgBumpT = 0, dbgBumpIdx = -1; float dbgBumpMin = 1e9f, dbgBumpTx = 0, dbgBumpTz = 0, dbgBumpLim = 0, dbgBumpY0 = 0;
void bumpSetup(int kind, float deg) {
  float bd = 1e30f; int bi = -1; float tx = 0, tz = 0, lim = 0; float a = deg * 0.0174533f;
  if (kind == 0) { for (int i = 0; i < MAXN; i++) { Npc& n = npcs[i]; if (!n.used || !n.alive || n.asleep || n.role >= R_MON || n.interior != CI.in) continue; float d = dist2(n.x, n.z, P.x, P.z); if (d < bd) { bd = d; bi = i; } } if (bi >= 0) { tx = npcs[bi].x; tz = npcs[bi].z; lim = npcRad(npcs[bi]) + 0.4f; npcs[bi].hp = npcs[bi].maxHp = 99999; } }
  else if (kind == 1) { for (int i = 0; i < nFA; i++) { Fauna& f = FA[i]; if (!f.used || !f.alive || f.sp == SP_HEN) continue; float d = dist2(f.x, f.z, P.x, P.z); if (d < bd) { bd = d; bi = i; } } if (bi >= 0) { tx = FA[bi].x; tz = FA[bi].z; lim = faRad(FA[bi]) + 0.4f; } }
  else { int want = kind == 2 ? xw::K_COTTAGE : kind == 3 ? xw::K_GATE : kind == 4 ? xw::K_BARN : kind == 5 ? xw::K_CHAPEL : xw::K_WALL; for (size_t i = 0; i < objs.size(); i++) { if (objs[i].kind != want) continue; float d = dist2(objs[i].x, objs[i].z, P.x, P.z); if (d < bd) { bd = d; bi = (int)i; } } if (bi >= 0) { tx = objs[bi].x; tz = objs[bi].z; lim = 0.f; } }
  if (bi < 0) { std::printf("LF_BUMP kind %d: no target\n", kind); return; }
  dbgBump = kind; dbgBumpIdx = bi; dbgBumpT = 0; dbgBumpMin = 1e9f; dbgBumpLim = lim; dbgBumpTx = tx; dbgBumpTz = tz;
  P.x = tx - std::sin(a) * 7.f; P.z = tz - std::cos(a) * 7.f; P.y = height(P.x, P.z); P.yaw = a; P.face = a; P.vx = P.vz = 0; dbgWalk = 2; dbgBumpY0 = P.y;
}
void bumpTick() {
  if (dbgBump < 0) return; float tx = dbgBumpTx, tz = dbgBumpTz;
  if (dbgBump == 0 && dbgBumpIdx >= 0) { tx = npcs[dbgBumpIdx].x; tz = npcs[dbgBumpIdx].z; } else if (dbgBump == 1 && dbgBumpIdx >= 0) { tx = FA[dbgBumpIdx].x; tz = FA[dbgBumpIdx].z; }
  if (dbgBump == 0 && dbgBumpIdx >= 0) { npcs[dbgBumpIdx].x = dbgBumpTx; npcs[dbgBumpIdx].z = dbgBumpTz; } else if (dbgBump == 1 && dbgBumpIdx >= 0) { FA[dbgBumpIdx].x = dbgBumpTx; FA[dbgBumpIdx].z = dbgBumpTz; }
  float d = std::sqrt(dist2(tx, tz, P.x, P.z)); if (d < dbgBumpMin) dbgBumpMin = d; dbgBumpT++;
  if (dbgBumpT == 360) { float x = P.x, z = P.z; bool inside = xw::collide(x, z, 0.2f, P.y); float moved = std::sqrt(dist2(x, z, P.x, P.z));
    std::printf("LF_BUMP kind %d target (%.1f,%.1f) end (%.1f,%.1f) minDist %.2f limit %.2f %s | final dist %.2f, standing-inside push %.2f m, dy %.2f -> %s\n", dbgBump, dbgBumpTx, dbgBumpTz, P.x, P.z, dbgBumpMin, dbgBumpLim, dbgBump < 2 ? (dbgBumpMin >= dbgBumpLim * 0.8f ? "SOLID" : "PASSED-THROUGH") : "", d, moved, P.y - dbgBumpY0, (dbgBump < 2 ? dbgBumpMin >= dbgBumpLim * 0.8f : moved < 0.05f) ? "OK" : "FAIL"); (void)inside; dbgBump = -1; dbgWalk = 0; }
}
