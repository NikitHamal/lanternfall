// Explorer combat / interiors module. Included inside the anonymous namespace of xgame.cpp (after damagePlayer).
// Body-part hit zones, bow + arrows, crime & bounty, interior cells (enter houses), floating damage text.
enum Zone { Z_TORSO, Z_HEAD, Z_ARM, Z_LEG };
const float ZMUL[4] = {1.f, 2.6f, 0.6f, 0.5f};
struct Bow { int weapon = 0, arrows = 30; float draw = 0, rel = -1, regen = 0, zoom = 0, drawT = 0.85f; } B;
int gold = 0; float bounty[3] = {0, 0, 0}; float dbgSide = 0; int dbgFx = -1; bool dbgDrawHold = false; int dbgWalk = 0; int dbgShoot = -1, dbgFrames = 0;
struct DmgTxt { float x = 0, y = 0, z = 0, t = -1; int val = 0, zone = 0; bool kill = false; uint32_t col = 0xffffff; char txt[20] = {0}; } DT[12]; int huntCount = 0; int dtNext = 0;
struct Arr { bool on = false, stuck = false; V3 p, v, dir, off; float life = 0, dmg = 0; int npc = -1; } ARR[40];
bool faunaArrow(Arr& a, V3 p0, V3 p1); bool faunaMelee(float dmg, float reach, float ox, float oy, float oz, float fx, float fz);
struct Cell { bool in = false; int idx = 0, stage = 0, want = 0, house = -1; float ox = 0, oz = 0, fade = 0; } CI;
struct Enter { float x, y, z, ry, R; int kind, idx; }; std::vector<Enter> enters; std::vector<uint8_t> looted;
inline float gH(float x, float z) { return CI.in ? xi::BASEY : surfaceY(x, z, P.y); }
bool roomCollide(float& x, float& z, float r) {
  const xi::Def& d = xi::DEFS[CI.idx]; float lx = x - d.cx, lz = z - d.cz; bool m = false; float bx = d.hx - 0.4f - r * 0.5f, bz = d.hz - 0.35f - r * 0.5f;
  if (lx < -bx) { lx = -bx; m = true; } if (lx > bx) { lx = bx; m = true; } if (lz < -bz) { lz = -bz; m = true; } if (lz > bz) { lz = bz; m = true; }
  for (int i = 0; i < d.nc; i++) { float dx = lx - d.c[i].x, dz = lz - d.c[i].z, dd = std::sqrt(dx * dx + dz * dz), mr = d.c[i].r + r * 0.6f; if (dd < mr) { if (dd < 1e-4f) { dx = 1; dz = 0; dd = 1; } lx = d.c[i].x + dx / dd * mr; lz = d.c[i].z + dz / dd * mr; m = true; } }
  x = d.cx + lx; z = d.cz + lz; return m;
}
bool collideBuilds(float& x, float& z, float r, bool friendly, int* hitBuild = nullptr);
struct GNode; GNode* nearestGNode(float px, float pz, float maxR); bool harvestNode(GNode* g, bool swingAnim);
int nearestRift(float maxR, bool onlyCorrupt); bool tryPurifyRift(int ri);
bool collideAny(float& x, float& z, float r, float ey = -1e9f) {
  if (CI.in) return roomCollide(x, z, r);
  bool a = collide(x, z, r, ey), b = collideBuilds(x, z, r, true, nullptr);
  return a || b;
}
int natOf(const Npc& n) { int k = n.settle >= 0 ? settles[n.settle].nation : (CI.in ? 0 : nationAt(n.x, n.z)); return k < 0 ? 0 : k > 2 ? 2 : k; }
// ------------------------------------------------------------------ body zones (bone-accurate capsules from the live pose)
struct BoneIx { const FigAsset* a = nullptr; int ix[15]; bool ok = false; } BIX[48]; int nBix = 0;
const BoneIx& boneIx(const FigAsset* a) {
  for (int i = 0; i < nBix; i++) if (BIX[i].a == a) return BIX[i];
  BoneIx& b = BIX[nBix < 47 ? nBix++ : 47]; b.a = a; static const char* N[15] = {"head", "neck", "hips", "lUp", "lFore", "lHand", "rUp", "rFore", "rHand", "lThigh", "lShin", "lFoot", "rThigh", "rShin", "rFoot"};
  b.ok = true; for (int k = 0; k < 15; k++) { b.ix[k] = figBone(*a, N[k], -1); if (b.ix[k] < 0) b.ok = false; } return b;
}
struct Cap { V3 a, b; float r; int zone; };
int buildCaps(const Npc& n, Cap* c) {
  const FigAsset* fa = n.role >= R_MON ? &gFM[n.mon] : rigs[n.rig].a; float sc = n.sc; int k = 0;
  if (fa && fa->ok && n.an.have) { const BoneIx& bi = boneIx(fa); if (bi.ok) { const int* I = bi.ix; float cs = std::cos(n.face), sn = std::sin(n.face);
    auto Wp = [&](int b, float up) { const float* p = n.an.pose + b * 7 + 4; return V3{n.x + (cs * p[0] + sn * p[2]) * sc, n.y + (p[1] + up) * sc, n.z + (-sn * p[0] + cs * p[2]) * sc}; };
    c[k++] = {Wp(I[0], 0.03f), Wp(I[0], 0.2f), 0.165f * sc, Z_HEAD}; c[k++] = {Wp(I[2], -0.05f), Wp(I[1], 0.02f), 0.21f * sc, Z_TORSO};
    c[k++] = {Wp(I[3], 0), Wp(I[4], 0), 0.08f * sc, Z_ARM}; c[k++] = {Wp(I[4], 0), Wp(I[5], -0.04f), 0.07f * sc, Z_ARM}; c[k++] = {Wp(I[6], 0), Wp(I[7], 0), 0.08f * sc, Z_ARM}; c[k++] = {Wp(I[7], 0), Wp(I[8], -0.04f), 0.07f * sc, Z_ARM};
    c[k++] = {Wp(I[9], 0), Wp(I[10], 0), 0.105f * sc, Z_LEG}; c[k++] = {Wp(I[10], 0), Wp(I[11], 0), 0.085f * sc, Z_LEG}; c[k++] = {Wp(I[12], 0), Wp(I[13], 0), 0.105f * sc, Z_LEG}; c[k++] = {Wp(I[13], 0), Wp(I[14], 0), 0.085f * sc, Z_LEG}; return k; } }
  float H_ = fa ? fa->hpY * sc / std::max(0.01f, fa->scale) : 1.8f * sc; float r = std::max(0.25f * sc, 0.17f * H_); V3 o{n.x, n.y, n.z};
  c[k++] = {o + V3{0, H_ * 0.82f, 0}, o + V3{0, H_ * 0.98f, 0}, r * 0.8f, Z_HEAD}; c[k++] = {o + V3{0, H_ * 0.38f, 0}, o + V3{0, H_ * 0.78f, 0}, r, Z_TORSO}; c[k++] = {o + V3{0, 0.05f, 0}, o + V3{0, H_ * 0.36f, 0}, r * 0.8f, Z_LEG}; return k;
}
float segSeg2(V3 p1, V3 q1, V3 p2, V3 q2, float& s1) {
  V3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2; float a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r), s, t;
  if (a < 1e-9f && e < 1e-9f) { s1 = 0; return dot(r, r); }
  if (a < 1e-9f) { s = 0; t = clampf(f / e, 0, 1); } else { float c = dot(d1, r); if (e < 1e-9f) { t = 0; s = clampf(-c / a, 0, 1); } else { float b = dot(d1, d2), den = a * e - b * b; s = den > 1e-9f ? clampf((b * f - c * e) / den, 0, 1) : 0; t = (b * s + f) / e; if (t < 0) { t = 0; s = clampf(-c / a, 0, 1); } else if (t > 1) { t = 1; s = clampf((b - c) / a, 0, 1); } } }
  s1 = s; V3 d = (p1 + d1 * s) - (p2 + d2 * t); return dot(d, d);
}
bool hitNpc(const Npc& n, V3 p0, V3 p1, float pad, float& sOut, int& zone) {
  Cap cp[12]; int k = buildCaps(n, cp); float best = 2.f; zone = Z_TORSO;
  for (int i = 0; i < k; i++) { float s; float d2 = segSeg2(p0, p1, cp[i].a, cp[i].b, s); float r = cp[i].r + pad; if (d2 < r * r) { if (s < best - 0.0001f || (s < best + 0.02f && cp[i].zone == Z_HEAD)) { best = s; zone = cp[i].zone; } } }
  if (best > 1.5f) return false; sOut = best; return true;
}
// ------------------------------------------------------------------ damage, crime
void addDmgText(const Npc& t, float d, int zone, bool kill) {
  DmgTxt& x = DT[dtNext++ % 12]; const FigAsset* fa = t.role >= R_MON ? &gFM[t.mon] : rigs[t.rig].a; float hy = fa ? fa->hpY * t.sc / std::max(0.01f, fa->scale) : 1.8f; x.x = t.x; x.y = t.y + hy * (zone == Z_HEAD ? 1.0f : zone == Z_LEG ? 0.35f : 0.7f); x.z = t.z; x.t = 0; x.val = (int)(d + 0.5f); x.zone = zone; x.kill = kill; static const char* ZT[4] = {"%d", "%d HEADSHOT", "%d ARM", "%d LEG"}; snprintf(x.txt, sizeof x.txt, ZT[zone & 3], x.val); x.col = kill ? 0xff6a50 : zone == Z_HEAD ? 0xffd040 : zone == Z_TORSO ? 0xffffff : 0xcfd8e0;
}
void crime(const Npc& v, bool kill) {
  int nat = natOf(v); float add = kill ? 70.f : 20.f; if (v.role >= R_GUARD) add *= 1.6f; bool was = bounty[nat] > 12.f; bounty[nat] = std::min(250.f, bounty[nat] + add);
  for (int i = 0; i < MAXN; i++) { Npc& h = npcs[i]; if (!h.used || !h.alive || h.role >= R_MON || &h == &v) continue; float d = std::sqrt(dist2(h.x, h.z, v.x, v.z)); if (d > (h.role == R_VILL ? 26.f : 46.f)) continue;
    if (h.role == R_VILL) { if (h.brave && h.hp > h.maxHp * 0.6f && d < 9.f && kill) h.angry = 14.f; else h.panic = 9.f; } else { h.angry = 30.f; h.state = S_CHASE; } }
  if (!was && bounty[nat] > 12.f) banner("CRIME WITNESSED", "THE GUARDS OF THE REALM ARE HUNTING YOU", 4.f);
}
void hurtNpc(Npc& t, float d, float fx, float fz, bool byPlayer, int zone = Z_TORSO) {
  if (!t.alive) return; d *= ZMUL[zone]; t.hp -= d; t.hitT = 0.28f; float dx = t.x - fx, dz = t.z - fz, l = std::sqrt(dx * dx + dz * dz) + 1e-4f; float kb = t.boss ? 0.15f : (zone == Z_HEAD ? 0.7f : 0.5f); float nx = t.x + dx / l * kb, nz = t.z + dz / l * kb;
  if (t.interior) roomCollide(nx, nz, 0.4f); else { collide(nx, nz, 0.4f, t.y); if (badStep(nx, nz, t.y)) { nx = t.x; nz = t.z; } } t.x = nx; t.z = nz;
  if (zone == Z_LEG) t.limp = 4.5f; if (zone == Z_HEAD) t.stun = t.boss ? 0.12f : 0.7f;
  if (byPlayer && t.role >= R_MON) { t.tgtKind = 1; if (t.role != R_MON && t.state != S_CHASE) t.state = S_CHASE; }
  bool kill = t.hp <= 0;
  if (byPlayer) { addDmgText(t, d, zone, kill); H.hitT = 0.3f; H.hitZone = zone; H.hitKill = kill; }
  if (byPlayer && t.role < R_MON && !kill) { if (t.role == R_VILL) { if (t.brave && t.hp > t.maxHp * 0.45f) t.angry = 18.f; else t.panic = 10.f; } else { t.angry = 30.f; t.state = S_CHASE; } crime(t, false); }
  if (kill) { t.alive = false; t.deadT = 0; t.atkT = -1; audio::play(t.boss ? S_DIEBIG : S_DIE, 0, 0.8f); if (byPlayer && t.role >= R_MON) { H.kills++; townOnKill(t); if (t.boss) banner("BOSS SLAIN", t.name, 5.f); } if (byPlayer && t.role < R_MON) { crime(t, true); H.murders++; } else if (byPlayer) gold += t.boss ? 120 : 6 + (int)(t.maxHp * 0.08f); }
}
void playerAttack() {
  if (P.atkT >= 0 || P.deadT > 0 || P.fly) return; P.atkT = 0; P.hitDone = false; P.comboT = 1.2f; P.combo = (P.combo + 1) % 3; P.atkWind = 0.30f - 0.03f * P.combo; P.atkRec = 0.36f; audio::play(S_SWING, 0, 0.6f);
}
void playerHit() {
  const roster::Hero& hr = roster::at(roster::sel()); const FigAsset* pa = roster::cur(); float psc = pa ? pa->scale : 1.f; float dmg = 24.f * hr.dmg * (P.combo == 0 ? 1.3f : 1.f) * (P.speed > 5.f ? 1.2f : 1.f) * (1.f + 0.25f * (float)wpnTier); float reach = 3.2f; bool hit = false;
  float pit = P.combo == 2 ? 0.42f : P.combo == 1 ? -0.38f : clampf(P.pitch * 0.9f, -0.3f, 0.4f); V3 org{P.x, P.y + 1.3f * psc, P.z}; float fx = std::sin(P.face), fz = std::cos(P.face);
  for (int i = 0; i < MAXN; i++) { Npc& n = npcs[i]; if (!n.used || !n.alive || n.asleep) continue; float dx = n.x - P.x, dz = n.z - P.z, d = std::sqrt(dx * dx + dz * dz); if (d > reach + n.sc * 0.6f || (dx * fx + dz * fz) / (d + 1e-4f) < 0.15f) continue;
    bool got = false; int zone = Z_TORSO; for (int k = -2; k <= 2 && !got; k++) { float yw = P.face + k * 0.24f; V3 dir{std::sin(yw) * std::cos(pit), std::sin(pit), std::cos(yw) * std::cos(pit)}; float s; int z; if (hitNpc(n, org, org + dir * (reach + 0.7f), 0.1f, s, z)) { got = true; zone = z; } }
    if (!got) { got = true; zone = Z_TORSO; } hurtNpc(n, dmg, P.x, P.z, true, zone); hit = true; }
  if (faunaMelee(dmg, reach, P.x, P.y + 1.0f * psc, P.z, fx, fz)) hit = true;
  if (hit) audio::play(S_HITMETAL, 0, 0.7f);
  else if (!CI.in) {
    float hx = P.x + fx * 1.6f, hz = P.z + fz * 1.6f;
    int ri = nearestRift(3.8f, true);
    if (ri >= 0) tryPurifyRift(ri);
    else harvestNode(nearestGNode(hx, hz, 2.8f), false);
  }
}
// ------------------------------------------------------------------ bow
void fireArrow(float draw) {
  const roster::Hero& hr = roster::at(roster::sel()); const FigAsset* pa = roster::cur(); float psc = pa ? pa->scale : 1.f;
  float cp = std::cos(P.pitch), sp = std::sin(P.pitch); V3 look{std::sin(P.yaw) * cp, sp, std::cos(P.yaw) * cp}; V3 eye = P.eye; float t0 = P.view == 0 ? 0.6f : P.camDist * 0.9f + 0.5f, tmax = t0 + 110.f, bestT = tmax;
  for (float t = t0; t < tmax; t += 1.2f) { V3 q = eye + look * t; if (q.y < gH(q.x, q.z)) { bestT = t; break; } if (CI.in) { const xi::Def& d = xi::DEFS[CI.idx]; if (std::fabs(q.x - d.cx) > d.hx || std::fabs(q.z - d.cz) > d.hz || q.y > xi::BASEY + d.h) { bestT = t; break; } } }
  V3 a0 = eye + look * t0, a1 = eye + look * bestT;
  for (int i = 0; i < MAXN; i++) { Npc& n = npcs[i]; if (!n.used || !n.alive || n.asleep || dist2(n.x, n.z, P.x, P.z) > 130.f * 130.f) continue; float s; int z; if (hitNpc(n, a0, a1, 0.05f, s, z)) { float t = t0 + s * (bestT - t0); if (t < bestT) { bestT = t; a1 = eye + look * bestT; } } }
  V3 tgt = eye + look * bestT, right{-std::cos(P.yaw), 0, std::sin(P.yaw)}; V3 org = P.view == 0 ? eye + look * 0.5f + right * 0.12f + V3{0, -0.14f, 0} : V3{P.x, P.y + 1.45f * psc, P.z} + right * 0.25f + V3{std::sin(P.yaw) * 0.5f, 0, std::cos(P.yaw) * 0.5f};
  V3 dir = norm(tgt - org); if (bestT < 3.f) dir = look; float spread = (1.f - draw) * 0.05f + std::min(0.04f, P.speed * 0.004f); dir = norm(dir + V3{(rng() - 0.5f) * spread, (rng() - 0.5f) * spread, (rng() - 0.5f) * spread});
  float speed = 26.f + 38.f * std::pow(draw, 1.3f);
  for (Arr& a : ARR) if (!a.on) { a.on = true; a.stuck = false; a.p = org; a.v = dir * speed; a.dir = dir; a.life = 6.f; a.dmg = 20.f * hr.dmg * (0.3f + 0.9f * draw) * (hr.style == roster::ST_BOW ? 1.25f : 1.f) * (1.f + 0.25f * (float)wpnTier); a.npc = -1; break; }
  B.arrows--; B.rel = 0; audio::play(draw > 0.8f ? S_BOWHEAVY : S_BOW, 0, 0.7f);
}
void bowUpdate(float dt, const pl::Input& in) {
  const roster::Hero& hr = roster::at(roster::sel()); B.drawT = 0.85f / std::max(0.6f, hr.draw);
  bool can = B.weapon == 1 && !P.fly && !H.map && P.deadT <= 0 && CI.stage != 1; B.zoom += (((can && in.rmb) ? 1.f : 0.f) - B.zoom) * std::min(1.f, dt * 9.f);
  if (B.rel >= 0) { B.rel += dt; if (B.rel > 0.4f) B.rel = -1; } bool hold = in.lmb || dbgDrawHold;
  if (can && hold && B.arrows > 0 && B.rel < 0.12f) B.draw = std::min(1.f, B.draw + dt / B.drawT);
  else if (B.draw > 0) { if (can && B.draw > 0.18f && B.arrows > 0) fireArrow(B.draw); B.draw = 0; }
  B.regen += dt; if (B.regen > 3.f) { B.regen = 0; if (B.arrows < 30) B.arrows++; }
}
void arrowsUpdate(float dt) {
  for (Arr& a : ARR) { if (!a.on) continue; a.life -= dt; if (a.life <= 0) { a.on = false; continue; }
    if (a.stuck) { if (a.npc >= 0) { const Npc& n = npcs[a.npc]; if (!n.used) { a.on = false; continue; } a.p = V3{n.x, n.y, n.z} + a.off; } continue; }
    for (int s = 0; s < 2 && !a.stuck; s++) { float h = dt * 0.5f; V3 p0 = a.p; a.v.y -= 12.f * h; V3 p1 = p0 + a.v * h; if (len(a.v) > 0.1f) a.dir = norm(a.v);
      float bs = 2.f; int bi = -1, bz = 0; for (int i = 0; i < MAXN; i++) { Npc& n = npcs[i]; if (!n.used || !n.alive || n.asleep) continue; if (std::fabs(n.x - p1.x) > 7.f || std::fabs(n.z - p1.z) > 7.f) continue; float sp; int z; if (hitNpc(n, p0, p1, 0.02f, sp, z) && sp < bs) { bs = sp; bi = i; bz = z; } }
      if (bi >= 0) { V3 hp = p0 + (p1 - p0) * bs; Npc& n = npcs[bi]; a.stuck = true; a.npc = bi; a.off = hp - V3{n.x, n.y, n.z}; a.p = hp; a.life = 14.f; audio::play(S_ARROWHIT, 0, 0.7f); hurtNpc(n, a.dmg, n.x - a.v.x, n.z - a.v.z, true, bz); a.v = V3{}; break; }
      if (faunaArrow(a, p0, p1)) break;
      float g = gH(p1.x, p1.z); bool wall = false; if (CI.in) { const xi::Def& d = xi::DEFS[CI.idx]; wall = std::fabs(p1.x - d.cx) > d.hx - 0.1f || std::fabs(p1.z - d.cz) > d.hz - 0.1f || p1.y > xi::BASEY + d.h - 0.1f; for (int i = 0; i < d.nc && !wall; i++) { float dx = p1.x - (d.cx + d.c[i].x), dz = p1.z - (d.cz + d.c[i].z); if (dx * dx + dz * dz < d.c[i].r * d.c[i].r * 0.5f && p1.y < xi::BASEY + 1.1f) wall = true; } }
      else if (p1.y < g + 8.f) { float cx = p1.x, cz = p1.z; if (collide(cx, cz, 0.05f)) wall = true; }
      if (p1.y < g || wall) { a.stuck = true; a.npc = -1; a.p = wall ? p0 : V3{p1.x, std::max(g - 0.05f, p1.y), p1.z}; a.life = 26.f; audio::play(S_ARROWHIT, 0, 0.35f); a.v = V3{}; break; }
      a.p = p1; } }
}
int buildDyn(xr::Dyn* out, int cap) {
  int k = 0; for (const Arr& a : ARR) { if (!a.on || k >= cap) continue; V3 f = a.dir; V3 xx = cross(V3{0, 1, 0}, f); if (len(xx) < 1e-3f) xx = V3{1, 0, 0}; xx = norm(xx); V3 yy = cross(f, xx); V3 pos = a.p; if (a.stuck) pos = pos - f * (a.life < 2.f ? 0.5f : 0.36f);
    M4 m = ident(); m.m[0] = xx.x; m.m[1] = xx.y; m.m[2] = xx.z; m.m[4] = yy.x; m.m[5] = yy.y; m.m[6] = yy.z; m.m[8] = f.x; m.m[9] = f.y; m.m[10] = f.z; m.m[12] = pos.x; m.m[13] = pos.y; m.m[14] = pos.z; out[k].m = m; out[k].tint[0] = out[k].tint[1] = out[k].tint[2] = 1.f; k++; }
  return k;
}
// ------------------------------------------------------------------ interiors
void buildEnters() {
  enters.clear(); for (const Obj& o : objs) { int idx = -1; float R = 0; switch (o.kind) { case K_COTTAGE: idx = 0; R = 4.8f; break; case K_HOUSE: idx = 0; R = 6.4f; break; case K_BARN: idx = 1; R = 5.6f; break; case K_LONG: idx = 1; R = 9.5f; break; case K_CHAPEL: idx = 2; R = 5.8f; break; case K_KEEP: idx = 1; R = 12.f; break; case K_WATCH: idx = 0; R = 4.4f; break; default: continue; } enters.push_back({o.x, o.y, o.z, o.ry, R * o.sx, o.kind, idx}); }
  looted.assign(enters.size(), 0);
  for (int i = 0; i < 3; i++) { const xi::Def& d = xi::DEFS[i]; Obj o{}; o.x = d.cx; o.y = xi::BASEY; o.z = d.cz; o.ry = 0; o.sx = o.sy = o.sz = 1; o.kind = (uint8_t)(K_INHOME + i); o.r = o.g = o.b = 128; o.sway = 0; objs.push_back(o);
    lights.push_back({d.cx, xi::BASEY + 1.2f, d.cz - d.hz + 1.3f, 0.95f, 0.45f, 0.18f, 16.f + d.hx}); lights.push_back({d.cx + d.hx * 0.35f, xi::BASEY + 2.2f, d.cz + d.hz * 0.2f, 0.6f, 0.4f, 0.22f, 12.f + d.hx}); if (i > 0) lights.push_back({d.cx - d.hx * 0.35f, xi::BASEY + 2.4f, d.cz + d.hz * 0.2f, 0.6f, 0.4f, 0.22f, 12.f + d.hx}); }
}
int nearEnter() { if (CI.in) return -1; int best = -1; float bd = 1e9f; for (size_t i = 0; i < enters.size(); i++) { const Enter& e = enters[i]; float dx = e.x - P.x, dz = e.z - P.z; if (std::fabs(dx) > e.R || std::fabs(dz) > e.R) continue; float d = std::sqrt(dx * dx + dz * dz) - e.R; if (d < 0 && d < bd) { bd = d; best = (int)i; } } return best; }
void spawnResidents(int idx) {
  const xi::Def& d = xi::DEFS[idx]; int n = idx == 0 ? 1 + (int)(rng() * 2.f) : idx == 1 ? 3 : 1;
  for (int k = 0; k < n; k++) { float x = d.cx + (rng() - 0.5f) * d.hx * 1.2f, z = d.cz + (rng() - 0.3f) * d.hz * 1.0f - 0.5f; roomCollide(x, z, 0.5f); Npc* p = spawnHuman(idx == 1 && k == 0 ? R_KNIGHT : R_VILL, x, z, rng() * 6.28f, -1); if (!p) break; p->interior = true; p->y = xi::BASEY; p->homeX = x; p->homeZ = z; p->wander = 1.8f; if (idx == 2) snprintf(p->name, sizeof p->name, "%s", "Father Aldric"); }
}
void doEnter(int ei) {
  const Enter& e = enters[ei]; CI.in = true; CI.idx = e.idx; CI.house = ei; const xi::Def& d = xi::DEFS[e.idx]; P.x = d.cx; P.z = d.cz + d.hz - 1.5f; P.y = xi::BASEY; P.yaw = PI; P.face = PI; P.vx = P.vz = 0; P.vy = 0; P.ground = true; P.swim = false; P.camInit = false; P.camDist = std::min(P.camDist, 2.8f); spawnResidents(e.idx);
  snprintf(H.place, sizeof H.place, "%s", d.name); H.locT = 3.f;
}
void doExit() {
  CI.in = false; for (Npc& n : npcs) if (n.used && n.interior) n.used = false; P.x = CI.ox; P.z = CI.oz; P.y = height(P.x, P.z); P.vx = P.vz = 0; P.camInit = false; for (Arr& a : ARR) if (a.on && a.p.y > 500.f) a.on = false; lastPlaceId = -99;
}
void cellUpdate(float dt) {
  if (CI.stage == 1) { CI.fade = std::min(1.f, CI.fade + dt * 4.5f); if (CI.fade >= 1.f) { if (CI.want > 0) doEnter(CI.want - 1); else doExit(); CI.stage = 2; } }
  else if (CI.stage == 2) { CI.fade -= dt * 3.f; if (CI.fade <= 0) { CI.fade = 0; CI.stage = 0; } }
}
// ------------------------------------------------------------------ nightly invasion (Athran-style defence loop seed)
float prevHour = 9.5f; int hordeAlive = 0;
void horde(bool force) {
  int si = settleNear(P.x, P.z, force ? 400.f : 150.f); if (si < 0 || settles[si].type > ST_VILLAGE) return; const Settle& s = settles[si]; int want = 8 + s.size * 4 + (s.type == ST_CAPITAL ? 10 : 0); static const int T[6] = {THORNLING, PEATWIGHT, GLOAMHOUND, RUSTBOUND, THORNLING, GLOAMWISP}; int made = 0;
  for (int t = 0; t < 200 && made < want; t++) { float a = rng() * 6.28f, d = (s.type == ST_CAPITAL ? 110.f : 70.f) + rng() * 40.f; float x = s.x + std::cos(a) * d, z = s.z + std::sin(a) * d; if (!validSpot(x, z, 0.8f)) continue; int type = T[(int)(rng() * 6.f) % 6]; if (made == 0 && s.size >= 2) type = CAIRNBACK;
    Npc* n = spawnMon(type, x, z, false, false); if (!n) break; n->homeX = s.x + (rng() - 0.5f) * 14.f; n->homeZ = s.z + (rng() - 0.5f) * 14.f; n->wander = 12.f; n->aggro *= 1.5f; n->state = S_RETURN; made++; }
  if (made) { char sub[40]; snprintf(sub, sizeof sub, "THE HORDE MARCHES ON %s", s.name); banner("NIGHT FALLS", sub, 5.f); audio::play(S_DIEBIG, 0, 0.5f); }
}
