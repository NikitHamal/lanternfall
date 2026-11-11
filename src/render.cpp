// Renderer: HDR scene (MSAA) -> AO + height fog + point-light in-scatter -> 4-level bloom -> DOF + AgX + grade (ported from 40-post.js),
// world (terrain/grass/sky/props/waylights), camera, lights and unit visuals. Everything is instanced; per-frame work uses static buffers only.
#include "render.h"
#include "shaders.h"
#include "meshes.h"
#include "figure.h"
#include <cmath>
#include <string>
#include <cstdio>
namespace gfx {
int winW = 1280, winH = 720, quality = 2, W = 2, H = 2, drawCalls = 0; bool closeView = false; float uiDim = 0, time_ = 0; const char* gArgv0 = nullptr; const char* qualityName[4] = {"LOW", "MEDIUM", "HIGH", "ULTRA"};
namespace {
struct Q { int msaa; float grass; bool ao, dof, shadow; float cap; };
const Q QS[4] = {{0, 0.35f, false, false, false, 1.2e6f}, {2, 0.65f, true, false, true, 2.1e6f}, {4, 1.0f, true, true, true, 3.7e6f}, {4, 1.0f, true, true, true, 8.3e6f}};
GLuint pMesh, pShadow, pTerr, pGrass, pSky, pPart, pMark, pFog, pDown, pFinal, vaoEmpty;
GLuint fboMS, texMSc, texMSd, fboScene, texScene, texDepth, fboA, texA, fboB[4], texB[4], fboSh, texSh; int bw[4], bh[4]; int curMsaa = -1;
Mesh terrNear, terrFar; GLuint grassVao, grassVbo, grassIbo, grassInst; int grassCount = 0, grassIdx = 15;
GLuint quadVao, quadVbo, partVao[2], partInst[2], markVao, markInst;
constexpr int SHS = 2048;
struct Cur { V3 fog, gA, gB, mud, g1, g2, g3, mote; float dens = 0.016f, wet = 0.4f, grassH = 1, moon = 1, cloud = 0.7f, drizzle = 0; } cur;
int reachIdx = 0; const Reach* tgt = &REACHES[0];
const V3 MOON_DIR = norm({0.50f, 0.40f, -0.77f});
// camera
struct CamS { float fx = 0, fz = 0, zoom = 1, yaw = 0, vfx = 0, vfz = 0, vzoom = 0; bool init = false; } camS;
V3 camPos, camTgt; M4 viewM, projM, vpM, invVP, camW, invProj; float camFocus = 18, camAperture = 0.6f; float alphaI = 1, fogPull = 0, camTime = 0;
// lights
struct Lit { V3 moonCol, hemiSky, hemiGnd, fillCol, fillDir; float pl[4][4]; float plc[4][4]; M4 shadowVP; V3 fogLP[4], fogLC[4]; } lit;
struct Way { bool used = false, lit = false; float x = 0, z = 0, glow = 0, fl = 1; }; Way ways[8];
// chunks
constexpr int CH = 20, MAXCH = 14, MAXPI = 140; struct PI_ { uint8_t k, v; float x, y, z, ry, s, rx; }; struct Chunk { bool used = false; int key = 0; int n = 0; PI_ it[MAXPI]; } chunks[MAXCH]; int chunkBase = 0; bool propsDirty = true;
Inst propInst[NPK][5][256]; int propN[NPK][5]; 
struct PInst { float x, y, z, s, r, g, b, a, vx, vy, vz, kind; }; PInst pbuf[2][MAXP + 64]; int pn[2];
struct MInst { float cx, cz, R, y, r, g, b, k0, prog, thick, alpha, kind; }; MInst mbuf[160]; int mn;

std::string src2(const char* a, const char* b = "", const char* c = "", const char* d = "") { return std::string("#version 330 core\n") + a + b + c + d; }
M4 lookView(V3 eye, V3 t) { return rigidInverse(camWorld(eye, t)); }
void setM(GLuint p, const char* n, const M4& m) { glUniformMatrix4fv(glGetUniformLocation(p, n), 1, GL_FALSE, m.m); }
void set3(GLuint p, const char* n, V3 v) { glUniform3f(glGetUniformLocation(p, n), v.x, v.y, v.z); }
void set1(GLuint p, const char* n, float v) { glUniform1f(glGetUniformLocation(p, n), v); }
void seti(GLuint p, const char* n, int v) { glUniform1i(glGetUniformLocation(p, n), v); }
void setLit(GLuint p) {
  glUseProgram(p); set3(p, "uCam", camPos); set3(p, "uMoonDir", MOON_DIR); set3(p, "uMoonCol", lit.moonCol); set3(p, "uHemiSky", lit.hemiSky); set3(p, "uHemiGnd", lit.hemiGnd); set3(p, "uFillDir", lit.fillDir); set3(p, "uFillCol", lit.fillCol);
  glUniform4fv(glGetUniformLocation(p, "uPL"), 4, &lit.pl[0][0]); glUniform4fv(glGetUniformLocation(p, "uPLC"), 4, &lit.plc[0][0]); setM(p, "uShadowVP", lit.shadowVP); seti(p, "uShadowTex", 5); set1(p, "uShadowOn", QS[quality].shadow ? 1.f : 0.f); setM(p, "uVP", vpM);
}
void texAlloc(GLuint& t, int w, int h, GLenum ifmt, GLenum fmt, GLenum type, GLint filter) { if (!t) glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t); glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, nullptr); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); }
void fboAttach(GLuint& f, GLenum att, GLenum target, GLuint tex) { if (!f) glGenFramebuffers(1, &f); glBindFramebuffer(GL_FRAMEBUFFER, f); glFramebufferTexture2D(GL_FRAMEBUFFER, att, target, tex, 0); }
void buildTerrain(Mesh& m, float w, float d, int sx, int sz) {
  std::vector<float> v; v.reserve((sx + 1) * (sz + 1) * 2); for (int j = 0; j <= sz; j++) for (int i = 0; i <= sx; i++) { v.push_back(-w / 2 + w * i / sx); v.push_back(-d / 2 + d * j / sz); }
  std::vector<uint32_t> ix; ix.reserve(sx * sz * 6); for (int j = 0; j < sz; j++) for (int i = 0; i < sx; i++) { uint32_t a = j * (sx + 1) + i, b = a + 1, c = a + sx + 1, e = c + 1; ix.insert(ix.end(), {a, c, b, b, c, e}); }
  glGenVertexArrays(1, &m.vao); glBindVertexArray(m.vao); glGenBuffers(1, &m.vbo); glBindBuffer(GL_ARRAY_BUFFER, m.vbo); glBufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STATIC_DRAW);
  glGenBuffers(1, &m.ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, ix.size() * 4, ix.data(), GL_STATIC_DRAW); m.nIdx = (int)ix.size(); glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr); glBindVertexArray(0);
}
void buildGrass() {
  Rng r(4242); const float TX = 72, TZ = 48; const int NC = 8600; std::vector<float> inst; inst.reserve(NC * 14 * 7);
  for (int c = 0; c < NC; c++) {
    float cx = r() * TX, cz = r() * TZ, kind = r(); bool tall = kind < 0.3f, heath = kind > 0.72f; int n = tall ? 14 + (int)(r() * 14) : (heath ? 16 + (int)(r() * 10) : 7 + (int)(r() * 8));
    float rad = tall ? 0.1f + r() * 0.22f : (heath ? 0.16f + r() * 0.2f : 0.18f + r() * 0.3f), hue = r(); r();
    for (int b = 0; b < n; b++) { float a = r() * TAU, d = std::sqrt(r()) * rad, bx = cx + std::cos(a) * d, bz = cz + std::sin(a) * d;
      float h = tall ? 0.34f + r() * 0.5f : (heath ? (0.12f + r() * 0.14f) * (1 - 0.5f * d / rad) : 0.1f + r() * 0.2f), ang = tall ? a + (r() - 0.5f) * 1.2f : r() * TAU, lean = tall ? 0.15f + d / rad * 0.75f + r() * 0.3f : (heath ? d / rad * 0.9f : r() * 0.8f), wd = tall ? 0.011f + r() * 0.008f : (heath ? 0.02f + r() * 0.012f : 0.012f + r() * 0.01f);
      float v[7] = {bx, bz, h, ang, lean, heath ? -0.2f - hue * 0.6f : (tall ? 0.55f + hue * 0.45f : hue * 0.5f), wd}; for (float f : v) inst.push_back(f); } }
  grassCount = (int)(inst.size() / 7);
  float pv[21]; const float T[3] = {0, 0.36f, 0.7f}; for (int s = 0; s < 3; s++) { pv[s * 6 + 0] = -1; pv[s * 6 + 1] = T[s]; pv[s * 6 + 2] = 0; pv[s * 6 + 3] = 1; pv[s * 6 + 4] = T[s]; pv[s * 6 + 5] = 0; } pv[18] = 0; pv[19] = 1; pv[20] = 0;
  const uint16_t id[15] = {0, 1, 2, 2, 1, 3, 2, 3, 4, 4, 3, 5, 4, 5, 6};
  glGenVertexArrays(1, &grassVao); glBindVertexArray(grassVao); glGenBuffers(1, &grassVbo); glBindBuffer(GL_ARRAY_BUFFER, grassVbo); glBufferData(GL_ARRAY_BUFFER, sizeof pv, pv, GL_STATIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
  glGenBuffers(1, &grassIbo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grassIbo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof id, id, GL_STATIC_DRAW);
  glGenBuffers(1, &grassInst); glBindBuffer(GL_ARRAY_BUFFER, grassInst); glBufferData(GL_ARRAY_BUFFER, inst.size() * 4, inst.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 28, nullptr); glVertexAttribDivisor(1, 1); glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 28, (void*)12); glVertexAttribDivisor(2, 1); glBindVertexArray(0);
}
void blendTo(float k) {
  const Reach& t = *tgt; auto L = [&](V3& a, V3 b) { a = mix3(a, b, k); };
  L(cur.fog, V3{t.fog[0], t.fog[1], t.fog[2]}); cur.dens = lerpf(cur.dens, t.dens, k); L(cur.gA, C(t.gA)); L(cur.gB, C(t.gB)); L(cur.mud, C(t.mud)); cur.wet = lerpf(cur.wet, t.wet, k); L(cur.g1, C(t.gr[0])); L(cur.g2, C(t.gr[1])); L(cur.g3, C(t.gr[2]));
  cur.grassH = lerpf(cur.grassH, t.grassH, k); cur.moon = lerpf(cur.moon, t.moon, k); cur.cloud = lerpf(cur.cloud, t.cloud, k); cur.drizzle = lerpf(cur.drizzle, t.drizzle, k); L(cur.mote, V3{t.mote[0], t.mote[1], t.mote[2]});
}
void allocTargets() {
  const Q& q = QS[quality]; float s = std::min(1.f, std::sqrt(q.cap / std::max(1.f, (float)winW * winH))); W = std::max(2, (int)std::round(winW * s)); H = std::max(2, (int)std::round(winH * s));
  texAlloc(texScene, W, H, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR); texAlloc(texDepth, W, H, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, GL_NEAREST);
  fboAttach(fboScene, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texScene); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texDepth, 0);
  texAlloc(texA, W, H, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR); fboAttach(fboA, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texA);
  int w = W, h = H; for (int i = 0; i < 4; i++) { w = std::max(2, w >> 1); h = std::max(2, h >> 1); bw[i] = w; bh[i] = h; texAlloc(texB[i], w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR); fboAttach(fboB[i], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texB[i]); }
  curMsaa = q.msaa;
  if (q.msaa > 0) { if (!texMSc) glGenTextures(1, &texMSc); if (!texMSd) glGenTextures(1, &texMSd);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texMSc); glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, q.msaa, GL_RGBA16F, W, H, GL_TRUE); glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texMSd); glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, q.msaa, GL_DEPTH_COMPONENT32F, W, H, GL_TRUE);
    fboAttach(fboMS, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, texMSc); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, texMSd, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { fprintf(stderr, "MSAA FBO incomplete, falling back\n"); curMsaa = 0; } }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
}  // namespace

