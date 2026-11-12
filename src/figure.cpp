#include "figure.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#ifndef _WIN32
#include <unistd.h>
#endif
FigAsset gFH[NCLS], gFM[NMON], gFMAlt[9], gFW[NWEAP], gFP[NPFP];
const float IDENT_BONE[12] = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0};
GLuint pFig = 0, pFigShadow = 0;

// ------------------------------------------------------------------ loading
static const char* HERO_KEYS[NCLS] = {"warden", "reaver", "fletcher", "lamplighter", "apothecary", "rimebinder"};
static const char* MON_KEYS[NMON] = {
  "thornling", "peatwight", "gloamhound", "gloamwisp", "rustbound", "cairnback",
  "barrowking", "mossjaw", "hollowstag",
  "polyfork/emberhound", "polyfork/thornboar", "polyfork/rootcolossus"
};
static const char* MON_ALT_KEYS[9] = {
  "polyfork/thornling", "polyfork/peatwight", "polyfork/gloamhound", "polyfork/gloamwisp",
  "polyfork/rustbound", "polyfork/cairnback", "polyfork/barrowking", "polyfork/mossjaw", "polyfork/hollowstag"
};
static const char* WEAP_KEYS[NWEAP] = {
  "polyfork/w_arming_sword", "polyfork/w_greatsword", "polyfork/w_bow", "polyfork/w_kite_shield",
  "polyfork/w_round_shield", "polyfork/w_staff", "polyfork/w_cleaver", "polyfork/w_lantern"
};
static const char* PROP_KEYS[NPFP] = {
  "polyfork/p_pine_tall", "polyfork/p_pine_young", "polyfork/p_twisted_oak", "polyfork/p_dead_tree",
  "polyfork/p_boulder", "polyfork/p_cairn", "polyfork/p_thorn_bush", "polyfork/p_round_bush",
  "polyfork/p_fallen_log", "polyfork/p_plank_bridge", "polyfork/p_brazier", "polyfork/p_lamp_post",
  "polyfork/p_signpost", "polyfork/p_campfire_tripod", "polyfork/p_banner_pole", "polyfork/p_iron_fence",
  "polyfork/p_cemetery_gate", "polyfork/b_stone_chapel", "polyfork/b_watchtower", "polyfork/b_thatch_cottage"
};
static char gDir[1024];
static bool exists(const char* p) { FILE* f = fopen(p, "rb"); if (f) { fclose(f); return true; } return false; }
static bool findDir(const char* argv0) {
  char exe[1024] = {0}, probe[1200];
#ifndef _WIN32
  ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1); if (n > 0) exe[n] = 0; else exe[0] = 0;
#endif
  if (!exe[0] && argv0) snprintf(exe, sizeof exe, "%s", argv0);
  for (char* c = exe; *c; c++) if (*c == '\\') *c = '/';
  char* sl = strrchr(exe, '/'); if (sl) *sl = 0; else strcpy(exe, ".");
  const char* rel[] = {"/assets", "/../assets", "/../../assets", "/../Resources/assets", "/../share/lanternfall/assets"};
  for (const char* r : rel) { snprintf(probe, sizeof probe, "%s%s/warden.lfm", exe, r); if (exists(probe)) { snprintf(gDir, sizeof gDir, "%s%s", exe, r); return true; } }
  const char* cw[] = {"assets", "../assets", "../../assets"};
  for (const char* r : cw) { snprintf(probe, sizeof probe, "%s/warden.lfm", r); if (exists(probe)) { snprintf(gDir, sizeof gDir, "%s", r); return true; } }
  return false;
}
template <class T> static T rd(const uint8_t*& p) { T v; memcpy(&v, p, sizeof(T)); p += sizeof(T); return v; }
static bool loadFig(FigAsset& a, const char* key) {
  char path[1300]; snprintf(path, sizeof path, "%s/%s.lfm", gDir, key); FILE* f = fopen(path, "rb"); if (!f) return false;
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET); uint8_t* buf = (uint8_t*)malloc(sz); if (!buf || fread(buf, 1, sz, f) != (size_t)sz) { fclose(f); free(buf); return false; } fclose(f);
  const uint8_t* p = buf; if (memcmp(p, "LFM1", 4)) { free(buf); return false; } p += 4;
  a.nv = (int)rd<uint32_t>(p); int ntri = (int)rd<uint32_t>(p); a.ni = ntri * 3; a.nb = (int)rd<uint32_t>(p); a.nclips = (int)rd<uint32_t>(p); a.nfx = (int)rd<uint32_t>(p); a.baseNV = (int)rd<uint32_t>(p); a.baseNI = (int)rd<uint32_t>(p);
  if (a.nb > FMAXB || a.nclips > 16 || a.nfx > 8) { free(buf); return false; }
  a.scale = rd<float>(p); a.radius = rd<float>(p); a.hpY = rd<float>(p); a.height = rd<float>(p); for (int i = 0; i < 3; i++) a.disCol[i] = rd<float>(p);
  a.hero = rd<uint8_t>(p); a.flying = rd<uint8_t>(p); a.lant = rd<uint8_t>(p); p++;
  memcpy(a.death, p, 12); a.death[12] = 0; p += 12; memcpy(a.key, p, 16); a.key[16] = 0; p += 16;
  for (int i = 0; i < a.nb; i++) { memcpy(a.bone[i], p, 12); a.bone[i][12] = 0; p += 12; }
  for (int i = 0; i < a.nb; i++) a.parent[i] = (int8_t)p[i]; p += (a.nb + 3) / 4 * 4;
  memcpy(a.head, p, a.nb * 12); p += a.nb * 12; p += (a.nb + 3) / 4 * 4;
  for (int i = 0; i < a.nfx; i++) { memcpy(a.fx[i].name, p, 12); a.fx[i].name[12] = 0; p += 12; a.fx[i].bone = rd<int32_t>(p); for (int k = 0; k < 3; k++) a.fx[i].p[k] = rd<float>(p); }
  glGenVertexArrays(1, &a.vao); glBindVertexArray(a.vao); glGenBuffers(1, &a.vbo); glBindBuffer(GL_ARRAY_BUFFER, a.vbo); const uint8_t* vtx = p; glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)a.nv * 36, p, GL_STATIC_DRAW); p += (size_t)a.nv * 36;
  glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 36, (void*)0);
  glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_BYTE, GL_TRUE, 36, (void*)12);
  glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 36, (void*)16);
  glEnableVertexAttribArray(3); glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 36, (void*)20);
  glEnableVertexAttribArray(4); glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, 36, (void*)28);
  glEnableVertexAttribArray(5); glVertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_FALSE, 36, (void*)32);
  glGenBuffers(1, &a.ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, a.ibo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)a.ni * 2, p, GL_STATIC_DRAW);
  {  // second index set without baked weapon geometry (bones wL / wR / str / arw) so socketed weapons are the only ones drawn
    bool wb[FMAXB] = {false}; bool any = false; for (int i = 0; i < a.nb; i++) if (!strcmp(a.bone[i], "str") || !strcmp(a.bone[i], "arw")) { wb[i] = true; any = true; }
    if (any) for (int i = 0; i < a.nb; i++) if (!strcmp(a.bone[i], "wL") || !strcmp(a.bone[i], "wR")) wb[i] = true;
    if (any) {
      const uint16_t* ix = (const uint16_t*)p; uint16_t* o = (uint16_t*)malloc((size_t)a.ni * 2); int no = 0, nbase = 0;
      for (int t = 0; t < a.ni; t += 3) { if (t == a.baseNI) nbase = no; bool drop = true; for (int k = 0; k < 3; k++) { int bi = vtx[(size_t)ix[t + k] * 36 + 32]; if (bi >= a.nb || !wb[bi]) drop = false; } if (drop) continue; for (int k = 0; k < 3; k++) o[no++] = ix[t + k]; }
      if (a.baseNI >= a.ni) nbase = no; a.baseNI2 = nbase; a.ni2 = no; glGenBuffers(1, &a.ibo2); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, a.ibo2); glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)no * 2, o, GL_STATIC_DRAW); free(o);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, a.ibo);
    }
  }
  p += (size_t)a.ni * 2; glBindVertexArray(0);
  const uint8_t* q = p; size_t total = 0;
  for (int i = 0; i < a.nclips; i++) { q += 12; uint16_t fr; memcpy(&fr, q, 2); q += 4 + 16; size_t bytes = (size_t)fr * a.nb * 14; q += bytes; total += bytes; }
  a.clipMem = total ? (int16_t*)malloc(total) : nullptr; size_t off = 0;
  for (int i = 0; i < a.nclips; i++) {
    FigClip& c = a.clips[i]; memcpy(c.name, p, 12); c.name[12] = 0; p += 12; c.frames = rd<uint16_t>(p); c.loop = rd<uint8_t>(p); p++; c.fps = rd<float>(p); c.hit = rd<float>(p); c.speed = rd<float>(p); c.T = rd<float>(p);
    size_t bytes = (size_t)c.frames * a.nb * 14; if (a.clipMem && bytes) memcpy((uint8_t*)a.clipMem + off, p, bytes); c.d = a.clipMem ? (const int16_t*)((uint8_t*)a.clipMem + off) : nullptr; off += bytes; p += bytes;
  }
  free(buf); a.ok = true; return true;
}
const char* figAssetDir() { return gDir; }
bool figInit(const char* argv0) {
  if (!findDir(argv0)) { fprintf(stderr, "lanternfall: assets/ folder not found (expected next to the executable)\n"); return false; }
  bool core = true;
  for (int i = 0; i < NCLS; i++) core &= loadFig(gFH[i], HERO_KEYS[i]);
  for (int i = 0; i < 9; i++) core &= loadFig(gFM[i], MON_KEYS[i]);
  for (int i = 9; i < NMON; i++) { if (loadFig(gFM[i], MON_KEYS[i])) gFM[i].scale = MON[i].scale; }
  for (int i = 0; i < 9; i++) loadFig(gFMAlt[i], MON_ALT_KEYS[i]);
  for (int i = 0; i < NWEAP; i++) loadFig(gFW[i], WEAP_KEYS[i]);
  for (int i = 0; i < NPFP; i++) loadFig(gFP[i], PROP_KEYS[i]);
  if (!core) fprintf(stderr, "lanternfall: some core figure assets failed to load from %s\n", gDir);
  return core;
}
void figShutdown() {
  for (auto& a : gFH) figFree(a);
  for (auto& a : gFM) figFree(a);
  for (auto& a : gFMAlt) figFree(a);
  for (auto& a : gFW) figFree(a);
  for (auto& a : gFP) figFree(a);
}

