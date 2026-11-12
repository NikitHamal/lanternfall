// Living Towns II (included inside ex::{anonymous} by xgame.cpp after xfauna.inc):
//  - NPC daily schedules: morning at home, work at stalls / wells / mills, evening on the plaza, night asleep indoors
//  - talk menu (E): chat, ask for work, rumours / patrol report, merchants (arrows, draughts, weapon + armour tiers, sell food)
//  - quest journal: slay / hunt / gather wood / gather stone / scout, reported back to the settlement for gold + pardon
//  - persistent explorer save (save/explorer.sav)
// Zero per-frame heap allocation: fixed pools, text formatted on events / 2 Hz timer only.
struct Sch { float wx = 0, wz = 0, px = 0, pz = 0, hx = 0, hz = 0; bool init = false; int phase = -1; };
Sch SCH[MAXN];
struct WorkSpot { float x, z; int settle; };
std::vector<WorkSpot> workSpots;
enum { O_CHAT, O_WORK, O_RUMOR, O_BYE, O_ARROWS, O_DRAUGHT, O_WPN, O_ARMOR, O_SELL };
struct TownMenu { bool open = false; int npc = -1, kind = 0, n = 0, id[8] = {0}; } TM;
enum { QT_SLAY, QT_HUNT, QT_WOOD, QT_STONE, QT_SCOUT };
struct Quest { bool on = false, ready = false; int type = 0, need = 0, have = 0, reward = 0, settle = -1, poi = -1; float x = 0, z = 0, r = 0; char title[36] = {0}; } QS[3];
int qLastWood = 0, qLastStone = 0; float qTimer = 0, saveTimer = 0; bool saveBlocked = false;
const int WPN_COST[3] = {120, 260, 480}, ARM_COST[3] = {100, 220, 420};

