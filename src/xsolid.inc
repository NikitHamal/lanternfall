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
