// Kingdom Building, Resource Harvesting, Warband Command, Spells, Mounts & Nightly Siege module.
// Included inside the anonymous namespace of xgame.cpp (after xgame_combat.inc). Zero per-frame heap allocations.

enum BType { BT_WALL = 0, BT_TOWER, BT_GATE, BT_BARRACKS, BT_FARM, BT_BEACON, NBT };
enum Order { ORD_FOLLOW = 0, ORD_HOLD, ORD_CHARGE, ORD_GARRISON };
enum Spell { SP_FIREBALL = 0, SP_FROST, SP_RALLY, SP_LANCE, NSPELL };

struct BDef { const char* name; const char* shortN; int kind, wood, stone, goldC; float hp, r, hw, hd, sc, shootR, shootCd; };
const BDef BDEFS[NBT] = {
  {"STONE WALL",     "WALL",     K_WALL,     12, 18,  0, 480.f, 2.35f, 2.55f, 1.25f, 1.00f,  0.f, 0.f},
  {"ARCHER TOWER",   "TOWER",    K_WATCH,    24, 28, 10, 420.f, 2.60f, 2.50f, 2.50f, 1.05f, 50.f, 1.05f},
  {"FORTIFIED GATE", "GATE",     K_GATE,     20, 32, 10, 620.f, 3.30f, 5.20f, 2.40f, 0.85f,  0.f, 0.f},
  {"ROYAL BARRACKS", "BARRACKS", K_LONG,     32, 24, 20, 520.f, 4.10f, 5.60f, 3.40f, 0.92f,  0.f, 0.f},
  {"HOMESTEAD FARM", "FARM",     K_COTTAGE,  18, 10,  0, 280.f, 3.20f, 3.40f, 3.00f, 1.05f,  0.f, 0.f},
  {"SUNFIRE BEACON", "BEACON",   K_PURIFIED, 14, 20, 15, 350.f, 1.85f, 1.85f, 1.85f, 0.72f, 13.f, 0.90f},
};
const char* ORD_NAMES[4] = {"FOLLOW", "HOLD LINE", "CHARGE", "GARRISON"};
const char* SP_NAMES[NSPELL] = {"FIREBALL", "FROST NOVA", "SOLAR AEGIS", "STORM LANCE"};
const float SP_COST[NSPELL] = {22.f, 26.f, 30.f, 20.f};
const float SP_CD[NSPELL] = {0.95f, 3.2f, 4.5f, 1.15f};

constexpr int MAXB = 128, MAXSP = 36, MAXRIFT = 32;
struct BuildObj {
  bool used = false; int btype = 0, kind = 0;
  float x = 0, y = 0, z = 0, ry = 0, hp = 100, maxHp = 100, cd = 0, hitT = 0, grow = 1.f, tick = 0;
};
struct SpellProj {
  bool on = false; int sp = 0, pierces = 0, lastHit = -1;
  V3 p, v, dir; float life = 0, dmg = 0;
};
struct RiftObj {
  bool used = false, pure = false; float x = 0, y = 0, z = 0, pulse = 0; int lairIdx = -1;
};
struct GNode {
  float x, z, r; uint8_t gtype, left; float regen;
};
struct NpcK {
  bool sworn = false, siegeUnit = false; int tgtBuild = -1; float boostT = 0;
};

struct KingState {
  int wood = 90, stone = 95, food = 40;
  bool buildMode = false, ghostValid = false, mounted = false;
  int bsel = 0, order = ORD_FOLLOW, spell = SP_FIREBALL;
  float bYaw = 0, mana = 100.f, maxMana = 100.f, spCd = 0, aegisT = 0, gatherCd = 0;
  float holdX = 0, holdZ = 0, holdYaw = 0;
  V3 ghostPos{0, 0, 0}; float ghostYaw = 0;
  int nightWave = 0, siegeLeft = 0, siegeTotal = 0, reclaimed = 0, totalRifts = 0, troops = 0, builtCount = 0;
  bool siegeActive = false;
  FigAnim mountAnim;
} K;

BuildObj builds[MAXB];
SpellProj sproj[MAXSP];
RiftObj rifts[MAXRIFT];
NpcK KEX[MAXN];
std::vector<GNode> gnodes;
int gCellStart[32 * 32 + 1];

inline int gCellOf(float x, float z) {
  int cx = std::min(31, std::max(0, (int)((x + HALF) / 64.f)));
  int cz = std::min(31, std::max(0, (int)((z + HALF) / 64.f)));
  return cz * 32 + cx;
}

void initKingdomWorld() {
  // Spawn ore veins along rocky slopes & hills + build spatial resource harvest grid
  Rng kr(7717u);
  for (int t = 0, made = 0; t < 2400 && made < 85; t++) {
    float x = (kr() * 2.f - 1.f) * (HALF - 60.f), z = (kr() * 2.f - 1.f) * (HALF - 60.f);
    float h = height(x, z), sl = slopeAt(x, z);
    if (h < 14.f || h > 105.f || sl < 0.22f || sl > 0.85f || inWater(x, z) || roadDist(x, z) < 9.f || settleNear(x, z, 28.f) >= 0) continue;
    Obj o{}; o.x = x; o.y = h - 0.1f; o.z = z; o.ry = kr() * TAU; o.sx = o.sy = o.sz = 0.95f + kr() * 0.55f;
    o.kind = K_ORE; o.r = o.g = o.b = 128; o.sway = 0; objs.push_back(o); made++;
  }
  // Bucket harvestable world objects into 64m cells (built once at startup)
  struct TmpN { int cell; GNode n; };
  std::vector<TmpN> tmp; tmp.reserve(8000);
  for (size_t i = 0; i < objs.size(); i++) {
    const Obj& o = objs[i]; int gt = -1; float r = 2.2f; uint8_t amt = 4;
    if (o.kind == K_PINE || o.kind == K_PINEY || o.kind == K_OAK || o.kind == K_DEAD || o.kind == K_LOG) { gt = 0; r = 2.5f; amt = 4; }
    else if (o.kind == K_BOULDER || o.kind == K_BIGROCK || o.kind == K_CAIRN) { gt = 1; r = 2.8f * std::min(1.6f, o.sx); amt = 4; }
    else if (o.kind == K_ORE) { gt = 2; r = 2.6f; amt = 5; }
    else if (o.kind == K_HAY || o.kind == K_BUSH) { gt = 3; r = 2.3f; amt = 3; }
    if (gt < 0) continue;
    if (gt <= 1 && (i % 3) != 0) continue;   // keep gnode table compact (~4k nodes across 4 sq km)
    tmp.push_back({gCellOf(o.x, o.z), {o.x, o.z, r, (uint8_t)gt, amt, 0.f}});
  }
  for (int c = 0; c <= 1024; c++) gCellStart[c] = 0;
  for (const TmpN& t : tmp) gCellStart[t.cell + 1]++;
  for (int c = 0; c < 1024; c++) gCellStart[c + 1] += gCellStart[c];
  gnodes.resize(tmp.size());
  int cur[1024]; memcpy(cur, gCellStart, sizeof cur);
  for (const TmpN& t : tmp) gnodes[cur[t.cell]++] = t.n;
}

