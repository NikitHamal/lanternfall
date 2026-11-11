// Native HUD/UI: 5x7 bitmap font atlas + batched quads (one draw call per frame). Replaces the DOM overlay of the original.
#include "ui.h"
#include "figure.h"
#include "play.h"
#include <vector>
#include <algorithm>
#include "render.h"
#include "shaders.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
namespace ui {
namespace {
const uint8_t FONT[][7] = {  // ASCII 32..: only the glyphs we need, others blank
 /*32 ' '*/{0,0,0,0,0,0,0}, /*!*/{4,4,4,4,4,0,4}, /*"*/{10,10,0,0,0,0,0}, /*#*/{10,10,31,10,31,10,10}, /*$*/{0,0,0,0,0,0,0}, /*%*/{24,25,2,4,8,19,3}, /*&*/{12,18,20,8,21,18,13}, /*'*/{4,4,8,0,0,0,0},
 /*(*/{2,4,8,8,8,4,2}, /*)*/{8,4,2,2,2,4,8}, /***/{0,4,21,14,21,4,0}, /*+*/{0,4,4,31,4,4,0}, /*,*/{0,0,0,0,12,4,8}, /*-*/{0,0,0,31,0,0,0}, /*.*/{0,0,0,0,0,12,12}, /*/*/{1,1,2,4,8,16,16},
 /*0*/{14,17,19,21,25,17,14}, /*1*/{4,12,4,4,4,4,14}, /*2*/{14,17,1,2,4,8,31}, /*3*/{30,1,1,14,1,1,30}, /*4*/{2,6,10,18,31,2,2}, /*5*/{31,16,30,1,1,17,14}, /*6*/{6,8,16,30,17,17,14}, /*7*/{31,1,2,4,8,8,8},
 /*8*/{14,17,17,14,17,17,14}, /*9*/{14,17,17,15,1,2,12}, /*:*/{0,12,12,0,12,12,0}, /*;*/{0,12,12,0,12,4,8}, /*<*/{2,4,8,16,8,4,2}, /*=*/{0,0,31,0,31,0,0}, /*>*/{8,4,2,1,2,4,8}, /*?*/{14,17,1,2,4,0,4},
 /*@*/{0,0,0,0,0,0,0}, /*A*/{14,17,17,31,17,17,17}, /*B*/{30,17,17,30,17,17,30}, /*C*/{14,17,16,16,16,17,14}, /*D*/{30,17,17,17,17,17,30}, /*E*/{31,16,16,30,16,16,31}, /*F*/{31,16,16,30,16,16,16}, /*G*/{14,17,16,23,17,17,15},
 /*H*/{17,17,17,31,17,17,17}, /*I*/{14,4,4,4,4,4,14}, /*J*/{7,2,2,2,2,18,12}, /*K*/{17,18,20,24,20,18,17}, /*L*/{16,16,16,16,16,16,31}, /*M*/{17,27,21,21,17,17,17}, /*N*/{17,25,21,19,17,17,17}, /*O*/{14,17,17,17,17,17,14},
 /*P*/{30,17,17,30,16,16,16}, /*Q*/{14,17,17,17,21,18,13}, /*R*/{30,17,17,30,20,18,17}, /*S*/{15,16,16,14,1,1,30}, /*T*/{31,4,4,4,4,4,4}, /*U*/{17,17,17,17,17,17,14}, /*V*/{17,17,17,17,17,10,4}, /*W*/{17,17,17,21,21,21,10},
 /*X*/{17,17,10,4,10,17,17}, /*Y*/{17,17,10,4,4,4,4}, /*Z*/{31,1,2,4,8,16,31}, /*[*/{14,8,8,8,8,8,14}, /*\*/{16,16,8,4,2,1,1}, /*]*/{14,2,2,2,2,2,14}, /*^*/{4,10,17,0,0,0,0}, /*_*/{0,0,0,0,0,0,31}};
GLuint prog, vao, vbo, ibo, tex; constexpr int MAXQ = 7000; float vb[MAXQ * 4 * 8]; int nq = 0; int SW = 1280, SH = 720; float S = 2;
struct Btn { float x, y, w, h; int id; }; Btn btns[64], prev[64]; int nb = 0, nprev = 0;
void quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, float r, float g, float b, float a) { if (nq >= MAXQ) return; float* v = vb + nq * 32; nq++;
  float d[4][8] = {{x, y, u0, v0, r, g, b, a}, {x + w, y, u1, v0, r, g, b, a}, {x + w, y + h, u1, v1, r, g, b, a}, {x, y + h, u0, v1, r, g, b, a}}; memcpy(v, d, sizeof d); }
struct Col { float r, g, b, a; };
Col hexc(uint32_t h, float a = 1) { return {((h >> 16) & 255) / 255.f, ((h >> 8) & 255) / 255.f, (h & 255) / 255.f, a}; }
void rect(float x, float y, float w, float h, Col c) { quad(x, y, w, h, -1, -1, -1, -1, c.r, c.g, c.b, c.a); }
void frame(float x, float y, float w, float h, Col c, float t = 1) { rect(x, y, w, t, c); rect(x, y + h - t, w, t, c); rect(x, y, t, h, c); rect(x + w - t, y, t, h, c); }
// serif glyph atlas (assets/font.bin, rendered from a TrueType face in the Blender/Python pipeline); falls back to the built-in 5x7 bitmap font
static bool serif = false; static int fCW = 44, fCH = 58, fEM = 40, fBase = 44; static uint8_t fAdv[96];
static const float TRACK = 0.55f;
float tw(const char* s, float sc) { if (!serif) return (float)strlen(s) * 6 * sc; float k = 11 * sc / fEM, w = 0; for (const char* p = s; *p; p++) { int ch = (unsigned char)*p; if (ch < 32 || ch > 127) ch = 63; w += fAdv[ch - 32] * k + TRACK * sc; } return w; }
void text(const char* s, float x, float y, float sc, Col c, int align = 0, bool shadow = true) {
  float w = tw(s, sc); if (align == 1) x -= w / 2; else if (align == 2) x -= w;
  float k = 11 * sc / fEM, ty = y - (fBase - 0.66f * fEM) * k + sc;
  for (int pass = shadow ? 0 : 1; pass < 2; pass++) { float ox = pass == 0 ? std::max(1.f, sc * 0.45f) : 0, oy = ox; Col kc = pass == 0 ? Col{0, 0, 0, c.a * 0.85f} : c; float cx = x;
    for (const char* p = s; *p; p++) {
      if (serif) { int ch = (unsigned char)*p; if (ch < 32 || ch > 127) ch = 63; int i = ch - 32;
        if (ch != ' ') { int col = i % 16, row = i / 16; float aw = fCW * 16.f, ah = fCH * 6.f, u0 = col * fCW / aw, v0 = row * fCH / ah; quad(cx + ox, ty + oy, fCW * k, fCH * k, u0, v0, u0 + fCW / aw, v0 + fCH / ah, kc.r, kc.g, kc.b, kc.a); }
        cx += fAdv[i] * k + TRACK * sc; }
      else { int ch = toupper((unsigned char)*p); if (ch >= 32 && ch < 96 && ch != ' ') { int i = ch - 32, col = i % 16, row = i / 16; float u0 = col * 8 / 128.f, v0 = row * 8 / 64.f; quad(cx + ox, y + oy, 8 * sc, 8 * sc, u0, v0, u0 + 8 / 128.f, v0 + 8 / 64.f, kc.r, kc.g, kc.b, kc.a); } cx += 6 * sc; } } }
}
void fmtn(char* b, size_t n, float v) { int k = (int)v; if (k < 10000) snprintf(b, n, "%d", k); else if (k < 1000000) snprintf(b, n, "%.1fK", k / 1e3); else snprintf(b, n, "%.2fM", k / 1e6); }
int addBtn(float x, float y, float w, float h, int id) { if (nb < 64) btns[nb++] = {x, y, w, h, id}; return id; }
const uint32_t CLSC[NCLS] = {0xc0504a, 0x8a8f95, 0x6aa05a, 0xf0a850, 0x7fc08a, 0x7fb4d6};
void bar(float x, float y, float w, float h, float f, Col fg, Col bg = {0.05f, 0.05f, 0.06f, 0.85f}) { rect(x - 1, y - 1, w + 2, h + 2, {0, 0, 0, 0.8f}); rect(x, y, w, h, bg); rect(x, y, w * clampf(f, 0, 1), h, fg); }
void panelBox(float cx, float cy, float w, float h) { rect(cx - w / 2, cy - h / 2, w, h, {0.047f, 0.05f, 0.07f, 0.95f}); frame(cx - w / 2, cy - h / 2, w, h, hexc(0x6a5a3c)); frame(cx - w / 2 - 2, cy - h / 2 - 2, w + 4, h + 4, {0, 0, 0, 1}); }
void button(float x, float y, float w, float h, const char* label, const char* sub, bool rec, bool dis, int id) {
  rect(x, y, w, h, rec ? Col{0.2f, 0.14f, 0.07f, 1} : Col{0.1f, 0.1f, 0.12f, 1}); frame(x, y, w, h, dis ? hexc(0x3a3226) : (rec ? hexc(0xe0a33c) : hexc(0x6a5a3c)));
  text(label, x + w / 2, y + h / 2 - (sub ? 8 * S : 3.5f * S), S, dis ? hexc(0x6a6252) : hexc(0xf0dcb0), 1); if (sub) text(sub, x + w / 2, y + h / 2 + 2 * S, S * 0.75f, hexc(0x9a917c), 1); if (!dis) addBtn(x, y, w, h, id);
}
void countdown(float cx, float y, float w, const char* label) { if (!G.panelTimer) { text("PAUSED UNTIL YOU CHOOSE", cx, y, S * 0.8f, hexc(0x9a917c), 1); return; } bar(cx - w / 2, y, w, 4 * S * 0.5f, G.panelT / std::max(0.1f, G.panelMax), hexc(0xe0a33c)); char b[64]; snprintf(b, 64, "%s IN %d", label, (int)std::ceil(G.panelT)); text(b, cx, y + 6 * S, S * 0.8f, hexc(0x9a917c), 1); }
void stopTimer() { G.panelTimer = false; }
}  // namespace
bool init() {
  prog = gfx::program(sh::UI_VS, sh::UI_FS, "ui"); uint8_t atlas[128 * 64]; memset(atlas, 0, sizeof atlas); int n = sizeof(FONT) / 7;
  for (int i = 0; i < n; i++) { int col = i % 16, row = i / 16; for (int y = 0; y < 7; y++) for (int x = 0; x < 5; x++) if (FONT[i][y] & (16 >> x)) atlas[(row * 8 + y + 1) * 128 + col * 8 + x + 1] = 255; }
  glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  { char fp[1300]; snprintf(fp, sizeof fp, "%s/font.bin", figAssetDir()); FILE* ff = fopen(fp, "rb"); if (ff) { uint16_t h[4]; if (fread(h, 2, 4, ff) == 4 && fread(fAdv, 1, 96, ff) == 96) { fCW = h[0]; fCH = h[1]; fEM = h[2]; fBase = h[3]; std::vector<uint8_t> px((size_t)fCW * 16 * fCH * 6); if (fread(px.data(), 1, px.size(), ff) == px.size()) { glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, fCW * 16, fCH * 6, 0, GL_RED, GL_UNSIGNED_BYTE, px.data()); serif = true; } } fclose(ff); } }
  if (!serif) glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 128, 64, 0, GL_RED, GL_UNSIGNED_BYTE, atlas);
  GLint filt = serif ? GL_LINEAR : GL_NEAREST; glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenVertexArrays(1, &vao); glBindVertexArray(vao); glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo); glBufferData(GL_ARRAY_BUFFER, sizeof vb, nullptr, GL_STREAM_DRAW);
  static uint32_t ix[MAXQ * 6]; for (int i = 0; i < MAXQ; i++) { uint32_t b = i * 4; uint32_t q[6] = {b, b + 1, b + 2, b, b + 2, b + 3}; memcpy(ix + i * 6, q, sizeof q); }
  glGenBuffers(1, &ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof ix, ix, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 32, nullptr); glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 32, (void*)8); glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 32, (void*)16); glBindVertexArray(0); return true;
}
static void drawHud();
static void drawPanels();
static void drawTitle(); static void drawPlayHud();
void draw(int w, int h, float dt) {
  SW = w; SH = h; S = std::max(1.f, std::round(h / 360.f)); nq = 0; memcpy(prev, btns, sizeof btns); nprev = nb; nb = 0; (void)dt;
  if (pl::active()) drawPlayHud(); else if (G.state == S_TITLE) drawTitle(); else { drawHud(); }
  drawPanels();
  if (G.state == S_WIPE || G.state == S_RUNEND) { float a = G.state == S_RUNEND ? 0.75f : std::min(0.75f, G.stT / 2.5f) * (G.run.flames <= 0 ? 1.1f : 0.8f); rect(0, 0, (float)w, (float)h, {0.01f, 0.01f, 0.02f, std::min(0.85f, a)}); if (G.state == S_WIPE && G.run.flames <= 0 && G.stT > 0.6f) { text("THE LANTERN WENT OUT", w / 2.f, h / 2.f - 10 * S, S * 3, hexc(0xe9b872), 1); } }
  glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDisable(GL_CULL_FACE); glUseProgram(prog); glUniform2f(glGetUniformLocation(prog, "uRes"), (float)w, (float)h); glUniform1i(glGetUniformLocation(prog, "tFont"), 0);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo); glBufferSubData(GL_ARRAY_BUFFER, 0, nq * 32 * 4, vb); glDrawElements(GL_TRIANGLES, nq * 6, GL_UNSIGNED_INT, nullptr); glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
}