GLuint program(const char* vs, const char* fs, const char* name) {
  auto cs = [&](GLenum t, const char* s) { GLuint id = glCreateShader(t); glShaderSource(id, 1, &s, nullptr); glCompileShader(id); GLint ok = 0; glGetShaderiv(id, GL_COMPILE_STATUS, &ok); if (!ok) { char log[2048]; glGetShaderInfoLog(id, 2048, nullptr, log); fprintf(stderr, "shader %s %s error:\n%s\n", name, t == GL_VERTEX_SHADER ? "VS" : "FS", log); } return id; };
  GLuint v = cs(GL_VERTEX_SHADER, vs), f = cs(GL_FRAGMENT_SHADER, fs), p = glCreateProgram(); glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p); GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) { char log[2048]; glGetProgramInfoLog(p, 2048, nullptr, log); fprintf(stderr, "link %s error:\n%s\n", name, log); } glDeleteShader(v); glDeleteShader(f); return p;
}
bool init(int q) {
  quality = std::min(3, std::max(0, q));
  const char* fsHead = "";
  pMesh = program(src2(sh::MESH_VS + 18).c_str(), src2(sh::LIGHT, sh::MESH_FS_BODY).c_str(), "mesh");
  pShadow = program(sh::SHADOW_VS, sh::SHADOW_FS, "shadow");
  pFig = program(src2(FIG_VS).c_str(), src2(sh::LIGHT, FIG_FS_BODY).c_str(), "figure"); pFigShadow = program(FIG_SHADOW_VS, sh::SHADOW_FS, "figshadow"); if (!figInit(gArgv0)) fprintf(stderr, "figure assets missing: units will not be drawn\n");
  pTerr = program(src2(sh::TERR, sh::TERR_VS_BODY).c_str(), src2(sh::LIGHT, sh::TERR, sh::TERR_FS_BODY).c_str(), "terrain");
  pGrass = program(src2(sh::TERR, sh::GRASS_VS_BODY).c_str(), src2(sh::LIGHT, sh::GRASS_FS_BODY).c_str(), "grass");
  pSky = program(sh::SKY_VS, sh::SKY_FS, "sky"); pPart = program(sh::PART_VS, sh::PART_FS, "particles"); pMark = program(sh::MARK_VS, sh::MARK_FS, "marks");
  pFog = program(sh::FS_VS, sh::FOG_FS, "fog"); pDown = program(sh::FS_VS, sh::DOWN_FS, "down"); pFinal = program(sh::FS_VS, sh::FINAL_FS, "final"); (void)fsHead;
  glGenVertexArrays(1, &vaoEmpty); buildMeshes(); buildTerrain(terrNear, 180, 110, 180, 110); buildTerrain(terrFar, 1100, 800, 138, 100); buildGrass();
  const float qv[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
  glGenVertexArrays(1, &quadVao); glBindVertexArray(quadVao); glGenBuffers(1, &quadVbo); glBindBuffer(GL_ARRAY_BUFFER, quadVbo); glBufferData(GL_ARRAY_BUFFER, sizeof qv, qv, GL_STATIC_DRAW); glBindVertexArray(0);
  for (int i = 0; i < 2; i++) { glGenVertexArrays(1, &partVao[i]); glBindVertexArray(partVao[i]); glBindBuffer(GL_ARRAY_BUFFER, quadVbo); glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
    glGenBuffers(1, &partInst[i]); glBindBuffer(GL_ARRAY_BUFFER, partInst[i]); glBufferData(GL_ARRAY_BUFFER, sizeof(pbuf[0]), nullptr, GL_DYNAMIC_DRAW);
    for (int a = 0; a < 3; a++) { glEnableVertexAttribArray(1 + a); glVertexAttribPointer(1 + a, 4, GL_FLOAT, GL_FALSE, sizeof(PInst), (void*)(intptr_t)(a * 16)); glVertexAttribDivisor(1 + a, 1); } }
  glGenVertexArrays(1, &markVao); glBindVertexArray(markVao); glBindBuffer(GL_ARRAY_BUFFER, quadVbo); glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
  glGenBuffers(1, &markInst); glBindBuffer(GL_ARRAY_BUFFER, markInst); glBufferData(GL_ARRAY_BUFFER, sizeof(mbuf), nullptr, GL_DYNAMIC_DRAW); for (int a = 0; a < 3; a++) { glEnableVertexAttribArray(1 + a); glVertexAttribPointer(1 + a, 4, GL_FLOAT, GL_FALSE, sizeof(MInst), (void*)(intptr_t)(a * 16)); glVertexAttribDivisor(1 + a, 1); } glBindVertexArray(0);
  glGenTextures(1, &texSh); glBindTexture(GL_TEXTURE_2D, texSh); glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHS, SHS, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  { float bc[4] = {1, 1, 1, 1}; glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bc); } glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  fboAttach(fboSh, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texSh); glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE); glBindFramebuffer(GL_FRAMEBUFFER, 0);
  setReach(0, true); resize(winW, winH); return true;
}
void shutdown() {}
void resize(int w, int h) { winW = std::max(2, w); winH = std::max(2, h); allocTargets(); }
void setQuality(int q) { quality = ((q % 4) + 4) % 4; allocTargets(); }
void setReach(int i, bool instant) { reachIdx = i % 5; tgt = &REACHES[reachIdx]; if (instant) blendTo(1); }
void resetCamera() { camS.init = false; }
void addWaylight(float x, bool litNow) { for (auto& w : ways) if (!w.used) { w.used = true; w.x = x; w.z = pathZ(x) - 3.1f; w.lit = litNow; w.glow = litNow ? 1.f : 0.f; return; } }
void lightLastWaylight() { Way* b = nullptr; for (auto& w : ways) if (w.used && (!b || w.x > b->x)) b = &w; if (!b) return; b->lit = true; fogPull = 1.6f; V3 p{b->x + 0.22f, 3.1f, b->z + 0.7f};
  V3 fc{3, 1.6f, 0.4f}; fx.embers(p.x, p.y, p.z, 60, &fc, 0.5f, 2); fx.sparks(p.x, p.y, p.z, 30, {3.5f, 2, 0.7f}, 5); fx.flash(p.x, p.y, p.z, {1, 0.7f, 0.35f}, 320, 1.4f); fx.ring(b->x, b->z, 0.5f, 16, 1.6f, {2, 1.2f, 0.5f}); G.screenFlash = std::max(G.screenFlash, 0.22f); }