void resetKingdomSession() {
  K = KingState(); K.mountAnim = FigAnim(); K.mountAnim.id = 8888u;
  gold = 65;
  for (BuildObj& b : builds) b.used = false;
  for (SpellProj& s : sproj) s.on = false;
  for (int i = 0; i < MAXN; i++) KEX[i] = NpcK();
  for (RiftObj& r : rifts) r = RiftObj();
  int nr = 0;
  for (size_t i = 0; i < lairs.size() && nr < MAXRIFT; i++) {
    const Lair& L = lairs[i]; float rx = L.x + 2.2f, rz = L.z - 1.8f;
    if (inWater(rx, rz)) { rx = L.x; rz = L.z; }
    RiftObj& rf = rifts[nr++]; rf.used = true; rf.pure = false; rf.x = rx; rf.z = rz; rf.y = height(rx, rz); rf.lairIdx = (int)i;
  }
  K.totalRifts = nr;
}

// Collide against player-built fortifications. `friendly` passes freely through BT_GATE archways!
bool collideBuilds(float& x, float& z, float r, bool friendly, int* hitBuild = nullptr) {
  bool moved = false;
  for (int i = 0; i < MAXB; i++) {
    const BuildObj& b = builds[i]; if (!b.used) continue;
    const BDef& d = BDEFS[b.btype];
    float dx = x - b.x, dz = z - b.z;
    if (std::fabs(dx) > 9.f || std::fabs(dz) > 9.f) continue;
    float cs = std::cos(b.ry), sn = std::sin(b.ry);
    float lx = dx * cs - dz * sn, lz = dx * sn + dz * cs;
    if (b.btype == BT_GATE) {
      if (friendly && std::fabs(lx) < 1.85f) continue;   // open gate passage for allies & player!
      if (friendly) {
        // collide only with the two gatehouse side towers
        for (int sd = -1; sd <= 1; sd += 2) {
          float tx = sd * 3.8f, ddx = lx - tx, ddz = lz; float dd = std::sqrt(ddx * ddx + ddz * ddz), mr = 1.9f + r;
          if (dd < mr) { if (dd < 1e-4f) { ddx = 1.f; ddz = 0.f; dd = 1.f; } lx = tx + ddx / dd * mr; lz = ddz / dd * mr; moved = true; }
        }
        if (moved) { x = b.x + lx * cs + lz * sn; z = b.z - lx * sn + lz * cs; }
        continue;
      }
    }
    if (b.btype == BT_WALL || b.btype == BT_GATE || b.btype == BT_BARRACKS) {
      float hx = d.hw * d.sc + r, hz = d.hd * d.sc + r;
      if (std::fabs(lx) < hx && std::fabs(lz) < hz) {
        float px = hx - std::fabs(lx), pz = hz - std::fabs(lz);
        if (px < pz) lx = (lx >= 0 ? hx : -hx); else lz = (lz >= 0 ? hz : -hz);
        x = b.x + lx * cs + lz * sn; z = b.z - lx * sn + lz * cs;
        moved = true; if (hitBuild) *hitBuild = i;
      }
    } else {
      float dd = std::sqrt(dx * dx + dz * dz), mr = d.r * d.sc + r;
      if (dd < mr) {
        if (dd < 1e-4f) { dx = 1.f; dz = 0.f; dd = 1.f; }
        x = b.x + dx / dd * mr; z = b.z + dz / dd * mr;
        moved = true; if (hitBuild) *hitBuild = i;
      }
    }
  }
  return moved;
}

void pushFloatText(float wx, float wy, float wz, const char* txt, uint32_t col) {
  DmgTxt& x = DT[dtNext++ % 12];
  x.x = wx; x.y = wy; x.z = wz; x.t = 0.f; x.val = 0; x.zone = 0; x.kill = false; x.col = col;
  snprintf(x.txt, sizeof x.txt, "%s", txt);
}

GNode* nearestGNode(float px, float pz, float maxR) {
  int cx0 = std::max(0, (int)((px - maxR + HALF) / 64.f)), cx1 = std::min(31, (int)((px + maxR + HALF) / 64.f));
  int cz0 = std::max(0, (int)((pz - maxR + HALF) / 64.f)), cz1 = std::min(31, (int)((pz + maxR + HALF) / 64.f));
  GNode* best = nullptr; float bd = maxR * maxR;
  for (int cz = cz0; cz <= cz1; cz++) for (int cx = cx0; cx <= cx1; cx++) {
    int c = cz * 32 + cx;
    for (int i = gCellStart[c]; i < gCellStart[c + 1]; i++) {
      GNode& g = gnodes[i]; if (!g.left) continue;
      float d2 = dist2(px, pz, g.x, g.z), lim = (maxR + g.r * 0.4f);
      if (d2 < lim * lim && d2 < bd) { bd = d2; best = &g; }
    }
  }
  return best;
}

bool harvestNode(GNode* g, bool swingAnim) {
  if (!g || !g->left) return false;
  g->left--; if (!g->left) g->regen = 75.f;
  if (swingAnim && P.atkT < 0) { P.atkT = 0; P.hitDone = true; P.atkWind = 0.22f; P.atkRec = 0.26f; }
  char msg[20]; float gy = height(g->x, g->z) + 1.6f;
  if (g->gtype == 0) { int v = 7 + (int)(rng() * 4.f); K.wood += v; snprintf(msg, sizeof msg, "+%d WOOD", v); pushFloatText(g->x, gy, g->z, msg, 0xd8b478); audio::play(S_HITWOOD, 0, 0.75f); }
  else if (g->gtype == 1) { int v = 7 + (int)(rng() * 4.f); K.stone += v; snprintf(msg, sizeof msg, "+%d STONE", v); pushFloatText(g->x, gy, g->z, msg, 0xc8d2dc); audio::play(S_HITMETAL, 0, 0.7f); }
  else if (g->gtype == 2) { int s = 9 + (int)(rng() * 5.f), gd = 12 + (int)(rng() * 8.f); K.stone += s; gold += gd; snprintf(msg, sizeof msg, "+%d ORE +%dG", s, gd); pushFloatText(g->x, gy, g->z, msg, 0xffd85a); audio::play(S_HITMETAL, 0, 0.85f); }
  else { int v = 5 + (int)(rng() * 4.f); K.food += v; snprintf(msg, sizeof msg, "+%d FOOD", v); pushFloatText(g->x, gy, g->z, msg, 0x98e070); audio::play(S_CLICK, 0, 0.7f); }
  return true;
}