static int mode_() { return pl::H.mode; }
static void drawPlayHud() {
  using namespace pl; const Player& p = P; float W_ = (float)SW, cx = SW / 2.f, cy = SH / 2.f; bool free_ = H.view == VW_FREE;
  // damage vignette + directional hit arc
  if (p.vig > 0.02f) { float a = p.vig * 0.38f, t = 22 * S; rect(0, 0, W_, t, {0.7f, 0.05f, 0.03f, a}); rect(0, SH - t, W_, t, {0.7f, 0.05f, 0.03f, a}); rect(0, 0, t, (float)SH, {0.7f, 0.05f, 0.03f, a}); rect(W_ - t, 0, t, (float)SH, {0.7f, 0.05f, 0.03f, a}); }
  if (p.hitK > 0.05f && mode_() == 2) for (int i = -3; i <= 3; i++) { float g = p.hitDir + i * 0.09f, R = 74 * S, x = cx + std::sin(g) * -R * 1.0f, y = cy - std::cos(g) * R; rect(x - 2 * S, y - 2 * S, 4 * S, 4 * S, {1, 0.2f, 0.12f, p.hitK * (1 - std::fabs(i) * 0.2f)}); }
  if (!free_ && !p.down) {
    // reticle: ticks tighten with the draw curve and turn gold in the sweet spot, wobble when over-held
    float r = H.spread * S * 0.55f + 2 * S, tl = 5 * S, th = std::max(1.f, S * 0.8f); bool sw = H.sweet > 0.5f, ov = p.drawing && p.holdT > 0.9f; Col c = sw ? Col{1, 0.82f, 0.25f, 1} : ov ? Col{1, 0.5f, 0.2f, 1} : Col{0.96f, 0.92f, 0.8f, 0.92f};
    rect(cx - r - tl, cy - th / 2 + 0.5f, tl, th, c); rect(cx + r, cy - th / 2 + 0.5f, tl, th, c); rect(cx - th / 2 + 0.5f, cy - r - tl, th, tl, c); rect(cx - th / 2 + 0.5f, cy + r, th, tl, c); rect(cx - S * 0.6f, cy - S * 0.6f, S * 1.2f, S * 1.2f, c);
    if (p.drawing) { // force-draw curve
      float gw = 66 * S, gh = 15 * S, gx = cx - gw / 2, gy = cy + 40 * S; const int N = 30; rect(gx - 2 * S, gy - 3 * S, gw + 4 * S, gh + 6 * S, {0, 0, 0, 0.35f});
      for (int i = 0; i < N; i++) { float u = (i + 0.5f) / N, f = curve(u), h = f * gh, x = gx + gw * i / N; bool on = u <= p.draw, zone = u >= SWEET0; Col k = on ? (zone ? Col{1, 0.82f, 0.25f, 1} : Col{0.95f, 0.78f, 0.45f, 0.95f}) : (zone ? Col{0.55f, 0.45f, 0.15f, 0.6f} : Col{0.35f, 0.32f, 0.28f, 0.55f}); rect(x, gy + gh - h, gw / N - 0.6f * S, h, k); }
      text(sw ? "RELEASE" : ov ? "TIRING - HOLD STEADY" : "DRAW", cx, gy + gh + 6 * S, S * 0.8f, sw ? hexc(0xffd24a) : ov ? hexc(0xff8a4a) : hexc(0xb3a88e), 1); }
    if (p.hmT > 0) { float k = p.hmT / 0.25f, d0 = (7 + (1 - k) * 4) * S; Col hc = p.hmKind > 1.5f ? Col{1, 0.25f, 0.2f, k} : p.hmKind > 0.5f ? Col{1, 0.82f, 0.25f, k} : Col{1, 1, 1, k}; for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int i = 0; i < 4; i++) { float d = d0 + i * 2.2f * S; rect(cx + sx * d - S, cy + sy * d - S, 2 * S, 2 * S, hc); } }
  }
  // bars
  float bx = 10 * S, by = SH - 24 * S; bar(bx, by, 96 * S, 6 * S, p.hp / p.maxHp, p.hp / p.maxHp < 0.33f ? hexc(0xd0483a) : hexc(0x7fb85a)); bar(bx, by + 9 * S, 96 * S, 3 * S, p.st / 100.f, hexc(0xe0c060)); text(mode_() == 2 ? "HEALTH" : "ARCHER", bx, by - 9 * S, S * 0.75f, hexc(0xb3a88e));
  if (mode_() == 2) for (int i = 0; i < 3; i++) { bool on = i < p.lives + 1; rect(bx + 104 * S + i * 8 * S, by - 2 * S, 5 * S, 8 * S, on ? hexc(0xffa24a) : Col{0.2f, 0.18f, 0.16f, 1}); if (on) rect(bx + 104 * S + i * 8 * S + S, by, 3 * S, 4 * S, hexc(0xffe9a0)); }
  // top: mode / wave, score, combo, accuracy
  rect(0, 0, W_, 14 * S, {0, 0, 0, 0.42f}); text(mode_() == 2 ? H.waveS : "PRACTICE RANGE", 6 * S, 3 * S, S, hexc(0xe9c98a)); if (mode_() == 2) { text("FOES", 6 * S + 70 * S, 3 * S, S * 0.85f, hexc(0xb3a88e)); for (int i = 0; i < std::min(H.foesLeft, 24); i++) rect(6 * S + 112 * S + i * 3.2f * S, 4 * S, 2.2f * S, 6 * S, hexc(0xc0483a)); }
  text(H.scoreS, W_ / 2, 2 * S, S * 1.3f, hexc(0xf0dcb0), 1); if (H.combo > 1) text(H.comboS, W_ / 2 + 40 * S, 3 * S, S * 1.1f, hexc(0xffa24a)); text(H.accS, W_ - 6 * S, 3 * S, S * 0.9f, hexc(0xb3a88e), 2);
  for (int i = 0; i < MAXFEED; i++) { const Feed& f = H.feed[i]; if (f.t <= 0 || !f.s[0]) continue; text(f.s, W_ - 6 * S, 18 * S + i * 9 * S, S * 0.85f, hexc(f.col, std::min(1.f, f.t * 1.5f)), 2); }
  // foe health bars (+ boss)
  for (const Foe& f : foes) { const Unit& u = f.u; if (!u.used || u.hp <= 0) continue; float sx, sy; if (u.boss) continue; if (std::hypot(u.x - p.x, u.z - p.z) > 48) continue; if (!gfx::project(u.x, terrH(u.x, u.z) + f.headY + 0.3f, u.z, sx, sy)) continue; float w = (13 + u.radius * 24) * S * 0.8f; bar(sx - w / 2, sy, w, 3 * S, u.hp / u.maxHp, u.elite ? hexc(0xe0a33c) : hexc(0xc0483a)); }
  for (const Foe& f : foes) if (f.u.used && f.u.hp > 0 && f.u.boss) { float bw2 = 220 * S; text(MON[f.u.cls].name, W_ / 2, 17 * S, S * 1.2f, hexc(0xf0c890), 1); bar(W_ / 2 - bw2 / 2, 28 * S, bw2, 5 * S, f.u.hp / f.u.maxHp, hexc(0xb83a30)); break; }
  for (const FloatText& f : fx.ft) if (f.on) { float sx, sy; if (!gfx::project(f.x, f.y, f.z, sx, sy)) continue; float u = f.t / f.dur, a = u < 0.1f ? u / 0.1f : 1 - std::pow(std::max(0.f, (u - 0.5f) / 0.5f), 2.f); text(f.s, sx, sy, S * f.size / 11.f * 0.95f, hexc(f.col, a), 1); }
  // banner + downed + help
  if (H.bannerT > 0 || H.ended) { float a = H.ended ? 1.f : std::min(1.f, H.bannerT * 1.5f); text(H.banner, cx, SH * 0.2f, S * 2.4f, hexc(0xf1dfb6, a), 1); text(H.sub, cx, SH * 0.2f + 26 * S, S * 0.9f, hexc(0xb3a88e, a), 1); }
  if (p.down && !p.over) { rect(0, 0, W_, (float)SH, {0.2f, 0.01f, 0.01f, 0.35f}); text("DOWNED", cx, SH * 0.4f, S * 3.f, hexc(0xe0685a), 1); bar(cx - 50 * S, SH * 0.4f + 36 * S, 100 * S, 4 * S, p.downT / 4.2f, hexc(0xffa24a)); }
  if (p.over) { rect(0, 0, W_, (float)SH, {0, 0, 0, 0.6f}); text("SCORE", cx, SH * 0.42f, S * 1.2f, hexc(0xb3a88e), 1); text(H.scoreS, cx, SH * 0.42f + 14 * S, S * 3.f, hexc(0xf0dcb0), 1); text(H.accS, cx, SH * 0.42f + 44 * S, S, hexc(0xb3a88e), 1); }
  const char* vn = H.view == VW_FIRST ? "[V] FIRST PERSON" : H.view == VW_THIRD ? "[V] THIRD PERSON" : "[V] FREE LOOK - MOUSE ORBITS, WHEEL ZOOMS"; text(vn, W_ - 6 * S, SH - 24 * S, S * 0.8f, hexc(0xe9c98a), 2);
  text(H.assist ? "[G] ARC ASSIST ON" : "[G] ARC ASSIST OFF", W_ - 6 * S, SH - 34 * S, S * 0.7f, hexc(0x9a917c), 2);
  text(mode_() == 2 ? "LMB DRAW/LOOSE  RMB BLOCK  SPACE ROLL  SHIFT SPRINT  R RESTART  ESC MENU" : "LMB DRAW/LOOSE  SPACE ROLL  SHIFT SPRINT  B AUTO-ARCHER  R RESET  ESC MENU", cx, SH - 8 * S, S * 0.65f, hexc(0x6a6252), 1);
}
static void drawTitle() {
  float cx = SW / 2.f, cy = SH / 2.f; rect(0, 0, (float)SW, SH * 0.16f, {0, 0, 0, 0.35f}); rect(0, SH * 0.84f, (float)SW, SH * 0.16f, {0, 0, 0, 0.45f});
  float sc = S * 5; const char* t = "LANTERNFALL"; float pulse = 0.85f + 0.15f * std::sin(gfx::time_ * 1.7f);
  text(t, cx + 0, SH * 0.12f, sc, {0.9f, 0.55f, 0.2f, 0.18f * pulse}, 1, false); text(t, cx, SH * 0.115f, sc, hexc(0xf1dfb6), 1);
  text("A LANTERN-LIT ROAD, AN OLD BOW, AND THE DARK BEYOND", cx, SH * 0.115f + 10 * sc, S, hexc(0xb3a88e), 1);
  { float bw_ = 132 * S, bh = 28 * S, gap = 6 * S, x0 = cx - (3 * bw_ + 2 * gap) / 2, y0 = SH * 0.64f; const char* lb[3] = {"STEWARD", "PRACTICE RANGE", "WAVE HUNT"}; const char* sb[3] = {"[1] THE PARTY FIGHTS ALONE", "[2] YOU ARE THE ARCHER", "[3] FIRST / THIRD PERSON"};
    for (int i = 0; i < 3; i++) { float x = x0 + i * (bw_ + gap); bool rec = i == 1; rect(x, y0, bw_, bh, rec ? Col{0.2f, 0.14f, 0.07f, 1} : Col{0.1f, 0.1f, 0.12f, 1}); frame(x, y0, bw_, bh, rec ? hexc(0xe0a33c) : hexc(0x6a5a3c)); frame(x - 2, y0 - 2, bw_ + 4, bh + 4, {0, 0, 0, 1});
      text(lb[i], x + bw_ / 2, y0 + 5 * S, S * 1.2f, hexc(0xf0dcb0), 1); text(sb[i], x + bw_ / 2, y0 + bh - 9 * S, S * 0.58f, hexc(0x9a917c), 1); addBtn(x, y0, bw_, bh, 100 + i); } }
  char b[96]; if (G.P.bestStage > 0) { snprintf(b, 96, "BEST WAYLIGHT %d   LEVEL %d   EMBERS %d", G.P.bestStage, G.P.bestLevel, G.P.embers); text(b, cx, SH * 0.64f + 38 * S, S * 0.9f, hexc(0x8f8672), 1); }
  text("ARCHER: WASD MOVE  LMB DRAW  RMB BLOCK  SPACE ROLL  V VIEW  G ARC  Q QUALITY  F11 FULLSCREEN  ESC QUIT", cx, SH * 0.93f, S * 0.8f, hexc(0x8f8672), 1); (void)cy;
}
static void drawHud() {
  const Run& r = G.run; float W_ = (float)SW; char b[96], b2[48];
  // top bar
  rect(0, 0, W_, 14 * S, {0, 0, 0, 0.45f}); G.stageName(r.stage, b2); snprintf(b, 96, "%s", b2); text(b, 6 * S, 3 * S, S, hexc(0xe9c98a)); snprintf(b, 96, "STAGE %d", r.stage); text(b, 6 * S + tw(b2, S) + 10 * S, 3 * S, S, hexc(0xb3a88e));
  snprintf(b, 96, "LV %d", r.level); text(b, W_ / 2 - 60 * S, 3 * S, S, hexc(0xf0dcb0), 0); bar(W_ / 2 - 40 * S, 5 * S, 80 * S, 4 * S, r.xp / G.xpNeed(r.level), hexc(0x8fd98a));
  char s1[24]; fmtn(s1, 24, r.silver); snprintf(b, 96, "SILVER %s", s1); text(b, W_ - 6 * S, 3 * S, S, hexc(0xe8c060), 2);
  snprintf(b, 96, "EMBERS %d", G.P.embers); text(b, W_ - 6 * S - tw("SILVER 000000", S) - 8 * S, 3 * S, S, hexc(0xff9a5a), 2);
  for (int i = 0; i < 3; i++) { bool on = i < r.flames; rect(W_ * 0.5f + 70 * S + i * 8 * S, 3 * S, 5 * S, 8 * S, on ? hexc(0xffa24a) : Col{0.2f, 0.18f, 0.16f, 1}); if (on) rect(W_ * 0.5f + 71 * S + i * 8 * S, 5 * S, 3 * S, 4 * S, hexc(0xffe9a0)); }
  snprintf(b, 96, "X%d  %s", G.P.speed, gfx::qualityName[gfx::quality]); text(b, W_ - 6 * S, 16 * S, S * 0.8f, hexc(0x8f8672), 2);
  snprintf(b, 96, "TONICS %d/%d   RELICS %d", r.tonics, r.maxTonics, r.relics); text(b, 6 * S, 16 * S, S * 0.8f, hexc(0x9a917c));
  // flare meter
  float fw = 120 * S, fx0 = W_ / 2 - fw / 2, fy = SH - 62 * S; bool ready = G.flare >= 1; float pl = 0.7f + 0.3f * std::sin(gfx::time_ * 8);
  bar(fx0, fy, fw, 5 * S, G.flare, ready ? Col{1, 0.7f * pl + 0.2f, 0.25f, 1} : hexc(0xb86a2a)); text(ready ? "LANTERN FLARE READY - SPACE" : "LANTERN FLARE", W_ / 2, fy - 9 * S, S * 0.8f, ready ? hexc(0xffd27a) : hexc(0x9a917c), 1);
  // heroes
  float cw = 78 * S, gap = 6 * S, x0 = W_ / 2 - (4 * cw + 3 * gap) / 2, y0 = SH - 52 * S;
  for (int i = 0; i < 4; i++) { const Unit& h = G.heroes[i]; const ClassDef& C = CL[h.cls]; float x = x0 + i * (cw + gap);
    rect(x, y0, cw, 46 * S, {0.047f, 0.05f, 0.07f, 0.82f}); frame(x, y0, cw, 46 * S, h.down ? hexc(0x5a2a26) : hexc(0x4a4030)); rect(x, y0, 3 * S, 46 * S, hexc(CLSC[h.cls]));
    text(C.name, x + 7 * S, y0 + 3 * S, S * 0.72f, h.down ? hexc(0x8a6a60) : hexc(0xf0dcb0)); snprintf(b, 96, "%d/%d", (int)std::max(0.f, h.hp), (int)h.maxHp); text(b, x + cw - 3 * S, y0 + 38 * S, S * 0.65f, hexc(0x9a917c), 2);
    bar(x + 7 * S, y0 + 13 * S, cw - 12 * S, 5 * S, h.hp / h.maxHp, h.hp / h.maxHp < 0.33f ? hexc(0xd0483a) : hexc(0x7fb85a));
    if (h.down) { snprintf(b, 96, "DOWN %ds", (int)std::ceil(h.downT)); text(b, x + 7 * S, y0 + 24 * S, S * 0.9f, hexc(0xe0685a)); }
    else for (int s = 0; s < 3; s++) { float cdv = std::max(0.f, h.cd[s]), mx = C.skills[s].cd * h.cdr; float f = C.skills[s].passive ? 1.f : 1 - clampf(cdv / std::max(0.1f, mx), 0, 1); float sx = x + 7 * S + s * 23 * S; rect(sx - 1, y0 + 22 * S - 1, 21 * S + 2, 12 * S + 2, {0, 0, 0, 0.7f}); rect(sx, y0 + 22 * S, 21 * S, 12 * S, {0.1f, 0.1f, 0.12f, 1}); rect(sx, y0 + 22 * S, 21 * S * f, 12 * S, f >= 1 ? Col{0.55f, 0.4f, 0.16f, 1} : Col{0.28f, 0.22f, 0.12f, 1});
      char ab[8]; snprintf(ab, 8, "%.3s", C.skills[s].name); text(ab, sx + 10.5f * S, y0 + 25 * S, S * 0.7f, f >= 1 ? hexc(0xf0dcb0) : hexc(0x8a8070), 1); }
    if (!h.down) { const Item* g = r.gear[h.cls]; for (int s = 0; s < 3; s++) { Col c = g[s].has ? hexc(GRADES[g[s].g].c) : Col{0.15f, 0.15f, 0.15f, 1}; rect(x + 7 * S + s * 8 * S, y0 + 37 * S, 6 * S, 5 * S, c); } } }
  // monsters: hp bars
  for (const Unit& m : G.mons) { if (!m.used || m.hp <= 0 || !m.seen || m.boss) continue; float sx, sy; if (!gfx::project(m.x, MON[m.cls].hpY + 0.3f, m.z, sx, sy)) continue; float w = (14 + m.radius * 26) * S * 0.8f; bar(sx - w / 2, sy, w, 3 * S, m.hp / m.maxHp, m.elite ? hexc(0xe0a33c) : hexc(0xc0483a));
    if (&m == G.focus) text("FOCUS", sx, sy - 7 * S, S * 0.7f, hexc(0xffd27a), 1); if (m.chillT > 0) text("CHILLED", sx, sy + 5 * S, S * 0.6f, hexc(0x7fb4d6), 1); }
  if (G.bossRef && G.bossRef->hp > 0) { const Unit& b_ = *G.bossRef; float bw2 = 220 * S; text(b_.title, W_ / 2, 22 * S, S * 1.4f, hexc(0xf0c890), 1); bar(W_ / 2 - bw2 / 2, 34 * S, bw2, 6 * S, b_.hp / b_.maxHp, b_.enr ? hexc(0xe0502a) : hexc(0xb83a30)); }
  // floating text
  for (const FloatText& f : fx.ft) if (f.on) { float sx, sy; if (!gfx::project(f.x, f.y, f.z, sx, sy)) continue; float u = f.t / f.dur, a = u < 0.1f ? u / 0.1f : 1 - std::pow(std::max(0.f, (u - 0.5f) / 0.5f), 2.f); text(f.s, sx, sy, S * f.size / 11.f * 0.95f, hexc(f.col, a), 1); }
  // toasts
  for (int i = 0; i < G.nToast; i++) { float a = std::min(1.f, G.toasts[i].t * 2); text(G.toasts[i].s, W_ / 2, 46 * S + i * 11 * S, S * 1.2f * (i == 0 ? 1.f : 0.8f), hexc(G.toasts[i].c, a), 1); }
  text("SPACE FLARE  M MENU  C CAMERA  +/- SPEED  CLICK: FOCUS / RALLY", W_ - 6 * S, SH - 8 * S, S * 0.65f, hexc(0x6a6252), 2);
}
static void drawPanels() {
  const Run& r = G.run; float cx = SW / 2.f, cy = SH / 2.f; char b[128];
  if (G.panel == P_NONE) return; rect(0, 0, (float)SW, (float)SH, {0, 0, 0, 0.5f});
  if (G.panel == P_CARDS) { float pw = 300 * S, ph = 130 * S; panelBox(cx, cy, pw, ph); snprintf(b, 128, "LEVEL %d", r.level); text(b, cx, cy - ph / 2 + 8 * S, S * 2, hexc(0xf1dfb6), 1); text("CHOOSE ONE", cx, cy - ph / 2 + 27 * S, S * 0.9f, hexc(0x9a917c), 1);
    float cw = 88 * S; for (int i = 0; i < G.nOffer; i++) { const Card& c = CARDS[G.offer[i]]; float x = cx - (G.nOffer * cw + (G.nOffer - 1) * 6 * S) / 2 + i * (cw + 6 * S), y = cy - ph / 2 + 38 * S, h = 82 * S; bool rec = i == G.offerRec && G.P.steward;
      rect(x, y, cw, h, rec ? Col{0.14f, 0.1f, 0.06f, 1} : Col{0.08f, 0.08f, 0.1f, 1}); frame(x, y, cw, h, rec ? hexc(0xe0a33c) : hexc(0x4a4030)); addBtn(x, y, cw, h, 200 + i);
      text(c.who < 0 ? "WHOLE PARTY" : CL[c.who].name, x + cw / 2, y + 5 * S, S * 0.8f, c.who < 0 ? hexc(0x9a917c) : hexc(CLSC[c.who]), 1); text(c.name, x + cw / 2, y + 16 * S, S * 1.0f, hexc(0xf0dcb0), 1);
      // wrapped description
      const char* d = c.d; int line = 0; char buf[24]; int maxc = (int)((cw - 8 * S) / (6 * S * 0.8f)); while (*d && line < 6) { int n = 0, last = -1; while (d[n] && n < maxc) { if (d[n] == ' ') last = n; n++; } if (d[n] && last > 0) n = last; memcpy(buf, d, n); buf[n] = 0; text(buf, x + cw / 2, y + 30 * S + line * 7 * S, S * 0.8f, hexc(0xb3a88e), 1); d += n; while (*d == ' ') d++; line++; }
      snprintf(b, 128, "RANK %d OF 3   [%d]", r.cards[G.offer[i]] + 1, i + 1); text(b, x + cw / 2, y + h - 9 * S, S * 0.7f, hexc(0x8a8070), 1); }
    countdown(cx, cy + ph / 2 - 14 * S, 120 * S, "THE STEWARD CHOOSES"); }
  else if (G.panel == P_CHOICE) { float pw = 260 * S, ph = 100 * S; panelBox(cx, cy, pw, ph); char nb[48]; snprintf(b, 128, "WAYLIGHT %d RELIT", r.stage); text(b, cx, cy - ph / 2 + 8 * S, S * 2, hexc(0xf1dfb6), 1);
    const char* d = G.danger(true); snprintf(b, 128, "%s - THE ROAD AHEAD LOOKS %s", G.stageName(r.stage, nb), d); text(b, cx, cy - ph / 2 + 24 * S, S * 0.9f, !strcmp(d, "SAFE") ? hexc(0x8fd98a) : !strcmp(d, "RISKY") ? hexc(0xe0a33c) : hexc(0xe0685a), 1);
    bool def = G.P.steward ? G.choiceRec : G.P.push; button(cx - 112 * S, cy - 4 * S, 108 * S, 26 * S, "PUSH ON", "STAGE HARDER, RICHER [1]", def, false, 210); snprintf(b, 128, "PICKED-OVER GROUND: 80%% REWARDS [2]"); button(cx + 4 * S, cy - 4 * S, 108 * S, 26 * S, "HOLD HERE", "80% REWARDS [2]", !def, false, 211);
    countdown(cx, cy + ph / 2 - 20 * S, 120 * S, def ? "PUSHING ON" : "HOLDING"); }
  else if (G.panel == P_RUNEND) { float pw = 320 * S, ph = 190 * S; panelBox(cx, cy, pw, ph); const LastRun& L = G.last; text("THE LANTERN FALLS", cx, cy - ph / 2 + 7 * S, S * 2, hexc(0xf1dfb6), 1); text("THE MOOR TAKES BACK THE DARK", cx, cy - ph / 2 + 22 * S, S * 0.8f, hexc(0x9a917c), 1);
    snprintf(b, 128, "FURTHEST WAYLIGHT %d   LEVEL %d   TIME %dM %02ds   FOES %d   EMBERS +%d", L.stage, L.level, (int)(L.time / 60), (int)L.time % 60, L.kills, L.embers); text(b, cx, cy - ph / 2 + 33 * S, S * 0.75f, hexc(0xf0dcb0), 1);
    snprintf(b, 128, "SPEND EMBERS - YOU HOLD %d", G.P.embers); text(b, cx, cy - ph / 2 + 45 * S, S * 0.9f, hexc(0xffb07a), 1);
    for (int i = 0; i < NBOON; i++) { const BoonDef& bd = BOONS[i]; float y = cy - ph / 2 + 56 * S + i * 13 * S; int cost = G.boonCost(i); bool max = G.boon(i) >= bd.max || (bd.unlock && G.P.unlocked[bd.cls]); snprintf(b, 128, "[%d] %s %s", i + 1, bd.name, bd.unlock ? "" : (std::string(" ") + std::to_string(G.boon(i))).c_str()); text(b, cx - pw / 2 + 8 * S, y, S * 0.85f, hexc(0xf0dcb0)); text(bd.d, cx - pw / 2 + 8 * S + 150 * S, y, S * 0.7f, hexc(0x8a8070));
      snprintf(b, 128, max ? "DONE" : "%d EMBERS", cost); bool dis = max || G.P.embers < cost; float bx = cx + pw / 2 - 62 * S; rect(bx, y - 2 * S, 54 * S, 10 * S, {0.1f, 0.1f, 0.12f, 1}); frame(bx, y - 2 * S, 54 * S, 10 * S, dis ? hexc(0x3a3226) : hexc(0xe0a33c)); text(b, bx + 27 * S, y, S * 0.8f, dis ? hexc(0x6a6252) : hexc(0xf0dcb0), 1); if (!dis) addBtn(bx, y - 2 * S, 54 * S, 10 * S, 300 + i); }
    button(cx - 50 * S, cy + ph / 2 - 26 * S, 100 * S, 14 * S, "LIGHT ANOTHER LANTERN", nullptr, true, false, 220); countdown(cx, cy + ph / 2 - 8 * S, 120 * S, "A NEW RUN BEGINS"); }
  else if (G.panel == P_MENU) { float pw = 330 * S, ph = 200 * S; panelBox(cx, cy, pw, ph); text("THE CAMP", cx, cy - ph / 2 + 7 * S, S * 2, hexc(0xf1dfb6), 1); snprintf(b, 128, "SILVER %d   (SPENT ON CAMP UPGRADES)", (int)r.silver); text(b, cx, cy - ph / 2 + 22 * S, S * 0.9f, hexc(0xe8c060), 1);
    for (int i = 0; i < NCAMP; i++) { float y = cy - ph / 2 + 36 * S + i * 16 * S; int cost = G.campCost(i); bool dis = r.silver < cost; snprintf(b, 128, "[%d] %s  LV %d", i + 1, CAMPS[i].name, r.camp[i]); text(b, cx - pw / 2 + 8 * S, y, S, hexc(0xf0dcb0)); text(CAMPS[i].d, cx - pw / 2 + 8 * S, y + 8 * S, S * 0.7f, hexc(0x8a8070));
      snprintf(b, 128, "%d SILVER", cost); float bx = cx + pw / 2 - 66 * S; rect(bx, y, 58 * S, 12 * S, {0.1f, 0.1f, 0.12f, 1}); frame(bx, y, 58 * S, 12 * S, dis ? hexc(0x3a3226) : hexc(0xe0a33c)); text(b, bx + 29 * S, y + 2.5f * S, S * 0.85f, dis ? hexc(0x6a6252) : hexc(0xf0dcb0), 1); if (!dis) addBtn(bx, y, 58 * S, 12 * S, 400 + i); }
    float y = cy - ph / 2 + 104 * S; text("PARTY", cx - pw / 2 + 8 * S, y, S, hexc(0x9a917c));
    for (int i = 0; i < 4; i++) { const Unit& h = G.heroes[i]; const Item* g = r.gear[h.cls]; float yy = y + 10 * S + i * 11 * S; snprintf(b, 128, "%s", CL[h.cls].name); text(b, cx - pw / 2 + 8 * S, yy, S * 0.85f, hexc(CLSC[h.cls]));
      for (int s = 0; s < 3; s++) { snprintf(b, 128, "%s", g[s].has ? g[s].name : "-"); for (char* c = b; *c; c++) *c = (char)toupper(*c); text(b, cx - pw / 2 + 70 * S + s * 86 * S, yy, S * 0.65f, g[s].has ? hexc(GRADES[g[s].g].c) : hexc(0x5a5240)); } }
    snprintf(b, 128, "[S] STEWARD AUTOPILOT: %s    [P] DEFAULT AFTER WAYLIGHT: %s    [Q] QUALITY: %s", G.P.steward ? "ON" : "OFF", G.P.push ? "PUSH" : "HOLD", gfx::qualityName[gfx::quality]); text(b, cx, cy + ph / 2 - 26 * S, S * 0.72f, hexc(0xb3a88e), 1);
    snprintf(b, 128, "[V] VOLUME %d%%  [N] MUTE %s     [F11] FULLSCREEN    ESC / M: CLOSE", (int)(G.P.vol * 100), G.P.mute ? "ON" : "OFF"); text(b, cx, cy + ph / 2 - 14 * S, S * 0.72f, hexc(0x8a8070), 1); }
}
static void act(int id) {
  G.panelTimer = G.panelTimer && false; Run& r = G.run;
  if (id >= 200 && id < 203) { int i = id - 200; if (i < G.nOffer) { int c = G.offer[i]; G.panel = P_NONE; G.takeCard(c); char b[64]; snprintf(b, 64, "%s", CARDS[c].name); G.toast(b, 0x8fd98a); if (G.pending) G.openPending(); } }
  else if (id == 100) G.beginRun(false); else if (id == 101) pl::pendingMode = pl::M_RANGE; else if (id == 102) pl::pendingMode = pl::M_WAVES;
  else if (id == 210) G.choose(true); else if (id == 211) G.choose(false);
  else if (id >= 300 && id < 310) { if (G.buyBoon(id - 300)) G.save(); }
  else if (id == 220) { G.stewardBoons(); G.panel = P_NONE; G.newRun(); gfx::clearWaylights(); gfx::resetCamera(); }
  else if (id >= 400 && id < 404) G.buyCamp(id - 400, false);
  else if (id == 100) { G.beginRun(false); }
  (void)r;
}
bool click(float x, float y) { for (int i = 0; i < nprev; i++) { const Btn& b = prev[i]; if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h) { act(b.id); return true; } } return G.panel != P_NONE; }
bool key(int c) {
  if (G.state == S_TITLE) { if (c == 13 || c == ' ' || c == '1') { G.beginRun(false); return true; } if (c == '2') { pl::pendingMode = pl::M_RANGE; return true; } if (c == '3') { pl::pendingMode = pl::M_WAVES; return true; } return false; }
  switch (G.panel) {
    case P_CARDS: if (c >= '1' && c <= '3') { act(200 + c - '1'); return true; } return true;
    case P_CHOICE: if (c == '1') { act(210); return true; } if (c == '2') { act(211); return true; } return true;
    case P_RUNEND: if (c == 13) { act(220); return true; } if (c >= '1' && c <= '7') { act(300 + c - '1'); return true; } return true;
    case P_MENU: if (c >= '1' && c <= '4') act(400 + c - '1'); else if (c == 'S') G.P.steward = !G.P.steward; else if (c == 'P') G.P.push = !G.P.push; else if (c == 'N') { G.P.mute = !G.P.mute; audio::setVolume(G.P.vol, G.P.mute); } else if (c == 'V') { G.P.vol = G.P.vol >= 0.99f ? 0.2f : G.P.vol + 0.2f; audio::setVolume(G.P.vol, G.P.mute); } else if (c == 27 || c == 'M') { G.panel = P_NONE; G.save(); } return c != 'Q';
    default: break;
  }
  return false;
}
}  // namespace ui
