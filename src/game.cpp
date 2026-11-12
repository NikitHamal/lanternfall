// Port of the Lanternfall simulation (61-game.js). Deterministic fixed step, pooled storage.
#include "game.h"
#include "roster.h"
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <cstdio>
Game G;
enum { K_PHYS, K_FIRE, K_FROST, K_GLOAM };
enum { HK_NONE, HK_ARROW, HK_AIMED, HK_PIN, HK_EMBER, HK_FLASK, HK_LANCEB, HK_FLARE, HK_VOLLEY, HK_TONIC, HK_FIREPOT, HK_CINDER, HK_GLOAM };
enum { V_ARROW, V_EMBER, V_FLASK, V_LANCE, V_GLOAM, V_CINDER };
static inline float distU(const Unit* a, const Unit* b) { return std::hypot(a->x - b->x, a->z - b->z); }
static inline bool alive(const Unit* u) { return u->team == 0 ? !u->down : (u->used && u->hp > 0); }
static inline float stageHp(int s) { return TUNE.h0 * std::pow(TUNE.hp, (float)(s - 1)); }
static inline float stageDmg(int s) { return TUNE.d0 * std::pow(TUNE.dmg, (float)(s - 1)); }
static inline void A(int s) { audio::play(s); }
static const float FIRE_COL[3] = {3.0f, 1.2f, 0.3f};

void Game::toast(const char* s, uint32_t c) {
  if (nToast < 6) nToast++; for (int i = nToast - 1; i > 0; i--) toasts[i] = toasts[i - 1];
  snprintf(toasts[0].s, 64, "%s", s); toasts[0].c = c; toasts[0].t = 3.4f;
}
float Game::xpNeed(int L) const { return std::round(26 * std::pow(TUNE.xp, (float)(L - 1)) + 8 * (L - 1)); }
float Game::partyHp() const { float a = 0, b = 0; for (const Unit& h : heroes) { a += std::max(0.f, h.down ? 0.f : h.hp); b += h.maxHp; } return b > 0 ? a / b : 1; }
const char* Game::stageName(int s, char* buf) const {
  int r = reachOf(s); static const char* RN[] = {"", " II", " III", " IV", " V", " VI", " VII"}; int k = r >= 5 ? std::min(6, r / 5) : 0;
  snprintf(buf, 48, "%s%s", REACHES[r % 5].name, RN[k]); return buf;
}
float Game::lightRadius() const { return 9 * (1 + 0.06f * run.camp[CAMP_OIL] + 0.15f * mod(C_P_LIGHT)) * (0.72f + 0.0933f * run.flames); }
float Game::yOf(const Unit& u) const { return u.team == 0 ? 1.15f : MON[u.cls].hpY * 0.55f; }
int Game::liveMonCount() { int n = 0; for (auto& m : mons) if (m.used && m.hp > 0) n++; return n; }
void Game::clearMons() { for (auto& m : mons) m.used = false; for (auto& z : zones) z.on = false; for (auto& t : teles) t.on = false; for (auto& p : projs) p.on = false; focus = nullptr; bossRef = nullptr; }

/* ---------- persistence ---------- */
static void savePath(char* out, size_t n) {
  const char* h = getenv("APPDATA"); const char* sep = "\\";
  if (!h) { h = getenv("XDG_DATA_HOME"); sep = "/"; }
  if (!h) { h = getenv("HOME"); sep = "/"; if (h) { snprintf(out, n, "%s/.lanternfall.sav", h); return; } }
  if (h) snprintf(out, n, "%s%slanternfall.sav", h, sep); else snprintf(out, n, "lanternfall.sav");
}
void Game::save() { char p[512]; savePath(p, sizeof p); FILE* f = fopen(p, "wb"); if (!f) return; uint32_t magic = 0x4C4E4631; fwrite(&magic, 4, 1, f); fwrite(&P, sizeof P, 1, f); fclose(f); }
bool Game::load() { char p[512]; savePath(p, sizeof p); FILE* f = fopen(p, "rb"); if (!f) return false; uint32_t m = 0; Profile q; bool ok = fread(&m, 4, 1, f) == 1 && m == 0x4C4E4631 && fread(&q, sizeof q, 1, f) == 1; fclose(f); if (ok) P = q; return ok; }

/* ---------- runs ---------- */
void Game::newRun() {
  Run r; int pool[4], np = 0; const int cand[4] = {REAVER, FLETCHER, APOTHECARY, RIMEBINDER}; for (int c : cand) if (P.unlocked[c]) pool[np++] = c;
  roster::init(); int lead = roster::at(roster::sel()).clsIx;
  r.party[0] = lead; r.party[3] = (lead == LAMPLIGHTER) ? WARDEN : LAMPLIGHTER;
  int p1 = REAVER, p2 = FLETCHER;
  if (np > 2) { int k = P.runs % np; p1 = pool[k]; p2 = pool[(k + 1) % np]; }
  if (p1 == lead || p1 == r.party[3]) p1 = (lead != WARDEN && r.party[3] != WARDEN) ? WARDEN : FLETCHER;
  if (p2 == lead || p2 == r.party[3] || p2 == p1) { for (int c = 0; c < NCLS; c++) if (c != lead && c != r.party[3] && c != p1) { p2 = c; break; } }
  r.party[1] = p1; r.party[2] = p2;
  int s0 = 1 + std::min(boon(B_START), std::max(0, P.bestStage - 3));
  r.stage = s0; r.maxStage = s0 - 1; r.flames = 3; r.tonics = 2; run = r; hasRun = true;
  rng = Rng((uint32_t)(seed + P.runs * 7919)); P.runs++;
  flare = std::min(flare, 1.f); resetField(); setupStage(true);
}
void Game::resetField() {
  clearMons(); pending = 0; rally = false; flare = std::min(flare, 1.f);
  for (int i = 0; i < 4; i++) {
    Unit& h = heroes[i]; h = Unit(); h.used = true; h.team = 0; h.cls = run.party[i]; h.idx = i; h.face = PI / 2; h.cd[0] = 2.f + i; h.cd[1] = 5 + i * 1.5f; h.cd[2] = 3; h.atkT = 0.5f + i * 0.2f; h.id = ++nextId;
  }
  recalc(true); placeParty();
}
void Game::slotOf(Unit* h, float& sx, float& sz) {
  const ClassDef& c = CL[h->cls]; int same = 0;
  for (const Unit& o : heroes) if (&o != h && std::fabs(CL[o.cls].slot[0] - c.slot[0]) < 0.5f && std::fabs(CL[o.cls].slot[1] - c.slot[1]) < 0.5f && o.idx < h->idx) same++;
  float ax = (rally ? rallyX : anchorX) + c.slot[0] - same * 0.9f, bz = rally ? rallyZ : pathZ(ax); sx = ax; sz = bz + c.slot[1] + same * 1.1f;
}
void Game::placeParty() { for (Unit& h : heroes) { float sx, sz; slotOf(&h, sx, sz); h.x = sx; h.z = sz; h.ox = sx; h.oz = sz; h.face = std::atan2(1.f, pathDZ(h.x)); h.faceT = h.face; } }
void Game::recalc(bool full) {
  float lv = 1 + 0.075f * (run.level - 1);
  for (Unit& h : heroes) {
    const ClassDef& C = CL[h.cls]; Item* g = run.gear[h.cls]; float w = g[0].has ? g[0].pow : 0, a = g[1].has ? g[1].pow : 0, t = g[2].has ? g[2].pow : 0;
    auto sp = [&](int k) { int n = 0; for (int s = 0; s < 3; s++) if (g[s].has && g[s].sp == k) n++; return n; };
    float old = h.maxHp;
    h.maxHp = std::round(C.hp * lv * (1 + a / 100) * (1 + 0.08f * run.camp[CAMP_DRESS]) * (1 + 0.12f * mod(C_P_HP)) * (1 + 0.06f * boon(B_HP)) * (h.cls == WARDEN ? 1 + 0.25f * mod(C_W_HP) : 1));
    h.dmg = C.dmg * lv * (1 + w / 100) * (1 + 0.08f * run.camp[CAMP_WHET]) * (1 + 0.1f * mod(C_P_DMG)) * (1 + 0.06f * boon(B_DMG)) * (h.cls == REAVER ? 1 + 0.18f * mod(C_R_DMG) : 1) * (h.cls == LAMPLIGHTER ? 1 + 0.18f * mod(C_L_DMG) : 1);
    h.power = 1 + t / 100;
    h.atk = C.atk / ((1 + 0.07f * mod(C_P_HASTE)) * (1 + 0.12f * sp(SP_HASTE)) * (h.cls == FLETCHER ? 1 + 0.15f * mod(C_F_HASTE) : 1));
    h.cdr = 1 - 0.12f * sp(SP_CDR); h.leech = 0.03f * sp(SP_LEECH); h.armor = std::min(0.75f, C.armor + 0.1f * sp(SP_WARD)); h.crit = 0.08f + 0.1f * sp(SP_CRIT); h.range = C.range; h.speed = C.speed; h.radius = 0.4f;
    if (full || old <= 1) h.hp = h.maxHp; else if (h.maxHp > old && !h.down) h.hp += h.maxHp - old;
  }
  int silverGear = 0; for (Unit& h : heroes) for (int s = 0; s < 3; s++) if (run.gear[h.cls][s].has && run.gear[h.cls][s].sp == SP_SILVER) silverGear++;
  run.silverMul = (1 + 0.25f * mod(C_P_SILVER)) * (1 + 0.1f * boon(B_SILVER)) * (1 + 0.2f * silverGear);
  run.maxTonics = std::min(6, 2 + (run.camp[CAMP_TONIC] + 1) / 2);
}
void Game::setupStage(bool first) {
  run.pack = 0; stDowns = 0; stMinHp = 1; stageT = 0; state = S_WALK; stT = 0; nextPackX = anchorX + (first ? 12 : TUNE.gap + 3);
  int rr = reachOf(run.stage); if (onReach) onReach(rr % 5, first); lastReach = rr; hasWay = false; bossRef = nullptr;
  if (!first) { char b[48], t[64]; snprintf(t, 64, "%s - STAGE %d", stageName(run.stage, b), run.stage); toast(t); }
}