Npc* recruitableNpc(float maxR) {
  Npc* best = nullptr; float bd = maxR * maxR;
  for (int i = 0; i < MAXN; i++) {
    Npc& n = npcs[i]; if (!n.used || !n.alive || n.role >= R_MON || n.onWall || n.interior != CI.in || KEX[i].sworn || n.angry > 0) continue;
    float d2 = dist2(n.x, n.z, P.x, P.z); if (d2 < bd) { bd = d2; best = &n; }
  }
  return best;
}

int nearestBarracks(float maxR) {
  int best = -1; float bd = maxR * maxR;
  for (int i = 0; i < MAXB; i++) if (builds[i].used && builds[i].btype == BT_BARRACKS) {
    float d2 = dist2(builds[i].x, builds[i].z, P.x, P.z); if (d2 < bd) { bd = d2; best = i; }
  }
  return best;
}

void makeSworn(Npc& n) {
  int idx = (int)(&n - npcs); KEX[idx].sworn = true;
  n.road = -1; n.post = -1; n.onWall = false;
  if (n.role == R_VILL) {
    n.role = R_SOLDIER; n.rig = poolSold[(int)(rng() * nPoolSold) % std::max(1, nPoolSold)];
    n.hp = n.maxHp = 210.f; n.dmg = 22.f; n.atkRange = 2.4f;
  } else {
    n.maxHp *= 1.35f; n.hp = n.maxHp; n.dmg *= 1.25f;
  }
  n.runSpd = std::max(n.runSpd, 6.2f); n.spd = std::max(n.spd, 2.8f); n.aggro = 42.f;
  n.state = S_IDLE;
}

bool tryEnlistOrGather() {
  if (CI.in || P.deadT > 0) return false;
  // 1) Near player-built Barracks -> train fresh Kingdom Soldier (12 Gold, 4 Food)
  int bi = nearestBarracks(5.8f);
  if (bi >= 0) {
    if (gold < 12 || K.food < 4) { banner("NEED SUPPLIES", "TRAINING A SOLDIER COSTS 12 GOLD & 4 FOOD", 2.5f); return true; }
    float a = rng() * TAU, sx = builds[bi].x + std::cos(a) * 4.8f, sz = builds[bi].z + std::sin(a) * 4.8f;
    Npc* s = spawnHuman(rng() < 0.3f ? R_KNIGHT : R_SOLDIER, sx, sz, P.yaw, -1);
    if (s) { gold -= 12; K.food -= 4; makeSworn(*s); pushFloatText(s->x, s->y + 2.2f, s->z, "SWORN SOLDIER", 0x7ad4ff); banner("SOLDIER TRAINED", "SWORN TO YOUR WARBAND - PRESS X FOR ORDERS", 2.8f); audio::play(S_LEVELUP, 0, 0.65f); }
    return true;
  }
  // 2) Near friendly Villager/Soldier/Guard/Knight -> enlist into Warband (15 Gold, 5 Food)
  Npc* ally = recruitableNpc(4.0f);
  if (ally) {
    int gCost = ally->role == R_KNIGHT ? 25 : 15, fCost = ally->role == R_KNIGHT ? 7 : 5;
    if (gold < gCost || K.food < fCost) {
      char sub[44]; snprintf(sub, sizeof sub, "ENLISTING %s COSTS %d GOLD & %d FOOD", ally->name, gCost, fCost);
      banner("NOT ENOUGH GOLD / FOOD", sub, 2.6f); return true;
    }
    gold -= gCost; K.food -= fCost; makeSworn(*ally);
    char sub[44]; snprintf(sub, sizeof sub, "%s JOINED YOUR WARBAND", ally->name);
    banner("TROOP ENLISTED", sub, 2.8f); pushFloatText(ally->x, ally->y + 2.1f, ally->z, "ENLISTED!", 0x7ad4ff); audio::play(S_LEVELUP, 0, 0.65f);
    return true;
  }
  // 3) Otherwise gather nearest wood/stone/ore/food node!
  if (K.gatherCd <= 0.f) {
    GNode* g = nearestGNode(P.x, P.z, 3.8f);
    if (harvestNode(g, true)) { K.gatherCd = 0.42f; return true; }
  }
  return false;
}

int nearestRift(float maxR, bool onlyCorrupt) {
  if (CI.in) return -1;
  int best = -1; float bd = maxR * maxR;
  for (int i = 0; i < MAXRIFT; i++) {
    if (!rifts[i].used || (onlyCorrupt && rifts[i].pure)) continue;
    float d2 = dist2(rifts[i].x, rifts[i].z, P.x, P.z); if (d2 < bd) { bd = d2; best = i; }
  }
  return best;
}

bool tryPurifyRift(int ri) {
  if (ri < 0 || ri >= MAXRIFT || !rifts[ri].used || rifts[ri].pure) return false;
  RiftObj& rf = rifts[ri];
  int guardCnt = 0;
  for (int k = 0; k < nMonL; k++) { const Npc& m = npcs[monList[k]]; if (m.alive && m.role == R_MON && dist2(m.x, m.z, rf.x, rf.z) < 26.f * 26.f) guardCnt++; }
  if (guardCnt > 1) {
    char sub[44]; snprintf(sub, sizeof sub, "SLAY %d GUARDIANS AROUND THE RIFT FIRST", guardCnt);
    banner("RIFT WARDED BY DEMONS", sub, 2.8f); return true;
  }
  rf.pure = true; K.reclaimed++;
  gold += 80; K.wood += 25; K.stone += 25; K.food += 12; P.hp = P.maxHp; K.mana = K.maxMana;
  Npc* k = spawnHuman(R_KNIGHT, rf.x + 2.4f, rf.z + 2.0f, P.yaw, -1);
  if (k) makeSworn(*k);
  char sub[44]; snprintf(sub, sizeof sub, "SANCTUM %d/%d PURIFIED  +80G +25W +25S +KNIGHT", K.reclaimed, std::max(1, K.totalRifts));
  banner("LOST REALM RECLAIMED", sub, 4.8f);
  pushFloatText(rf.x, rf.y + 4.5f, rf.z, "PURIFIED!", 0x7ae8ff);
  audio::play(S_LEVELUP, 0, 0.95f);
  return true;
}

