// 3D archery play modes (practice range + wave hunt): first / third person / free-look showcase.
// Fixed pools only; no heap use and no string formatting per frame (strings are formatted on events).
#pragma once
#include "game.h"
#include "figure.h"
namespace pl {
enum Mode { M_OFF, M_RANGE, M_WAVES, M_SELECT };
enum View { VW_FIRST, VW_THIRD, VW_FREE, NVIEW };
constexpr int MAXZONE = 6, MAXFOE = 24, MAXARR = 112, MAXTGT = 14, MAXFEED = 5;
struct Input { float mx = 0, mz = 0; bool sprint = false, lmb = false, rmb = false, roll = false; };
struct Arrow { bool on = false, stuck = false; int own = 0, tg = -1; float x, y, z, vx, vy, vz, px, py, pz, t, life, dmg, pw, lu, lv, drop; int kind = 0, pierce = 0; uint32_t hm = 0; float grav = 0, rad = 0; };
struct Target { bool on = false; int kind = 0; float x, y, z, ox, oy, oz, amp, spd, ph, hitT, tilt, R; int hits = 0; };
struct Foe { Unit u; float cd = 1, strafe = 0, kx = 0, kz = 0, tele = 0, headY = 1.5f, burnT = 0, burnDps = 0, burnAcc = 0, hx = 0, hz = 0; int ai = 0; bool shot = false, deathFx = false, boss = false; };
struct Feed { char s[56] = {0}; uint32_t col = 0xffffff; float t = 0; };
struct Zone { bool on = false; int kind = 0; float x = 0, z = 0, r = 3, t = 0, dps = 0, tick = 0; };
struct VMW { bool on = false; int wp[2] = {-1, -1}; M4 m[2]; };
struct Player {
  int style = 0, atkKind = 0, atkIdx = 0, atkCombo = 0; bool atkOn = false, atkDone = false, atkCharged = false, charging = false, lmbPrev = false;
  float atkT = 0, atkWind = 0.3f, atkRec = 0.3f, atkYaw = 0, atkCf = 0, chargeT = 0, comboT = 0, lungeT = 0, lungeX = 0, lungeZ = 0, blockT = 0, ripT = 0;
  float x = 0, y = 0, z = 0, ox = 0, oz = 0, vx = 0, vz = 0, yaw = 1.5708f, pitch = 0, face = 1.5708f, speed = 0;
  float hp = 100, maxHp = 100, st = 100, stWait = 0, draw = 0, holdT = 0, relT = 9, nockT = 0, rollT = 0, rollDx = 0, rollDz = 0, iT = 0, hurtT = 0, stagT = 0, hitDir = 0, hitK = 0, downT = 0, blockK = 0, blocking = 0;
  float swayX = 0, swayY = 0, kickP = 0, kickY = 0, bob = 0, fovK = 0, hmT = 0, hmKind = 0, vig = 0, flinchT = 0, powerShown = 0;
  bool drawing = false, down = false, over = false; int lives = 3; float rel = 0;
  float abilCd = 0, abilT = 0, abilMax = 14, quiverT = 0, vic = 0, dashT = 0, dashX = 0, dashZ = 0, armor = 0, dMul = 1, stab = 1, dmgMul = 1, mvMul = 1, regen = 0; int quiver = 16, quiverMax = 16;
};
struct Hud {
  int view = VW_FIRST; bool assist = true, bot = false; int score = 0, combo = 0, bestCombo = 0, shots = 0, hits = 0, kills = 0, wave = 0, foesLeft = 0, lastPts = 0; float bannerT = 0, comboT = 0, scoreShow = 0, spread = 20, sweet = 0, shake = 0, zoomK = 0;
  char banner[48] = {0}, sub[48] = {0}, scoreS[16] = {0}, accS[24] = {0}, waveS[32] = {0}, comboS[16] = {0}; Feed feed[MAXFEED]; float resT = 0; bool ended = false; int mode = M_OFF;
};
struct CamState { V3 eye, tgt; float fov = 1.0f; bool init = false; float blend = 1; V3 oldEye, oldTgt; float oldFov = 1, dist = 3.4f, orbitYaw = 0, orbitPitch = 0.25f, orbitD = 6, idle = 0; V3 pivot; V3 sm; };
extern Zone zones[MAXZONE]; extern VMW vmw; extern Player P; extern Hud H; extern Foe foes[MAXFOE]; extern Arrow arrows[MAXARR]; extern Target tgts[MAXTGT]; extern CamState cam; extern Unit playerUnit;
extern V3 dots[40]; extern int nDots; extern V3 landPt; extern bool landOk;
constexpr float ROLL_T = 0.55f, DRAW_T = 0.85f, SWEET0 = 0.93f;
extern int pendingMode;
bool active(); void start(int mode); void stop(); void update(float dtReal, const Input& in); void look(float dx, float dy); void scroll(float dy); void keyPress(int key);
float timeScale(); void camera(float dt); void buildShapes(); float curve(float p); V3 aimDir(); V3 lanternPos(); bool freeDrag(); void setView(int v);
void heroStep(int d); void useAbility(); extern float selT; void setBiome(int b); int biome();
void debugAim(float yaw, float pitch); void debugDraw(float d); void debugBot(bool on);
}