void clearWaylights() { for (auto& w : ways) w.used = false; }
void shiftWorld(float dx) { for (auto& w : ways) if (w.used) w.x -= dx; for (auto& c : chunks) c.used = false; propsDirty = true; chunkBase = 0; camS.fx -= dx; }

// ---------- chunks ----------
static void buildChunk(Chunk& c, int key, int ci) {
  c.used = true; c.key = key; c.n = 0; Rng r((uint32_t)(G.seed * 7919u + (uint32_t)(key * 104729) + 17u)); float x0 = ci * (float)CH; const float* R = tgt->props;  // rock standing tree wall cairn fence
  auto put = [&](int k, int var, float x, float z, float s, float ry, float sink, float rx = 0) { if (c.n >= MAXPI) return; { float off = z - pathZ(x); if (off > 0 && off < 26 && (k == PK_TREE || k == PK_STAND || k == PK_WALL || k == PK_CAIRN || (k == PK_ROCK && s > 1.f))) return; } c.it[c.n++] = {(uint8_t)k, (uint8_t)var, x, terrH(x, z) - sink, z, ry, s, rx}; };
  auto side = [&]() { return r() < 0.62f ? -1.f : 1.f; }; auto zAt = [&](float x, float lo, float hi, float s) { return pathZ(x) + s * (lo + std::pow(r(), 1.6f) * (hi - lo)); };
  auto count = [&](float w) { float f = w * (0.6f + r() * 0.8f); int n = (int)f; return n + (r() < f - n ? 1 : 0); };
  auto var = [&](int k) { return (int)(r() * PROP_VARIANTS[k]) % PROP_VARIANTS[k]; };
  for (int i = count(R[0] * 1.4f); i-- > 0;) { float x = x0 + r() * CH, s = side(); bool big = r() < 0.3f; put(PK_ROCK, var(PK_ROCK), x, zAt(x, big ? 5.5f : 2.6f, big ? 42.f : 12.f, s), big ? 1.2f + r() * 2.6f : 0.22f + r() * 0.5f, r() * TAU, 0.02f); }
  for (int i = count(R[0] * 2.2f); i-- > 0;) { float x = x0 + r() * CH; put(PK_ROCK, var(PK_ROCK), x, pathZ(x) + (r() - 0.5f) * 4.6f, 0.05f + r() * 0.12f, r() * TAU, 0.01f); }
  for (int i = count(R[1]); i-- > 0;) { float x = x0 + r() * CH; put(PK_STAND, var(PK_STAND), x, zAt(x, 6.5f, 46, side()), 0.9f + r() * 0.7f, r() * TAU, 0.1f); }
  for (int i = count(R[2]); i-- > 0;) { float x = x0 + r() * CH; put(PK_TREE, var(PK_TREE), x, zAt(x, 6.5f, 50, side()), 0.8f + r() * 0.9f, r() * TAU, 0.1f); }
  for (int i = count(R[4]); i-- > 0;) { float x = x0 + r() * CH; put(PK_CAIRN, var(PK_CAIRN), x, zAt(x, 4.2f, 24, side()), 0.8f + r() * 0.6f, r() * TAU, 0.03f); }
  if (r() < R[3] * 0.35f) { float s = side(); int n = 2 + (int)(r() * 3); float off = 5.5f + r() * 9, x = x0 + r() * 4, ang = (r() - 0.5f) * 0.5f; for (int k = 0; k < n; k++) { float z = pathZ(x) + s * off; put(PK_WALL, var(PK_WALL), x, z, 1, -ang - std::atan(pathDZ(x)), 0.04f); x += 4.1f * std::cos(ang); off += std::sin(ang) * 4.1f * s; ang += (r() - 0.5f) * 0.3f; } }
  if (r() < R[5] * 0.3f) { float s = side(), off = 3.4f + r() * 3; float x = x0 + r() * 6; int n = 2 + (int)(r() * 2); for (int k = 0; k < n; k++) { put(PK_FENCE, var(PK_FENCE), x, pathZ(x) + s * off, 1, -std::atan(pathDZ(x)), 0); x += 5.6f; } }
}
static void updateChunks(float fx) {
  int c0 = (int)std::floor((fx - 50) / CH), c1 = (int)std::floor((fx + 110) / CH);
  for (int c = c0; c <= c1; c++) { int key = c + chunkBase; bool have = false; for (auto& k : chunks) if (k.used && k.key == key) have = true; if (have) continue; for (auto& k : chunks) if (!k.used) { buildChunk(k, key, c); propsDirty = true; break; } }
  for (auto& k : chunks) if (k.used) { int c = k.key - chunkBase; if (c < c0 - 1 || c > c1 + 1) { k.used = false; propsDirty = true; } }
  if (!propsDirty) return; propsDirty = false; memset(propN, 0, sizeof propN);
  for (auto& k : chunks) if (k.used) for (int i = 0; i < k.n; i++) { PI_& p = k.it[i]; int& n = propN[p.k][p.v]; if (n >= 256) continue; Inst& in = propInst[p.k][p.v][n++];
    M4 m = mul(translate(p.x, p.y, p.z), mul(rotY(p.ry), mul(rotX(p.rx), scale4(p.s, p.s, p.s)))); memcpy(in.m, m.m, 64); in.tint[0] = in.tint[1] = in.tint[2] = 1; in.tint[3] = 0; in.mat[0] = in.mat[1] = in.mat[2] = 1; in.mat[3] = 0; }
}

