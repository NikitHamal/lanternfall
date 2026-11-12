#include "roster.h"
#include "settings.h"
#include <cstdio>
namespace roster {
using H = Hero;
// key, name, role, weapon, active, activeDesc, passive, passiveDesc, spd stab dmg move hp ammo style abil pass cd dur nativeIx clsIx wpR wpL
static const H TAB[] = {
 {"fletcher", "FLETCHER", "RANGER", "YEW LONGBOW", "RAPID VOLLEY", "DRAW SPEED X1.8 & RAIN ARROWS FOR 5 S", "STEADY HANDS", "AIM SWAY REDUCED BY A THIRD",
  1.05f, 1.25f, 1.05f, 1.02f, 110, 16, ST_BOW, AB_VOLLEY, PS_STEADY, 13, 5.f, FLETCHER, FLETCHER, WP_NONE, WP_NONE},
 {"warden", "WARDEN", "VANGUARD TANK", "ARMING SWORD & SHIELD", "BULWARK", "70% GUARD FOR 4 S & SHIELD BASH SHOCKWAVE", "STEEL SKIN", "-18% DAMAGE TAKEN, RIPOSTE ON PARRY",
  1.05f, 1.15f, 1.15f, 0.96f, 155, 0, ST_SWORD, AB_BULWARK, PS_STEEL, 14, 4.f, WARDEN, WARDEN, WP_NONE, WP_NONE},
 {"reaver", "REAVER", "HEAVY BERSERKER", "IRON GREATSWORD", "SURGE", "DASH 9 M WITH I-FRAMES, CLEAVE ALL IN PATH", "BLOODLUST", "+12% SPEED, UP TO +35% DAMAGE WHEN HURT",
  0.92f, 1.00f, 1.38f, 1.06f, 135, 0, ST_HEAVY, AB_SURGE, PS_BLOOD, 9, 0.45f, REAVER, REAVER, WP_NONE, WP_NONE},
 {"lamplighter", "LAMPLIGHTER", "PYROMANCER", "LANTERN POLE & EMBER", "CINDERFALL", "ERUPT A 7 M RING OF SEARING LANTERN FIRE", "KINDLED", "SPELLS IGNITE FOES FOR BURNING DAMAGE",
  1.08f, 1.10f, 1.12f, 1.00f, 110, 12, ST_FIRE, AB_EMBER, PS_KINDLED, 11, 0.5f, LAMPLIGHTER, LAMPLIGHTER, WP_NONE, WP_NONE},
 {"apothecary", "APOTHECARY", "ALCHEMIST", "VOLATILE FLASKS", "PANACEA TONIC", "HEAL 45 HP, REFILL STAMINA, REGEN FOR 4 S", "FIELD MEDIC", "PASSIVE HEALTH REGEN & CORROSIVE SPLASH",
  1.10f, 1.15f, 1.00f, 1.06f, 115, 10, ST_ALCH, AB_TONIC, PS_MEDIC, 14, 4.f, APOTHECARY, APOTHECARY, WP_NONE, WP_NONE},
 {"rimebinder", "RIMEBINDER", "CRYOMANCER", "PALE SHARD STAFF", "HOARFROST NOVA", "FREEZE & STAGGER ALL FOES WITHIN 10 M", "RIME SHATTER", "SPELLS CHILL FOES; HEAVY LANCES SHATTER",
  1.02f, 1.20f, 1.15f, 1.00f, 105, 12, ST_FROST, AB_FROST, PS_RIME, 13, 4.f, RIMEBINDER, RIMEBINDER, WP_NONE, WP_NONE},

 {"polyfork/fletcher", "ASHEN ASSASSIN", "SHADOW RANGER", "RECURVE SHORTBOW", "RAPID VOLLEY", "DRAW SPEED X1.8 & MULTI-SHOT FOR 5 S", "STEADY HANDS", "AIM SWAY REDUCED, FASTER FOOTWORK",
  1.22f, 1.10f, 1.00f, 1.12f, 100, 18, ST_BOW, AB_VOLLEY, PS_STEADY, 12, 5.f, -1, FLETCHER, WP_NONE, WP_BOW},
 {"polyfork/warden", "KNIGHT OF THE FORGE", "FORGE PALADIN", "BLADE & KITE SHIELD", "BULWARK", "70% GUARD FOR 4 S & SHIELD BASH SHOCKWAVE", "STEEL SKIN", "-18% DAMAGE TAKEN, RIPOSTE ON PARRY",
  1.00f, 1.25f, 1.22f, 0.94f, 165, 0, ST_SWORD, AB_BULWARK, PS_STEEL, 14, 4.f, -1, WARDEN, WP_SWORD, WP_KITE},
 {"polyfork/reaver", "VARKA", "WARLORD MELEE", "RUNE GREATSWORD", "SURGE", "DASH 9 M WITH I-FRAMES, CRUSH FOES IN PATH", "BLOODLUST", "+12% SPEED, UP TO +35% DAMAGE WHEN HURT",
  0.95f, 1.05f, 1.45f, 1.06f, 145, 0, ST_HEAVY, AB_SURGE, PS_BLOOD, 9, 0.45f, -1, REAVER, WP_GREATSWORD, WP_NONE},
 {"polyfork/lamplighter", "FIRE CULTIST", "FLAME ZEALOT", "RITUAL LANTERN & STAFF", "CINDERFALL", "ERUPT A 7 M RING OF CULTIST HELLFIRE", "KINDLED", "SPELLS IGNITE FOES FOR BURNING DAMAGE",
  1.12f, 1.05f, 1.18f, 1.02f, 105, 12, ST_FIRE, AB_EMBER, PS_KINDLED, 11, 0.5f, -1, LAMPLIGHTER, WP_STAFF, WP_LANTERN},
 {"polyfork/apothecary", "FOREST WITCH", "HEX ALCHEMIST", "WITCH STAFF & BREWS", "PANACEA TONIC", "HEAL 45 HP, REFILL STAMINA, REGEN FOR 4 S", "FIELD MEDIC", "PASSIVE HEALTH REGEN & CORROSIVE SPLASH",
  1.15f, 1.10f, 1.05f, 1.08f, 110, 10, ST_ALCH, AB_TONIC, PS_MEDIC, 14, 4.f, -1, APOTHECARY, WP_STAFF, WP_LANTERN},
 {"polyfork/rimebinder", "ORDER JUDGE", "GLACIAL INQUISITOR", "INQUISITOR RIME STAFF", "HOARFROST NOVA", "FREEZE & STAGGER ALL FOES WITHIN 10 M", "RIME SHATTER", "SPELLS CHILL FOES; HEAVY LANCES SHATTER",
  1.05f, 1.25f, 1.20f, 0.98f, 115, 12, ST_FROST, AB_FROST, PS_RIME, 13, 4.f, -1, RIMEBINDER, WP_STAFF, WP_NONE},

 {"polyfork/thornling", "BLOOM ELEMENTAL", "THORN SKIRMISHER", "BRIAR LONGBOW", "RAPID VOLLEY", "THORN BARRAGE & X1.8 DRAW FOR 5 S", "FLEET BRIAR", "+15% MOVE SPEED & CRIT HEADSHOTS",
  1.28f, 1.00f, 0.92f, 1.18f, 95, 20, ST_BOW, AB_VOLLEY, PS_BLOOD, 11, 5.f, -1, FLETCHER, WP_NONE, WP_BOW},
 {"polyfork/peatwight", "BOG FIEND", "MIRE BRUISER", "BOG CLEAVER", "EARTHQUAKE", "8 M MIRE SLAM: HEAVY DAMAGE & STAGGER", "PEAT HIDE", "-18% DAMAGE TAKEN, HEAVY POISE",
  0.96f, 1.05f, 1.32f, 0.96f, 165, 0, ST_HEAVY, AB_QUAKE, PS_STEEL, 13, 0.5f, -1, REAVER, WP_CLEAVER, WP_ROUND},
 {"polyfork/rustbound", "BELL WARDEN", "IRON SENTINEL", "CLEAVER & IRON SHIELD", "BULWARK", "70% GUARD FOR 4 S & IRON TOLL BASH", "STEEL SKIN", "-18% DAMAGE TAKEN, RIPOSTE ON PARRY",
  0.94f, 1.25f, 1.28f, 0.92f, 170, 0, ST_SWORD, AB_BULWARK, PS_STEEL, 15, 4.f, -1, WARDEN, WP_CLEAVER, WP_ROUND},
 {"polyfork/cairnback", "CATHEDRAL STATUE", "GARGOYLE TITAN", "MONOLITH GREATBLADE", "EARTHQUAKE", "8 M STONE SLAM: CRUSHING STAGGER WAVE", "TITAN POISE", "HIGH STABILITY & CRUSHING SHOCKWAVES",
  0.86f, 1.35f, 1.48f, 0.90f, 185, 0, ST_HEAVY, AB_QUAKE, PS_TITAN, 14, 0.5f, -1, REAVER, WP_GREATSWORD, WP_NONE},
 {"polyfork/rootcolossus", "ROOT COLOSSUS", "ANCIENT JUGGERNAUT", "HEARTWOOD GREATBLADE", "EARTHQUAKE", "8 M ROOT QUAKE: MASSIVE KNOCKBACK SLAM", "TITAN POISE", "UNYIELDING ARMOR & SEISMIC CLEAVES",
  0.82f, 1.35f, 1.58f, 0.88f, 205, 0, ST_HEAVY, AB_QUAKE, PS_TITAN, 14, 0.5f, -1, REAVER, WP_GREATSWORD, WP_NONE},
 {"polyfork/barrowking", "CROWNLESS REVENANT", "DEATH KNIGHT", "BARROW KING'S BLADE", "SURGE", "SPECTRAL REAPER DASH THROUGH ALL FOES", "BLOODLUST", "+12% SPEED & EXECUTION CRITS",
  0.94f, 1.20f, 1.50f, 1.00f, 175, 0, ST_SWORD, AB_SURGE, PS_BLOOD, 10, 0.5f, -1, WARDEN, WP_GREATSWORD, WP_KITE},

 {"polyfork/rustbound", "SPEARMAN", "ROAD SKIRMISHER", "ASHWOOD WAR SPEAR", "SKEWER", "DASH 9 M, IMPALING EVERY FOE IN YOUR PATH", "PHALANX DRILL", "-15% DAMAGE TAKEN, SPEAR TIPS CRIT FOR X1.35",
  1.12f, 1.10f, 1.18f, 1.06f, 140, 0, ST_SPEAR, AB_SURGE, PS_STEEL, 10, 0.5f, -1, WARDEN, WP_SPEAR, WP_NONE},
 {"polyfork/warden", "HOPLITE OF THE FORGE", "SHIELDED LANCER", "FORGE SPEAR & KITE SHIELD", "SHIELD WALL", "70% GUARD FOR 4 S & SHIELD BASH SHOCKWAVE", "FORMATION", "-15% DAMAGE TAKEN, DEFLECT ARROWS WITH THE SHIELD",
  1.02f, 1.25f, 1.20f, 0.98f, 165, 0, ST_SPEAR, AB_BULWARK, PS_STEEL, 14, 4.f, -1, WARDEN, WP_SPEAR, WP_KITE},
};
const int N = (int)(sizeof TAB / sizeof TAB[0]);
static int cur_ = 0;
static struct Slot { int idx = -1; FigAsset a; } slot[2];
static bool inited = false;
const Hero& at(int i) { return TAB[((i % N) + N) % N]; }
int sel() { return cur_; }
const char* styleName(int st) {
  static const char* S[NSTYLE] = {"MARKSMAN BOW", "SWORD & SHIELD", "2H HEAVY MELEE", "FIRE CASTER", "ALCHEMIST", "FROST CASTER", "SPEAR & REACH"};
  return S[(st >= 0 && st < NSTYLE) ? st : 0];
}
const char* styleHint(int st) {
  static const char* S[NSTYLE] = {"LMB DRAW   RMB BLOCK   SPACE ROLL   SHIFT SPRINT", "LMB COMBO   RMB SHIELD / PARRY   SPACE ROLL", "TAP LMB CLEAVE   HOLD LMB SLAM   RMB GUARD   SPACE ROLL", "LMB FIREBOLT   HOLD CINDER ORB   RMB WARD   SPACE ROLL", "LMB FLASK   HOLD FIREPOT   RMB MEND WARD   SPACE ROLL", "LMB SHARDS   HOLD FROST LANCE   RMB ICE WARD   SPACE ROLL", "LMB THRUST COMBO   HOLD THROW   RMB DEFLECT   SPACE ROLL"};
  return S[(st >= 0 && st < NSTYLE) ? st : 0];
}
const char* styleTip(int st) {
  static const char* S[NSTYLE] = {"HOLD LMB TO DRAW - RELEASE AT THE GOLD MARK", "LMB COMBO - RMB PARRY - LMB IN GUARD: BASH", "TAP LMB CLEAVE - HOLD + RELEASE: SLAM", "LMB BOLT - HOLD FOR A CINDER ORB - RMB WARD", "LMB FLASK - HOLD FOR AN ACID FIREPOT", "LMB RIME SHARDS - HOLD FOR A FROST LANCE", "LMB THRUST - TIP HITS CRIT - HOLD TO THROW"};
  return S[(st >= 0 && st < NSTYLE) ? st : 0];
}
static FigAsset* resident(int i) {
  i = ((i % N) + N) % N;
  if (TAB[i].nativeIx >= 0) return gFH[TAB[i].nativeIx].ok ? &gFH[TAB[i].nativeIx] : nullptr;
  for (auto& s : slot) if (s.idx == i && s.a.ok) return &s.a;
  return nullptr;
}
static void need(int i, int keep) {
  i = ((i % N) + N) % N;
  if (TAB[i].nativeIx >= 0 || resident(i)) return;
  Slot* s = nullptr;
  for (auto& q : slot) if (q.idx < 0) { s = &q; break; }
  if (!s) for (auto& q : slot) if (q.idx != keep) { s = &q; break; }
  if (!s) s = &slot[0];
  if (s->a.ok || s->a.vao) figFree(s->a);
  s->idx = i;
  char path[128]; snprintf(path, sizeof path, "%s", TAB[i].key);
  if (!figLoad(s->a, path)) { fprintf(stderr, "roster: %s failed to load, falling back\n", path); s->idx = -1; }
}
void select(int i) {
  int prev = cur_;
  cur_ = ((i % N) + N) % N;
  cfg::S.hero = cur_;
  need(cur_, cur_);
  need(cur_ + 1, cur_);
  for (auto& s : slot) if (s.idx >= 0 && s.idx != cur_ && s.idx != (cur_ + 1) % N) { figFree(s.a); s.idx = -1; }
  if (inited && cur_ != prev) cfg::save();
}
void step(int d) { select(cur_ + d); }
const FigAsset* fig(int i) { return resident(i); }
const FigAsset* cur() { const FigAsset* a = resident(cur_); return a ? a : &gFH[FLETCHER]; }
void init() { if (inited) return; inited = true; select(cfg::S.hero); }
static FigAsset rslot; static int ridx = -1;
const FigAsset* rival(int i) {
  i = ((i % N) + N) % N; if (const FigAsset* a = resident(i)) return a;
  if (ridx != i) { if (rslot.ok || rslot.vao) figFree(rslot); ridx = -1; if (figLoad(rslot, TAB[i].key)) ridx = i; }
  return (ridx == i && rslot.ok) ? &rslot : nullptr;
}
void shutdown() { for (auto& s : slot) { figFree(s.a); s.idx = -1; } if (rslot.ok || rslot.vao) figFree(rslot); ridx = -1; }
}