void computeGhost() {
  const BDef& d = BDEFS[K.bsel];
  float dist = K.bsel == BT_BARRACKS ? 9.5f : K.bsel == BT_GATE ? 8.5f : 7.0f;
  float gx = P.x + std::sin(P.yaw) * dist, gz = P.z + std::cos(P.yaw) * dist;
  float gyaw = P.yaw + K.bYaw;
  // Auto-snap wall/gate segments end-to-end onto existing nearby walls!
  if (K.bsel == BT_WALL || K.bsel == BT_GATE) {
    float snapSpan = K.bsel == BT_GATE ? 7.6f : 5.05f;
    for (int i = 0; i < MAXB; i++) {
      const BuildObj& b = builds[i]; if (!b.used || (b.btype != BT_WALL && b.btype != BT_GATE)) continue;
      float bSpan = b.btype == BT_GATE ? 5.1f : 2.55f;
      float cs = std::cos(b.ry), sn = std::sin(b.ry);
      for (int sd = -1; sd <= 1; sd += 2) {
        float ex = b.x + cs * sd * (bSpan + (snapSpan - bSpan) * 0.5f + (K.bsel == BT_WALL ? 2.5f : 3.8f) - (snapSpan - bSpan) * 0.5f);
        float ez = b.z - sn * sd * (bSpan + (K.bsel == BT_WALL ? 2.5f : 3.8f));
        if (dist2(gx, gz, ex, ez) < 2.7f * 2.7f) { gx = ex; gz = ez; gyaw = b.ry + K.bYaw; break; }
      }
    }
  }
  float gy = height(gx, gz);
  K.ghostPos = {gx, gy, gz}; K.ghostYaw = gyaw;
  bool afford = K.wood >= d.wood && K.stone >= d.stone && gold >= d.goldC;
  bool terrainOk = !CI.in && std::fabs(gx) < HALF - 20.f && std::fabs(gz) < HALF - 20.f && !inWater(gx, gz) && waterAt(gx, gz) < -1e8f && slopeAt(gx, gz) < 0.55f;
  bool clearOk = dist2(gx, gz, P.x, P.z) > (d.r * 0.7f) * (d.r * 0.7f);
  for (int i = 0; i < MAXB && clearOk; i++) if (builds[i].used) {
    float minD = (d.r * d.sc + BDEFS[builds[i].btype].r * BDEFS[builds[i].btype].sc) * 0.62f;
    if ((K.bsel == BT_WALL || K.bsel == BT_GATE) && (builds[i].btype == BT_WALL || builds[i].btype == BT_GATE)) minD = 2.1f;
    if (dist2(gx, gz, builds[i].x, builds[i].z) < minD * minD) clearOk = false;
  }
  K.ghostValid = afford && terrainOk && clearOk;
}

int placeBuilding(int btype, float x, float z, float ry, bool freeCost) {
  const BDef& d = BDEFS[btype];
  if (!freeCost && (K.wood < d.wood || K.stone < d.stone || gold < d.goldC)) return -1;
  int slot = -1; for (int i = 0; i < MAXB; i++) if (!builds[i].used) { slot = i; break; }
  if (slot < 0) return -1;
  if (!freeCost) { K.wood -= d.wood; K.stone -= d.stone; gold -= d.goldC; }
  BuildObj& b = builds[slot]; b = BuildObj();
  b.used = true; b.btype = btype; b.kind = d.kind; b.x = x; b.y = height(x, z); b.z = z; b.ry = ry;
  b.hp = b.maxHp = d.hp; b.grow = freeCost ? 1.f : 0.15f; b.cd = 0.3f;
  if (btype == BT_BARRACKS) {
    for (int k = 0; k < 2; k++) {
      float sx = x + std::sin(ry) * 5.2f + std::cos(ry) * (k == 0 ? -1.4f : 1.4f);
      float sz = z + std::cos(ry) * 5.2f - std::sin(ry) * (k == 0 ? -1.4f : 1.4f);
      Npc* s = spawnHuman(k == 0 ? R_KNIGHT : R_SOLDIER, sx, sz, ry, -1);
      if (s) makeSworn(*s);
    }
  }
  if (!freeCost) {
    char sub[44]; snprintf(sub, sizeof sub, "%s BUILT%s", d.name, btype == BT_BARRACKS ? "  (+2 SWORN TROOPS)" : "");
    banner("FORTIFICATION ERECTED", sub, 2.4f); audio::play(S_HITWOOD, 0, 0.8f);
  }
  return slot;
}

void repairNearestBuild() {
  float gx = P.x + std::sin(P.yaw) * 5.5f, gz = P.z + std::cos(P.yaw) * 5.5f;
  int best = -1; float bd = 5.5f * 5.5f;
  for (int i = 0; i < MAXB; i++) if (builds[i].used && builds[i].hp < builds[i].maxHp) {
    float d2 = dist2(builds[i].x, builds[i].z, gx, gz); if (d2 < bd) { bd = d2; best = i; }
  }
  if (best < 0) return;
  if (K.wood < 4 || K.stone < 4) { banner("NEED 4 WOOD & 4 STONE", "TO REPAIR FORTIFICATION", 2.f); return; }
  K.wood -= 4; K.stone -= 4; BuildObj& b = builds[best]; b.hp = std::min(b.maxHp, b.hp + 180.f);
  pushFloatText(b.x, b.y + 3.0f, b.z, "+180 REPAIR", 0x98e878); audio::play(S_HITWOOD, 0, 0.65f);
}

void damageBuild(int bi, float dmg) {
  if (bi < 0 || bi >= MAXB || !builds[bi].used) return;
  BuildObj& b = builds[bi]; b.hp -= dmg; b.hitT = 0.35f;
  if (dist2(b.x, b.z, P.x, P.z) < 45.f * 45.f) audio::play(S_HITWOOD, 0, 0.45f);
  if (b.hp <= 0.f) {
    b.used = false;
    char sub[44]; snprintf(sub, sizeof sub, "%s DESTROYED BY THE HORDE!", BDEFS[b.btype].name);
    banner("BREACH!", sub, 3.2f); audio::play(S_DIEBIG, 0, 0.7f);
  }
}

