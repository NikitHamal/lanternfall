#include "settings.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif
namespace cfg {
Settings S;
static const char* ANAME[ACT_COUNT] = {"FORWARD", "BACK", "LEFT", "RIGHT", "SPRINT", "ROLL / DODGE", "SWITCH VIEW", "ARC ASSIST"};
const char* actionName(int a) { return a >= 0 && a < ACT_COUNT ? ANAME[a] : "?"; }
void defaultKeys() { const int k[ACT_COUNT] = {'W', 'S', 'A', 'D', 340, 32, 'V', 'G'}; for (int i = 0; i < ACT_COUNT; i++) S.key[i] = k[i]; }
void defaults() { S = Settings(); defaultKeys(); }
void keyName(int k, char* o, int n) {
  if (k >= 'A' && k <= 'Z') snprintf(o, n, "%c", k); else if (k >= '0' && k <= '9') snprintf(o, n, "%c", k);
  else switch (k) { case 32: snprintf(o, n, "SPACE"); break; case 340: snprintf(o, n, "L-SHIFT"); break; case 344: snprintf(o, n, "R-SHIFT"); break; case 341: snprintf(o, n, "L-CTRL"); break;
    case 345: snprintf(o, n, "R-CTRL"); break; case 342: snprintf(o, n, "L-ALT"); break; case 258: snprintf(o, n, "TAB"); break; case 262: snprintf(o, n, "RIGHT"); break; case 263: snprintf(o, n, "LEFT"); break; case 264: snprintf(o, n, "DOWN"); break; case 265: snprintf(o, n, "UP"); break; case 257: snprintf(o, n, "ENTER"); break;
    default: snprintf(o, n, "KEY %d", k); }
}
static bool num(const char* t, const char* name, double& v) { char pat[40]; snprintf(pat, sizeof pat, "\"%s\":", name); const char* p = strstr(t, pat); if (!p) return false; v = atof(p + strlen(pat)); return true; }
void load() {
  defaults(); FILE* f = fopen("save/settings.json", "rb"); if (!f) return; static char buf[4096]; size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f); double v;
  if (num(buf, "sens", v)) S.sens = (float)v; if (num(buf, "fov", v)) S.fov = (float)v; if (num(buf, "shake", v)) S.shake = (float)v; if (num(buf, "master", v)) S.master = (float)v;
  if (num(buf, "invertY", v)) S.invertY = v != 0; if (num(buf, "assist", v)) S.assist = v != 0; if (num(buf, "reduceMotion", v)) S.reduceMotion = v != 0; if (num(buf, "colorblind", v)) S.colorblind = v != 0;
  if (num(buf, "toggleAim", v)) S.toggleAim = v != 0; if (num(buf, "subtitles", v)) S.subtitles = v != 0; if (num(buf, "reticle", v)) S.reticle = (int)v & 3; if (num(buf, "hero", v)) S.hero = (int)v;
  const char* k = strstr(buf, "\"keys\":["); if (k) { k += 8; for (int i = 0; i < ACT_COUNT; i++) { int x = (int)strtol(k, (char**)&k, 10); if (x > 0) S.key[i] = x; while (*k == ',' || *k == ' ') k++; } }
  if (S.sens < 0.25f) S.sens = 0.25f; if (S.sens > 3.f) S.sens = 3.f; if (S.fov < 55.f) S.fov = 55.f; if (S.fov > 100.f) S.fov = 100.f; if (S.shake < 0) S.shake = 0; if (S.shake > 1) S.shake = 1; if (S.master < 0) S.master = 0; if (S.master > 1) S.master = 1;
}
void save() {
  MKDIR("save"); FILE* f = fopen("save/settings.json", "wb"); if (!f) return;
  fprintf(f, "{\n\"sens\":%.3f,\n\"fov\":%.1f,\n\"shake\":%.2f,\n\"master\":%.2f,\n\"invertY\":%d,\n\"assist\":%d,\n\"reduceMotion\":%d,\n\"colorblind\":%d,\n\"toggleAim\":%d,\n\"subtitles\":%d,\n\"reticle\":%d,\n\"hero\":%d,\n\"keys\":[", S.sens, S.fov, S.shake, S.master, S.invertY, S.assist, S.reduceMotion, S.colorblind, S.toggleAim, S.subtitles, S.reticle, S.hero);
  for (int i = 0; i < ACT_COUNT; i++) fprintf(f, "%s%d", i ? "," : "", S.key[i]); fprintf(f, "]\n}\n"); fclose(f);
}
}