// ---------- unit visuals ----------
static inline float ix(const Unit& u) { return u.ox + (u.x - u.ox) * alphaI; }
static inline float iz(const Unit& u) { return u.oz + (u.z - u.oz) * alphaI; }
static float sm01(float a, float b, float x) { return smooth(a, b, x); }
static FigAnim animH[4], animM[MAXMON]; static FigDraw fdraw[4 + MAXMON]; static int nfd = 0;
static void figUnit(const Unit& u, bool mon, FigAnim& an, float dt, float t, bool carrier) {
  const FigAsset& a = mon ? gFM[u.cls] : gFH[u.cls]; if (!a.ok || nfd >= 4 + MAXMON) return;
  if (an.id != u.id) { an = FigAnim(); an.id = u.id; an.t = (u.id % 97) * 0.37f; }
  float sc = a.scale, x = ix(u), z = iz(u), y = terrH(x, z); bool dead = mon && u.hp <= 0; FigDraw& d = fdraw[nfd++];
  d.a = &a; d.mat = an.mat; d.dissolve = 0; d.disH = std::max(1.f, a.height); d.glow = 1; d.shadow = true; d.dim = 1; d.lantern = !mon && carrier && u.cls != LAMPLIGHTER && a.lant;
  for (int k = 0; k < 3; k++) d.disCol[k] = a.disCol[k];
  float sx = 1, sy = 1, sink = 0;
  if (!dead) {
    int ac = -1; float au = -1;
    if (u.act.on) { ac = figActClip(a, u.act.kind); if (ac >= 0) { const Act& A = u.act; float hit = a.clips[ac].hit > 0.01f ? a.clips[ac].hit : 0.5f; au = A.t < A.wind ? A.t / std::max(0.01f, A.wind) * hit : hit + (A.t - A.wind) / std::max(0.01f, A.rec) * (1 - hit); } }
    bool combat = mon ? (u.seen && u.target) : u.ready;
    figAnimUpdate(an, a, dt, u.moving / sc, combat, ac, au, u.stagV, !mon && u.down); figAnimPose(an, a);
  } else {
    float td = u.deadT; if (!an.dying) { an.dying = true; if (!strcmp(a.death, "shatter")) figShatter(an, a, u.boss ? 3.2f : 2.4f, -std::sin(u.face) * 0.5f, -std::cos(u.face) * 0.5f); }
    if (!strcmp(a.death, "shatter")) { figStepFree(an, a, dt); figAnimPose(an, a); float k = std::min(1.f, std::max(0.f, (td - 1.5f) / 1.5f)); d.disH = 0.6f; d.dissolve = k; d.shadow = k < 0.3f; }
    else if (!strcmp(a.death, "burn")) { sink = -0.25f * std::min(1.f, td / 1.8f); d.dissolve = std::min(1.f, td / 1.9f); d.shadow = d.dissolve < 0.5f; }
    else if (!strcmp(a.death, "burst")) { d.dissolve = std::min(1.f, td / 0.45f); sx = sy = 1 + td * 0.6f; d.shadow = false; }
    else { float k = std::min(1.f, td / 0.9f); d.dissolve = k; sx = 1 - 0.5f * k; sy = 1 + 0.3f * td; d.shadow = false; }
    d.glow = std::max(0.f, 1 - td * 2.5f);
  }
  d.model = mul(translate(x, y + sink * sc, z), mul(rotY(u.face), scale4(sc * sx, sc * sy, sc * sx)));
  float fl = u.flashV * u.flashV * (mon ? (u.boss ? 0.05f : 0.16f) : 0.1f); d.flash[0] = fl * 1.2f; d.flash[1] = fl * 0.9f; d.flash[2] = fl * 0.6f;
  if (mon && u.chillT > 0 && !dead) { d.flash[0] += 0.02f; d.flash[1] += 0.07f; d.flash[2] += 0.14f; }
  if (mon) { if (!dead) d.glow = u.boss ? (u.enr ? 3.f + std::sin(t * 9) : 3.f) : 2.2f; }
  else { an.dim += ((u.down ? 0.4f : 1.f) - an.dim) * std::min(1.f, dt * 3); d.dim = an.dim; if (u.cls == LAMPLIGHTER) d.glow = (0.9f + 0.06f * std::sin(t * 9.1f) + 0.04f * std::sin(t * 23.7f)) * (1 + G.flareV * 3); }
}
static V3 lanternPos(const Unit& c) {
  int hi = (int)(&c - &G.heroes[0]); float x = ix(c), z = iz(c), y = terrH(x, z); const FigAsset& a = gFH[c.cls]; int k = a.fxIx("lan");
  if (k >= 0 && hi >= 0 && hi < 4 && animH[hi].have) { float o[3]; figPoint(a, animH[hi].mat, a.fx[k].bone, a.fx[k].p[0], a.fx[k].p[1], a.fx[k].p[2], o); float cf = std::cos(c.face), sf = std::sin(c.face); return {x + cf * o[0] + sf * o[2], y + o[1], z - sf * o[0] + cf * o[2]}; }
  return {x, y + 2.f, z};
}
static void cameraUpdate(float dt) {
  float time = time_; float an = G.anchorX; float tx = an + 1.2f, tz = pathZ(an); int n = 1; bool anyHs = false;
  for (const Unit& h : G.heroes) if (!h.down) { tx += ix(h); tz += iz(h); n++; anyHs = true; } tx /= n; tz /= n; (void)anyHs;
  float mx = 0, mz = 0; int mc = 0; float lo = 1e9f, hi = -1e9f;
  for (const Unit& m : G.mons) if (m.used && m.hp > 0 && m.seen && m.x - an < 13) { mx += ix(m); mz += iz(m); mc++; }
  if (mc) { tx = lerpf(tx, mx / mc, 0.36f); tz = lerpf(tz, mz / mc, 0.3f); } else if (G.anchorMoving) tx += 2.4f;
  float zoom = 1.28f, yaw = 0;
  if (mc) { for (const Unit& h : G.heroes) if (!h.down) { lo = std::min(lo, h.x); hi = std::max(hi, h.x); } for (const Unit& m : G.mons) if (m.used && m.hp > 0 && m.seen && m.x - an < 13) { lo = std::min(lo, m.x); hi = std::max(hi, m.x + m.radius); } zoom = clampf((hi - lo + 6) / 12, 1.34f, 1.8f); tx = lerpf(tx, (lo + hi) / 2, 0.7f); }
  if (G.state == S_TITLE) { zoom = 0.62f; yaw = 0.5f + std::sin(time * 0.07f) * 0.35f; tx = an + 0.5f; }
  else if (G.state == S_RELIGHT || G.state == S_CHOICE) { zoom = 0.8f; if (G.hasWay) { tx = lerpf(tx, G.wayX, 0.5f); tz = lerpf(tz, pathZ(G.wayX) - 1.5f, 0.5f); } }
  else if (G.bossRef && G.bossRef->act.on && !G.bossRef->act.hit && G.bossRef->act.wind > 1) zoom *= 0.92f;
  else if (G.state == S_WIPE) zoom = 0.85f;
  if (G.flareV > 0.2f) zoom *= 0.9f;
  if (closeView) { zoom = 0.42f; const Unit* h = &G.heroes[0]; for (const Unit& q : G.heroes) if (!q.down) { h = &q; break; } tx = ix(*h); tz = iz(*h); }
  if (!camS.init) { camS = CamS(); camS.fx = tx; camS.fz = tz; camS.zoom = zoom; camS.yaw = yaw; camS.init = true; }
  { float d2 = std::min(dt, 0.05f); auto sp = [&](float& v, float& vv, float tg, float w) { vv += ((tg - v) * w * w - 2 * w * vv) * d2; v += vv * d2; }; sp(camS.fx, camS.vfx, tx, 3.4f); sp(camS.fz, camS.vfz, tz, 3.4f); sp(camS.zoom, camS.vzoom, zoom, 2.6f); }
  camS.yaw += (yaw - camS.yaw) * (1 - std::exp(-dt * 1.2f));
  float z = camS.zoom, drift = std::sin(time * 0.11f) * 0.5f, low = G.state == S_TITLE ? 0.42f : (closeView ? 0.55f : 1.f);
  float ox = -4.4f + drift, oy = 8.3f * low, oz = 12.4f, cy = std::cos(camS.yaw), sy = std::sin(camS.yaw), rx = ox * cy + oz * sy, rz = -ox * sy + oz * cy;
  G.shakeA = std::max(0.f, G.shakeA - dt * 2.6f); float shk = G.shakeA * G.shakeA * 0.22f; static Rng sr(5);
  camPos = {camS.fx + rx * z + (sr() - 0.5f) * shk, oy * z + (G.state == S_TITLE ? 0.6f : 0.f) + (sr() - 0.5f) * shk, camS.fz + rz * z + (sr() - 0.5f) * shk};
  float ly = G.state == S_TITLE ? 1.5f : (closeView ? 1.1f : 0.9f); camTgt = {camS.fx, ly, camS.fz};
  camFocus = len(camPos - camTgt); camAperture = closeView || G.state == S_TITLE ? 1.1f : 0.6f;
  camW = camWorld(camPos, camTgt); viewM = rigidInverse(camW); projM = perspective(30.f * PI / 180.f, (float)W / H, 0.5f, 2600.f); vpM = mul(projM, viewM); invVP = inverse(vpM); invProj = inverse(projM);
}
static void lightsUpdate(float dt) {
  float t = time_; const Unit* car = G.carrier ? G.carrier : &G.heroes[3]; 
  float base = (0.92f + 0.05f * std::sin(t * 7.1f) + 0.035f * std::sin(t * 17.3f + 1.f) + 0.02f * std::sin(t * 31.f)) * (car->down ? 0.4f : 1.f);
  G.flareV = std::max(0.f, G.flareV - dt * 0.9f); float R = G.hasRun ? G.lightRadius() : 9.f; V3 lp = lanternPos(*car);
  lit.moonCol = C(0x8fb2f2) * (2.0f * cur.moon); lit.hemiSky = C(0x2c4a7a) * (0.4f + 0.1f * cur.moon); lit.hemiGnd = C(0x0b0a08) * (0.4f + 0.1f * cur.moon); lit.fillDir = norm({-6, 9, 14}); lit.fillCol = C(0x8a9ab8) * 0.16f;
  float inten = 50 * base * (R / 9) * (1 + G.flareV * 2.5f); V3 lc = C(0xffb878);
  lit.pl[0][0] = lp.x; lit.pl[0][1] = lp.y; lit.pl[0][2] = lp.z; lit.pl[0][3] = R * 2.7f; lit.plc[0][0] = lc.x * inten; lit.plc[0][1] = lc.y * inten; lit.plc[0][2] = lc.z * inten; lit.plc[0][3] = 1.25f;
  lit.fogLP[0] = lp; lit.fogLC[0] = V3{1.0f, 0.62f, 0.3f} * (inten * 0.02f);
  // waylights: nearest two lit
  int k = 1; Way* pick[2] = {nullptr, nullptr}; for (auto& w : ways) { if (!w.used) continue; w.glow = lerpf(w.glow, w.lit ? 1.f : 0.f, 1 - std::exp(-dt * 1.6f)); w.fl = 0.86f + 0.1f * std::sin(t * 9.1f + w.x) + 0.06f * std::sin(t * 23.7f + w.x * 2); if (w.glow < 0.01f || std::fabs(w.x - G.anchorX) > 60) continue;
    for (int i = 0; i < 2; i++) if (!pick[i] || std::fabs(w.x - camS.fx) < std::fabs(pick[i]->x - camS.fx)) { if (i == 0) pick[1] = pick[0]; pick[i] = &w; break; } }
  V3 wc = C(0xffa24a); for (int i = 0; i < 2; i++) { int s = 1 + i; Way* w = pick[i]; if (!w) { lit.plc[s][0] = lit.plc[s][1] = lit.plc[s][2] = 0; lit.fogLC[s] = {}; continue; } float in2 = w->glow * 150 * w->fl;
    lit.pl[s][0] = w->x + 0.2f; lit.pl[s][1] = 3.1f; lit.pl[s][2] = w->z + 0.72f; lit.pl[s][3] = 26; lit.plc[s][0] = wc.x * in2; lit.plc[s][1] = wc.y * in2; lit.plc[s][2] = wc.z * in2; lit.plc[s][3] = 1.7f;
    lit.fogLP[s] = {w->x + 0.22f, 3.1f, w->z + 0.7f}; lit.fogLC[s] = V3{1.0f, 0.6f, 0.28f} * (w->glow * 2.0f); } (void)k;
  FlashLight& f = fx.fl; float fi = f.on ? f.inten * std::pow(std::max(0.f, 1 - f.t / f.dur), 2.f) : 0.f; V3 fc{f.r, f.g, f.b};
  lit.pl[3][0] = f.x; lit.pl[3][1] = f.y; lit.pl[3][2] = f.z; lit.pl[3][3] = 14; lit.plc[3][0] = fc.x * fi; lit.plc[3][1] = fc.y * fi; lit.plc[3][2] = fc.z * fi; lit.plc[3][3] = 1.8f; lit.fogLP[3] = {f.x, f.y, f.z}; lit.fogLC[3] = fc * (fi * 0.012f);
  fogPull = std::max(0.f, fogPull - dt * 0.05f); G.screenFlash = std::max(0.f, G.screenFlash - dt * 1.2f);
  // moon shadow follows the focus, snapped to texels
  float tx = std::round(camS.fx / 0.5f) * 0.5f, tz = std::round(camS.fz / 0.5f) * 0.5f; V3 tg{tx + 3, 0, tz}, mp = tg + MOON_DIR * 60.f; lit.shadowVP = mul(ortho(-24, 24, -24, 24, 1, 120), rigidInverse(camWorld(mp, tg)));
}
void update(float dt, float vdt, bool paused, float alpha) {
  alphaI = alpha; time_ += paused ? dt * 0.35f : std::max(vdt, dt * 0.2f); float t = time_;
  for (Unit& h : G.heroes) { h.flashV *= std::exp(-dt * 9); h.stagV *= std::exp(-dt * 6); float want = h.moving > 0.05f ? h.moving / 3.4f : 0.f; h.mAmp += (want - h.mAmp) * std::min(1.f, dt * 10); h.animT += dt * h.moving * 3.0f * (paused ? 0.f : 1.f); }
  for (Unit& m : G.mons) if (m.used) { m.flashV *= std::exp(-dt * 9); m.stagV *= std::exp(-dt * 6); float want = m.moving > 0.05f ? m.moving / 3.4f : 0.f; m.mAmp += (want - m.mAmp) * std::min(1.f, dt * 10); m.animT += dt * m.moving * (MON[m.cls].scale > 1.5f ? 2.2f : 3.0f) * (paused ? 0.f : 1.f); }
  blendTo(1 - std::exp(-(paused ? dt * 0.35f : std::max(vdt, dt * 0.3f)) * 0.35f)); cameraUpdate(dt); lightsUpdate(dt); updateChunks(camS.fx);
  // unit instances
  nfd = 0; { float fdt = paused ? 0.f : dt; const Unit* carU = G.carrier ? G.carrier : &G.heroes[3]; for (int i = 0; i < 4; i++) figUnit(G.heroes[i], false, animH[i], fdt, t, &G.heroes[i] == carU); for (int i = 0; i < MAXMON; i++) if (G.mons[i].used) figUnit(G.mons[i], true, animM[i], fdt, t, false); }
  for (int k = 0; k < NPK; k++) for (int v = 0; v < PROP_VARIANTS[k]; v++) if (k != PK_WAY) { static int last[NPK][5]; (void)last; meshUpload(gProp[k][v], propInst[k][v], propN[k][v]); }
  { int n = 0; static Inst wi[8]; for (auto& w : ways) if (w.used && n < 8) { M4 m = mul(translate(w.x, 0, w.z), rotY(-PI / 2 + 0.3f)); memcpy(wi[n].m, m.m, 64); wi[n].tint[0] = wi[n].tint[1] = wi[n].tint[2] = 1; wi[n].tint[3] = 0; wi[n].mat[0] = wi[n].mat[1] = 1; wi[n].mat[2] = w.glow * 7 * w.fl; wi[n].mat[3] = 0; n++; } meshUpload(gProp[PK_WAY][0], wi, n); }
  // zone particles
  for (auto& z : G.zones) if (z.on && vdt > 0) { int nf = (int)(vdt * 60 * z.r * 0.12f) + (fx.rnd() < 0.4f ? 1 : 0); for (int q = 0; q < nf; q++) { float a = fx.rnd() * TAU, d = std::sqrt(fx.rnd()) * z.r * 0.85f; if (z.kind == 0) { V3 c{3, 1.3f, 0.3f}; fx.embers(z.x + std::cos(a) * d, 0.15f, z.z + std::sin(a) * d, 1, &c, 0.15f, 1.5f); } else fx.frost(z.x + std::cos(a) * d, 0.2f, z.z + std::sin(a) * d, 1, 0.1f); } }
  fx.update(vdt, t, camS.fx, 0, camS.fz, cur.drizzle, cur.mote);
  // particle buffers
  pn[0] = pn[1] = 0; for (const Particle& q : fx.p) { if (q.t >= q.life) continue; float u = q.t / q.life, fade; int b = 0;
    switch (q.kind) { case 0: fade = q.life > 3 ? std::sin(PI * u) * 0.7f : (1 - u) * (1 - u); break; case 1: fade = std::sin(PI * std::min(1.f, u * 1.1f)); b = 1; break; case 2: fade = u < 0.8f ? 1.f : (1 - u) / 0.2f; b = 1; break; case 3: fade = 1 - u; break; default: fade = std::min(1.f, q.t * 8); }
    float sz = q.size * (q.kind == 1 ? (1 + u * 1.2f) : (q.kind == 0 && q.life <= 3 ? (1 - 0.5f * u) : 1.f)); PInst& o = pbuf[b][pn[b]++]; o = {q.x, q.y, q.z, sz, q.r, q.g, q.b, q.a * fade, q.vx, q.vy, q.vz, (float)q.kind}; }
  glBindBuffer(GL_ARRAY_BUFFER, partInst[0]); if (pn[0]) glBufferSubData(GL_ARRAY_BUFFER, 0, pn[0] * sizeof(PInst), pbuf[0]); glBindBuffer(GL_ARRAY_BUFFER, partInst[1]); if (pn[1]) glBufferSubData(GL_ARRAY_BUFFER, 0, pn[1] * sizeof(PInst), pbuf[1]);
  // marks
  mn = 0; auto mk = [&](float cx, float cz, float R, float r, float g, float b, float prog, float thick, float alp, float kind) { if (mn < 160) mbuf[mn++] = {cx, cz, R, terrH(cx, cz) + 0.07f, r, g, b, 0, prog, thick, alp, kind}; };
  for (auto& r : fx.rings) if (r.on) { float u = r.t / r.dur, e = 1 - std::pow(1 - u, 3.f), R = lerpf(r.r0, r.r1, e); mk(r.x, r.z, R, r.r, r.g, r.b, u, r.thick / std::max(0.5f, R) + 0.02f, 1, 0); }
  for (auto& z : G.zones) if (z.on) { float a = std::min(1.f, z.t * 5) * std::min(1.f, (z.dur - z.t) * 2.5f) * 0.9f; if (z.kind == 0) mk(z.x, z.z, z.r, 1.5f, 0.5f, 0.1f, 0, 0, a, 2); else mk(z.x, z.z, z.r, 0.3f, 0.7f, 1.3f, 0, 0, a, 3); }
  for (auto& te : G.teles) if (te.on) mk(te.x, te.z, te.r, 1.6f, 0.25f, 0.08f, std::min(1.f, te.t / te.dur), 0, std::min(1.f, te.t * 5) * 0.9f, 1);
  if (G.focus && G.focus->hp > 0) { const Unit& f = *G.focus; mk(ix(f), iz(f), f.radius + 0.5f + 0.05f * std::sin(t * 8), 1.2f, 0.9f, 0.5f, 0, 0, 0.9f, 4); }
  if (G.rally) mk(G.rallyX, G.rallyZ, 0.7f, 1.2f, 0.9f, 0.5f, 0, 0, 0.8f, 4);
  { static const uint32_t LC[5] = {0xe8c060, 0xa8a290, 0x7fb4d6, 0xe0a33c, 0xff7a3a}; for (auto& l : G.loot) if (l.on && fx.rnd() < 0.7f) { V3 c = C(LC[l.kind]); fx.glow(l.x, l.y, l.z, l.kind ? 0.22f : 0.12f, c * (l.kind ? 4.f : 3.f), 0.12f); } }
  glBindBuffer(GL_ARRAY_BUFFER, markInst); if (mn) glBufferSubData(GL_ARRAY_BUFFER, 0, mn * sizeof(MInst), mbuf);
}