void castSpell() {
  if (P.deadT > 0 || K.spCd > 0.f) return;
  int sp = K.spell; float cost = SP_COST[sp];
  if (K.mana < cost) { banner("LOW MANA", "WAIT FOR ARCANE MANA TO REGENERATE", 1.8f); return; }
  K.mana -= cost; K.spCd = SP_CD[sp];
  const roster::Hero& hr = roster::at(roster::sel());
  float cp = std::cos(P.pitch), spn = std::sin(P.pitch);
  V3 look{std::sin(P.yaw) * cp, spn, std::cos(P.yaw) * cp};
  V3 org = P.view == 0 ? P.eye + look * 0.7f + V3{0, -0.12f, 0} : V3{P.x + std::sin(P.yaw) * 0.7f, P.y + (K.mounted ? 2.4f : 1.45f), P.z + std::cos(P.yaw) * 0.7f};
  if (P.atkT < 0) { P.atkT = 0; P.hitDone = true; P.atkWind = 0.22f; P.atkRec = 0.28f; }

  if (sp == SP_FIREBALL || sp == SP_LANCE) {
    // Converge toward camera aim point
    float t0 = P.view == 0 ? 0.8f : P.camDist + 0.6f, bestT = 85.f;
    for (float t = t0; t < 85.f; t += 1.5f) { V3 q = P.eye + look * t; if (q.y < gH(q.x, q.z)) { bestT = t; break; } }
    V3 dir = norm((P.eye + look * bestT) - org);
    for (SpellProj& s : sproj) if (!s.on) {
      s.on = true; s.sp = sp; s.p = org; s.dir = dir;
      s.v = dir * (sp == SP_LANCE ? 62.f : 34.f);
      s.life = sp == SP_LANCE ? 1.8f : 3.2f;
      s.dmg = (sp == SP_LANCE ? 48.f : 56.f) * hr.dmg * (K.aegisT > 0 ? 1.25f : 1.f);
      s.pierces = sp == SP_LANCE ? 3 : 0; s.lastHit = -1;
      break;
    }
    audio::play(sp == SP_LANCE ? S_BOWHEAVY : S_SWING, 0, 0.85f);
  } else if (sp == SP_FROST) {
    // Ring of 10 frost shards + immediate freeze shockwave around player!
    float baseDmg = 42.f * hr.dmg * (K.aegisT > 0 ? 1.25f : 1.f);
    for (int k = 0; k < 10; k++) {
      float a = k * TAU / 10.f; V3 dir{std::sin(a), 0.04f, std::cos(a)};
      for (SpellProj& s : sproj) if (!s.on) {
        s.on = true; s.sp = SP_FROST; s.p = V3{P.x, P.y + 1.0f, P.z} + dir * 0.9f; s.dir = norm(dir);
        s.v = s.dir * 20.f; s.life = 0.55f; s.dmg = baseDmg * 0.5f; s.pierces = 2; s.lastHit = -1; break;
      }
    }
    for (int i = 0; i < MAXN; i++) {
      Npc& n = npcs[i]; if (!n.used || !n.alive) continue;
      bool foe = n.role >= R_MON || n.angry > 0;
      if (!foe) continue;
      if (dist2(n.x, n.z, P.x, P.z) < 8.2f * 8.2f) {
        hurtNpc(n, baseDmg, P.x, P.z, true, Z_TORSO);
        n.stun = std::max(n.stun, n.boss ? 0.9f : 2.4f); n.limp = 6.f;
      }
    }
    pushFloatText(P.x, P.y + 2.3f, P.z, "FROST NOVA!", 0x8ae8ff);
    audio::play(S_DIE, 0, 0.8f);
  } else if (sp == SP_RALLY) {
    // Solar Aegis: heal player, heal sworn/allied NPCs, buff warband, mend nearby fortifications!
    P.hp = std::min(P.maxHp, P.hp + 45.f); K.aegisT = 10.f;
    for (int i = 0; i < MAXN; i++) {
      Npc& n = npcs[i]; if (!n.used || !n.alive || n.role >= R_MON) continue;
      if (dist2(n.x, n.z, P.x, P.z) < 20.f * 20.f) { n.hp = std::min(n.maxHp, n.hp + 70.f); KEX[i].boostT = 10.f; }
    }
    for (int i = 0; i < MAXB; i++) if (builds[i].used && dist2(builds[i].x, builds[i].z, P.x, P.z) < 22.f * 22.f) {
      builds[i].hp = std::min(builds[i].maxHp, builds[i].hp + 150.f);
    }
    pushFloatText(P.x, P.y + 2.4f, P.z, "SOLAR AEGIS!", 0xffe070);
    banner("SOLAR AEGIS", "ALLIES HEALED  +25% MIGHT  WALLS MENDED", 2.6f);
    audio::play(S_LEVELUP, 0, 0.8f);
  }
}

void startKingdomSiege(bool force) {
  int si = settleNear(P.x, P.z, force ? 600.f : 220.f);
  float tx = P.x, tz = P.z; const char* tname = "YOUR STRONGHOLD";
  // Prefer player's built stronghold center if >= 2 structures built nearby, else nearest settlement
  float bx = 0, bz = 0; int bc = 0;
  for (int i = 0; i < MAXB; i++) if (builds[i].used && dist2(builds[i].x, builds[i].z, P.x, P.z) < 180.f * 180.f) { bx += builds[i].x; bz += builds[i].z; bc++; }
  if (bc >= 2) { tx = bx / bc; tz = bz / bc; }
  else if (si >= 0 && settles[si].type <= ST_FORT) { tx = settles[si].x; tz = settles[si].z; tname = settles[si].name; }

  K.nightWave++;
  int want = 10 + K.nightWave * 3 + std::min(8, bc * 2);
  static const int WT[7] = {THORNLING, PEATWIGHT, GLOAMHOUND, RUSTBOUND, EMBERHOUND, GLOAMWISP, CAIRNBACK};
  int made = 0;
  float baseAng = rng() * TAU;
  for (int t = 0; t < 240 && made < want; t++) {
    float a = baseAng + (rng() - 0.5f) * 2.2f, d = 58.f + rng() * 34.f;
    float x = tx + std::cos(a) * d, z = tz + std::sin(a) * d;
    if (!validSpot(x, z, 0.85f)) continue;
    bool siegeBoss = (made == 0 && K.nightWave >= 1);
    int type = siegeBoss ? (K.nightWave >= 2 ? BARROWKING : CAIRNBACK) : WT[(int)(rng() * 7.f) % 7];
    Npc* n = spawnMon(type, x, z, false, siegeBoss);
    if (!n) break;
    int idx = (int)(n - npcs); KEX[idx].siegeUnit = true; KEX[idx].tgtBuild = -1;
    n->homeX = tx + (rng() - 0.5f) * 8.f; n->homeZ = tz + (rng() - 0.5f) * 8.f;
    n->wander = 10.f; n->aggro = 55.f; n->state = S_RETURN; made++;
  }
  if (made > 0) {
    K.siegeActive = true; K.siegeTotal = made; K.siegeLeft = made;
    char head[40], sub[44];
    snprintf(head, sizeof head, "NIGHT %d DEMON SIEGE", K.nightWave);
    snprintf(sub, sizeof sub, "%d DEMONS MARCH ON %s!", made, tname);
    banner(head, sub, 5.2f); audio::play(S_DIEBIG, 0, 0.75f);
  }
}