/* ---------- spawning ---------- */
Unit* Game::mkMon(int type, float x, float z, int s) {
  Unit* m = nullptr; for (auto& q : mons) if (!q.used) { m = &q; break; } if (!m) return nullptr;
  const MonDef& M = MON[type]; *m = Unit(); m->used = true; m->team = 1; m->cls = type; m->x = x; m->z = z; m->ox = x; m->oz = z; m->face = -PI / 2; m->faceT = m->face; m->radius = M.radius;
  m->maxHp = std::round(M.hp * stageHp(s) * (M.boss ? 1 + 0.05f * (s - 1) : 1)); m->hp = m->maxHp; m->dmg = M.dmg * stageDmg(s); m->atk = M.atk; m->range = M.range; m->speed = M.speed * (0.92f + R() * 0.16f); m->armor = M.armor;
  m->atkT = 0.3f + R() * 0.8f; m->boss = M.boss; m->elite = M.elite; m->specT = 7 + R() * 3; m->side = R() < 0.5f ? -1 : 1; m->id = ++nextId; m->slowK = 1; return m;
}
void Game::spawnPack() {
  int s = run.stage, types[NMON], nt = 0; for (int k = 0; k < NMON; k++) if (!MON[k].boss && MON[k].from <= s) types[nt++] = k;
  float budget = TUNE.b0 + s * TUNE.b1 + run.pack * 1.5f; int n = 0; int cap = std::min(10, 6 + s / 2); float bx = anchorX + 19 + R() * 3;
  while (budget > 2.5f && n < cap) {
    int pick = -1, tries = 0; while (tries++ < 8) { int t = types[(int)(R() * nt) % nt]; float c = MON[t].cost; if (c <= budget + 2 && !(MON[t].elite && R() < 0.45f)) { pick = t; break; } }
    if (pick < 0) pick = THORNLING;
    int cnt = pick == THORNLING ? 2 + (int)(R() * 2) : 1;
    for (int i = 0; i < cnt && n < cap + 2; i++) { float x = bx + R() * 6 + (MON[pick].ranged ? 3 : 0), z = pathZ(x) + (R() - 0.5f) * 7; mkMon(pick, x, z, s); n++; }
    budget -= MON[pick].cost * (pick == THORNLING ? cnt * 0.8f : 1);
  }
  state = S_FIGHT; stT = 0; run.pack++;
}
void Game::spawnBoss() {
  int s = run.stage, type = bossOf(s); float x = wayX + 1.5f; Unit* b = mkMon(type, x, pathZ(x), s); if (!b) return; snprintf(b->title, 32, "%s", bossTitle(s));
  int adds = std::min(4, 1 + s / 4), types[NMON], nt = 0; for (int k = 0; k < NMON; k++) if (!MON[k].boss && !MON[k].elite && MON[k].from <= s) types[nt++] = k;
  for (int i = 0; i < adds; i++) { int t = types[(int)(R() * nt) % nt]; float xx = x + 2 + R() * 4; mkMon(t, xx, pathZ(xx) + (R() - 0.5f) * 8, s); }
  bossRef = b; state = S_BOSS; stT = 0; A(S_BOSSINTRO); toast(b->title, 0xff7a3a); if (onBossIntro) onBossIntro(b);
}

/* ---------- combat ---------- */
int Game::monsNear(float x, float z, float r, Unit** out, int cap) { int n = 0; for (auto& m : mons) if (m.used && m.hp > 0 && std::hypot(m.x - x, m.z - z) <= r + m.radius && n < cap) out[n++] = &m; return n; }
Unit* Game::nearestMon(Unit* u, float maxD, int pred) {
  Unit* b = nullptr; float bd = maxD;
  for (auto& m : mons) { if (!m.used || m.hp <= 0 || !m.seen) continue;
    if (pred == 1 && !(MON[m.cls].ranged || m.boss || m.elite)) continue; if (pred == 2 && !(MON[m.cls].flank || m.boss || m.elite) ) continue; if (pred == 2 && m.slowT > 0) continue;
    if (pred == 3 && MON[m.cls].ranged) continue; if (pred == 4 && !(m.x - anchorX < 11 && m.x - anchorX > -11)) continue;
    float d = distU(u, &m); if (d < bd) { bd = d; b = &m; } }
  return b;
}
bool Game::inRange(Unit* u, Unit* t, float extra) { return distU(u, t) <= u->range + t->radius + extra; }
void Game::faceTo(Unit* u, Unit* t) { u->faceT = std::atan2(t->x - u->x, t->z - u->z); }
void Game::hitFx(Unit* t, int kind, float y, Unit* src) {
  int mat = t->team == 0 ? MM : MON[t->cls].mat; float dx = src ? (t->x > src->x ? 0.4f : -0.4f) : 0;
  bool mute = dot; int id = kind == K_FIRE ? S_HITFIRE : kind == K_FROST ? S_HITFROST : kind == K_GLOAM ? S_HITGLOAM : mat == MM ? (t->team == 0 ? S_HITSHIELD : S_HITMETAL) : mat == MS ? S_HITSTONE : mat == MW ? S_HITWOOD : S_HITSOFT;
  if (!mute && time - sndT[id & 15] < 0.07f) mute = true; if (!mute) sndT[id & 15] = time;
  if (kind == K_FIRE) { fx.embers(t->x, y, t->z, 5, nullptr, 0.25f); fx.sparks(t->x, y, t->z, 4, {3.5f, 1.5f, 0.3f}, 3); }
  else if (kind == K_FROST) fx.frost(t->x, y, t->z, 7, 0.3f);
  else if (kind == K_GLOAM) fx.sparks(t->x, y, t->z, 6, {0.7f, 2.6f, 1.3f}, 3);
  else if (mat == MM) { fx.sparks(t->x, y, t->z, 8, {3.6f, 2.4f, 1.0f}, 5, dx, 0); fx.flash(t->x, y + 0.2f, t->z, {1, 0.8f, 0.5f}, 14, 0.12f); }
  else if (mat == MS) { V3 c{0.2f, 0.2f, 0.2f}, d{0.13f, 0.13f, 0.13f}; fx.chips(t->x, y, t->z, 5, c, 3); fx.dust(t->x, y, t->z, 2, &d, 0.2f); fx.sparks(t->x, y, t->z, 3, {2.6f, 2.0f, 1.2f}, 4); }
  else if (mat == MW) fx.chips(t->x, y, t->z, 6, {0.22f, 0.16f, 0.09f}, 3.5f);
  else if (mat == MP) { V3 c{0.05f, 0.04f, 0.03f}; fx.chips(t->x, y, t->z, 5, c, 2.6f); fx.dust(t->x, y, t->z, 1, &c, 0.2f); }
  else { V3 c{0.07f, 0.08f, 0.08f}; fx.dust(t->x, y, t->z, 2, &c, 0.2f); fx.sparks(t->x, y, t->z, 3, {0.7f, 2.2f, 1.2f}, 2.5f); }
  if (!mute) A(id);
}
float Game::hurt(Unit* t, float amt, Unit* src, int kind, bool heavy, float stag, bool pierce) {
  if (!alive(t)) return 0;
  float d = amt * (0.92f + R() * 0.16f); bool crit = false;
  if (src && src->team == 0) {
    if (R() < src->crit) { d *= 1.8f; crit = true; }
    if (t->markT > 0) d *= 1.15f;
    if (std::hypot(t->x - lightX, t->z - lightZ) < lightRadius()) d *= 1 + 0.02f * run.camp[CAMP_OIL] + 0.04f;
    if (src->cls == REAVER && src->hp < src->maxHp * (0.5f + 0.15f * mod(C_R_WIND))) d *= 1.35f + 0.1f * mod(C_R_WIND);
  }
  if (src && src->team == 1 && src->chillT > 0) d *= 1 - 0.1f * mod(C_B_RIME);
  d *= 1 - t->armor * (pierce ? 0.5f : 1);
  if (t->team == 0 && t->guardT > 0) d *= 0.55f - 0.08f * mod(C_W_HOLD);
  d = std::max(1.f, std::round(d)); t->hp -= d;
  if (src && src->team == 0) { src->dps += d; if (src->leech > 0) heal(src, d * src->leech, true); }
  if (heavy) hitStop = std::max(hitStop, t->boss || (src && src->boss) ? 0.07f : 0.045f);
  float y = yOf(*t); t->flashV = 1; t->stagV += heavy ? 1.f : 0.45f; t->hitFrom = src ? std::atan2(t->x - src->x, t->z - src->z) : 0;
  char b[16]; snprintf(b, 16, "%d", (int)d);
  fx.floatText(t->x, y + 0.7f, t->z, b, t->team == 0 ? 0xe0685a : crit ? 0xffd27a : kind == K_FROST ? 0xa8d8ff : kind == K_FIRE ? 0xffb070 : 0xe8e0cc, crit ? 19.f : (t->team == 0 ? 14.f : 13.f + std::min(5.f, d / 60)), 1);
  hitFx(t, kind, y, src);
  if (t->team == 0) {
    stMinHp = std::min(stMinHp, partyHp());
    if (t->hp <= 0) heroDown(t);
    else if (t->cls == WARDEN && src && src->team == 1 && t->ripT <= 0 && !t->act.on && R() < 0.3f + 0.12f * mod(C_W_RIP) && distU(t, src) < t->range + 0.6f) { t->ripT = 4 * t->cdr; t->ripTarget = src; }
  } else {
    if (stag > 0 && !t->boss) { t->stagT = std::max(t->stagT, stag); t->act.on = false; } else if (stag > 0 && t->boss) t->stagT = std::max(t->stagT, stag * 0.25f);
    if (t->hp <= 0) monDie(t, src);
    else if (t->boss && !t->enr && t->hp < t->maxHp * 0.3f) { t->enr = true; t->atk *= 0.74f; char tb[64]; snprintf(tb, 64, "%s IS ENRAGED", t->title); toast(tb, 0xe0685a); A(S_ENRAGE); }
  }
  return d;
}
void Game::heal(Unit* h, float amt, bool silent) {
  if (h->down) return; float a = std::min(h->maxHp - h->hp, std::round(amt)); if (a <= 0) return; h->hp += a;
  if (!silent && a > 2) { char b[16]; snprintf(b, 16, "+%d", (int)a); fx.floatText(h->x, 2.0f, h->z, b, 0x8fd98a, 13, 0.8f); V3 c{0.6f, 2.2f, 0.7f}; fx.embers(h->x, 0.8f, h->z, 4, &c, 0.3f, 1.2f); }
}
void Game::heroDown(Unit* h) {
  if (run.stage < 5) run.d5++;
  h->hp = 0; h->down = true; h->downT = 20 * (1 - 0.25f * mod(C_A_KIT)); h->act.on = false; stDowns++; A(S_HERODOWN); char b[64]; snprintf(b, 64, "%s HAS FALLEN", CL[h->cls].name); toast(b, 0xe0685a);
  bool all = true; for (Unit& q : heroes) if (!q.down) all = false;
  if (all) { state = S_WIPE; stT = 0; run.flames--; A(S_FLAMEOUT); slowMo = 1.4f; shake(1); toast(run.flames > 0 ? "A FLAME GUTTERS OUT" : "THE LAST FLAME GUTTERS", 0xe0685a); }
}
void Game::spawnLoot(float x, float z, int kind, float silver, const Item* it, int hero) {
  for (auto& l : loot) if (!l.on) { l.on = true; l.kind = kind; l.x = x + (R() - 0.5f) * 1.2f; l.z = z + (R() - 0.5f) * 1.2f; l.y = 0.2f; l.t = 0; l.delay = 0.5f + R() * 0.3f; l.silver = (int)silver; if (it) l.item = *it; l.hero = hero; return; }
  if (kind == 0) { run.silver += silver; }  // pool full: pay out directly
}
void Game::monDie(Unit* m, Unit* src) {
  (void)src; const MonDef& M = MON[m->cls]; m->hp = 0; m->deadT = 0; run.kills++; flare = std::min(1.f, flare + (m->boss ? 0.5f : m->elite ? 0.1f : 0.045f) * (1 + 0.3f * mod(C_P_FLARE)));
  int s = run.stage; float held = run.holdCount > 0 ? 0.8f : 1, xp = M.xp * std::pow(1.13f, (float)(s - 1)) * (1 + 0.15f * mod(C_P_XP)) * (1 + 0.08f * boon(B_XP)) * held;
  addXp(xp);
  float silver = std::max(1.f, std::round((M.xp * 0.5f + 1) * std::pow(1.12f, (float)(s - 1)) * run.silverMul * (0.8f + R() * 0.4f) * held));
  int n = m->boss ? 8 : (m->elite ? 3 : 1); for (int i = 0; i < n; i++) spawnLoot(m->x, m->z, 0, i == 0 ? silver : 0, nullptr, 0);
  float pDrop = m->boss ? 1.f : (m->elite ? 0.3f : 0.085f) * (run.pack <= 1 && s == 1 ? 1.6f : 1.f); int drops = m->boss ? 2 + (R() < 0.5f ? 1 : 0) : (R() < pDrop ? 1 : 0);
  for (int i = 0; i < drops; i++) dropGear(m, m->boss ? 1 : 0);
  A(M.boss || M.elite ? S_DIEBIG : S_DIE);
  if (m->boss) { bossRef = nullptr; for (auto& o : mons) if (&o != m && o.used && o.hp > 0) { o.hp = 0; o.deadT = 0; } }
  if (focus == m) focus = nullptr;
}
void Game::addXp(float x) {
  run.xp += x; float need = xpNeed(run.level);
  while (run.xp >= need) { run.xp -= need; run.level++; pending++; recalc(false); need = xpNeed(run.level); A(S_LEVELUP); char b[32]; snprintf(b, 32, "LEVEL %d", run.level); toast(b, 0x8fd98a); }
}
Item Game::makeItem(int slot, int cls, int stage, int minGrade) {
  float r = R(); int g = r < 0.015f ? 3 : r < 0.1f ? 2 : r < 0.38f ? 1 : 0; g = std::max(g, minGrade);
  Item it; it.has = true; it.slot = slot; it.g = g; it.pow = (int)std::round(7 * std::pow(1.105f, (float)(stage - 1)) * GRADES[g].m * (0.88f + R() * 0.24f));
  const char* base = slot == 2 ? ITEM_TRINKET[(int)(R() * 7) % 7] : slot == 0 ? ITEM_WEAPON[cls] : ITEM_ARMOUR[cls];
  if (g == 3) { it.sp = (int)(R() * NSP) % NSP; snprintf(it.name, 48, "%s, %s", RELIC_NAMES[(int)(R() * 12) % 12], base); } else snprintf(it.name, 48, "%s", base);
  return it;
}
void Game::dropGear(Unit* m, int minGrade) {
  int hi = (int)(R() * 4) % 4, slot = (int)(R() * 3) % 3; Item it = makeItem(slot, heroes[hi].cls, run.stage, minGrade); spawnLoot(m->x, m->z, it.g + 1, 0, &it, hi);
}
static const uint32_t LOOT_COL[5] = {0xe8c060, 0xa8a290, 0x7fb4d6, 0xe0a33c, 0xff7a3a};
void Game::stepLoot(float dt) {
  float tx = lightX, tz = lightZ;
  for (auto& l : loot) { if (!l.on) continue; l.t += dt; if (l.t < l.delay) { l.y = 0.25f + std::sin(l.t * 8) * 0.05f; continue; }
    float dx = tx - l.x, dz = tz - l.z, d = std::hypot(dx, dz), sp = 6 + (l.t - l.delay) * 14; float s = std::min(d, sp * dt); if (d > 1e-3f) { l.x += dx / d * s; l.z += dz / d * s; } l.y = lerpf(l.y, 1.4f, std::min(1.f, dt * 5));
    if (d < 0.5f) { l.on = false;
      if (l.kind == 0) { if (l.silver > 0) { run.silver += l.silver; A(S_COIN); } }
      else { Unit* h = &heroes[l.hero]; Item& it = l.item; Item& cur = run.gear[h->cls][it.slot];
        if (!cur.has || it.pow > cur.pow || (it.g == 3 && cur.g < 3 && it.pow > cur.pow * 0.85f)) {
          int dlt = it.pow - (cur.has ? cur.pow : 0); Item old = cur; cur = it; recalc(false); char b[24]; snprintf(b, 24, "+%d%% %s", dlt, it.slot == 0 ? "damage" : it.slot == 1 ? "life" : "skill"); fx.floatText(h->x, 2.3f, h->z, b, GRADES[it.g].c, 14, 1.3f);
          char tb[64]; if (it.g == 3) { run.relics++; snprintf(tb, 64, "RELIC: %s", it.name); for (char* c = tb; *c; c++) *c = (char)toupper(*c); toast(tb, 0xff7a3a); A(S_RELIC); } else if (it.g == 2) { snprintf(tb, 64, "GILDED %s", it.name); for (char* c = tb; *c; c++) *c = (char)toupper(*c); toast(tb, 0xe0a33c); A(S_GEAR2); } else A(S_GEAR);
          if (old.has) run.silver += std::round(old.pow * 0.6f * run.silverMul);
        } else { run.silver += std::round(it.pow * 0.6f * run.silverMul * std::pow(1.02f, (float)run.stage)); A(S_COIN); } } }
  }
}

