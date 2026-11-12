// Playable roster: 6 native heroes + Polyfork heroes + bestiary bipeds. Only the current and the next hero are resident (lazy load, previous released).
#pragma once
#include "figure.h"
namespace roster {
enum Ability { AB_VOLLEY, AB_BULWARK, AB_SURGE, AB_EMBER, AB_TONIC, AB_FROST, AB_QUAKE, NAB };
enum Passive { PS_NONE, PS_STEADY, PS_STEEL, PS_BLOOD, PS_KINDLED, PS_MEDIC, PS_RIME, PS_TITAN };
struct Hero {
  const char *key, *name, *role, *weapon, *active, *activeDesc, *passive, *passiveDesc;
  float draw, stab, dmg, move, hp; int quiver, abil, pass; float cd, dur; int nativeIx;   // nativeIx >= 0: shares gFH[nativeIx]
};
extern const int N;
const Hero& at(int i);
int sel(); void select(int i); void step(int d);    // select() persists the choice and re-balances resident figures
const FigAsset* fig(int i);                          // null if the file failed to load
const FigAsset* cur();                               // currently selected figure (never null once init() ran: falls back to native fletcher)
void init(); void shutdown();
}