bool aiSwornHuman(Npc& n, int idx, float dt, float pd) {
  if (!KEX[idx].sworn) return false;
  if (KEX[idx].boostT > 0.f) KEX[idx].boostT -= dt;
  float searchR = K.order == ORD_CHARGE ? 58.f : K.order == ORD_HOLD ? 22.f : 32.f;
  Npc* foe = nearestMonster(n, searchR);
  if (foe) {
    float d = std::sqrt(dist2(n.x, n.z, foe->x, foe->z)), reach = n.atkRange + foe->sc * 0.65f;
    if (d > reach) {
      if (n.atkT < 0) {
        float sp = n.runSpd * (K.order == ORD_CHARGE ? 1.15f : 1.f);
        steer(n, foe->x, foe->z, sp, dt);
        collideBuilds(n.x, n.z, 0.4f * n.sc, true);
      }
      n.state = S_CHASE;
    } else {
      n.moving = 0; faceToward(n, foe->x, foe->z, dt);
      if (n.atkT < 0 && n.cd <= 0) { n.atkT = 0; n.hitDone = false; }
      n.state = S_ATK;
    }
    float origDmg = n.dmg;
    if (KEX[idx].boostT > 0.f || K.aegisT > 0.f) n.dmg *= 1.3f;
    tickAttack(n, dt, false, foe);
    n.dmg = origDmg;
    return true;
  }
  // Formation movement by active Warband Order
  int rank = 0; for (int i = 0; i < idx; i++) if (npcs[i].used && npcs[i].alive && KEX[i].sworn) rank++;
  float gx = P.x, gz = P.z, gyaw = P.yaw;
  if (K.order == ORD_FOLLOW || K.order == ORD_CHARGE) {
    float row = 1.f + (rank / 3), col = (rank % 3) - 1.f;
    if (rank % 3 == 0 && rank > 0) col = 0.f;
    float back = 2.6f + row * 1.8f, side = col * 2.1f;
    gx = P.x - std::sin(P.yaw) * back - std::cos(P.yaw) * side;
    gz = P.z - std::cos(P.yaw) * back + std::sin(P.yaw) * side;
  } else if (K.order == ORD_HOLD) {
    float col = (rank - K.troops * 0.5f + 0.5f) * 2.0f;
    gx = K.holdX - std::cos(K.holdYaw) * col;
    gz = K.holdZ + std::sin(K.holdYaw) * col;
    gyaw = K.holdYaw;
  } else if (K.order == ORD_GARRISON) {
    int bi = -1; float bd = 1e30f;
    for (int i = 0; i < MAXB; i++) if (builds[i].used) { float d2 = dist2(builds[i].x, builds[i].z, n.x, n.z); if (d2 < bd) { bd = d2; bi = i; } }
    if (bi >= 0) { float a = rank * 1.7f + tAbs * 0.25f; gx = builds[bi].x + std::cos(a) * 4.5f; gz = builds[bi].z + std::sin(a) * 4.5f; }
  }
  float d = std::sqrt(dist2(n.x, n.z, gx, gz));
  if (d > 48.f && K.order == ORD_FOLLOW && !CI.in) { n.x = gx; n.z = gz; n.y = groundAt(gx, gz); n.moving = 0; }
  else if (d > 1.35f) {
    float wantSpd = d > 6.f ? std::max(n.runSpd, P.speed * 1.05f) : n.spd * 1.4f;
    steer(n, gx, gz, wantSpd, dt);
    collideBuilds(n.x, n.z, 0.4f * n.sc, true);
    n.state = S_WALK;
  } else {
    n.moving = 0; n.state = S_IDLE;
    float da = angDiff(n.face, gyaw); n.face += clampf(da, -dt * 3.5f, dt * 3.5f);
  }
  (void)pd;
  return true;
}

bool aiSiegeMonster(Npc& n, int idx, float dt) {
  int hitB = -1;
  bool blocked = collideBuilds(n.x, n.z, 0.45f * n.sc, false, &hitB);
  if (hitB >= 0) KEX[idx].tgtBuild = hitB;
  int tb = KEX[idx].tgtBuild;
  if (tb < 0 || !builds[tb].used) {
    KEX[idx].tgtBuild = -1;
    // Siege units look for fortifications blocking or near them
    if (KEX[idx].siegeUnit || blocked) {
      float bd = 12.f * 12.f;
      for (int i = 0; i < MAXB; i++) if (builds[i].used) {
        float d2 = dist2(n.x, n.z, builds[i].x, builds[i].z); if (d2 < bd) { bd = d2; tb = i; }
      }
      KEX[idx].tgtBuild = tb;
    }
  }
  if (tb >= 0 && builds[tb].used) {
    const BuildObj& b = builds[tb]; const BDef& bd = BDEFS[b.btype];
    float d = std::sqrt(dist2(n.x, n.z, b.x, b.z)), reach = bd.r * bd.sc + n.atkRange + 0.9f;
    if (d > reach + 6.f) { KEX[idx].tgtBuild = -1; return false; }
    if (d > reach) { if (n.atkT < 0) steer(n, b.x, b.z, n.runSpd, dt); collideBuilds(n.x, n.z, 0.45f * n.sc, false); n.state = S_CHASE; }
    else {
      n.moving = 0; faceToward(n, b.x, b.z, dt);
      if (n.atkT < 0 && n.cd <= 0) { n.atkT = 0; n.hitDone = false; }
      n.state = S_ATK;
    }
    if (n.cd > 0) n.cd -= dt;
    if (n.atkT >= 0) {
      n.atkT += dt;
      if (!n.hitDone && n.atkT >= n.atkWind) {
        n.hitDone = true;
        float mult = (n.mon == CAIRNBACK || n.mon == BARROWKING || n.mon == MOSSJAW) ? 2.2f : 1.0f;
        damageBuild(tb, n.dmg * mult);
      }
      if (n.atkT >= n.atkWind + n.atkRec) { n.atkT = -1; n.cd = MD[n.mon].cd; }
    }
    return true;
  }
  // Keep marching toward stronghold if siege unit has no immediate target yet
  if (KEX[idx].siegeUnit && n.tgtKind == 0 && dist2(n.x, n.z, n.homeX, n.homeZ) > 10.f * 10.f) {
    steer(n, n.homeX, n.homeZ, n.runSpd * 0.88f, dt);
    collideBuilds(n.x, n.z, 0.45f * n.sc, false, &hitB);
    if (hitB >= 0) KEX[idx].tgtBuild = hitB;
    n.state = S_CHASE;
    return true;
  }
  return false;
}