/* ---------- actions & projectiles ---------- */
void Game::act(Unit* u, int kind, float wind, float rec, Unit* tgt, int clip) { Act a; a.on = true; a.kind = kind; a.wind = wind; a.rec = rec; a.tgt = tgt; a.clip = clip; u->act = a; }
void Game::runAct(Unit* u, float dt) { Act& a = u->act; a.t += dt; u->moving = 0; if (!a.hit && a.t >= a.wind) { a.hit = true; Act c = a; actHit(u, c); } if (u->act.on && u->act.t >= u->act.wind + u->act.rec) u->act.on = false; }
Proj* Game::proj(int vis, float sx, float sy, float sz, float ex, float ey, float ez, float dur, float arc, int hk, int hero) {
  for (auto& p : projs) if (!p.on) { p = Proj(); p.on = true; p.vis = vis; p.sx = sx; p.sy = sy; p.sz = sz; p.ex = ex; p.ey = ey; p.ez = ez; p.dur = std::max(0.05f, dur); p.arc = arc; p.hk = hk; p.hero = hero; p.x = sx; p.y = sy; p.z = sz; return &p; }
  return nullptr;
}
void Game::shoot(Unit* u, Unit* t, int vis, float speed, float arc, int hk, int hero, float a) {
  float sy = 1.45f, d = std::max(0.5f, distU(u, t)), ty = yOf(*t), f = u->face; Proj* p = proj(vis, u->x + std::sin(f) * 0.5f, sy, u->z + std::cos(f) * 0.5f, t->x, ty, t->z, d / speed, arc * d, hk, hero); if (p) { p->tgt = t; p->a = a; }
}
void Game::zone(int kind, int hero, float x, float z, float r, float dur, float tick) {
  for (auto& q : zones) if (!q.on) { q = Zone(); q.on = true; q.kind = kind; q.hero = hero; q.x = x; q.z = z; q.r = r; q.dur = dur; q.tick = tick; return; }
}
void Game::zoneTick(Zone& zn) {
  Unit* h = zn.hero >= 0 ? &heroes[zn.hero] : nullptr; Unit* l[MAXMON]; int n = monsNear(zn.x, zn.z, zn.r, l, MAXMON); dot = true;
  if (zn.sub == 3) { for (int i = 0; i < n; i++) { Unit* m = l[i]; m->chillT = std::max(m->chillT, 2.5f); m->slowT = std::max(m->slowT, 1.f); m->slowK = 0.5f - 0.1f * mod(C_B_HOAR); hurt(m, zn.k, h, K_FROST); } }
  else { for (int i = 0; i < n; i++) hurt(l[i], zn.k, h, K_FIRE);
    if (zn.sub == 2) { if (!zn.landed) { zn.landed = true; A(S_CINDERLAND); }
      for (int i = 0; i < 5; i++) { float a = fx.rnd() * TAU, d = std::sqrt(fx.rnd()) * zn.r; Proj* p = proj(V_CINDER, zn.x + std::cos(a) * d + 1.5f, 7, zn.z + std::sin(a) * d - 1, zn.x + std::cos(a) * d, 0.1f, zn.z + std::sin(a) * d, 0.35f + fx.rnd() * 0.2f, 0, HK_CINDER, -1); (void)p; } } }
  dot = false;
}
void Game::projHit(Proj& p) {
  Unit* h = p.hero >= 0 ? &heroes[p.hero] : nullptr; Unit* t = p.tgt; Unit* l[MAXMON]; float x = p.ex, y = p.ey, z = p.ez;
  switch (p.hk) {
    case HK_ARROW: if (t && alive(t)) { hurt(t, h->dmg, h); A(S_ARROWHIT); } break;
    case HK_AIMED: if (t && alive(t)) { hurt(t, h->dmg * 2.8f * h->power * (1 + 0.4f * mod(C_F_AIMED)), h, K_PHYS, true, 0, true); A(S_ARROWHIT); } break;
    case HK_PIN: if (t && alive(t)) { hurt(t, h->dmg * 1.25f * h->power, h); t->slowT = 4.f + 2 * mod(C_F_PIN); t->slowK = 0.4f; if (mod(C_F_PIN)) t->markT = t->slowT; A(S_ARROWHIT); } break;
    case HK_EMBER: if (t && alive(t)) { hurt(t, h->dmg, h, K_FIRE); int n = monsNear(x, z, 1.2f, l, MAXMON); for (int i = 0; i < n; i++) if (l[i] != t) hurt(l[i], h->dmg * 0.4f, h, K_FIRE); } break;
    case HK_FLASK: if (t && alive(t)) { hurt(t, h->dmg, h, K_FIRE); int n = monsNear(t->x, t->z, 1.5f, l, MAXMON); for (int i = 0; i < n; i++) if (l[i] != t) hurt(l[i], h->dmg * 0.5f, h, K_FIRE); A(S_GLASS); } break;
    case HK_LANCEB: if (t && alive(t)) { hurt(t, h->dmg, h, K_FROST); t->chillT = std::max(t->chillT, 3.f); } break;
    case HK_FLARE: { int n = monsNear(x, z, 2.6f, l, MAXMON); for (int i = 0; i < n; i++) hurt(l[i], h->dmg * 2.2f * h->power, h, K_FIRE, true);
      if (mod(C_L_FLARE)) { zone(0, p.hero, x, z, 2.2f, 3.5f, 0.5f); for (auto& q : zones) if (q.on && q.t == 0 && q.x == x && q.z == z) { q.sub = 0; q.k = h->dmg * 0.3f * mod(C_L_FLARE) * h->power; } }
      fx.ring(x, z, 0.3f, 2.6f, 0.3f, {2.4f, 1.0f, 0.25f}); fx.embers(x, 0.6f, z, 26, nullptr, 1.2f, 2); fx.flash(x, 1, z, {1, 0.55f, 0.2f}, 70, 0.3f); fx.glow(x, 0.8f, z, 3.5f, {3, 1.2f, 0.3f}, 0.25f); shake(0.25f); A(S_FIREBURST); break; }
    case HK_VOLLEY: { dot = p.n > 0; int n = monsNear(x, z, 1.1f, l, MAXMON); for (int i = 0; i < n; i++) hurt(l[i], h->dmg * 0.85f * h->power, h); dot = false; V3 c{0.2f, 0.17f, 0.12f}; fx.dust(x, 0.05f, z, 1, &c, 0.1f, 0.5f); if (p.n == 0) A(S_ARROWHIT); break; }
    case HK_TONIC: if (t) { float amt = t->maxHp * 0.26f * (1 + 0.3f * mod(C_A_TONIC)) * h->power; heal(t, amt);
      if (mod(C_A_TONIC)) for (Unit& o : heroes) if (&o != t && distU(&o, t) < 3.5f) heal(&o, amt * 0.3f * mod(C_A_TONIC));
      fx.chips(t->x, 1.2f, t->z, 5, {0.4f, 0.5f, 0.4f}, 2); V3 c{0.6f, 2.2f, 0.7f}; fx.embers(t->x, 1, t->z, 10, &c, 0.5f, 1); A(S_GLASS); A(S_HEAL); } break;
    case HK_FIREPOT: { int n = monsNear(x, z, 2.4f, l, MAXMON); for (int i = 0; i < n; i++) hurt(l[i], h->dmg * 1.5f * h->power, h, K_FIRE);
      zone(0, p.hero, x, z, 2.4f, 5.f + 2 * mod(C_A_FIRE), 0.5f); for (auto& q : zones) if (q.on && q.t == 0 && q.x == x && q.z == z) { q.sub = 1; q.k = h->dmg * 0.55f * h->power * (1 + 0.25f * mod(C_A_FIRE)); }
      fx.embers(x, 0.4f, z, 20, nullptr, 1, 2); fx.flash(x, 0.8f, z, {1, 0.5f, 0.15f}, 50, 0.3f); fx.ring(x, z, 0.3f, 2.4f, 0.3f, {2, 0.8f, 0.2f}); A(S_GLASS); A(S_FIREBURST); break; }
    case HK_CINDER: fx.embers(x, 0.2f, z, 4, nullptr, 0.3f, 1.5f); break;
    case HK_GLOAM: if (t && alive(t)) hurt(t, p.a, nullptr, K_GLOAM); break;
    default: break;
  }
}
void Game::stepProjs(float dt) {
  for (auto& p : projs) { if (!p.on) continue; p.t += dt;
    if (p.tgt && alive(p.tgt) && p.vis != V_LANCE) { p.ex = p.tgt->x; p.ez = p.tgt->z; p.ey = yOf(*p.tgt); }
    float u = std::min(1.f, p.t / p.dur); p.x = lerpf(p.sx, p.ex, u); p.z = lerpf(p.sz, p.ez, u); p.y = lerpf(p.sy, p.ey, u) + p.arc * 4 * u * (1 - u) * 0.5f;
    if (p.vis == V_EMBER || p.vis == V_CINDER || p.vis == V_GLOAM) { V3 c{3.0f, 1.2f, 0.25f}; if (p.vis == V_GLOAM) c = {0.7f, 2.6f, 1.3f}; fx.sparks(p.x, p.y, p.z, 1, c, 0.3f); }
    if (p.t >= p.dur) { p.on = false; if (p.hk != HK_NONE) projHit(p); } }
}
void Game::actHit(Unit* u, Act& a) {
  Unit* h = u; Unit* t = a.tgt; Unit* l[MAXMON]; int hi = u->idx;
  switch (a.kind) {
    case A_BASIC: {
      if (u->team == 1) break;
      switch (u->cls) {
        case WARDEN: case REAVER: if (t && alive(t) && inRange(h, t, 0.9f)) { hurt(t, h->dmg, h, K_PHYS, h->cls == REAVER); fx.arc(h->x, 1.25f, h->z, h->face, h->range + 0.2f, -1.2f, 1.3f, h->cls == REAVER ? -0.5f : -0.7f, nullptr, 0.14f); } A(h->cls == REAVER ? S_SWINGHEAVY : S_SWING); break;
        case FLETCHER: A(S_BOW); if (t && alive(t)) shoot(h, t, V_ARROW, 32, 0.02f, HK_ARROW, hi); break;
        case LAMPLIGHTER: A(S_FIRESMALL); if (t && alive(t)) shoot(h, t, V_EMBER, 15, 0.05f, HK_EMBER, hi); break;
        case APOTHECARY: A(S_THROW); if (t && alive(t)) shoot(h, t, V_FLASK, 12, 0.22f, HK_FLASK, hi); break;
        default: A(S_FROSTSMALL); if (t && alive(t)) shoot(h, t, V_LANCE, 20, 0.0f, HK_LANCEB, hi); break;
      } break; }
    case A_BASH: { int n = 0; if (mod(C_W_BASH)) n = monsNear(h->x, h->z, h->range + 0.8f, l, MAXMON); else if (t) { l[0] = t; n = 1; } for (int i = 0; i < n; i++) hurt(l[i], h->dmg * 1.4f * h->power * (1 + 0.25f * mod(C_W_BASH)), h, K_PHYS, true, 1.6f);
      if (t) fx.ring(t->x, t->z, 0.3f, 1.6f, 0.25f, {1.2f, 1.0f, 0.7f}); shake(0.25f); A(S_BASH); break; }
    case A_HOLD: { float d = 5.f + 2 * mod(C_W_HOLD); h->guardT = d; h->tauntT = d; int n = monsNear(h->x, h->z, 7, l, MAXMON); for (int i = 0; i < n; i++) { l[i]->tauntBy = h; l[i]->tauntT = d; }
      fx.ring(h->x, h->z, 0.5f, 7, 0.5f, {1.0f, 0.8f, 0.45f}); fx.floatText(h->x, 2.3f, h->z, "HOLD THE LINE", 0xe9c98a, 13, 0.6f); A(S_HOLD); break; }
    case A_CLEAVE: { float r = (h->range + 0.9f) * (1 + 0.15f * mod(C_R_CLEAVE)); int n = monsNear(h->x + std::sin(h->face) * 0.8f, h->z + std::cos(h->face) * 0.8f, r, l, MAXMON);
      for (int i = 0; i < n; i++) hurt(l[i], h->dmg * 1.25f * h->power * (1 + 0.3f * mod(C_R_CLEAVE)), h, K_PHYS, true);
      V3 c{1.8f, 1.6f, 1.3f}; fx.arc(h->x, 1.15f, h->z, h->face, r + 0.3f, -1.9f, 1.9f, 0.08f, &c, 0.2f); shake(0.2f); A(S_CLEAVE); break; }
    case A_SUNDER: { float L = 5.5f + 1.5f * mod(C_R_SUNDER), fx_ = std::sin(h->face), fz = std::cos(h->face);
      for (auto& m : mons) { if (!m.used || m.hp <= 0) continue; float dx = m.x - h->x, dz = m.z - h->z, al = dx * fx_ + dz * fz, pe = std::fabs(dx * fz - dz * fx_); if (al > -0.5f && al < L && pe < 1.3f + m.radius) hurt(&m, h->dmg * 2.6f * h->power, h, K_PHYS, true, 1.2f); }
      V3 d{0.1f, 0.09f, 0.07f}, c{0.06f, 0.05f, 0.04f}; for (int i = 1; i <= 5; i++) { float dd = i / 5.f * L; fx.dust(h->x + fx_ * dd, 0.1f, h->z + fz * dd, 3, &d, 0.5f, 1.5f); fx.chips(h->x + fx_ * dd, 0.1f, h->z + fz * dd, 3, c, 3); }
      fx.ring(h->x + fx_ * 1.5f, h->z + fz * 1.5f, 0.4f, 2.6f, 0.35f, {1.3f, 1.0f, 0.6f}); shake(0.6f); fx.flash(h->x + fx_ * 1.5f, 0.6f, h->z + fz * 1.5f, {1, 0.8f, 0.5f}, 30, 0.2f); A(S_SUNDER); break; }
    case A_AIMED: A(S_BOWHEAVY); if (t) shoot(h, t, V_ARROW, 34, 0.02f, HK_AIMED, hi); break;
    case A_VOLLEY: if (t) { int n = 6 + 3 * mod(C_F_VOLLEY); float cx = t->x, cz = t->z; A(S_BOWVOLLEY);
      for (int i = 0; i < n; i++) { float an = R() * TAU, d = std::sqrt(R()) * 3.2f, x = cx + std::cos(an) * d, z = cz + std::sin(an) * d; Proj* p = proj(V_ARROW, h->x, 1.6f, h->z, x, 0.1f, z, 0.85f + i * 0.04f, 6.5f, HK_VOLLEY, hi); if (p) p->n = i; } } break;
    case A_PIN: A(S_BOW); if (t) shoot(h, t, V_ARROW, 32, 0.02f, HK_PIN, hi); break;
    case A_FLARE: A(S_FIRECAST); if (t) shoot(h, t, V_EMBER, 15, 0.06f, HK_FLARE, hi); break;
    case A_KINDLE: { float amt = 0.24f * (1 + 0.3f * mod(C_L_KINDLE)) * h->power; for (Unit& o : heroes) if (!o.down) { o.regenT = 4; o.regenA = o.maxHp * amt / 4; }
      fx.ring(h->x, h->z, 0.5f, 8, 0.8f, {1.6f, 1.0f, 0.4f}); V3 c{2.6f, 1.6f, 0.5f}; fx.embers(h->x, 2.4f, h->z, 30, &c, 1.5f, 0.6f); fx.flash(h->x, 2.6f, h->z, {1, 0.7f, 0.35f}, 60, 0.5f); A(S_KINDLE); break; }
    case A_CINDER: if (t) { float r = 3.2f * (1 + 0.15f * mod(C_L_CINDER)); A(S_FIRECAST); zone(0, hi, t->x, t->z, r, 4, 0.5f); for (auto& q : zones) if (q.on && q.t == 0 && q.x == t->x && q.z == t->z) { q.sub = 2; q.k = h->dmg * 0.62f * h->power * (1 + 0.25f * mod(C_L_CINDER)); } } break;
    case A_TONIC: if (t) { A(S_THROW); Proj* p = proj(V_FLASK, h->x, 1.5f, h->z, t->x, 1.2f, t->z, std::max(0.25f, distU(h, t) / 12), 2.5f, HK_TONIC, hi); if (p) p->tgt = t; } break;
    case A_FIREPOT: if (t) { A(S_THROW); proj(V_FLASK, h->x, 1.6f, h->z, t->x, 0.1f, t->z, std::max(0.35f, distU(h, t) / 11), 3, HK_FIREPOT, hi); } break;
    case A_QUICK: { float d = 6.f + 2 * mod(C_A_QUICK); for (Unit& o : heroes) if (!o.down) o.hasteT = d; fx.ring(h->x, h->z, 0.5f, 6, 0.6f, {1.0f, 1.3f, 1.6f}); fx.floatText(h->x, 2.3f, h->z, "QUICKSILVER", 0xbfe3ff, 13, 0.6f); A(S_QUICK); break; }
    case A_LANCE: { float fx_ = std::sin(h->face), fz = std::cos(h->face), L = 10; A(S_FROSTCAST); proj(V_LANCE, h->x + fx_, 1.3f, h->z + fz, h->x + fx_ * L, 1.1f, h->z + fz * L, 0.3f, 0, HK_NONE, hi);
      for (auto& m : mons) { if (!m.used || m.hp <= 0) continue; float dx = m.x - h->x, dz = m.z - h->z, al = dx * fx_ + dz * fz, pe = std::fabs(dx * fz - dz * fx_);
        if (al > 0 && al < L && pe < 0.9f + m.radius) { hurt(&m, h->dmg * 1.8f * h->power * (1 + 0.25f * mod(C_B_LANCE)), h, K_FROST, true); m.chillT = 5.f + 2 * mod(C_B_LANCE); m.slowT = std::max(m.slowT, 2.5f); m.slowK = std::min(m.slowK < 1 ? m.slowK : 1.f, 0.7f); } } break; }
    case A_HOAR: if (t) { float r = 3.5f * (1 + 0.15f * mod(C_B_HOAR)); A(S_FROSTFIELD); fx.ring(t->x, t->z, 0.3f, r, 0.4f, {0.5f, 1.2f, 2.2f}); fx.frost(t->x, 0.4f, t->z, 30, r * 0.6f); zone(1, hi, t->x, t->z, r, 6, 0.4f); for (auto& q : zones) if (q.on && q.t == 0 && q.x == t->x && q.z == t->z) { q.sub = 3; q.k = h->dmg * 0.22f * h->power; } } break;
    case A_SHATTER: { int n = 0; for (auto& m : mons) if (m.used && m.hp > 0 && m.chillT > 0) { hurt(&m, h->dmg * 2.5f * h->power * (1 + 0.35f * mod(C_B_SHATTER)) * (m.slowT > 0 ? 1.3f : 1.f), h, K_FROST, true, 0.8f); fx.frost(m.x, yOf(m), m.z, 16, 0.5f); fx.ring(m.x, m.z, 0.2f, 1.6f, 0.25f, {0.6f, 1.4f, 2.4f}); m.chillT = 0; n++; }
      if (n) { A(S_SHATTER); shake(0.3f); fx.flash(h->x, 2, h->z, {0.5f, 0.8f, 1}, 40, 0.25f); } break; }
    case A_RIPOSTE: if (t && alive(t)) { hurt(t, h->dmg * (1.6f + 0.3f * mod(C_W_RIP)), h, K_PHYS, true); V3 c{2, 1.8f, 1.4f}; fx.arc(h->x, 1.2f, h->z, h->face, h->range + 0.2f, 1.2f, -1.3f, 0.5f, &c, 0.12f); fx.floatText(h->x, 2.2f, h->z, "RIPOSTE", 0xe9c98a, 12, 0.5f); } A(S_SWING); break;
    case A_MONATK: { Unit* m = u; if (t && alive(t) && inRange(m, t, 0.8f)) hurt(t, m->dmg, m, K_PHYS, m->elite || m->boss);
      if (m->elite || m->boss) { V3 c{0.2f, 0.17f, 0.13f}; (void)c; fx.dust(m->x + std::sin(m->face) * m->range * 0.7f, 0.1f, m->z + std::cos(m->face) * m->range * 0.7f, 3, nullptr, 0.4f, 1.5f); if (m->boss || m->cls == CAIRNBACK) shake(m->boss ? 0.35f : 0.2f); }
      A(m->boss || m->elite ? S_MONHEAVY : S_MONATK); break; }
    case A_MONCAST: { Unit* m = u; A(S_GLOAMCAST); if (t && alive(t)) { Proj* p = proj(V_GLOAM, m->x, 1.75f, m->z, t->x, 1.2f, t->z, distU(m, t) / 9, 0.3f, HK_GLOAM, -1); if (p) { p->tgt = t; p->a = m->dmg; } } break; }
    case A_SPECIAL: { Unit* m = u; int kind = MON[m->cls].spec;
      if (kind == 3) { float L = std::min(11.f, a.tx), fx_ = std::sin(m->faceT), fz = std::cos(m->faceT);
        for (auto& tl : teles) if (tl.on) for (Unit& hh : heroes) if (!hh.down && std::hypot(hh.x - tl.x, hh.z - tl.z) < tl.r + 0.3f && !hh.hitSpec) { hh.hitSpec = true; hurt(&hh, m->dmg * 2.4f, m, K_PHYS, true); }
        for (Unit& hh : heroes) hh.hitSpec = false; m->dashT = 0.35f; m->dashX = fx_ * L / 0.35f; m->dashZ = fz * L / 0.35f; shake(0.7f);
        for (auto& tl : teles) if (tl.on) fx.dust(tl.x, 0.1f, tl.z, 5, nullptr, 0.8f, 2.5f); }
      else { float r = kind == 2 ? 4.2f : 4.0f, cx = a.tx, cz = a.tz;
        for (Unit& hh : heroes) if (!hh.down && std::hypot(hh.x - cx, hh.z - cz) < r + 0.2f) hurt(&hh, m->dmg * (kind == 2 ? 2.7f : 2.3f), m, K_PHYS, true);
        shake(0.9f); fx.ring(cx, cz, 0.5f, r, 0.4f, {1.6f, 1.2f, 0.7f}); fx.dust(cx, 0.1f, cz, 14, nullptr, r * 0.6f, 3); fx.chips(cx, 0.2f, cz, 14, {0.07f, 0.06f, 0.05f}, 5);
        V3 c{1.6f, 2.2f, 1.7f}; if (kind != 2) fx.arc(m->x, 2.8f, m->z, m->face, r + 1, -2.2f, 2.2f, 0.05f, &c, 0.25f); fx.flash(cx, 1, cz, {0.8f, 1, 0.8f}, 40, 0.25f); }
      A(S_BOSSSLAM); break; }
    case A_RELIGHT: { relit = true; run.lit++; if (onRelight) onRelight(); A(S_WAYLIGHT); char b[48]; snprintf(b, 48, "WAYLIGHT %d RELIT", run.stage); toast(b, 0x8fd98a);
      for (Unit& hh : heroes) { if (hh.down) revive(&hh, 1); hh.hp = hh.maxHp; } run.tonics = run.maxTonics; break; }
    default: break;
  }
}
static const float SKILL_WIND[16][2] = {};
bool Game::tryHeroSkill(Unit* h, Unit* t) {
  const ClassDef& C = CL[h->cls]; float* c = h->cd; Unit* l[MAXMON]; int n = 0; for (auto& m : mons) if (m.used && m.hp > 0 && m.seen) n++;
  auto use = [&](int i, int kind, float wind, float rec, Unit* tg) { c[i] = C.skills[i].cd * h->cdr * (!strcmp(C.skills[i].id, "sunder") ? 1 - 0.2f * mod(C_R_SUNDER) : 1.f); act(h, kind, wind, rec, tg, kind); return true; };
  auto bestGroup = [&](float maxD, float rad, int bonusBoss, int minN) { Unit* best = nullptr; int bn = minN; for (auto& m : mons) if (m.used && m.hp > 0 && m.seen && distU(h, &m) < maxD) { int k = monsNear(m.x, m.z, rad, l, MAXMON) + (m.boss ? bonusBoss : 0); if (k > bn) { bn = k; best = &m; } } return best; };
  switch (h->cls) {
    case WARDEN: if (c[0] <= 0 && t && inRange(h, t)) return use(0, A_BASH, 0.3f, 0.42f, t);
      if (c[1] <= 0) { bool lowAlly = false; for (Unit& o : heroes) if (!o.down && &o != h && o.hp < o.maxHp * 0.45f && nearestMon(&o, 2.6f)) lowAlly = true; if (monsNear(h->x, h->z, 6, l, MAXMON) >= 3 || bossRef || lowAlly) return use(1, A_HOLD, 0.25f, 0.5f, nullptr); } break;
    case REAVER: if (c[1] <= 0 && t && inRange(h, t, 1.5f) && (t->maxHp > h->dmg * 4 || monsNear(h->x, h->z, 4, l, MAXMON) >= 2)) return use(1, A_SUNDER, 0.62f, 0.6f, t);
      if (c[0] <= 0 && t && inRange(h, t) && monsNear(h->x, h->z, h->range + 1, l, MAXMON) >= 2) return use(0, A_CLEAVE, 0.42f, 0.5f, t); break;
    case FLETCHER: { if (c[1] <= 0 && t) { Unit* b = bestGroup(15, 3.2f, 0, 2); if (b) return use(1, A_VOLLEY, 0.6f, 0.5f, b); }
      if (c[0] <= 0 && t) { Unit* pr = nearestMon(h, 15, 1); if (!pr) pr = t; if (distU(h, pr) < 15) return use(0, A_AIMED, 0.62f, 0.4f, pr); }
      if (c[2] <= 0 && t) { Unit* pr = nearestMon(h, 14, 2); if (pr) return use(2, A_PIN, 0.5f, 0.4f, pr); } break; }
    case LAMPLIGHTER: { bool hurtAlly = false; for (Unit& o : heroes) if (!o.down && o.hp < o.maxHp * 0.6f) hurtAlly = true;
      if (c[1] <= 0 && partyHp() < 0.72f && hurtAlly) return use(1, A_KINDLE, 0.5f, 0.6f, nullptr);
      if (c[2] <= 0 && t) { Unit* b = bestGroup(13, 3.2f, 2, 2); if (b) return use(2, A_CINDER, 0.5f, 0.6f, b); }
      if (c[0] <= 0 && t && distU(h, t) < 12) return use(0, A_FLARE, 0.55f, 0.5f, t); break; }
    case APOTHECARY: { if (c[0] <= 0) { Unit* w = nullptr; float wf = 0.7f; for (Unit& o : heroes) if (!o.down && o.hp / o.maxHp < wf) { wf = o.hp / o.maxHp; w = &o; } if (w) return use(0, A_TONIC, 0.45f, 0.45f, w); }
      if (c[2] <= 0 && n >= 3) return use(2, A_QUICK, 0.45f, 0.45f, nullptr);
      if (c[1] <= 0 && t && distU(h, t) < 10) { Unit* b = bestGroup(10, 2.4f, 2, 0); return use(1, A_FIREPOT, 0.5f, 0.45f, b ? b : t); } break; }
    default: { int chilled = 0; for (auto& m : mons) if (m.used && m.hp > 0 && m.chillT > 0.4f) chilled++;
      if (c[2] <= 0 && chilled >= std::min(3, std::max(1, n))) return use(2, A_SHATTER, 0.55f, 0.55f, nullptr);
      if (c[1] <= 0 && t) { Unit* b = bestGroup(12, 3.5f, 2, 1); if (b) return use(1, A_HOAR, 0.55f, 0.55f, b); }
      if (c[0] <= 0 && t && distU(h, t) < 10) return use(0, A_LANCE, 0.55f, 0.5f, t); break; }
  }
  return false;
}
void Game::heroBasic(Unit* h, Unit* t) {
  bool mel = !CL[h->cls].ranged; float w = h->atk * (mel ? 0.5f : 0.46f), r = h->atk * (mel ? 0.44f : 0.38f); h->atkT = h->atk * (mel ? 0.06f : 0.16f); act(h, A_BASIC, w, r, t, 0);
}

