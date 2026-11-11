// Fixed-step (60 Hz) simulation: port of 61-game.js. All state lives in fixed-size pools (no heap use per step).
#pragma once
#include "data.h"
#include "audio.h"
#include "fx.h"
#include <cstdio>
struct Unit;
enum ActKind { A_NONE, A_BASIC, A_BASH, A_HOLD, A_CLEAVE, A_SUNDER, A_AIMED, A_VOLLEY, A_PIN, A_FLARE, A_KINDLE, A_CINDER, A_TONIC, A_FIREPOT, A_QUICK, A_LANCE, A_HOAR, A_SHATTER, A_RIPOSTE, A_MONATK, A_MONCAST, A_SPECIAL, A_RELIGHT };
struct Act { bool on = false, hit = false; float t = 0, wind = 0, rec = 0; int kind = A_NONE, clip = 0; Unit* tgt = nullptr; float tx = 0, tz = 0; };
struct Unit {
  bool used = false; int team = 0, cls = 0, idx = 0; uint32_t id = 0;
  float x = 0, z = 0, ox = 0, oz = 0, face = 0, faceT = 0, hp = 1, maxHp = 1, radius = 0.4f, dmg = 1, atk = 1, range = 1, speed = 3, armor = 0, crit = 0.08f, leech = 0, cdr = 1, power = 1;
  float cd[3] = {0, 0, 0}, atkT = 0, stagT = 0, slowT = 0, slowK = 1, chillT = 0, markT = 0, tauntT = 0, hasteT = 0, guardT = 0, ripT = 0, regenT = 0, regenA = 0, repoT = 0, idleT = 0, specT = 0, deadT = 0, downT = 0, flashV = 0, stagV = 0, moving = 0, dashT = 0, dashX = 0, dashZ = 0, dps = 0, animT = 0, hitFrom = 0;
  bool down = false, boss = false, elite = false, enr = false, seen = false, eng = false, fleeing = false, dodgeOk = false, hitSpec = false, ready = false;
  int side = 1; Unit* tauntBy = nullptr; Unit* ripTarget = nullptr; Unit* target = nullptr; Act act; char title[32] = {0};
};
struct Item { bool has = false; int slot = 0, g = 0, pow = 0, sp = -1; char name[48] = {0}; };
struct Proj { bool on = false; int vis = 0, hk = 0, hero = -1, n = 0; float t = 0, dur = 0.1f, sx, sy, sz, ex, ey, ez, arc = 0, a = 0; Unit* tgt = nullptr; float x = 0, y = 0, z = 0; };  // vis: 0 arrow 1 ember 2 flask 3 lance 4 gloam 5 cinder
struct Zone { bool on = false; int kind = 0, sub = 0, hero = -1; float x = 0, z = 0, r = 1, t = 0, dur = 1, tick = 0.5f, tk = 0, k = 0; bool landed = false; };  // kind 0 fire 1 frost; sub: 0 flare 1 firepot 2 cinder 3 hoar
struct Tele { bool on = false; float x, z, r, t, dur; };
struct LootBit { bool on = false; int kind = 0; float x = 0, y = 0, z = 0, t = 0, vx = 0, vz = 0, delay = 0; int silver = 0; Item item; int hero = 0; };
struct Run {
  int party[4]; int level = 1; float xp = 0; float silver = 0; int stage = 1, maxStage = 0, flames = 3; float time = 0; int camp[NCAMP] = {0, 0, 0, 0};
  int cards[NCARDS] = {0}; Item gear[NCLS][3]; int kills = 0, lit = 0, tonics = 2, pack = 0, holdCount = 0, relics = 0, maxTonics = 2, d5 = 0; float silverMul = 1;
};
struct Profile { int embers = 0; int boons[NBOON] = {0}; bool unlocked[NCLS] = {true, true, true, true, false, false}; int bestStage = 0, bestLevel = 0, runs = 0; float bestTime = 0; bool steward = true, push = true; int speed = 1; float vol = 0.8f; bool mute = false; };
enum State { S_TITLE, S_WALK, S_FIGHT, S_BOSS, S_BREATHER, S_RELIGHT, S_CHOICE, S_WIPE, S_RUNEND };
enum Panel { P_NONE, P_CARDS, P_CHOICE, P_RUNEND, P_MENU };
struct LastRun { int stage = 0, level = 0; float time = 0; int embers = 0, kills = 0, lit = 0; };
constexpr int MAXMON = 64, MAXPROJ = 96, MAXZONE = 16, MAXLOOT = 64;
struct Game {
  Profile P; Run run; bool hasRun = false; Unit heroes[4]; Unit mons[MAXMON]; Proj projs[MAXPROJ]; Zone zones[MAXZONE]; Tele teles[8]; LootBit loot[MAXLOOT];
  float anchorX = 40; bool anchorMoving = false; int state = S_TITLE; float stT = 0, flare = 0, time = 0, nextPackX = 0, wayX = -1e9f, relT = 0, tonicCd = 0, wdT = 0, wdX = -1; bool relit = false, relitAct = false;
  bool hasWay = false; int panel = P_NONE; Unit* focus = nullptr; float focusT = 0; bool rally = false; float rallyX = 0, rallyZ = 0, rallyT = 0;
  int pending = 0; Unit* bossRef = nullptr; Unit* carrier = nullptr; float lightX = 0, lightZ = 0; LastRun last; int lastReach = -1; Rng rng{1}; uint32_t nextId = 0, seed = 7; bool dot = false;
  float flareV = 0, hitStop = 0, slowMo = 0, shakeA = 0, screenFlash = 0, stageT = 0, stMinHp = 1; int stDowns = 0; Unit* sndAt = nullptr; float sndT[16] = {0};
  // panel data
  int offer[3] = {0, 0, 0}, nOffer = 0, offerRec = 0; bool choiceRec = true; float panelT = 0, panelMax = 0; bool panelTimer = false;
  // toasts
  struct Toast { char s[64]; uint32_t c; float t; }; Toast toasts[6]; int nToast = 0;
  // hooks set by app
  void (*onWaylight)(float) = nullptr; void (*onRelight)() = nullptr; void (*onShift)(float) = nullptr; void (*onBossIntro)(Unit*) = nullptr; void (*onReach)(int, bool) = nullptr; void tickPanel(float dt); void stepLootProjs(float dt); void pickAndGo();
  float R() { return rng(); }
  int mod(int k) const { return run.cards[k]; }
  int boon(int k) const { return P.boons[k]; }
  void toast(const char* s, uint32_t c = 0xe9c98a);
  void newRun(); void resetField(); void placeParty(); void setupStage(bool first); void recalc(bool full);
  float lightRadius() const; void step(float dt); void fireFlare(bool byPlayer);
  void save(); bool load(); float xpNeed(int L) const; float partyHp() const; const char* stageName(int s, char* buf) const; int reachOf(int s) const { return (s - 1) / 5; }
  int bossOf(int s) const { return BOSS_ORDER[(s - 1) % 3]; } const char* bossTitle(int s) const { return BOSS_TITLES[(s - 1) % 3][((s - 1) / 3) % 4]; }
  void takeCard(int id); void openPending(); void choose(bool push); void openChoice(); void endRun(); int emberGain() const; int boonCost(int id) const; bool buyBoon(int id); void stewardBoons(); void stewardCamp();
  int campCost(int id) const; bool buyCamp(int id, bool quiet); const char* danger(bool next) const; bool stewardPush() const;
  void setFocus(Unit* m); void setRally(float x, float z); void beginRun(bool fresh);
  float yOf(const Unit& u) const;
  // internal
  Unit* mkMon(int type, float x, float z, int s); void spawnPack(); void spawnBoss(); float hurt(Unit* t, float amt, Unit* src, int kind = 0, bool heavy = false, float stag = 0, bool pierce = false);
  void heal(Unit* h, float amt, bool silent = false); void heroDown(Unit* h); void monDie(Unit* m, Unit* src); void addXp(float x); void revive(Unit* h, float f);
  void act(Unit* u, int kind, float wind, float rec, Unit* tgt = nullptr, int clip = 0); void runAct(Unit* u, float dt); void actHit(Unit* u, Act& a);
  void stepHero(Unit* h, float dt); void stepMon(Unit* m, float dt); bool tryHeroSkill(Unit* h, Unit* t); void heroBasic(Unit* h, Unit* t); void bossSpecial(Unit* m, Unit* t);
  void separate(); bool moveTo(Unit* u, float tx, float tz, float sp, float dt, float stopD = 0); void faceTo(Unit* u, Unit* t); bool inRange(Unit* u, Unit* t, float extra = 0);
  Unit* nearestMon(Unit* u, float maxD, int pred = 0); int monsNear(float x, float z, float r, Unit** out, int cap); Unit* pickHeroTarget(Unit* m); void slotOf(Unit* h, float& sx, float& sz);
  void shoot(Unit* u, Unit* t, int vis, float speed, float arc, int hk, int hero, float a = 0); Proj* proj(int vis, float sx, float sy, float sz, float ex, float ey, float ez, float dur, float arc, int hk, int hero);
  void projHit(Proj& p); void zone(int kind, int hero, float x, float z, float r, float dur, float tick); void zoneTick(Zone& z); void dropGear(Unit* m, int minGrade); Item makeItem(int slot, int cls, int stage, int minGrade);
  void spawnLoot(float x, float z, int kind, float silver, const Item* it, int hero); void shiftWorld(float dx); void stepLoot(float dt); void stepProjs(float dt); void hitFx(Unit* t, int kind, float y, Unit* src);
  void turn(Unit* u, float dt); void shake(float a) { shakeA = std::max(shakeA, a); } int liveMonCount(); void clearMons();
};
extern Game G;