void updateKingdom(float dt, const pl::Input& in) {
  static bool prevLmb = false, prevRmb = false;
  bool lmbEdge = in.lmb && !prevLmb, rmbEdge = in.rmb && !prevRmb;
  prevLmb = in.lmb; prevRmb = in.rmb;

  K.mana = std::min(K.maxMana, K.mana + dt * 7.5f);
  K.spCd = std::max(0.f, K.spCd - dt);
  K.aegisT = std::max(0.f, K.aegisT - dt);
  K.gatherCd = std::max(0.f, K.gatherCd - dt);
  if (CI.in && K.mounted) K.mounted = false;
  if (CI.in && K.buildMode) K.buildMode = false;

  // Regrow depleted resource nodes
  for (GNode& g : gnodes) if (!g.left && g.regen > 0.f) { g.regen -= dt; if (g.regen <= 0.f) g.left = 3; }

  // Mount trample & stamina perk
  if (K.mounted && P.speed > 7.5f && !CI.in) {
    P.st = std::min(100.f, P.st + 18.f * dt);
    for (int k = 0; k < nMonL; k++) {
      Npc& m = npcs[monList[k]]; if (!m.alive || m.role != R_MON || m.boss || m.hitT > 0.05f) continue;
      if (dist2(m.x, m.z, P.x, P.z) < 1.9f * 1.9f) hurtNpc(m, 14.f, P.x, P.z, true, Z_TORSO);
    }
  }

  // Build mode placement & repair
  if (K.buildMode) {
    computeGhost();
    if (lmbEdge && !H.map) {
      if (K.ghostValid) placeBuilding(K.bsel, K.ghostPos.x, K.ghostPos.z, K.ghostYaw, false);
      else banner("CANNOT BUILD HERE", "CHECK RESOURCES OR CLEAR / FLATTEN GROUND", 1.8f);
    }
    if (rmbEdge && !H.map) repairNearestBuild();
  }

  // Tick player-built fortifications (Tower Defense ballistae, Sunfire Beacons, Farmsteads)
  int bCount = 0;
  for (int i = 0; i < MAXB; i++) {
    BuildObj& b = builds[i]; if (!b.used) continue;
    bCount++;
    if (b.grow < 1.f) b.grow = std::min(1.f, b.grow + dt * 3.2f);
    if (b.hitT > 0.f) b.hitT = std::max(0.f, b.hitT - dt);
    if (b.cd > 0.f) b.cd -= dt;

    if (b.btype == BT_TOWER && b.cd <= 0.f) {
      const BDef& d = BDEFS[BT_TOWER];
      Npc* best = nullptr; float bd = d.shootR * d.shootR;
      for (int k = 0; k < nMonL; k++) {
        Npc& m = npcs[monList[k]]; if (!m.alive || m.role != R_MON) continue;
        float d2 = dist2(b.x, b.z, m.x, m.z); if (d2 < bd) { bd = d2; best = &m; }
      }
      if (best) {
        b.cd = d.shootCd;
        V3 org{b.x, b.y + 6.4f * d.sc, b.z};
        float dist = std::sqrt(bd), spd = 48.f, tLead = dist / spd;
        V3 tgt{best->x + std::sin(best->face) * best->moving * tLead * 0.6f, best->y + 1.1f * best->sc + 0.5f * 12.f * tLead * tLead, best->z + std::cos(best->face) * best->moving * tLead * 0.6f};
        V3 dir = norm(tgt - org);
        for (Arr& a : ARR) if (!a.on) {
          a.on = true; a.stuck = false; a.p = org; a.v = dir * spd; a.dir = dir; a.life = 4.5f; a.dmg = 34.f; a.npc = -1; break;
        }
        if (dist2(b.x, b.z, P.x, P.z) < 60.f * 60.f) audio::play(S_BOW, 0, 0.45f);
      }
    } else if (b.btype == BT_BEACON && b.cd <= 0.f) {
      b.cd = 0.9f;
      for (int k = 0; k < nMonL; k++) {
        Npc& m = npcs[monList[k]]; if (!m.alive || m.role != R_MON) continue;
        if (dist2(b.x, b.z, m.x, m.z) < 13.5f * 13.5f) hurtNpc(m, 10.f, b.x, b.z, false, Z_TORSO);
      }
    } else if (b.btype == BT_FARM) {
      b.tick += dt;
      if (b.tick >= 38.f) {
        b.tick = 0.f; K.food += 5; gold += 7;
        if (dist2(b.x, b.z, P.x, P.z) < 55.f * 55.f) pushFloatText(b.x, b.y + 3.2f, b.z, "+5 FOOD +7G", 0x98e070);
      }
      if (dist2(b.x, b.z, P.x, P.z) < 16.f * 16.f) P.hp = std::min(P.maxHp, P.hp + dt * 2.5f);
    }
  }
  K.builtCount = bCount;

  // Tick Spell Projectiles
  for (SpellProj& s : sproj) {
    if (!s.on) continue;
    s.life -= dt; if (s.life <= 0.f) { s.on = false; continue; }
    V3 p0 = s.p;
    if (s.sp == SP_FIREBALL) s.v.y -= 3.5f * dt;
    V3 p1 = p0 + s.v * dt;
    if (len(s.v) > 0.1f) s.dir = norm(s.v);

    int hitI = -1, hitZ = Z_TORSO; float bestS = 2.f;
    for (int i = 0; i < MAXN; i++) {
      Npc& n = npcs[i]; if (!n.used || !n.alive || i == s.lastHit) continue;
      if (KEX[i].sworn) continue;
      if (std::fabs(n.x - p1.x) > 6.f || std::fabs(n.z - p1.z) > 6.f) continue;
      float sp; int z;
      if (hitNpc(n, p0, p1, s.sp == SP_FIREBALL ? 0.32f : 0.18f, sp, z) && sp < bestS) { bestS = sp; hitI = i; hitZ = z; }
    }
    float g = gH(p1.x, p1.z);
    bool hitGround = p1.y <= g + 0.12f;
    if (hitI >= 0 || hitGround) {
      V3 hp = hitI >= 0 ? p0 + (p1 - p0) * bestS : V3{p1.x, g, p1.z};
      if (s.sp == SP_FIREBALL) {
        // Fiery AoE detonation!
        for (int i = 0; i < MAXN; i++) {
          Npc& n = npcs[i]; if (!n.used || !n.alive || KEX[i].sworn) continue;
          if (n.role < R_MON && i != hitI && n.angry <= 0) continue;
          float d2 = dist2(n.x, n.z, hp.x, hp.z);
          if (d2 < 5.2f * 5.2f) {
            float fall = 1.f - 0.5f * std::sqrt(d2) / 5.2f;
            hurtNpc(n, s.dmg * fall, hp.x, hp.z, true, i == hitI ? hitZ : Z_TORSO);
          }
        }
        audio::play(S_DIEBIG, 0, 0.55f);
        s.on = false; continue;
      } else {
        if (hitI >= 0) {
          Npc& n = npcs[hitI];
          hurtNpc(n, s.dmg, p0.x, p0.z, true, hitZ);
          if (s.sp == SP_FROST) { n.stun = std::max(n.stun, 1.4f); n.limp = 5.f; }
          s.lastHit = hitI;
          if (s.pierces-- <= 0) { s.on = false; continue; }
        }
        if (hitGround) { s.on = false; continue; }
      }
    }
    s.p = p1;
  }

  // Count Sworn Warband & Siege Demons; award Dawn Siege Victory!
  int sw = 0, sLeft = 0;
  for (int i = 0; i < MAXN; i++) if (npcs[i].used && npcs[i].alive) {
    if (KEX[i].sworn) sw++;
    if (KEX[i].siegeUnit && npcs[i].role == R_MON) sLeft++;
  }
  K.troops = sw;
  if (K.siegeActive) {
    K.siegeLeft = sLeft;
    bool dawn = clockH >= 6.0f && clockH < 19.0f;
    if (sLeft == 0 || dawn) {
      K.siegeActive = false; K.siegeLeft = 0;
      int rwG = 45 + K.nightWave * 20 + K.builtCount * 5, rwW = 18 + K.builtCount * 3, rwS = 18 + K.builtCount * 3;
      gold += rwG; K.wood += rwW; K.stone += rwS; K.food += 10;
      for (int i = 0; i < MAXB; i++) if (builds[i].used) builds[i].hp = std::min(builds[i].maxHp, builds[i].hp + builds[i].maxHp * 0.3f);
      char sub[48]; snprintf(sub, sizeof sub, "+%d GOLD  +%d WOOD  +%d STONE", rwG, rwW, rwS);
      banner("SIEGE REPELLED!", sub, 5.f); audio::play(S_LEVELUP, 0, 0.9f);
    }
  }

  // Export Kingdom state to HUD struct
  H.wood = K.wood; H.stone = K.stone; H.food = K.food;
  H.mana = K.mana; H.maxMana = K.maxMana; H.spell = K.spell;
  H.buildMode = K.buildMode; H.bsel = K.bsel; H.ghostOk = K.ghostValid;
  H.troops = K.troops; H.order = K.order; H.mounted = K.mounted;
  H.siegeLeft = K.siegeActive ? K.siegeLeft : 0; H.nightWave = K.nightWave;
  H.reclaimed = K.reclaimed; H.totalRifts = K.totalRifts;
}