/* ---------- unit stepping ---------- */
bool Game::moveTo(Unit* u, float tx, float tz, float sp, float dt, float stopD) {
  float dx = tx - u->x, dz = tz - u->z, d = std::hypot(dx, dz); float sd = stopD > 0 ? stopD : 0.08f; if (d <= sd) { u->moving = 0; return true; }
  float s = std::min(sp * dt, d - stopD * 0.9f); if (s < 0) s = 0; u->x += dx / d * s; u->z += dz / d * s; u->moving = sp; u->faceT = std::atan2(dx, dz); return false;
}
void Game::revive(Unit* h, float f) { h->down = false; h->hp = std::round(h->maxHp * f); h->act.on = false; V3 c{2.6f, 1.6f, 0.5f}; fx.embers(h->x, 0.6f, h->z, 16, &c, 0.5f, 1.5f); fx.floatText(h->x, 2.2f, h->z, "RISES", 0xe9c98a, 13, 0.7f); A(S_REVIVE); }
void Game::stepHero(Unit* h, float dt) {
  if (h->down) { h->downT -= dt; h->moving = 0; if (h->downT <= 0 && state != S_WIPE) revive(h, 0.35f); return; }
  float hk = h->hasteT > 0 ? 1.4f : 1.f; h->repoT -= dt; h->hasteT -= dt; h->guardT -= dt; h->tauntT -= dt; h->ripT -= dt; h->atkT -= dt * hk; for (int i = 0; i < 3; i++) h->cd[i] -= dt * (h->hasteT > 0 ? 1.15f : 1.f);
  if (h->regenT > 0) { h->regenT -= dt; h->hp = std::min(h->maxHp, h->hp + h->regenA * dt); }
  if (std::hypot(h->x - lightX, h->z - lightZ) < lightRadius()) h->hp = std::min(h->maxHp, h->hp + h->maxHp * std::min(0.012f, 0.002f + 0.0007f * run.camp[CAMP_OIL]) * dt);
  if (h->act.on) { runAct(h, dt * hk); return; }
  for (auto& t : teles) { if (!t.on) continue; float d = std::hypot(h->x - t.x, h->z - t.z); if (d < t.r + 0.6f && (CL[h->cls].ranged || h->dodgeOk)) { float k = d < 0.01f ? 1.f : 1 / d; moveTo(h, t.x + (h->x - t.x) * k * (t.r + 1.5f), t.z + (h->z - t.z) * k * (t.r + 1.5f) - 0.01f, h->speed * 1.5f, dt); h->ready = true; return; } }
  if (h->ripTarget && alive(h->ripTarget) && inRange(h, h->ripTarget, 0.6f)) { Unit* t = h->ripTarget; h->ripTarget = nullptr; faceTo(h, t); act(h, A_RIPOSTE, 0.16f, 0.3f, t, A_BASIC); return; }
  h->ripTarget = nullptr;
  bool fighting = false; for (auto& m : mons) if (m.used && m.hp > 0 && m.seen) { fighting = true; break; }
  Unit* t = nullptr;
  if (fighting) { if (focus && alive(focus) && focus->seen) t = focus; else if (CL[h->cls].ranged) t = nearestMon(h, h->range + 4); else { t = nearestMon(h, 3.2f); if (!t) t = nearestMon(h, 30, 4); } }
  h->ready = fighting; float sx, sz;
  if (rally && !t) { slotOf(h, sx, sz); moveTo(h, sx, sz, h->speed, dt, 0.15f); return; }
  if (t) h->idleT = 0; else if (!anchorMoving && !rally && state != S_RELIGHT) { h->idleT += dt; if (h->idleT < 1.6f) { h->moving = 0; return; } }
  if (!t) { slotOf(h, sx, sz); if (sx < h->x - 0.05f && sx > h->x - 5 && !rally) { h->moving = 0; h->faceT = std::atan2(1.f, pathDZ(h->x)); return; }
    float far_ = std::hypot(sx - h->x, sz - h->z); bool done = moveTo(h, sx, sz, far_ > 3 ? h->speed * 1.3f : (anchorMoving ? WALK * clampf(0.85f + far_ * 0.5f, 0.85f, 1.3f) : h->speed * 0.7f), dt, 0.12f); if (done && !anchorMoving) h->faceT = std::atan2(1.f, pathDZ(h->x)); return; }
  if (rally) { slotOf(h, sx, sz); if (std::hypot(sx - h->x, sz - h->z) > 1.2f) { moveTo(h, sx, sz, h->speed * 1.2f, dt, 0.3f); return; } }
  if (CL[h->cls].ranged) {
    Unit* thr = nearestMon(h, h->fleeing ? 3.0f : 1.9f, 3);
    if (thr && h->x > anchorX - 6.5f && (h->fleeing || h->repoT <= 0)) { if (h->atkT > 0.1f || h->fleeing) { h->fleeing = true; float d = distU(h, thr); if (d < 0.01f) d = 1; moveTo(h, h->x + (h->x - thr->x) / d * 2, h->z + (h->z - thr->z) / d * 2, 1.5f, dt); return; } }
    else if (h->fleeing) { h->fleeing = false; h->repoT = 0.8f; }
    if (distU(h, t) > h->range) { moveTo(h, t->x, t->z, h->speed, dt, h->range * 0.9f); return; }
    h->moving = 0; faceTo(h, t); if (tryHeroSkill(h, t)) return; if (h->atkT <= 0) heroBasic(h, t); return;
  }
  { float dd = distU(h, t) - (h->range + t->radius);
    if (dd > (h->eng ? 0.6f : 0.f)) { h->eng = false; bool nr = dd < 1.5f; faceTo(h, t); if (nr && h->repoT > 0) { h->moving = 0; return; } moveTo(h, t->x, t->z, nr ? 1.1f : h->speed, dt, h->range + t->radius - 0.3f); if (h->cls == WARDEN && h->cd[1] <= 0) tryHeroSkill(h, nullptr); return; }
    if (!h->eng) { h->eng = true; h->repoT = 0.8f; } }
  h->moving = 0; faceTo(h, t); if (tryHeroSkill(h, t)) return; if (h->atkT <= 0) heroBasic(h, t);
}
Unit* Game::pickHeroTarget(Unit* m) {
  if (m->tauntT > 0 && m->tauntBy && alive(m->tauntBy)) return m->tauntBy;
  Unit* b = nullptr; float bd = 1e9f;
  for (Unit& h : heroes) { if (h.down) continue; float d = distU(m, &h); if (MON[m->cls].flank) d = d * 0.35f + (h.x - anchorX) * 2.2f + (CL[h.cls].ranged ? 0.f : 6.f); else if (h.cls == WARDEN) d -= 1.5f; if (d < bd) { bd = d; b = &h; } }
  return b;
}
void Game::bossSpecial(Unit* m, Unit* t) {
  m->specT = 9 + R() * 3; int kind = MON[m->cls].spec; float wind = 1.6f; for (Unit& h : heroes) h.dodgeOk = R() < 0.62f; faceTo(m, t);
  Act a; a.on = true; a.kind = A_SPECIAL; a.wind = wind; a.rec = 0.9f;
  if (kind == 3) { float d = std::max(0.01f, distU(m, t)), fx_ = (t->x - m->x) / d, fz = (t->z - m->z) / d, L = std::min(11.f, d + 5); a.tx = L; m->faceT = std::atan2(fx_, fz);
    for (auto& tl : teles) tl.on = false; for (int i = 0; i < 4; i++) { float k = (i + 0.5f) / 4 * L; teles[i] = {true, m->x + fx_ * k, m->z + fz * k, 1.7f, 0, wind}; } }
  else { float r = kind == 2 ? 4.2f : 4.0f, cx = kind == 2 ? t->x : m->x + std::sin(m->faceT) * 2.6f, cz = kind == 2 ? t->z : m->z + std::cos(m->faceT) * 2.6f; a.tx = cx; a.tz = cz; for (auto& tl : teles) tl.on = false; teles[0] = {true, cx, cz, r, 0, wind}; }
  m->act = a; A(S_BOSSWIND);
}
void Game::stepMon(Unit* m, float dt) {
  if (m->hp <= 0) return; const MonDef& M = MON[m->cls];
  if (!m->seen && m->x - anchorX < 15.5f) m->seen = true;
  m->tauntT -= dt; m->chillT -= dt; m->markT -= dt; if (m->slowT > 0) m->slowT -= dt; else m->slowK = 1;
  if (m->stagT > 0) { m->stagT -= dt; m->moving = 0; return; }
  float sk = m->slowT > 0 ? m->slowK : 1.f; m->atkT -= dt * sk;
  if (m->act.on) { runAct(m, dt * sk); return; }
  Unit* t = pickHeroTarget(m); m->target = t; if (!t) { m->moving = 0; return; }
  float sp = m->speed * sk * (m->enr ? 1.15f : 1.f);
  if (m->boss) { m->specT -= dt * (m->enr ? 1.4f : 1.f); if (m->specT <= 0 && distU(m, t) < 9) { bossSpecial(m, t); return; } }
  if (M.flank && m->x > t->x + 2.5f && std::fabs(m->z - (pathZ(m->x) + m->side * 4.6f)) > 0.8f && m->x > anchorX + 2.2f && !(m->tauntT > 0)) { moveTo(m, m->x - 1.5f, pathZ(m->x - 1.5f) + m->side * 4.8f, sp, dt); return; }
  if (M.ranged) {
    float d = distU(m, t); if (d > m->range) { moveTo(m, t->x, t->z, sp, dt, m->range * 0.92f); return; }
    if (d < 4.5f && m->atkT > 0.3f) { moveTo(m, m->x + (m->x - t->x) / d * 2, m->z + (m->z - t->z) / d * 2, sp * 0.8f, dt); faceTo(m, t); return; }
    m->moving = 0; faceTo(m, t); if (m->atkT <= 0) { m->atkT = m->atk; act(m, A_MONCAST, m->atk * 0.4f, m->atk * 0.25f, t); } return;
  }
  { float dd = distU(m, t) - (m->range + t->radius); if (dd > (m->eng ? 0.55f : 0.f)) { m->eng = false; moveTo(m, t->x, t->z, dd < 1.2f ? std::min(sp, 1.3f) : sp, dt, m->range + t->radius - 0.25f); return; } m->eng = true; }
  m->moving = 0; faceTo(m, t);
  if (m->atkT <= 0) { m->atkT = m->atk; act(m, A_MONATK, m->atk * M.wind, m->atk * 0.44f, t); }
}
void Game::turn(Unit* u, float dt) { u->face += angDiff(u->face, u->faceT) * std::min(1.f, dt * 9); }
void Game::separate() {
  Unit* U[4 + MAXMON]; int n = 0; for (Unit& h : heroes) U[n++] = &h; for (auto& m : mons) if (m.used) U[n++] = &m; bool calm = true; for (auto& m : mons) if (m.used && m.hp > 0 && m.seen) { calm = false; break; }
  for (int i = 0; i < n; i++) { Unit* a = U[i]; if (!alive(a)) continue; for (int j = i + 1; j < n; j++) { Unit* b = U[j]; if (!alive(b)) continue; if (calm && a->team == 0 && b->team == 0) continue;
    float dx = b->x - a->x, dz = b->z - a->z, d = std::hypot(dx, dz), mn = (a->radius + b->radius) * 0.92f;
    if (d < mn && d > 1e-4f) { float p = (mn - d) / d * 0.5f, wa = a->boss ? 0.02f : ((a->act.on || a->eng) ? 0.03f : 1.f), wb = b->boss ? 0.02f : ((b->act.on || b->eng) ? 0.03f : 1.f), s = wa + wb; if (s <= 0) s = 1;
      a->x -= dx * p * wa / s * 2; a->z -= dz * p * wa / s * 2; b->x += dx * p * wb / s * 2; b->z += dz * p * wb / s * 2; } } }
}
void Game::fireFlare(bool byPlayer) {
  (void)byPlayer; bool allDown = true; for (Unit& h : heroes) if (!h.down) allDown = false;
  if (flare < 1 || !hasRun || state == S_TITLE || allDown) return; flare = 0;
  float r = lightRadius(), dmg = 0; for (Unit& h : heroes) dmg += h.dmg; dmg *= 1.5f * (1 + 0.3f * mod(C_P_FLARE)); Unit* l[MAXMON]; int n = monsNear(lightX, lightZ, r, l, MAXMON);
  for (int i = 0; i < n; i++) hurt(l[i], dmg, nullptr, K_FIRE, true, 2.0f);
  for (Unit& h : heroes) heal(&h, h.maxHp * 0.1f);
  float px = lightX, pz = lightZ; fx.ring(px, pz, 0.5f, r, 0.7f, {1.3f, 0.65f, 0.2f}); fx.ring(px, pz, 0.3f, r * 0.7f, 0.9f, {0.6f, 0.32f, 0.1f}, 0.08f); V3 fc{3, 1.6f, 0.4f}; fx.embers(px, 2, pz, 60, &fc, 3, 1.5f);
  fx.flash(px, 2.8f, pz, {1, 0.75f, 0.4f}, 420, 0.9f); screenFlash = 0.4f; shake(0.7f); flareV = 1; slowMo = 0.9f;
  for (int i = 1; i <= 3; i++) fx.ring(px, pz, 0.3f, r * (0.5f + i * 0.22f), 0.6f + i * 0.25f, {0.5f, 0.24f, 0.07f}, 0.05f + i * 0.02f);
  V3 e{3, 1.4f, 0.3f}; for (int i = 0; i < 26; i++) { float a = i / 26.f * TAU; fx.embers(px + std::cos(a) * r * 0.6f, 0.3f, pz + std::sin(a) * r * 0.6f, 3, &e, 0.5f, 2.2f); }
  for (int i = 0; i < n; i++) fx.sparks(l[i]->x, yOf(*l[i]), l[i]->z, 10, {3.5f, 1.8f, 0.5f}, 5);
  A(S_FLARE); toast("LANTERN FLARE", 0x8fd98a);
}