// ------------------------------------------------------------------ animation
static inline float sat01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
static inline float smth(float a, float b, float x) { x = sat01((x - a) / (b - a)); return x * x * (3 - 2 * x); }
static void sampleClip(const FigAsset& a, const FigClip& c, float fr, float* out) {
  if (!c.d || c.frames <= 0) return;
  int n = c.frames, i0, i1; float w;
  if (c.loop) { fr = std::fmod(fr, (float)n); if (fr < 0) fr += n; i0 = (int)fr; w = fr - i0; i1 = (i0 + 1) % n; }
  else { fr = fr < 0 ? 0 : (fr > n - 1 ? (float)(n - 1) : fr); i0 = (int)fr; w = fr - i0; i1 = std::min(i0 + 1, n - 1); }
  const int16_t* A = c.d + (size_t)i0 * a.nb * 7; const int16_t* B = c.d + (size_t)i1 * a.nb * 7;
  for (int b = 0; b < a.nb; b++) {
    const int16_t* p = A + b * 7; const int16_t* q = B + b * 7; float* o = out + b * 7;
    float d = (float)p[0] * q[0] + (float)p[1] * q[1] + (float)p[2] * q[2] + (float)p[3] * q[3]; float s = d < 0 ? -1.f : 1.f;
    float x = (p[0] + (s * q[0] - p[0]) * w), y = (p[1] + (s * q[1] - p[1]) * w), z = (p[2] + (s * q[2] - p[2]) * w), ww = (p[3] + (s * q[3] - p[3]) * w);
    float l = 1.f / std::sqrt(x * x + y * y + z * z + ww * ww + 1e-12f); o[0] = x * l; o[1] = y * l; o[2] = z * l; o[3] = ww * l;
    for (int k = 0; k < 3; k++) o[4 + k] = (p[4 + k] + (q[4 + k] - p[4 + k]) * w) * (1.f / 2048.f);
  }
}
static void blendPose(float* a, const float* b, float w, int nb) {
  if (w <= 0.0005f) return;
  for (int i = 0; i < nb; i++) {
    float* p = a + i * 7; const float* q = b + i * 7; float d = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3], s = d < 0 ? -1.f : 1.f;
    float x = p[0] + (s * q[0] - p[0]) * w, y = p[1] + (s * q[1] - p[1]) * w, z = p[2] + (s * q[2] - p[2]) * w, ww = p[3] + (s * q[3] - p[3]) * w, l = 1.f / std::sqrt(x * x + y * y + z * z + ww * ww + 1e-12f);
    p[0] = x * l; p[1] = y * l; p[2] = z * l; p[3] = ww * l; for (int k = 4; k < 7; k++) p[k] += (q[k] - p[k]) * w;
  }
}
int figActClip(const FigAsset& a, int kind) {
  static const char* H[] = {"", "attack", "bash", "guard", "cleave", "sunder", "aimed", "volley", "aimed", "attack", "kindle", "cinder", "tonic", "firepot", "quick", "lance", "hoar", "hoar", "attack", "attack", "attack", "special", "kindle"};
  if (kind < 0 || kind > 22) kind = 1;
  int i = a.clipIx(H[kind]);
  if (i < 0 && kind == 3) i = a.clipIx("defend");
  if (i < 0) i = a.clipIx("attack");
  return i;
}
static bool inSub(const FigAsset& a, int b, int root);
static void procLoco(FigAnim& an, const FigAsset& a, float dt, float as, float w, float ready); static bool isBiped(const FigAsset& a);
void figAnimUpdate(FigAnim& an, const FigAsset& a, float dt, float speed, bool combat, int actClip, float actU, float stagV, bool down, float guard, float dodgeU, float hitDir) {
  if (a.nclips <= 0) {
    for (int b = 0; b < a.nb; b++) { float* o = an.pose + b * 7; o[0] = o[1] = o[2] = 0; o[3] = 1; o[4] = a.head[b][0]; o[5] = a.head[b][1]; o[6] = a.head[b][2]; }
    an.have = true; return;
  }
  const int nb = a.nb; float tmp[FMAXB * 7], tmp2[FMAXB * 7];
  an.t += dt; float as = std::fabs(speed), mv = std::min(1.35f, as / 1.45f);
  an.moveW += ((mv > 0.04f ? 1.f : 0.f) - an.moveW) * (1 - std::exp(-dt * (mv > 0.04f ? 8.f : 6.f)));
  an.readyW += ((combat ? 1.f : 0.f) - an.readyW) * (1 - std::exp(-dt * 7.f));
  an.guardW += (sat01(guard) - an.guardW) * (1 - std::exp(-dt * 14.f));
  int ci = a.clipIx("idle"), cr = a.clipIx("ready"), cw = a.clipIx("walk"), cn = a.clipIx("run");
  if (ci < 0) ci = 0; if (cr < 0) cr = ci;
  const FigClip& I = a.clips[ci]; const FigClip& R = a.clips[cr];
  sampleClip(a, I, an.t * (I.frames / std::max(0.1f, I.T)), an.pose);
  if (an.readyW > 0.002f) { sampleClip(a, R, an.t * (R.frames / std::max(0.1f, R.T)), tmp); blendPose(an.pose, tmp, an.readyW, nb); }
  if (an.moveW > 0.002f && gProcGait && isBiped(a)) procLoco(an, a, dt, as, an.moveW, an.readyW);
  else if (an.moveW > 0.002f && (cw >= 0 || cn >= 0)) {
    float wk = 0; const FigClip *W = nullptr, *N = nullptr;
    if (a.hero) { W = cw >= 0 ? &a.clips[cw] : nullptr; N = cn >= 0 ? &a.clips[cn] : W; if (!W) W = N; wk = smth(1.5f, 3.1f, as); }
    else { N = cn >= 0 ? &a.clips[cn] : &a.clips[cw]; W = cw >= 0 ? &a.clips[cw] : N; wk = smth(N->speed * 0.58f, N->speed * 0.90f, as); }
    float dist = W->T * W->speed + (N->T * N->speed - W->T * W->speed) * wk; if (dist < 0.22f) dist = 0.22f;
    an.ph += as * dt / dist; an.ph -= std::floor(an.ph);
    sampleClip(a, *W, an.ph * W->frames, tmp); if (wk > 0.001f && N != W) { sampleClip(a, *N, an.ph * N->frames, tmp2); blendPose(tmp, tmp2, wk, nb); }
    blendPose(an.pose, tmp, an.moveW, nb);
  }
  // Defensive guard / shield block clip blend
  if (an.guardW > 0.005f) {
    int cg = a.clipIx("defend"); if (cg < 0) cg = a.clipIx("guard");
    if (cg >= 0) { const FigClip& c = a.clips[cg]; sampleClip(a, c, c.hit > 0.01f ? c.hit * (c.frames - 1) : 0.35f * (c.frames - 1), tmp); blendPose(an.pose, tmp, an.guardW * 0.88f, nb); }
  }
  // Evasive dodge clip blend when authored on rig
  if (dodgeU >= 0.f && dodgeU < 1.f) {
    int cdg = a.clipIx("dodge");
    if (cdg >= 0) { const FigClip& c = a.clips[cdg]; sampleClip(a, c, dodgeU * (c.frames - 1), tmp); blendPose(an.pose, tmp, std::sin(dodgeU * PI) * 0.92f, nb); }
  }
  // Smooth action clip blending with graceful blend-out tail + Polyfork high-arm strike arc polish
  bool actOn = actClip >= 0 && actU >= 0.f && actU < 1.f;
  if (actOn) { an.actLast = actClip; an.actLastU = actU; an.actW = std::min(1.f, actU / 0.06f) * std::min(1.f, (1.f - actU) / 0.10f); }
  else { an.actW *= std::exp(-dt * 14.f); }
  if (an.actW > 0.004f && an.actLast >= 0 && an.actLast < a.nclips) {
    const FigClip& c = a.clips[an.actLast];
    sampleClip(a, c, sat01(an.actLastU) * (c.frames - 1), tmp);
    // Upper-body blend during locomotion so legs keep striding naturally when attacking on the move!
    float legKeep = an.moveW * 0.65f;
    for (int b = 0; b < nb; b++) {
      bool isLeg = (strstr(a.bone[b], "Thigh") || strstr(a.bone[b], "Shin") || strstr(a.bone[b], "Foot") || strstr(a.bone[b], "Toe"));
      float bw = an.actW * (isLeg ? (1.f - legKeep) : 1.f);
      blendPose(an.pose + b * 7 - b * 7, tmp, 0, 0); (void)bw;
    }
    if (legKeep > 0.02f) {
      for (int b = 0; b < nb; b++) {
        bool isLeg = (strstr(a.bone[b], "Thigh") || strstr(a.bone[b], "Shin") || strstr(a.bone[b], "Foot") || strstr(a.bone[b], "Toe"));
        float bw = an.actW * (isLeg ? (1.f - legKeep) : 1.f);
        float* p = an.pose + b * 7; const float* q = tmp + b * 7;
        float d = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3], s = d < 0 ? -1.f : 1.f;
        float x = p[0] + (s * q[0] - p[0]) * bw, y = p[1] + (s * q[1] - p[1]) * bw, z = p[2] + (s * q[2] - p[2]) * bw, ww = p[3] + (s * q[3] - p[3]) * bw;
        float l = 1.f / std::sqrt(x * x + y * y + z * z + ww * ww + 1e-12f);
        p[0] = x * l; p[1] = y * l; p[2] = z * l; p[3] = ww * l;
        for (int k = 4; k < 7; k++) p[k] += (q[k] - p[k]) * bw;
      }
    } else {
      blendPose(an.pose, tmp, an.actW, nb);
    }
    // Polyfork 25-bone authored melee/cast strike-height polish (fixes arm-too-high hit frame noted in ASSETS_STATUS)
    if (nb == 25 && strcmp(c.name, "aimed") != 0 && strcmp(c.name, "volley") != 0) {
      float hit = c.hit > 0.01f ? c.hit : 0.45f;
      float bell = std::exp(-std::pow((an.actLastU - hit) / 0.26f, 2.f)) * an.actW;
      int rU = figBone(a, "rUp", 12), sp = figBone(a, "spine", 2);
      if (bell > 0.01f) {
        figAddRot(an, a, rU, 1, 0, 0, 0.56f * bell, a.head[rU]);
        figAddRot(an, a, sp, 1, 0.35f, 0, 0.16f * bell, a.head[sp]);
      }
    }
  }
  // Directional hit reaction (uses authored hit_f / hit_l / hit_r on Polyfork rigs, fallback to stag)
  if (stagV > 0.2f && an.stagK > 0.38f) {
    an.stagK = 0;
    int cs = -1;
    if (std::fabs(hitDir) > 0.65f) cs = a.clipIx(hitDir > 0 ? "hit_r" : "hit_l");
    if (cs < 0) cs = a.clipIx("hit_f");
    if (cs < 0) cs = a.clipIx("stag");
    an.stagClip = cs;
  }
  if (an.stagClip >= 0 && an.stagK < 0.55f) {
    an.stagK += dt;
    const FigClip& c = a.clips[an.stagClip];
    float u = sat01(an.stagK / 0.45f);
    sampleClip(a, c, u * (c.frames - 1), tmp);
    blendPose(an.pose, tmp, std::sin(sat01(1.f - u * 0.85f) * PI * 0.5f), nb);
  }
  an.downW += ((down ? 1.f : 0.f) - an.downW) * (1 - std::exp(-dt * (down ? 3.5f : 4.5f)));
  int cd = a.clipIx("down");
  if (cd >= 0 && an.downW > 0.002f) { const FigClip& c = a.clips[cd]; sampleClip(a, c, an.downW * (c.frames - 1), tmp); blendPose(an.pose, tmp, sat01(an.downW * 5.f), nb); }
  an.have = true;
}
static void quatR(const float* q, float* R) {
  float x = q[0], y = q[1], z = q[2], w = q[3];
  R[0] = 1 - 2 * (y * y + z * z); R[1] = 2 * (x * y - z * w); R[2] = 2 * (x * z + y * w); R[3] = 2 * (x * y + z * w); R[4] = 1 - 2 * (x * x + z * z); R[5] = 2 * (y * z - x * w);
  R[6] = 2 * (x * z - y * w); R[7] = 2 * (y * z + x * w); R[8] = 1 - 2 * (x * x + y * y);
}
void figAnimPose(FigAnim& an, const FigAsset& a) {
  for (int b = 0; b < a.nb; b++) {
    const float* p = an.pose + b * 7; float R[9]; const float* H = a.head[b], *Wp = p + 4;
    if (an.shat) { memcpy(R, an.fR + b * 9, 36); Wp = an.fp + b * 3; } else quatR(p, R);
    float* m = an.mat + b * 12;
    m[0] = R[0]; m[1] = R[1]; m[2] = R[2]; m[3] = Wp[0] - (R[0] * H[0] + R[1] * H[1] + R[2] * H[2]);
    m[4] = R[3]; m[5] = R[4]; m[6] = R[5]; m[7] = Wp[1] - (R[3] * H[0] + R[4] * H[1] + R[5] * H[2]);
    m[8] = R[6]; m[9] = R[7]; m[10] = R[8]; m[11] = Wp[2] - (R[6] * H[0] + R[7] * H[1] + R[8] * H[2]);
  }
}
static bool inSub(const FigAsset& a, int b, int root) { for (int g = 0; b >= 0 && g < FMAXB; g++) { if (b == root) return true; b = a.parent[b] == (int8_t)255 || a.parent[b] < 0 ? -1 : a.parent[b]; } return false; }
void figAddRot(FigAnim& an, const FigAsset& a, int root, float ax, float ay, float az, float ang, const float* pv) {
  if (root < 0 || std::fabs(ang) < 1e-5f) return; float l = std::sqrt(ax * ax + ay * ay + az * az); if (l < 1e-6f) return; ax /= l; ay /= l; az /= l;
  float s = std::sin(ang * 0.5f), c = std::cos(ang * 0.5f), qx = ax * s, qy = ay * s, qz = az * s, qw = c; float R[9]; float q[4] = {qx, qy, qz, qw}; quatR(q, R);
  for (int b = 0; b < a.nb; b++) { if (!inSub(a, b, root)) continue; float* p = an.pose + b * 7;
    float px = p[4] - pv[0], py = p[5] - pv[1], pz = p[6] - pv[2]; p[4] = pv[0] + R[0] * px + R[1] * py + R[2] * pz; p[5] = pv[1] + R[3] * px + R[4] * py + R[5] * pz; p[6] = pv[2] + R[6] * px + R[7] * py + R[8] * pz;
    float x = p[0], y = p[1], z = p[2], w = p[3]; p[0] = qw * x + qx * w + qy * z - qz * y; p[1] = qw * y - qx * z + qy * w + qz * x; p[2] = qw * z + qx * y - qy * x + qz * w; p[3] = qw * w - qx * x - qy * y - qz * z; }
}
void figAddMove(FigAnim& an, const FigAsset& a, int root, float dx, float dy, float dz) { if (root < 0) return; for (int b = 0; b < a.nb; b++) if (inSub(a, b, root)) { an.pose[b * 7 + 4] += dx; an.pose[b * 7 + 5] += dy; an.pose[b * 7 + 6] += dz; } }