int appendKingdomDyn(xr::Dyn* out, int k, int cap, xr::PtLight* dl, int& ndl, int dlCap) {
  if (CI.in) return k;
  // 1) Player-built structures
  for (int i = 0; i < MAXB && k < cap; i++) {
    const BuildObj& b = builds[i]; if (!b.used) continue;
    const BDef& d = BDEFS[b.btype];
    float sy = d.sc * b.grow;
    M4 m = mul(translate(b.x, b.y - (1.f - b.grow) * 2.5f, b.z), mul(rotY(b.ry), scale4(d.sc, sy, d.sc)));
    xr::Dyn& o = out[k++]; o.kind = b.kind; o.m = m; o.mode = 0.f;
    float fl = b.hitT > 0.f ? b.hitT * 2.2f : 0.f;
    o.tint[0] = 1.f + fl * 1.4f; o.tint[1] = 1.f - fl * 0.35f; o.tint[2] = 1.f - fl * 0.45f;
    if (b.btype == BT_BEACON && ndl < dlCap) dl[ndl++] = {b.x, b.y + 3.8f, b.z, 1.0f, 0.78f, 0.38f, 22.f};
  }
  // 2) Build Mode Hologram Ghost
  if (K.buildMode && k < cap) {
    const BDef& d = BDEFS[K.bsel];
    M4 m = mul(translate(K.ghostPos.x, K.ghostPos.y + 0.03f, K.ghostPos.z), mul(rotY(K.ghostYaw), scale4(d.sc, d.sc, d.sc)));
    xr::Dyn& o = out[k++]; o.kind = d.kind; o.m = m; o.mode = -1.f;
    if (K.ghostValid) { o.tint[0] = 0.38f; o.tint[1] = 1.55f; o.tint[2] = 0.68f; }
    else { o.tint[0] = 1.65f; o.tint[1] = 0.32f; o.tint[2] = 0.28f; }
  }
  // 3) Corrupted & Purified Rifts
  for (int i = 0; i < MAXRIFT && k < cap; i++) {
    const RiftObj& rf = rifts[i]; if (!rf.used) continue;
    float d2 = dist2(rf.x, rf.z, P.eye.x, P.eye.z); if (d2 > 520.f * 520.f) continue;
    M4 m = mul(translate(rf.x, rf.y, rf.z), rotY(i * 1.1f));
    xr::Dyn& o = out[k++]; o.kind = rf.pure ? K_PURIFIED : K_RIFT; o.m = m; o.mode = 0.f;
    o.tint[0] = o.tint[1] = o.tint[2] = 1.f;
    if (d2 < 90.f * 90.f && ndl < dlCap) {
      if (rf.pure) dl[ndl++] = {rf.x, rf.y + 4.5f, rf.z, 0.45f, 0.88f, 1.0f, 20.f};
      else dl[ndl++] = {rf.x, rf.y + 4.5f, rf.z, 1.0f, 0.18f, 0.42f, 22.f};
    }
  }
  // 4) Spell Projectiles
  for (const SpellProj& s : sproj) {
    if (!s.on || k >= cap) continue;
    V3 f = s.dir; V3 xx = cross(V3{0, 1, 0}, f); if (len(xx) < 1e-3f) xx = V3{1, 0, 0}; xx = norm(xx); V3 yy = cross(f, xx);
    float sc = s.sp == SP_FIREBALL ? 1.35f : s.sp == SP_LANCE ? 1.6f : 1.1f;
    M4 m = ident();
    m.m[0] = xx.x * sc; m.m[1] = xx.y * sc; m.m[2] = xx.z * sc;
    m.m[4] = yy.x * sc; m.m[5] = yy.y * sc; m.m[6] = yy.z * sc;
    m.m[8] = f.x * sc; m.m[9] = f.y * sc; m.m[10] = f.z * sc;
    m.m[12] = s.p.x; m.m[13] = s.p.y; m.m[14] = s.p.z;
    xr::Dyn& o = out[k++];
    o.kind = s.sp == SP_FIREBALL ? K_SPELL_FIRE : K_SPELL_FROST; o.m = m; o.mode = 0.f;
    if (s.sp == SP_LANCE) { o.tint[0] = 1.3f; o.tint[1] = 1.35f; o.tint[2] = 0.55f; }
    else { o.tint[0] = o.tint[1] = o.tint[2] = 1.f; }
    if (s.sp == SP_FIREBALL && ndl < dlCap) dl[ndl++] = {s.p.x, s.p.y, s.p.z, 1.0f, 0.52f, 0.16f, 16.f};
  }
  if (K.aegisT > 8.5f && ndl < dlCap) dl[ndl++] = {P.x, P.y + 1.8f, P.z, 1.0f, 0.85f, 0.35f, 18.f};
  return k;
}