/* ---------- director ---------- */
void Game::shiftWorld(float dx) {
  anchorX -= dx; nextPackX -= dx; if (hasWay) wayX -= dx; for (Unit& h : heroes) { h.x -= dx; h.ox -= dx; } for (auto& m : mons) if (m.used) { m.x -= dx; m.ox -= dx; }
  for (auto& z : zones) z.x -= dx; for (auto& t : teles) t.x -= dx; for (auto& p : projs) { p.sx -= dx; p.ex -= dx; p.x -= dx; } for (auto& l : loot) l.x -= dx; if (rally) rallyX -= dx; fx.shift(dx); if (onShift) onShift(dx);
}
void Game::stewardCamp() {
  if (!P.steward) return; static const float pr[4] = {1, 1.05f, 1.5f, 1.7f}; int guard = 0;
  while (guard++ < 20) { int best = -1; float bc = 1e18f; for (int c = 0; c < NCAMP; c++) { float cost = campCost(c) * pr[c]; if (cost < bc) { bc = cost; best = c; } } if (best >= 0 && run.silver >= campCost(best)) buyCamp(best, true); else break; }
}
int Game::campCost(int id) const { return (int)std::round(CAMPS[id].base * std::pow(1.42f, (float)run.camp[id])); }
bool Game::buyCamp(int id, bool quiet) { int cost = campCost(id); if (run.silver < cost) return false; run.silver -= cost; run.camp[id]++; recalc(false); if (id == CAMP_TONIC) run.tonics = std::min(run.maxTonics, run.tonics + 1); if (!quiet) A(S_BUY); else { char b[64]; snprintf(b, 64, "CAMP: %s %d", CAMPS[id].name, run.camp[id]); toast(b); A(S_BUY); } return true; }
void Game::takeCard(int id) { run.cards[id]++; recalc(false); A(S_CARD); }
void Game::openPending() {
  if (pending <= 0) return; pending--; int pool[NCARDS], np = 0;
  for (int c = 0; c < NCARDS; c++) { bool ok = CARDS[c].who < 0; for (int p : run.party) if (p == CARDS[c].who) ok = true; if (ok && run.cards[c] < 3) pool[np++] = c; }
  nOffer = 0; while (nOffer < 3 && np) { int i = (int)(R() * np) % np; offer[nOffer++] = pool[i]; pool[i] = pool[--np]; }
  if (!nOffer) return; int rec = 0; float bs = -1; for (int i = 0; i < nOffer; i++) { int c = offer[i]; float s = (CARDS[c].who < 0 ? 1.2f : 1.f) + R() * 0.8f + (c == C_R_DMG || c == C_L_DMG || c == C_P_DMG || c == C_P_HP ? 0.3f : 0.f); if (s > bs) { bs = s; rec = i; } }
  offerRec = rec; panel = P_CARDS; panelT = panelMax = 6; panelTimer = true;
}
const char* Game::danger(bool next) const { float score = stDowns * 1.4f; score += stMinHp < 0.3f ? 2 : stMinHp < 0.5f ? 1 : stMinHp < 0.65f ? 0.4f : 0; if (stageT > 170) score += 0.6f; if (next) score += 0.5f; return score >= 2.2f ? "DEADLY" : score >= 0.9f ? "RISKY" : "SAFE"; }
bool Game::stewardPush() const { const char* d = danger(true); if (!strcmp(d, "SAFE")) return true; if (!strcmp(d, "RISKY")) return run.flames >= 2 || run.holdCount >= 2; return run.holdCount >= 3 + (3 - run.flames); }
void Game::openChoice() { choiceRec = stewardPush(); panel = P_CHOICE; panelT = panelMax = 7; panelTimer = true; }
void Game::choose(bool push) {
  panel = P_NONE; panelTimer = false; if (push) { run.stage++; run.holdCount = 0; } else run.holdCount++;
  for (int c = 0; c < NCLS; c++) if (UNLOCK_AT[c] && !P.unlocked[c] && run.maxStage >= UNLOCK_AT[c]) { P.unlocked[c] = true; char b[64]; snprintf(b, 64, "%s JOINS THE ROAD", CL[c].name); toast(b, 0x8fd98a); A(S_UNLOCK); }
  P.bestStage = std::max(P.bestStage, run.maxStage); P.bestLevel = std::max(P.bestLevel, run.level); setupStage(false); A(push ? S_PUSH : S_HOLDCH); save();
}
int Game::emberGain() const { return std::max(1, (int)std::floor(std::pow((float)std::max(0, run.maxStage), 1.25f) * 0.75f + run.relics)); }
void Game::endRun() {
  int gain = emberGain(); P.embers += gain; P.bestStage = std::max(P.bestStage, run.maxStage); P.bestLevel = std::max(P.bestLevel, run.level); P.bestTime = std::max(P.bestTime, run.time);
  last = {run.maxStage, run.level, run.time, gain, run.kills, run.lit}; for (int c = 0; c < NCLS; c++) if (UNLOCK_AT[c] && !P.unlocked[c] && run.maxStage >= UNLOCK_AT[c]) P.unlocked[c] = true;
  state = S_RUNEND; hasRun = false; A(S_LANTERNFALL); save(); panel = P_RUNEND; panelT = panelMax = 8; panelTimer = true;
}
int Game::boonCost(int id) const { const BoonDef& b = BOONS[id]; return b.unlock ? b.cost : (int)std::round(b.cost * (1 + 0.6f * boon(id))); }
bool Game::buyBoon(int id) { const BoonDef& b = BOONS[id]; int cost = boonCost(id); if (P.embers < cost || boon(id) >= b.max || (b.unlock && P.unlocked[b.cls])) return false; P.embers -= cost; P.boons[id]++; if (b.unlock) P.unlocked[b.cls] = true; A(S_BUY); return true; }
void Game::stewardBoons() { int g = 0; while (g++ < 40) { int best = -1; float bc = 1e9f; for (int i = 0; i < NBOON; i++) { const BoonDef& b = BOONS[i]; if (boon(i) >= b.max || (b.unlock && P.unlocked[b.cls])) continue; float c = boonCost(i) * (i == B_START ? 1.6f : 1.f); if (c < bc) { bc = c; best = i; } } if (best < 0 || !buyBoon(best)) break; } }
void Game::setFocus(Unit* m) { focus = m; focusT = 6; A(S_CLICK); }
void Game::setRally(float x, float z) { rally = true; rallyX = clampf(x, anchorX - 9, anchorX + 9); rallyZ = clampf(z, pathZ(x) - 6, pathZ(x) + 6); rallyT = 4; A(S_CLICK); }
void Game::beginRun(bool fresh) { if (fresh || !hasRun) newRun(); state = S_WALK; stT = 0; A(S_PUSH); }
void Game::tickPanel(float dt) {
  for (int i = 0; i < nToast; i++) toasts[i].t -= dt; while (nToast > 0 && toasts[nToast - 1].t <= 0) nToast--;
  if (!panelTimer) return; panelT -= dt; if (panelT > 0) return; panelTimer = false;
  if (panel == P_CARDS) { int id = offer[offerRec]; panel = P_NONE; takeCard(id); if (pending) openPending(); }
  else if (panel == P_CHOICE) choose(P.steward ? choiceRec : P.push);
  else if (panel == P_RUNEND) { stewardBoons(); panel = P_NONE; newRun(); }
}
void Game::step(float dt) {
  if (!hasRun || panel != P_NONE || state == S_TITLE || state == S_RUNEND) return;
  for (Unit& h : heroes) { h.ox = h.x; h.oz = h.z; } for (auto& m : mons) if (m.used) { m.ox = m.x; m.oz = m.z; }
  time += dt; run.time += dt; stT += dt; stageT += dt; anchorMoving = false;
  if (rally) { rallyT -= dt; if (rallyT <= 0) rally = false; } if (focus) { focusT -= dt; if (focusT <= 0 || !alive(focus)) focus = nullptr; }
  bool near_ = false; int liveN = 0; for (auto& m : mons) if (m.used && m.hp > 0) { liveN++; if (m.x - anchorX < 10.5f) near_ = true; }
  Unit* car = nullptr; for (Unit& h : heroes) if (h.cls == LAMPLIGHTER) car = &h; if (!car) car = &heroes[3]; lightX = car->x; lightZ = car->z; carrier = car;
  if (!liveN && anchorX == wdX && (state == S_WALK || state == S_FIGHT || state == S_BOSS || state == S_BREATHER)) { wdT += dt; if (wdT > 6) { wdT = 0; pending = 0; state = S_WALK; stT = 1; nextPackX = anchorX; } } else wdT = 0; wdX = anchorX;
  switch (state) {
    case S_WALK:
      if (pending > 0 && stT > 0.4f) { openPending(); break; }
      anchorMoving = true; anchorX += WALK * dt;
      if (run.pack < PACKS) { if (anchorX >= nextPackX) spawnPack(); }
      else { if (!hasWay) { hasWay = true; wayX = anchorX + 17; if (onWaylight) onWaylight(wayX); } if (anchorX >= wayX - 13) spawnBoss(); }
      break;
    case S_FIGHT: case S_BOSS:
      if (!near_ && liveN) { anchorMoving = true; anchorX += WALK * dt; }
      if (!liveN) {
        for (Unit& h : heroes) { if (h.down) revive(&h, 0.4f); else heal(&h, h.maxHp * std::min(0.4f, TUNE.heal + 0.025f * run.camp[CAMP_DRESS]), true); }
        if (state == S_BOSS) { state = S_RELIGHT; stT = 0; run.maxStage = std::max(run.maxStage, run.stage); A(S_BOSSDOWN); } else { state = S_BREATHER; stT = 0; nextPackX = anchorX + TUNE.gap + R() * 4; }
        for (auto& t : teles) t.on = false; stewardCamp();
      }
      break;
    case S_BREATHER: if (stT > 1.0f) { state = S_WALK; stT = 0; } break;
    case S_RELIGHT: {
      float wx = wayX - 0.9f, wz = pathZ(wayX) - 2.0f;
      if (!relit) { if (moveTo(car, wx, wz, car->speed, dt, 0.25f) || stT > 6) { car->faceT = std::atan2(0.8f, -1.f); if (!car->act.on && !relitAct) { relitAct = true; act(car, A_RELIGHT, 0.8f, 0.9f, nullptr, A_KINDLE); } }
        if (car->act.on) runAct(car, dt);
        for (Unit& h : heroes) { if (&h != car) stepHero(&h, dt); turn(&h, dt); } turn(car, dt); relT = 0; if (anchorX > WORLD_L + 64) shiftWorld(WORLD_L); return; }
      relT += dt; if (relT > 2.6f) { relit = false; relitAct = false; state = S_CHOICE; stT = 0; save(); openChoice(); }
      break; }
    case S_CHOICE: break;
    case S_WIPE:
      if (stT > 3.2f) {
        if (run.flames <= 0) { endRun(); return; }
        clearMons(); fx.clear(); run.stage = std::max(1, run.stage - 1); for (Unit& h : heroes) { h.down = false; h.hp = h.maxHp; h.act.on = false; } run.tonics = run.maxTonics; anchorX += 6; placeParty(); setupStage(false);
        char b[64]; snprintf(b, 64, "THE PARTY FALLS BACK TO STAGE %d", run.stage); toast(b, 0xe0685a); save();
      }
      break;
  }
  if (state == S_WIPE) { stepProjs(dt); return; }
  for (Unit& h : heroes) { stepHero(&h, dt); turn(&h, dt); }
  for (auto& m : mons) { if (!m.used) continue; turn(&m, dt); if (m.dashT > 0) { m.dashT -= dt; m.x += m.dashX * dt; m.z += m.dashZ * dt; } stepMon(&m, dt); }
  separate();
  for (auto& z : zones) { if (!z.on) continue; z.t += dt; z.tk += dt; if (z.tk >= z.tick) { z.tk = 0; zoneTick(z); } if (z.t >= z.dur) z.on = false; }
  for (auto& t : teles) if (t.on) { t.t += dt; if (t.t >= t.dur + 0.1f) t.on = false; }
  for (auto& m : mons) if (m.used && m.hp <= 0) { m.deadT += dt; if (m.deadT > 3.4f) m.used = false; }
  stepProjs(dt); stepLoot(dt);
  if (liveN) flare = std::min(1.f, flare + dt * 0.012f * (1 + 0.3f * mod(C_P_FLARE)));
  tonicCd -= dt;
  if (run.tonics > 0 && tonicCd <= 0) { for (Unit& h : heroes) if (!h.down && h.hp < h.maxHp * 0.33f) { run.tonics--; tonicCd = 5; heal(&h, h.maxHp * std::min(0.7f, 0.35f + 0.025f * run.camp[CAMP_TONIC])); A(S_HEAL); fx.floatText(h.x, 2.4f, h.z, "TONIC", 0x8fd98a, 12, 0.5f); break; } }
  if (P.steward && flare >= 1 && liveN) { Unit* l[MAXMON]; int inR = monsNear(lightX, lightZ, lightRadius(), l, MAXMON); bool lowHero = false; for (Unit& h : heroes) if (!h.down && h.hp < h.maxHp * 0.25f) lowHero = true;
    if (inR >= std::min(5, std::max(3, liveN)) || (bossRef && ((bossRef->act.on && !bossRef->act.hit) || partyHp() < 0.5f)) || lowHero) fireFlare(false); }
  if (anchorX > WORLD_L + 64) shiftWorld(WORLD_L);
}