inline bool townNight() { return clockH >= 20.5f || clockH < 5.5f; }
const char* compassWord(float dx, float dz) {   // +x east, +z south
  static const char* W8[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"}; float b = std::atan2(dx, -dz) * 180.f / PI; if (b < 0) b += 360.f; return W8[((int)(b / 45.f + 0.5f)) & 7];
}
void townBuildSpots() {
  workSpots.clear(); for (const Obj& o : objs) { if (o.kind == K_STALL || o.kind == K_WELL || o.kind == K_MILL || o.kind == K_HAY || o.kind == K_BARN || o.kind == K_BARREL || o.kind == K_CRATE) { int s = settleNear(o.x, o.z, 6.f); if (s >= 0 && settles[s].type <= ST_HAMLET) workSpots.push_back({o.x, o.z, s}); } }
}
void townSchedInit(Npc& n, Sch& s) {
  s.init = true; const Settle& st = settles[n.settle]; s.hx = n.homeX; s.hz = n.homeZ; float a = rng() * 6.28f, d = 3.f + rng() * 7.f; s.px = st.x + std::cos(a) * d; s.pz = st.z + std::sin(a) * d;
  int cnt = 0; for (const WorkSpot& w : workSpots) if (w.settle == n.settle) cnt++;
  if (cnt > 0) { int pick = (int)(rng() * cnt) % cnt; for (const WorkSpot& w : workSpots) if (w.settle == n.settle && pick-- == 0) { float dx = st.x - w.x, dz = st.z - w.z, l = std::sqrt(dx * dx + dz * dz) + 1e-3f; s.wx = w.x + dx / l * 2.6f; s.wz = w.z + dz / l * 2.6f; break; } }
  else { s.wx = s.px; s.wz = s.pz; }
}
bool townSleepTick(Npc& n) {
  if (!townNight() || n.panic > 0 || n.angry > 0) { n.asleep = false; n.state = S_IDLE; n.stT = rng() * 3.f; n.moving = 0; return false; }
  n.moving = 0; return true;
}
bool townSchedule(Npc& n, int idx, float dt) {
  if (n.settle < 0 || n.onWall || n.interior || n.angry > 0 || n.panic > 0) return false;
  Sch& s = SCH[idx]; if (!s.init) townSchedInit(n, s);
  float h = clockH; int ph = (h >= 5.5f && h < 8.f) ? 0 : (h >= 8.f && h < 17.5f) ? 1 : (h >= 17.5f && h < 20.5f) ? 2 : 3;
  if (ph != s.phase) { s.phase = ph; n.state = S_IDLE; n.stT = rng() * 2.5f; }
  if (ph == 0) return false;
  float ax, az, rad; if (ph == 1) { ax = s.wx; az = s.wz; rad = 2.8f; } else if (ph == 2) { ax = s.px; az = s.pz; rad = 5.5f; } else { ax = s.hx; az = s.hz; rad = 0.f; }
  if (ph == 3 && dist2(n.x, n.z, s.hx, s.hz) < 2.4f * 2.4f) { n.asleep = true; n.moving = 0; return true; }
  if (n.state == S_IDLE) { n.moving = 0; n.stT -= dt; if (n.stT <= 0) { float a = rng() * 6.28f, d = rng() * rad; n.tx = ax + std::cos(a) * d; n.tz = az + std::sin(a) * d; n.state = S_WALK; n.stT = 25.f; } }
  else { n.stT -= dt; float d = steer(n, n.tx, n.tz, ph == 3 ? n.spd * 1.3f : n.spd, dt); if (d < 0.6f || n.stT <= 0) { n.state = S_IDLE; n.moving = 0; n.stT = ph == 3 ? 0.f : 4.f + rng() * 9.f; } }
  return true;
}

// ------------------------------------------------------------------ quests
void questRefresh() {
  H.qN = 0; for (int i = 0; i < 3; i++) {
    const Quest& q = QS[i]; if (!q.on) continue; int k = H.qN++; snprintf(H.qTitle[k], sizeof H.qTitle[k], "%s", q.title); H.qReady[k] = q.ready;
    float dx, dz; const char* nm = ""; char dist[28] = {0};
    if (q.ready) { const Settle& s = settles[q.settle]; dx = s.x - P.x; dz = s.z - P.z; nm = s.name; snprintf(H.qProg[k], sizeof H.qProg[k], "DONE - REPORT TO %.14s %s %dM", nm, compassWord(dx, dz), (int)std::sqrt(dx * dx + dz * dz)); continue; }
    dx = q.x - P.x; dz = q.z - P.z; snprintf(dist, sizeof dist, "%s %dM", compassWord(dx, dz), (int)std::sqrt(dx * dx + dz * dz));
    if (q.type == QT_SLAY) snprintf(H.qProg[k], sizeof H.qProg[k], "%d/%d SLAIN - AREA %s", q.have, q.need, dist);
    else if (q.type == QT_HUNT) snprintf(H.qProg[k], sizeof H.qProg[k], "%d/%d BEASTS HUNTED", q.have, q.need);
    else if (q.type == QT_WOOD) snprintf(H.qProg[k], sizeof H.qProg[k], "%d/%d WOOD GATHERED", q.have, q.need);
    else if (q.type == QT_STONE) snprintf(H.qProg[k], sizeof H.qProg[k], "%d/%d STONE GATHERED", q.have, q.need);
    else snprintf(H.qProg[k], sizeof H.qProg[k], "FIND IT - %s", dist);
  }
}
void questDone(Quest& q) { if (q.ready) return; q.ready = true; q.have = q.need; char sub[56]; snprintf(sub, sizeof sub, "REPORT TO %s FOR %d GOLD", settles[q.settle].name, q.reward); banner("QUEST COMPLETE", sub, 3.5f); audio::play(S_LEVELUP, 0, 0.6f); questRefresh(); }
int questCount() { int c = 0; for (const Quest& q : QS) if (q.on) c++; return c; }
int makeQuest(const Npc& giver, bool military) {
  Quest* slot = nullptr; for (Quest& q : QS) if (!q.on) { slot = &q; break; } if (!slot) return -1; int st = giver.settle >= 0 ? giver.settle : 0; const Settle& S0 = settles[st]; Quest q; q.on = true; q.settle = st; q.x = S0.x; q.z = S0.z;
  float r = rng(); int type = military ? (r < 0.65f ? QT_SLAY : r < 0.85f ? QT_SCOUT : QT_HUNT) : (r < 0.22f ? QT_HUNT : r < 0.45f ? QT_WOOD : r < 0.60f ? QT_STONE : r < 0.80f ? QT_SCOUT : QT_SLAY);
  if (type == QT_SCOUT) { int best = -1; float bd = 1e30f; for (size_t i = 0; i < pois.size(); i++) { if (pois[i].found) continue; bool taken = false; for (const Quest& o : QS) if (o.on && o.type == QT_SCOUT && o.poi == (int)i) taken = true; if (taken) continue; float d = dist2(pois[i].x, pois[i].z, S0.x, S0.z) * (0.7f + 0.6f * hfr((int)i + dayIdx)); if (d < bd) { bd = d; best = (int)i; } }
    if (best < 0) type = QT_HUNT; else { q.poi = best; q.x = pois[best].x; q.z = pois[best].z; snprintf(q.title, sizeof q.title, "SCOUT: %.26s", pois[best].name); q.need = 1; q.reward = 60 + (int)(std::sqrt(bd) / 12.f); } }
  q.type = type;
  if (type == QT_SLAY) { q.need = 3 + (int)(rng() * 4.f); q.r = 340.f; q.reward = 38 + q.need * 17; snprintf(q.title, sizeof q.title, "CULL THE GLOAM X%d", q.need); }
  else if (type == QT_HUNT) { q.need = 2 + (int)(rng() * 3.f); q.reward = 24 + q.need * 12; snprintf(q.title, sizeof q.title, "HUNT %d BEASTS FOR THE LARDER", q.need); }
  else if (type == QT_WOOD) { q.need = 24 + 8 * (int)(rng() * 4.f); q.reward = 22 + q.need; snprintf(q.title, sizeof q.title, "FETCH %d WOOD", q.need); }
  else if (type == QT_STONE) { q.need = 24 + 8 * (int)(rng() * 4.f); q.reward = 26 + q.need; snprintf(q.title, sizeof q.title, "QUARRY %d STONE", q.need); }
  *slot = q; qLastWood = K.wood; qLastStone = K.stone; questRefresh(); return (int)(slot - QS);
}
void questProgress(Quest& q) { if (q.have >= q.need) { questDone(q); return; } char sub[44]; snprintf(sub, sizeof sub, "%.24s  %d/%d", q.title, q.have, q.need); banner("QUEST UPDATE", sub, 1.6f); questRefresh(); }
void townOnKill(const Npc& t) {
  for (Quest& q : QS) if (q.on && !q.ready && q.type == QT_SLAY && dist2(t.x, t.z, q.x, q.z) < q.r * q.r) { q.have++; questProgress(q); }
  if (rng() < (t.boss ? 1.f : 0.06f) && potions < 5) { potions++; banner("HEALING DRAUGHT FOUND", "PRESS U TO DRINK", 2.4f); }
}
void townOnAnimal() { for (Quest& q : QS) if (q.on && !q.ready && q.type == QT_HUNT) { q.have++; questProgress(q); } }
bool turnIn(const Npc& n) {
  for (Quest& q : QS) if (q.on && q.ready && (n.settle < 0 || q.settle == n.settle)) {
    gold += q.reward; B.arrows = std::min(60, B.arrows + 6); for (int k = 0; k < 3; k++) bounty[k] = std::max(0.f, bounty[k] - 35.f); if (rng() < 0.3f && potions < 5) potions++;
    char sub[48]; snprintf(sub, sizeof sub, "+%d GOLD  +6 ARROWS  CRIMES FORGIVEN", q.reward); banner("QUEST REWARD", sub, 3.5f); audio::play(S_LEVELUP, 0, 0.7f); q = Quest(); questRefresh(); return true; }
  return false;
}

// ------------------------------------------------------------------ talk menu
void talkLine(const char* s) { snprintf(H.talkLine, sizeof H.talkLine, "%s", s); }
void townRefresh() {
  H.talkOpen = TM.open; H.talkN = TM.n; H.potions = potions; H.wpnTier = wpnTier; H.armorTier = armorTier; if (!TM.open) return;
  const Npc& n = npcs[TM.npc]; bool ready = false; for (const Quest& q : QS) if (q.on && q.ready && (n.settle < 0 || q.settle == n.settle)) ready = true;
  for (int i = 0; i < TM.n; i++) { char* o = H.talkOpt[i]; bool ok = true; size_t L = sizeof H.talkOpt[i];
    switch (TM.id[i]) {
      case O_CHAT: snprintf(o, L, "CHAT"); break;
      case O_WORK: if (ready) snprintf(o, L, "REPORT A COMPLETED TASK"); else if (questCount() >= 3) { snprintf(o, L, "ASK FOR WORK (JOURNAL FULL)"); ok = false; } else snprintf(o, L, n.role >= R_GUARD ? "ASK FOR A BOUNTY" : "ASK FOR WORK"); break;
      case O_RUMOR: snprintf(o, L, n.role >= R_GUARD ? "ASK FOR A PATROL REPORT" : "ASK FOR RUMOURS"); break;
      case O_BYE: snprintf(o, L, "FAREWELL"); break;
      case O_ARROWS: snprintf(o, L, "BUY 10 ARROWS - 12 GOLD"); ok = gold >= 12 && B.arrows < 60; break;
      case O_DRAUGHT: snprintf(o, L, "BUY HEALING DRAUGHT - 30 GOLD  (HAVE %d)", potions); ok = gold >= 30 && potions < 5; break;
      case O_WPN: if (wpnTier < 3) { snprintf(o, L, "TEMPER WEAPONS TIER %d (+25%% DAMAGE) - %d GOLD", wpnTier + 1, WPN_COST[wpnTier]); ok = gold >= WPN_COST[wpnTier]; } else { snprintf(o, L, "WEAPONS FULLY TEMPERED"); ok = false; } break;
      case O_ARMOR: if (armorTier < 3) { snprintf(o, L, "FORGE ARMOUR TIER %d (-12%% DAMAGE TAKEN) - %d GOLD", armorTier + 1, ARM_COST[armorTier]); ok = gold >= ARM_COST[armorTier]; } else { snprintf(o, L, "ARMOUR FULLY FORGED"); ok = false; } break;
      case O_SELL: snprintf(o, L, "SELL 20 FOOD FOR 16 GOLD  (HAVE %d)", K.food); ok = K.food >= 20; break; }
    H.talkOk[i] = ok; }
}
bool isMerchant(const Npc& n) { return n.role == R_VILL && n.post >= 0 && posts[n.post].type == 4; }
bool townOpen(Npc& n, int idx, const char* who, const char* line) {
  if (n.asleep || CI.stage) return false; TM.open = true; TM.npc = idx; n.talking = true; TM.n = 0;
  if (isMerchant(n)) { TM.kind = 1; int ids[7] = {O_ARROWS, O_DRAUGHT, O_WPN, O_ARMOR, O_SELL, O_WORK, O_BYE}; for (int k : ids) TM.id[TM.n++] = k; talkLine("Fine wares for a traveller with coin. Arrows, draughts, steel - what do you need?"); }
  else if (n.role >= R_GUARD) { TM.kind = 2; int ids[4] = {O_CHAT, O_WORK, O_RUMOR, O_BYE}; for (int k : ids) TM.id[TM.n++] = k; talkLine(line); }
  else { TM.kind = 0; int ids[4] = {O_CHAT, O_WORK, O_RUMOR, O_BYE}; for (int k : ids) TM.id[TM.n++] = k; talkLine(line); }
  snprintf(H.talkName, sizeof H.talkName, "%s", who); audio::play(S_CLICK, 0, 0.5f); townRefresh(); return true;
}
void townClose() { if (TM.npc >= 0) npcs[TM.npc].talking = false; TM.open = false; TM.npc = -1; townRefresh(); }
void townChoose(int k) {
  if (k < 0 || k >= TM.n) return; Npc& n = npcs[TM.npc]; int id = TM.id[k]; char buf[140];
  switch (id) {
    case O_CHAT: { const char* l; if (n.role == R_VILL) { int r = (int)(rng() * 13); l = r < 10 ? L_VILL[r] : L_NAT[n.settle >= 0 ? settles[n.settle].nation : 0][r % 2]; } else if (n.role == R_GUARD) l = L_GUARD[(int)(rng() * 6) % 6]; else if (n.role == R_KNIGHT) l = L_KNIGHT[(int)(rng() * 4) % 4]; else l = L_SOLD[(int)(rng() * 3) % 3]; talkLine(l); break; }
    case O_WORK: {
      bool ready = false; for (const Quest& q : QS) if (q.on && q.ready && (n.settle < 0 || q.settle == n.settle)) ready = true;
      if (ready) { turnIn(n); talkLine("Well done. The realm is safer for it - take your due, and my thanks."); }
      else if (questCount() >= 3) talkLine("You already carry enough burdens. Finish those first.");
      else if (int qi = makeQuest(n, n.role >= R_GUARD); qi >= 0) { const Quest& q = QS[qi];
        snprintf(buf, sizeof buf, "I have a task for you: %.34s. %d gold when it is done - it is in your journal.", q.title, q.reward); talkLine(buf); banner("QUEST ACCEPTED", q.title, 3.f); }
      break; }
    case O_RUMOR: {
      if (n.role >= R_GUARD) { int bi = -1; float bd = 1e30f; for (size_t i = 0; i < lairs.size(); i++) { float d = dist2(lairs[i].x, lairs[i].z, P.x, P.z); if (d < bd) { bd = d; bi = (int)i; } }
        if (bi >= 0) { snprintf(buf, sizeof buf, "Beasts den about %d paces %s of here. Go armed, and never alone after dusk.", (int)std::sqrt(bd), compassWord(lairs[bi].x - P.x, lairs[bi].z - P.z)); talkLine(buf); } else talkLine("The roads are quiet. Mind the dark all the same."); }
      else { int bi = -1; float bd = 1e30f; for (size_t i = 0; i < pois.size(); i++) { if (pois[i].found) continue; float d = dist2(pois[i].x, pois[i].z, P.x, P.z); if (d < bd) { bd = d; bi = (int)i; } }
        if (bi >= 0) { snprintf(buf, sizeof buf, "Travellers whisper of %.28s, some %d paces %s of here.", pois[bi].name, (int)std::sqrt(bd), compassWord(pois[bi].x - P.x, pois[bi].z - P.z)); talkLine(buf); } else talkLine("You have seen every wonder I have heard of, wanderer."); }
      break; }
    case O_ARROWS: if (gold >= 12 && B.arrows < 60) { gold -= 12; B.arrows = std::min(60, B.arrows + 10); talkLine("Straight shafts, well fletched. Loose them true."); audio::play(S_CLICK, 0, 0.7f); } else talkLine(gold < 12 ? "Come back when your purse is heavier." : "Your quiver is already full."); break;
    case O_DRAUGHT: if (gold >= 30 && potions < 5) { gold -= 30; potions++; talkLine("A healing draught. Press U when the fight turns against you."); audio::play(S_CLICK, 0, 0.7f); } else talkLine(gold < 30 ? "Thirty gold, friend. No credit." : "You cannot carry more of them."); break;
    case O_WPN: if (wpnTier < 3 && gold >= WPN_COST[wpnTier]) { gold -= WPN_COST[wpnTier]; wpnTier++; talkLine("Bow limbs re-laced, blade ground keen. They will bite deeper now."); audio::play(S_LEVELUP, 0, 0.6f); banner("WEAPONS TEMPERED", wpnTier == 1 ? "TIER 1 - +25% DAMAGE" : wpnTier == 2 ? "TIER 2 - +50% DAMAGE" : "TIER 3 - +75% DAMAGE", 2.5f); } else talkLine("I need more gold for that work."); break;
    case O_ARMOR: if (armorTier < 3 && gold >= ARM_COST[armorTier]) { gold -= ARM_COST[armorTier]; armorTier++; talkLine("Fits you well. Blows will glance off where they once bit."); audio::play(S_LEVELUP, 0, 0.6f); banner("ARMOUR FORGED", armorTier == 1 ? "TIER 1 - 12% LESS DAMAGE" : armorTier == 2 ? "TIER 2 - 24% LESS DAMAGE" : "TIER 3 - 36% LESS DAMAGE", 2.5f); } else talkLine("I need more gold for that work."); break;
    case O_SELL: if (K.food >= 20) { K.food -= 20; gold += 16; talkLine("Good provisions. The larder thanks you."); audio::play(S_CLICK, 0, 0.7f); } else talkLine("Not enough to make it worth my while."); break;
    default: townClose(); return; }
  townRefresh(); questRefresh();
}
bool townKey(int key) {
  if (key == 27 || key == 'E') { townClose(); return true; }
  if (key >= '1' && key <= '8') { int k = key - '1'; if (k < TM.n) { if (TM.id[k] == O_BYE) townClose(); else townChoose(k); } return true; }
  return false;
}
void townDraught() {
  if (potions <= 0) { banner("NO DRAUGHTS", "BUY THEM FROM MERCHANTS - SOMETIMES MONSTERS DROP ONE", 1.8f); return; } if (P.hp >= P.maxHp - 1.f && K.mana >= K.maxMana - 1.f) return;
  potions--; P.hp = std::min(P.maxHp, P.hp + P.maxHp * 0.7f); K.mana = std::min(K.maxMana, K.mana + 35.f); banner("HEALING DRAUGHT", "HEALTH AND MANA RESTORED", 1.6f); audio::play(S_LEVELUP, 0, 0.5f); townRefresh();
}
bool dbgCensus = false; int dbgCensusN = 0;
bool townBlocks() { return TM.open; }

// ------------------------------------------------------------------ save / load
void townSave() {
  if (saveBlocked || P.deadT > 0 || P.hp <= 0) return; TMKDIR("save"); FILE* f = fopen("save/explorer.sav", "wb"); if (!f) return;
  float sx = CI.in ? CI.ox : P.x, sz = CI.in ? CI.oz : P.z;
  fprintf(f, "LFX2\n%d %d %d %d %d %d %d %d %.3f %d %.2f %.2f %.1f %.1f %d %d\n", gold, B.arrows, K.wood, K.stone, K.food, wpnTier, armorTier, potions, clockH, dayIdx, sx, sz, P.hp, K.mana, H.kills, H.reclaimed);
  for (const Poi& p : pois) fputc(p.found ? '1' : '0', f); fputc('\n', f);
  for (const Quest& q : QS) fprintf(f, "%d %d %d %d %d %d %d %d %.1f %.1f %.1f %s\n", q.on ? 1 : 0, q.ready ? 1 : 0, q.type, q.need, q.have, q.reward, q.settle, q.poi, q.x, q.z, q.r, q.title[0] ? q.title : "-");
  fclose(f);
}
bool townLoad() {
  FILE* f = fopen("save/explorer.sav", "rb"); if (!f) return false; char hdr[8] = {0}; if (!fgets(hdr, sizeof hdr, f) || strncmp(hdr, "LFX2", 4)) { fclose(f); return false; }
  int g, ar, wd, sn, fd, wt, at, pt, dy, kl, rc; float ck, x, z, hp, mn; if (fscanf(f, "%d %d %d %d %d %d %d %d %f %d %f %f %f %f %d %d", &g, &ar, &wd, &sn, &fd, &wt, &at, &pt, &ck, &dy, &x, &z, &hp, &mn, &kl, &rc) != 16) { fclose(f); return false; }
  static char bits[2048]; if (fscanf(f, "%2040s", bits) != 1) { fclose(f); return false; } for (size_t i = 0; i < pois.size() && bits[i]; i++) pois[i].found = pois[i].found || bits[i] == '1';
  for (Quest& q : QS) { q = Quest(); int on, rd; char title[40] = {0}; if (fscanf(f, "%d %d %d %d %d %d %d %d %f %f %f", &on, &rd, &q.type, &q.need, &q.have, &q.reward, &q.settle, &q.poi, &q.x, &q.z, &q.r) != 11) break; fgetc(f); if (!fgets(title, sizeof title, f)) title[0] = 0; size_t l = std::strlen(title); while (l && (title[l - 1] == '\n' || title[l - 1] == '\r')) title[--l] = 0;
    q.on = on != 0 && q.settle >= 0 && q.settle < (int)settles.size(); q.ready = rd != 0; snprintf(q.title, sizeof q.title, "%s", title); }
  fclose(f); gold = g; B.arrows = std::max(10, ar); K.wood = wd; K.stone = sn; K.food = fd; wpnTier = clampf((float)wt, 0, 3); armorTier = clampf((float)at, 0, 3); potions = clampf((float)pt, 0, 5); clockH = ck; dayIdx = dy; H.kills = kl; H.reclaimed = rc;
  if (std::fabs(x) < HALF - 20 && std::fabs(z) < HALF - 20) { P.x = x; P.z = z; P.y = height(x, z); P.camInit = false; } P.hp = clampf(hp, 20.f, P.maxHp); K.mana = clampf(mn, 0.f, K.maxMana); return true;
}
void townStart() {
  static bool inited = false; if (!inited) { inited = true; townBuildSpots(); }
  TM = TownMenu(); for (Quest& q : QS) q = Quest(); wpnTier = armorTier = potions = 0; saveTimer = 0; saveBlocked = false;
  bool loaded = townLoad(); H.disc = 0; for (const Poi& p : pois) if (p.found) H.disc++;
  qLastWood = K.wood; qLastStone = K.stone; questRefresh(); townRefresh(); if (loaded) banner("EXPLORER SAVE LOADED", "YOUR JOURNEY CONTINUES", 3.f);
}
void townTick(float dt) {
  // talk menu: close if the speaker is gone / asleep / far
  if (TM.open) { const Npc& n = npcs[TM.npc]; if (!n.used || !n.alive || n.asleep || dist2(n.x, n.z, P.x, P.z) > 7.f * 7.f || P.deadT > 0 || CI.stage) townClose(); }
  // wood / stone quests track the stockpile delta
  { int dw = K.wood - qLastWood, ds = K.stone - qLastStone; qLastWood = K.wood; qLastStone = K.stone;
    if (dw > 0 || ds > 0) for (Quest& q : QS) if (q.on && !q.ready) { if (q.type == QT_WOOD && dw > 0) { q.have += dw; questProgress(q); } else if (q.type == QT_STONE && ds > 0) { q.have += ds; questProgress(q); } } }
  qTimer -= dt; if (qTimer <= 0) { qTimer = 0.5f;
    for (Quest& q : QS) if (q.on && !q.ready && q.type == QT_SCOUT && q.poi >= 0 && q.poi < (int)pois.size() && pois[q.poi].found) questDone(q);
    questRefresh(); if (TM.open) townRefresh(); }
  if (dbgCensus && ++dbgCensusN % 900 == 0) { int tot = 0, sl = 0, wk = 0, pl = 0, hm = 0; for (int i = 0; i < MAXN; i++) { const Npc& n = npcs[i]; if (!n.used || !n.alive || n.role != R_VILL || n.settle < 0) continue; tot++; const Sch& c = SCH[i]; if (n.asleep) sl++; else if (c.init && dist2(n.x, n.z, c.wx, c.wz) < 36.f) wk++; else if (dist2(n.x, n.z, settles[n.settle].x, settles[n.settle].z) < 225.f) pl++; else hm++; } fprintf(stderr, "census h=%.2f villagers=%d asleep=%d atWork=%d plaza=%d other=%d\n", clockH, tot, sl, wk, pl, hm); }
  H.potions = potions; H.wpnTier = wpnTier; H.armorTier = armorTier;
  saveTimer += dt; if (saveTimer > 45.f && sinceStart > 20.f && !CI.in) { saveTimer = 0; townSave(); }
}