// ------------------------------------------------------------------ procedural biped locomotion (foot-planted gait, 2-bone leg IK)
bool gProcGait = true;
static inline float smr(float x) { x = sat01(x); return x * x * x * (x * (x * 6 - 15) + 10); }
static void rotArc(FigAnim& an, const FigAsset& a, int root, V3 pv, V3 from, V3 to) {
  V3 ax = cross(from, to); float s = len(ax), c = clampf(dot(from, to), -1.f, 1.f); if (s < 1e-6f) return; float ang = std::atan2(s, c); float p[3] = {pv.x, pv.y, pv.z}; figAddRot(an, a, root, ax.x, ax.y, ax.z, ang, p);
}
static V3 bpos(const FigAnim& an, int b) { const float* p = an.pose + b * 7 + 4; return {p[0], p[1], p[2]}; }
static void legIK(FigAnim& an, const FigAsset& a, int t, int s, int f, V3 tgt, float footPitch) {
  V3 hip = bpos(an, t), knee = bpos(an, s), ank = bpos(an, f); float l1 = len(knee - hip), l2 = len(ank - knee); if (l1 < 1e-3f || l2 < 1e-3f) return;
  V3 d = tgt - hip; float dl0 = len(d); if (dl0 < 1e-4f) return; V3 dn = d * (1.f / dl0); float dl = clampf(dl0, std::fabs(l1 - l2) + 0.03f, (l1 + l2) * 0.998f);
  float a1 = std::acos(clampf((l1 * l1 + dl * dl - l2 * l2) / (2 * l1 * dl), -1.f, 1.f)); V3 pole{0, 0, 1}; pole = pole - dn * dot(pole, dn); pole = norm(pole);
  V3 kneeN = hip + (dn * std::cos(a1) + pole * std::sin(a1)) * l1; V3 ankN = hip + dn * dl;
  rotArc(an, a, t, hip, norm(knee - hip), norm(kneeN - hip));
  V3 k2 = bpos(an, s), f2 = bpos(an, f); rotArc(an, a, s, k2, norm(f2 - k2), norm(ankN - k2));
  // foot: absolute pitch about X (toes down = positive), children keep their bind offsets
  V3 F = bpos(an, f); float cs = std::cos(footPitch * 0.5f), sn = std::sin(footPitch * 0.5f), ca = std::cos(footPitch), sa = std::sin(footPitch);
  for (int b = 0; b < a.nb; b++) { if (!inSub(a, b, f)) continue; float* p = an.pose + b * 7; p[0] = sn; p[1] = 0; p[2] = 0; p[3] = cs;
    if (b != f) { float oy = a.head[b][1] - a.head[f][1], oz = a.head[b][2] - a.head[f][2], ox = a.head[b][0] - a.head[f][0]; p[4] = F.x + ox; p[5] = F.y + oy * ca - oz * sa; p[6] = F.z + oy * sa + oz * ca; } else { p[4] = F.x; p[5] = F.y; p[6] = F.z; } }
}
static bool isBiped(const FigAsset& a) { return figBone(a, "lThigh", -1) >= 0 && figBone(a, "rThigh", -1) >= 0 && figBone(a, "lUp", -1) >= 0 && figBone(a, "hips", -1) >= 0 && figBone(a, "lFoot", -1) >= 0 && figBone(a, "lShin", -1) >= 0 && figBone(a, "spine", -1) >= 0 && figBone(a, "head", -1) >= 0; }
static void procLoco(FigAnim& an, const FigAsset& a, float dt, float as, float w, float ready) {
  int hips = figBone(a, "hips", -1), spn = figBone(a, "spine", -1), nk = figBone(a, "neck", -1), hd = figBone(a, "head", -1);
  int arm[2][2] = {{figBone(a, "lUp", -1), figBone(a, "lFore", -1)}, {figBone(a, "rUp", -1), figBone(a, "rFore", -1)}}; int leg[2][3] = {{figBone(a, "lThigh", -1), figBone(a, "lShin", -1), figBone(a, "lFoot", -1)}, {figBone(a, "rThigh", -1), figBone(a, "rShin", -1), figBone(a, "rFoot", -1)}};
  if (nk < 0) nk = hd; const float side = a.head[leg[0][0]][0] >= 0 ? 1.f : -1.f;   // sign of the left side along X
  float acc = (as - an.lastAs) / std::max(dt, 1e-3f); an.lastAs = as; an.lean += (clampf(acc * 0.012f, -0.1f, 0.14f) - an.lean) * (1 - std::exp(-dt * 6.f));
  float run = smr((as - 2.0f) / 2.4f), slow = sat01(0.28f + as / 1.45f); float S0 = lerpf(0.30f, 0.47f, run) * slow; float duty = lerpf(0.64f, 0.40f, run);
  float rate = as * duty / std::max(0.2f, 2 * S0); rate = std::min(rate, 3.3f); an.ph += rate * dt; an.ph -= std::floor(an.ph); float ph = an.ph;
  V3 rest[2] = {bpos(an, leg[0][2]), bpos(an, leg[1][2])}; float TAUf = 6.2831853f;
  float psh = lerpf(0.06f, 0.2f, run), D0 = lerpf(0.04f, 0.10f, run), D1 = lerpf(0.05f, 0.075f, run); float drop = (D0 + D1 * (0.5f + 0.5f * std::cos(2 * TAUf * (ph - psh)))) * w;
  float fwdL = std::cos(TAUf * ph);   // left leg forward-ness
  figAddMove(an, a, hips, side * 0.022f * std::sin(TAUf * ph) * (1.f - run * 0.5f) * w, -drop, 0.f);
  float pyaw = -side * (0.07f + 0.07f * run) * fwdL * w; { V3 hp = bpos(an, hips); float p[3] = {hp.x, hp.y, hp.z}; figAddRot(an, a, hips, 0, 1, 0, pyaw, p); }
  { V3 sp = bpos(an, spn); float p[3] = {sp.x, sp.y, sp.z}; figAddRot(an, a, spn, 0, 1, 0, -2.5f * pyaw, p); float lean = (0.03f + 0.20f * run + an.lean) * w; figAddRot(an, a, spn, 1, 0, 0, lean, p); figAddRot(an, a, spn, 0, 0, 1, side * 0.025f * std::sin(TAUf * ph + 1.2f) * (1 + run) * w, p);
    V3 np = bpos(an, nk); float q[3] = {np.x, np.y, np.z}; figAddRot(an, a, nk, 1, 0, 0, -0.65f * lean, q); figAddRot(an, a, nk, 0, 1, 0, 1.7f * pyaw, q); }
  float aw = (1.f - 0.8f * ready) * w; float swing = (0.30f + 0.45f * run) * aw;
  for (int sd = 0; sd < 2; sd++) { if (arm[sd][0] < 0 || arm[sd][1] < 0) continue; float f = sd == 0 ? fwdL : -fwdL; float rot = f * swing; V3 up = bpos(an, arm[sd][0]); float p[3] = {up.x, up.y, up.z}; figAddRot(an, a, arm[sd][0], 1, 0, 0, rot, p);
    figAddRot(an, a, arm[sd][0], 0, 0, 1, (sd == 0 ? side : -side) * -0.06f * w, p); float flex = (0.22f + 0.2f * std::fabs(f) + 1.0f * run) * aw + 0.0f; V3 fo = bpos(an, arm[sd][1]); float q[3] = {fo.x, fo.y, fo.z}; figAddRot(an, a, arm[sd][1], 1, 0, 0, -flex, q); }
  float rig = std::max(0.5f, a.height > 0.1f ? a.height / 1.9f : 1.f); (void)rig;
  for (int sd = 0; sd < 2; sd++) { float psi = ph + (sd == 0 ? 0.f : 0.5f); psi -= std::floor(psi); float Sf = S0 * (1.f - 0.25f * run), Sb = S0 * (1.f + 0.2f * run); float z, lift, pitch;
    if (psi < duty) { float u = psi / duty; z = lerpf(Sf, -Sb, u); lift = 0.012f * run; pitch = lerpf(-0.28f, 0.f, smr(u * 4.f)) + 0.5f * smr((u - 0.72f) / 0.28f) * (0.6f + 0.4f * run); }
    else { float u = (psi - duty) / (1 - duty); z = lerpf(-Sb, Sf, smr(u)); lift = (0.055f + 0.17f * run) * std::sin(PI * std::pow(u, 0.75f)); pitch = lerpf(0.5f, -0.25f, smr(u * 1.15f)); }
    float stepX = sd == 0 ? side : -side; (void)stepX; V3 tg = rest[sd] + V3{0, lift, z * w}; V3 cur = bpos(an, leg[sd][2]); tg = cur * (1 - w) + tg * w; legIK(an, a, leg[sd][0], leg[sd][1], leg[sd][2], tg, pitch * w); }
}
float figClipU(const FigAsset& a, const char* clip, float u, FigAnim& an, float w) {
  int ci = a.clipIx(clip); if (ci < 0 || w <= 0.001f) return 0; float tmp[FMAXB * 7]; const FigClip& c = a.clips[ci]; sampleClip(a, c, sat01(u) * (c.frames - 1), tmp); blendPose(an.pose, tmp, w > 1 ? 1 : w, a.nb); return c.hit;
}
static uint32_t rs = 0x9e3779b9u; static float rnd01() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (rs >> 8) * (1.f / 16777216.f); }
void figShatter(FigAnim& an, const FigAsset& a, float power, float dx, float dz) {
  an.shat = true;
  for (int i = 0; i < a.nb; i++) {
    quatR(an.pose + i * 7, an.fR + i * 9); for (int k = 0; k < 3; k++) an.fp[i * 3 + k] = an.pose[i * 7 + 4 + k];
    float ax = rnd01() - .5f, ay = rnd01() - .5f, az = rnd01() - .5f, al = std::sqrt(ax * ax + ay * ay + az * az) + 1e-6f;
    an.fv[i * 3] = (rnd01() - .5f) * power + dx * power * .6f; an.fv[i * 3 + 1] = rnd01() * power * .9f + .4f; an.fv[i * 3 + 2] = (rnd01() - .5f) * power + dz * power * .6f;
    an.fax[i * 3] = ax / al; an.fax[i * 3 + 1] = ay / al; an.fax[i * 3 + 2] = az / al; an.fw[i] = (rnd01() * 4 + 1.5f) * (power > 1.5f ? 1.f : .6f); an.frest[i] = false; an.ffl[i] = .06f + rnd01() * .1f;
  }
}
void figStepFree(FigAnim& an, const FigAsset& a, float dt) {
  if (!an.shat) return;
  for (int i = 0; i < a.nb; i++) {
    if (an.frest[i]) continue; float* v = an.fv + i * 3; float* p = an.fp + i * 3;
    v[1] -= 12 * dt; p[0] += v[0] * dt; p[1] += v[1] * dt; p[2] += v[2] * dt;
    if (p[1] < an.ffl[i]) { p[1] = an.ffl[i]; if (std::fabs(v[1]) < 1.2f) { an.frest[i] = true; continue; } v[1] *= -.28f; v[0] *= .55f; v[2] *= .55f; an.fw[i] *= .5f; }
    float ang = an.fw[i] * dt, c = std::cos(ang), s = std::sin(ang), t = 1 - c, x = an.fax[i * 3], y = an.fax[i * 3 + 1], z = an.fax[i * 3 + 2]; float* R = an.fR + i * 9;
    float m0 = t * x * x + c, m1 = t * x * y - s * z, m2 = t * x * z + s * y, m3 = t * x * y + s * z, m4 = t * y * y + c, m5 = t * y * z - s * x, m6 = t * x * z - s * y, m7 = t * y * z + s * x, m8 = t * z * z + c;
    float n[9] = {m0 * R[0] + m1 * R[3] + m2 * R[6], m0 * R[1] + m1 * R[4] + m2 * R[7], m0 * R[2] + m1 * R[5] + m2 * R[8], m3 * R[0] + m4 * R[3] + m5 * R[6], m3 * R[1] + m4 * R[4] + m5 * R[7], m3 * R[2] + m4 * R[5] + m5 * R[8],
                  m6 * R[0] + m7 * R[3] + m8 * R[6], m6 * R[1] + m7 * R[4] + m8 * R[7], m6 * R[2] + m7 * R[5] + m8 * R[8]};
    memcpy(R, n, 36);
  }
}
void figPoint(const FigAsset&, const float* m, int b, float x, float y, float z, float* o) {
  const float* r = m + b * 12; o[0] = r[0] * x + r[1] * y + r[2] * z + r[3]; o[1] = r[4] * x + r[5] * y + r[6] * z + r[7]; o[2] = r[8] * x + r[9] * y + r[10] * z + r[11];
}
// Build orthonormal model-to-world matrix for a 1-bone Polyfork weapon socketed onto rHand/lHand
void figWeaponMat(const FigAsset& rig, const FigAnim& an, bool left, int wpId, const M4& unitModel, float time, M4& out) {
  int bf = figBone(rig, left ? "lFore" : "rFore", left ? 9 : 13);
  int bh = figBone(rig, left ? "lHand" : "rHand", left ? 10 : 14);
  V3 pf{an.pose[bf * 7 + 4], an.pose[bf * 7 + 5], an.pose[bf * 7 + 6]};
  V3 ph{an.pose[bh * 7 + 4], an.pose[bh * 7 + 5], an.pose[bh * 7 + 6]};
  V3 arm = ph - pf; float al = len(arm);
  arm = al > 1e-4f ? arm * (1.f / al) : V3{0, -0.85f, 0.5f};
  V3 yAxis{0, 1, 0}, zAxis{0, 0, 1}, xAxis{1, 0, 0}, pos = ph; float sc = 1.f;
  if (wpId == 3 || wpId == 4) {  // WP_KITE / WP_ROUND shield strapped to forearm
    pos = ph * 0.62f + pf * 0.38f;
    zAxis = norm(V3{left ? 0.52f : -0.52f, 0.08f, 0.85f});
    yAxis = norm(V3{0, 1.f, 0.15f} - zAxis * dot(V3{0, 1.f, 0.15f}, zAxis));
    xAxis = norm(cross(yAxis, zAxis));
    pos = pos - yAxis * (wpId == 3 ? 0.46f : 0.39f) + zAxis * 0.07f;
    sc = 0.95f;
  } else if (wpId == 2) {  // WP_BOW held in bow hand
    yAxis = norm(V3{0.12f, 0.98f, 0.1f} - arm * (0.35f * arm.y));
    zAxis = norm(V3{0, 0.05f, 1.f} - yAxis * dot(V3{0, 0.05f, 1.f}, yAxis));
    xAxis = norm(cross(yAxis, zAxis));
    pos = ph - yAxis * 0.72f + zAxis * 0.04f;
  } else if (wpId == 7) {  // WP_LANTERN hanging from hand with pendulum sway
    yAxis = norm(V3{-0.08f * std::sin(time * 3.1f), 1.f, -0.07f * std::cos(time * 2.6f)});
    xAxis = norm(cross(yAxis, V3{0, 0, 1}));
    zAxis = cross(xAxis, yAxis);
    pos = ph - yAxis * 0.34f + arm * 0.04f;
    sc = 1.15f;
  } else {  // WP_SWORD (0), WP_GREATSWORD (1), WP_STAFF (5), WP_CLEAVER (6)
    V3 ref = left ? V3{0.92f, 0.25f, 0.1f} : V3{-0.92f, 0.25f, 0.1f};
    if (std::fabs(dot(arm, ref)) > 0.88f) ref = {0, 0, 1};
    yAxis = norm(cross(ref, arm));
    if (yAxis.z < -0.25f && yAxis.y < 0.2f) yAxis = yAxis * -1.f;
    zAxis = norm(arm - yAxis * dot(arm, yAxis));
    xAxis = norm(cross(yAxis, zAxis));
    float grip = wpId == 5 ? 0.62f : (wpId == 1 ? 0.18f : 0.12f);
    pos = ph - yAxis * grip;
    sc = wpId == 1 ? 1.05f : 1.0f;
  }
  M4 loc = ident();
  loc.m[0] = xAxis.x * sc; loc.m[1] = xAxis.y * sc; loc.m[2] = xAxis.z * sc;
  loc.m[4] = yAxis.x * sc; loc.m[5] = yAxis.y * sc; loc.m[6] = yAxis.z * sc;
  loc.m[8] = zAxis.x * sc; loc.m[9] = zAxis.y * sc; loc.m[10] = zAxis.z * sc;
  loc.m[12] = pos.x; loc.m[13] = pos.y; loc.m[14] = pos.z;
  out = mul(unitModel, loc);
}
bool gFigNoBaked = false;
// ------------------------------------------------------------------ drawing
void figDraw(GLuint prog, const FigDraw* d, int n, bool shadowPass) {
  static GLint lM = -1, lB, lD, lH, lC, lF, lDim, lG; static GLuint last = 0;
  if (last != prog) { last = prog; lM = glGetUniformLocation(prog, "uModel"); lB = glGetUniformLocation(prog, "uBones"); lD = glGetUniformLocation(prog, "uDissolve"); lH = glGetUniformLocation(prog, "uDisH"); lC = glGetUniformLocation(prog, "uDisCol"); lF = glGetUniformLocation(prog, "uFlash"); lDim = glGetUniformLocation(prog, "uDim"); lG = glGetUniformLocation(prog, "uGlow"); }
  for (int i = 0; i < n; i++) {
    const FigDraw& f = d[i]; if (!f.a || !f.a->ok) continue; if (shadowPass && !f.shadow) continue; if (f.vm) glDepthRange(0.0, 0.02);
    glUniformMatrix4fv(lM, 1, GL_FALSE, f.model.m); glUniform4fv(lB, f.a->nb * 3, f.mat);
    if (!shadowPass) { glUniform1f(lD, f.dissolve); glUniform1f(lH, f.disH); glUniform3fv(lC, 1, f.disCol); glUniform3fv(lF, 1, f.flash); glUniform1f(lDim, f.dim); glUniform1f(lG, f.glow); }
    glBindVertexArray(f.a->vao); if ((f.noBaked || gFigNoBaked) && f.a->ibo2) { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, f.a->ibo2); glDrawElements(GL_TRIANGLES, f.lantern ? f.a->ni2 : f.a->baseNI2, GL_UNSIGNED_SHORT, nullptr); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, f.a->ibo); }
    else glDrawElements(GL_TRIANGLES, f.lantern ? f.a->ni : f.a->baseNI, GL_UNSIGNED_SHORT, nullptr); if (f.vm) glDepthRange(0.0, 1.0);
  }
}
// ------------------------------------------------------------------ shaders
const char* FIG_VS = R"GLSL(
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec4 aCol; layout(location=3) in vec2 aUv; layout(location=4) in vec4 aMat; layout(location=5) in vec4 aBn;
uniform mat4 uVP; uniform mat4 uModel; uniform vec4 uBones[96];
out vec3 vWp; out vec3 vN; out vec3 vCol; out vec3 vRest; out vec4 vMat; out vec2 vUvS; out float vAO; out float vCav;
vec3 skinP(int b, vec3 x){ return vec3(dot(uBones[b*3].xyz,x)+uBones[b*3].w, dot(uBones[b*3+1].xyz,x)+uBones[b*3+1].w, dot(uBones[b*3+2].xyz,x)+uBones[b*3+2].w); }
vec3 skinN(int b, vec3 n){ return vec3(dot(uBones[b*3].xyz,n), dot(uBones[b*3+1].xyz,n), dot(uBones[b*3+2].xyz,n)); }
void main(){
  int ba = int(aBn.x+.5), bb = int(aBn.y+.5); float w = aBn.z/255.;
  vec3 p = skinP(ba,aPos), q = skinN(ba,aNrm);
  if (w > 0.) { p = mix(p, skinP(bb,aPos), w); q = mix(q, skinN(bb,aNrm), w); }
  vec4 wp = uModel*vec4(p,1.); vWp = wp.xyz; vN = normalize(mat3(uModel)*q);
  vCol = aCol.rgb*aCol.rgb; vAO = aCol.a; vCav = aBn.w/255.*2.-1.; vMat = vec4(aMat.xy, aMat.z*255., aMat.w*2.); vUvS = aUv; vRest = aPos;
  gl_Position = uVP*wp; }
)GLSL";
const char* FIG_SHADOW_VS = R"GLSL(#version 330 core
layout(location=0) in vec3 aPos; layout(location=5) in vec4 aBn; uniform mat4 uVP; uniform mat4 uModel; uniform vec4 uBones[96];
vec3 skinP(int b, vec3 x){ return vec3(dot(uBones[b*3].xyz,x)+uBones[b*3].w, dot(uBones[b*3+1].xyz,x)+uBones[b*3+1].w, dot(uBones[b*3+2].xyz,x)+uBones[b*3+2].w); }
void main(){ int ba = int(aBn.x+.5), bb = int(aBn.y+.5); float w = aBn.z/255.; vec3 p = skinP(ba,aPos); if (w > 0.) p = mix(p, skinP(bb,aPos), w); gl_Position = uVP*(uModel*vec4(p,1.)); }
)GLSL";
const char* FIG_FS_BODY = R"GLSL(
in vec3 vWp; in vec3 vN; in vec3 vCol; in vec3 vRest; in vec4 vMat; in vec2 vUvS; in float vAO; in float vCav; out vec4 oC;
uniform float uDissolve; uniform float uDisH; uniform vec3 uDisCol; uniform vec3 uFlash; uniform float uDim; uniform float uGlow;
float h12(vec2 p){ vec3 p3 = fract(vec3(p.xyx) * .1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float h13(vec3 p3){ p3 = fract(p3 * .1031); p3 += dot(p3, p3.zyx + 31.32); return fract((p3.x + p3.y) * p3.z); }
float n2(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f); return mix(mix(h12(i),h12(i+vec2(1.,0.)),f.x),mix(h12(i+vec2(0.,1.)),h12(i+vec2(1.,1.)),f.x),f.y); }
float n3(vec3 p){ vec3 i=floor(p), f=fract(p); f=f*f*(3.-2.*f);
  return mix(mix(mix(h13(i),h13(i+vec3(1.,0.,0.)),f.x),mix(h13(i+vec3(0.,1.,0.)),h13(i+vec3(1.,1.,0.)),f.x),f.y),
             mix(mix(h13(i+vec3(0.,0.,1.)),h13(i+vec3(1.,0.,1.)),f.x),mix(h13(i+vec3(0.,1.,1.)),h13(i+vec3(1.,1.,1.)),f.x),f.y),f.z); }
float fb3(vec3 p){ return .5*n3(p)+.25*n3(p*2.03+7.1)+.125*n3(p*4.1+3.3); }
void main(){
  float ty = vMat.z; vec3 col = vCol * vAO * (1. + .22*max(vCav,0.)); float rough = vMat.x, metal = vMat.y, h = 0.; vec3 em = vec3(0.);
  vec2 uv = vUvS; vec3 rp = vRest;
  float px = max(fwidth(uv.x), fwidth(uv.y));
  float big = fb3(rp*9.), fine = n3(rp*70.);
  float dz = (rp.y/uDisH)*.72 + n3(rp*17.)*.28;
  if (uDissolve > 0.) { float th = uDissolve*1.06; if (dz < th) discard; em += uDisCol * smoothstep(.07, 0., dz-th) * 7.; }
  float mudK = 1.;
  float fol = 0.;
  if (ty < .5) { col *= .82 + .36*big; h = fine*.0006;
    float gdom = (vCol.g - max(vCol.r, vCol.b)) / max(vCol.g, .02), pdom = (min(vCol.r, vCol.b) - vCol.g) / max(vCol.r, .02);
    fol = max(smoothstep(.04, .22, gdom), smoothstep(.12, .35, pdom));
    if (fol > 0.) {
      float cl = fb3(rp*3.1 + 1.3), lf = n3(rp*31.), lf2 = n3(rp*67. + 5.), lf3 = n3(rp*13. + 9.);
      float fc = (.5 + .85*cl) * (.7 + .55*lf) * (.85 + .3*lf3);
      vec3 fcol = col * fc; fcol = mix(fcol, fcol*vec3(1.28,1.18,.72), smoothstep(.58,.92,lf2)*.55);
      fcol = mix(fcol, fcol*vec3(.7,.85,.7), smoothstep(.55,.2,cl)*.5);
      col = mix(col, fcol, fol); h = mix(h, lf*.028 + lf2*.01 + lf3*.04, fol); rough = mix(rough, .66, fol);
    } }
  else if (ty < 1.5) {
    float f = smoothstep(.0016,.0006,px); float w = sin(uv.x*1570.)*sin(uv.y*1570.);
    float fold = n3(rp*vec3(26.,5.,26.));
    h = w*.0004*f + big*.002 + fold*.006; col *= (.72 + .42*big) * (1. + .14*w*f) * (.8+.4*fold);
  } else if (ty < 2.5) {
    float q = abs(sin(uv.x*98. + 1.5*n3(rp*vec3(3.,9.,3.)))); float pil = pow(q,.7); h = pil*.0034 + fine*.0006 + n3(rp*vec3(30.,6.,30.))*.004;
    col *= (.78+.22*pil) * (.74+.44*big); col *= 1. - .3*smoothstep(.12,.0,q);
    float st = smoothstep(.75,1., sin(uv.y*900.)) * smoothstep(.2,.05,q); col *= 1.+.6*st;
  } else if (ty < 3.5) {
    float f = smoothstep(.0036,.0012,px);
    vec2 g = uv*349.; float row = floor(g.y/3.14159); g.x += mod(row,2.)*1.5708;
    float r = abs(sin(g.x))*abs(sin(g.y));
    h = r*.0024*f; col *= mix(.72, .3+1.0*r, f) * (.75+.5*big);
    float gl = step(.9, h13(floor(rp*240.))); rough = mix(rough, .22, gl*.7);
    float rust = smoothstep(.5,.75, fb3(rp*13.+1.7)); col = mix(col, vec3(.07,.036,.02), rust*.5); metal *= 1.-rust*.5;
  } else if (ty < 4.5) {
    float cr = abs(n3(rp*vec3(60.,24.,60.))-.5)*2.; cr = smoothstep(.0,.22,cr); float fold = n3(rp*vec3(18.,7.,18.));
    h = fine*.0008 + cr*.0007 + fold*.004; col *= (.72+.4*big)*(.9+.1*cr)*(.85+.3*fold); rough -= .12*fine;
  } else if (ty < 5.5) {
    float sc = n3(rp*vec3(34.,260.,34.)), dent = n3(rp*27.);
    h = dent*.0028 + sc*.0004;
    float rust = smoothstep(.46,.66, fb3(rp*17.+3.) + (vMat.w-1.)*.2 + vMat.w*.12*(1.-smoothstep(.2,1.3,rp.y)));
    col *= .74+.42*sc; rough += (sc-.5)*.3 + (dent-.5)*.2;
    col = mix(col, vec3(.085,.042,.022)*(.5+.9*fine), rust*.88); metal *= 1.-rust*.85; rough = mix(rough, .93, rust);
    mudK = .7;
  } else if (ty < 6.5) {
    float ff = smoothstep(.004,.0012,px); float s = mix(.5, n3(rp*vec3(210.,26.,210.)), ff), s2 = n3(rp*vec3(64.,11.,64.)+9.), s3 = n3(rp*19.);
    h = s*.003*ff+s2*.006+s3*.012; col *= (.36 + .5*s2 + .4*s*s) * (.55+.9*s3);
  } else if (ty < 7.5) {
    float gr = n2(vec2(uv.x*260., uv.y*9.)), g2 = n2(vec2(uv.x*60., uv.y*3.)+4.);
    h = gr*.0016; col *= .6+.34*gr+.26*g2;
  } else if (ty < 8.5) {
    float s = fb3(rp*5.), s2 = fb3(rp*23.+5.), s3 = n3(rp*90.);
    h = s*.03 + s2*.01 + s3*.0015; col *= .42+.6*s2+.25*s;
    float lich = smoothstep(.5,.6, fb3(rp*3.7+11.)); col = mix(col, vec3(.035,.05,.025), lich*.7);
    float l2 = smoothstep(.64,.7, fb3(rp*8.1+31.)); col = mix(col, vec3(.2,.2,.17), l2*.4);
    mudK = .4;
  } else if (ty < 9.5) {
    float rd = abs(n2(vec2(uv.x*80., uv.y*13.))-.5)*2., s2 = fb3(rp*14.);
    h = rd*.008 + s2*.008; col *= .45+.5*rd+.4*s2;
  } else if (ty < 10.5) {
    em += col * vMat.w * uGlow; col *= .05; mudK = 0.;
  } else {
    float gr = n2(vec2(uv.x*150., uv.y*18.)); h = gr*.001; col *= .7+.3*big+.12*gr;
    float st = smoothstep(.5,.7,fb3(rp*11.)); col = mix(col, col*vec3(.5,.42,.3), st*.7);
  }
  if (mudK > 0. && ty < 9.5) {
    float mud = vMat.w * mudK * (1.-smoothstep(.02,.62, rp.y + (big-.45)*.5));
    col = mix(col, vec3(.04,.032,.024)*(.7+.6*fine), mud*.8); rough = mix(rough,.95,mud); metal *= 1.-mud*.9;
  }
  col *= uDim; rough = clamp(rough,.05,1.); metal = clamp(metal,0.,1.); em += uFlash;
  vec3 N = normalize(vN); if (!gl_FrontFacing) N = -N;
  { vec3 dp1 = dFdx(vWp), dp2 = dFdy(vWp); float dhx = dFdx(h), dhy = dFdy(h);
    vec3 r1 = cross(dp2, N), r2 = cross(N, dp1); float det = dot(dp1, r1);
    if (abs(det) > 1e-12) { vec3 gr = sign(det) * (dhx*r1 + dhy*r2); N = normalize(abs(det)*N - clamp(gr, -abs(det)*2.5, abs(det)*2.5)); } }
  float sh = shadowAt(vWp, N); vec3 c = shade(col, rough, metal, N, vWp, sh) + em;
  if (fol > 0.) c += col * uMoonCol * fol * .5 * pow(max(dot(-N, uMoonDir), 0.), 1.4) * sh * (.5 + .5*vAO);
  oC = vec4(c, 1.); }
)GLSL";
bool figLoad(FigAsset& a, const char* key) { a = FigAsset(); if (!loadFig(a, key)) { free(a.clipMem); a = FigAsset(); return false; } return true; }
void figFree(FigAsset& a) { if (a.vao) glDeleteVertexArrays(1, &a.vao); if (a.vbo) glDeleteBuffers(1, &a.vbo); if (a.ibo) glDeleteBuffers(1, &a.ibo); if (a.ibo2) glDeleteBuffers(1, &a.ibo2); free(a.clipMem); a = FigAsset(); }
int figBone(const FigAsset& a, const char* name, int fallback) { for (int i = 0; i < a.nb; i++) if (!strcmp(a.bone[i], name)) return i; return fallback; }