// ---------- draw ----------
static void drawProps() { for (int k = 0; k < NPK; k++) for (int v = 0; v < PROP_VARIANTS[k]; v++) if (gProp[k][v].count) { meshDraw(gProp[k][v]); drawCalls++; } }
static void drawFigures(bool shadow) { if (!nfd) return; glDisable(GL_CULL_FACE); if (shadow) { glUseProgram(pFigShadow); setM(pFigShadow, "uVP", lit.shadowVP); figDraw(pFigShadow, fdraw, nfd, true); } else { setLit(pFig); figDraw(pFig, fdraw, nfd, false); glEnable(GL_CULL_FACE); } drawCalls += nfd; }
void draw() {
  drawCalls = 0; const Q& q = QS[quality]; float t = time_;
  glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE); glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
  if (q.shadow) { glBindFramebuffer(GL_FRAMEBUFFER, fboSh); glViewport(0, 0, SHS, SHS); glClear(GL_DEPTH_BUFFER_BIT); glUseProgram(pShadow); setM(pShadow, "uVP", lit.shadowVP); glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(2.f, 4.f); glDisable(GL_CULL_FACE); drawProps(); drawFigures(true); glDisable(GL_POLYGON_OFFSET_FILL); glEnable(GL_CULL_FACE); }
  GLuint sceneTarget = curMsaa > 0 ? fboMS : fboScene; glBindFramebuffer(GL_FRAMEBUFFER, sceneTarget); glViewport(0, 0, W, H); glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glActiveTexture(GL_TEXTURE0 + 5); glBindTexture(GL_TEXTURE_2D, texSh); glActiveTexture(GL_TEXTURE0);
  // sky
  glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glUseProgram(pSky); setM(pSky, "uInvVP", invVP); set3(pSky, "uCam", camPos); set3(pSky, "uMoon", MOON_DIR); glUniform3f(glGetUniformLocation(pSky, "uA"), 0.006f, 0.011f, 0.024f); glUniform3f(glGetUniformLocation(pSky, "uB"), 0.03f, 0.045f, 0.07f);
  set1(pSky, "uTime", t); set1(pSky, "uCloud", cur.cloud); set1(pSky, "uMoonI", cur.moon); glBindVertexArray(vaoEmpty); glDrawArrays(GL_TRIANGLES, 0, 3);
  glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDisable(GL_CULL_FACE);
  // terrain
  setLit(pTerr); set3(pTerr, "uGA", cur.gA); set3(pTerr, "uGB", cur.gB); set3(pTerr, "uMud", cur.mud); set1(pTerr, "uWet", cur.wet);
  glUniform2f(glGetUniformLocation(pTerr, "uOrigin"), std::round(camS.fx), std::round(camS.fz - 10)); set1(pTerr, "uY", 0); glBindVertexArray(terrNear.vao); glDrawElements(GL_TRIANGLES, terrNear.nIdx, GL_UNSIGNED_INT, nullptr);
  glUniform2f(glGetUniformLocation(pTerr, "uOrigin"), std::round(camS.fx / 8) * 8, std::round((camS.fz - 120) / 8) * 8); set1(pTerr, "uY", -0.25f); glBindVertexArray(terrFar.vao); glDrawElements(GL_TRIANGLES, terrFar.nIdx, GL_UNSIGNED_INT, nullptr);
  // grass
  setLit(pGrass); glUniform3f(glGetUniformLocation(pGrass, "uCenter"), camS.fx + 6, 0, camS.fz - 6); glUniform2f(glGetUniformLocation(pGrass, "uTile"), 72, 48); set1(pGrass, "uTime", t); set1(pGrass, "uGrassH", cur.grassH); set1(pGrass, "uHeath", 1);
  set3(pGrass, "uGc1", cur.g1); set3(pGrass, "uGc2", cur.g2); set3(pGrass, "uGc3", cur.g3); glBindVertexArray(grassVao); glDrawElementsInstanced(GL_TRIANGLES, 15, GL_UNSIGNED_SHORT, nullptr, (int)(grassCount * q.grass));
  // props + characters
  setLit(pMesh); drawProps(); drawFigures(false);
  // marks + particles (blended)
  glDepthMask(GL_FALSE); glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE); glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(-2, -2);
  if (mn) { glUseProgram(pMark); setM(pMark, "uVP", vpM); set1(pMark, "uTime", t); glBindVertexArray(markVao); glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, mn); }
  glDisable(GL_POLYGON_OFFSET_FILL);
  glUseProgram(pPart); setM(pPart, "uVP", vpM); set3(pPart, "uCam", camPos); V3 right{camW.m[0], camW.m[1], camW.m[2]}, up{camW.m[4], camW.m[5], camW.m[6]}; set3(pPart, "uRight", right); set3(pPart, "uUp", up);
  set1(pPart, "uAdd", 1); glBindVertexArray(partVao[0]); if (pn[0]) glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, pn[0]);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); set1(pPart, "uAdd", 0); glBindVertexArray(partVao[1]); if (pn[1]) glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, pn[1]);
  glDisable(GL_BLEND); glDepthMask(GL_TRUE);
  if (curMsaa > 0) { glBindFramebuffer(GL_READ_FRAMEBUFFER, fboMS); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboScene); glBlitFramebuffer(0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST); }
  // ---- post ----
  glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glBindVertexArray(vaoEmpty);
  glBindFramebuffer(GL_FRAMEBUFFER, fboA); glViewport(0, 0, W, H); glUseProgram(pFog);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texScene); glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texDepth); seti(pFog, "tC", 0); seti(pFog, "tD", 1);
  setM(pFog, "uInvProj", invProj); setM(pFog, "uCamW", camW); glUniform2f(glGetUniformLocation(pFog, "uNF"), 0.5f, 2600.f); glUniform2f(glGetUniformLocation(pFog, "uRes"), (float)W, (float)H); set3(pFog, "uFog", cur.fog);
  set1(pFog, "uDens", cur.dens * (1 - 0.45f * std::min(1.f, fogPull))); set1(pFog, "uAO", q.ao ? 1.f : 0.f); set1(pFog, "uTime", t); set3(pFog, "uMoon", MOON_DIR); set1(pFog, "uMoonI", cur.moon);
  { float lp[12], lc[12]; for (int i = 0; i < 4; i++) { lp[i * 3] = lit.fogLP[i].x; lp[i * 3 + 1] = lit.fogLP[i].y; lp[i * 3 + 2] = lit.fogLP[i].z; lc[i * 3] = lit.fogLC[i].x; lc[i * 3 + 1] = lit.fogLC[i].y; lc[i * 3 + 2] = lit.fogLC[i].z; } glUniform3fv(glGetUniformLocation(pFog, "uLP"), 4, lp); glUniform3fv(glGetUniformLocation(pFog, "uLC"), 4, lc); }
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glUseProgram(pDown); seti(pDown, "tC", 0); GLuint src = texA; int sw = W, sh_ = H;
  for (int i = 0; i < 4; i++) { glBindFramebuffer(GL_FRAMEBUFFER, fboB[i]); glViewport(0, 0, bw[i], bh[i]); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, src); glUniform2f(glGetUniformLocation(pDown, "uTx"), 1.f / sw, 1.f / sh_); set1(pDown, "uThr", i == 0 ? 1.05f : 0.f); glDrawArrays(GL_TRIANGLES, 0, 3); src = texB[i]; sw = bw[i]; sh_ = bh[i]; }
  glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0, 0, winW, winH); glUseProgram(pFinal);
  GLuint tx[5] = {texA, texB[0], texB[1], texB[2], texB[3]}; const char* nm[5] = {"tC", "tB1", "tB2", "tB3", "tB4"}; for (int i = 0; i < 5; i++) { glActiveTexture(GL_TEXTURE0 + i); glBindTexture(GL_TEXTURE_2D, tx[i]); seti(pFinal, nm[i], i); } glActiveTexture(GL_TEXTURE0);
  glUniform2f(glGetUniformLocation(pFinal, "uRes"), (float)W, (float)H); set1(pFinal, "uTime", t); set1(pFinal, "uExp", 0.62f); set1(pFinal, "uFocus", camFocus); set1(pFinal, "uAp", q.dof ? camAperture : 0.f); set1(pFinal, "uBloom", 0.5f); set1(pFinal, "uVig", 1.f);
  set1(pFinal, "uFlash", G.screenFlash); set1(pFinal, "uSat", 1.f); set1(pFinal, "uCon", 0.6f); set1(pFinal, "uCol", 1.f); set1(pFinal, "uFade", 1.f - 0.0f * uiDim);
  glDrawArrays(GL_TRIANGLES, 0, 3); glEnable(GL_DEPTH_TEST);
}
bool project(float x, float y, float z, float& sx, float& sy) { const float* m = vpM.m; float cx = m[0] * x + m[4] * y + m[8] * z + m[12], cy = m[1] * x + m[5] * y + m[9] * z + m[13], cw = m[3] * x + m[7] * y + m[11] * z + m[15]; if (cw <= 0.01f) return false; sx = (cx / cw * 0.5f + 0.5f) * winW; sy = (1 - (cy / cw * 0.5f + 0.5f)) * winH; return true; }
bool pickGround(float px, float py, float& x, float& z) {
  float nx = px / winW * 2 - 1, ny = 1 - py / winH * 2; V3 a = xformPoint(invVP, {nx, ny, -1}), b = xformPoint(invVP, {nx, ny, 1}); V3 d = b - a; if (std::fabs(d.y) < 1e-6f) return false; float k = -a.y / d.y; if (k < 0) return false; x = a.x + d.x * k; z = a.z + d.z * k; return true;
}
Unit* pickUnit(float px, float py) {
  Unit* best = nullptr; float bd = 52; for (Unit& m : G.mons) { if (!m.used || m.hp <= 0 || !m.seen) continue; float sx, sy; if (!project(m.x, MON[m.cls].hpY * 0.5f, m.z, sx, sy)) continue; float d = std::hypot(sx - px, sy - py) - m.radius * 14 * (winH / 720.f); if (d < bd) { bd = d; best = &m; } } return best;
}
}  // namespace gfx
