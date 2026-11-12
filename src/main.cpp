// Lanternfall native: window, input, fixed-step loop, screenshot smoke-test mode.
#define GLFW_INCLUDE_NONE
#include "gl.h"
#include <GLFW/glfw3.h>
#include "render.h"
#include "ui.h"
#include "play.h"
#include "settings.h"
#include "roster.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
static bool rollReq = false, locked = false, firstMove = true; static double lastX = 0, lastY = 0; static GLFWwindow* win; static bool fullscreen = false, vsync = true; static int wx = 100, wy = 100, ww = 1280, wh = 720; static double mx = 0, my = 0;
static void toggleFullscreen() {
  if (!fullscreen) { glfwGetWindowPos(win, &wx, &wy); glfwGetWindowSize(win, &ww, &wh); GLFWmonitor* m = glfwGetPrimaryMonitor(); const GLFWvidmode* v = glfwGetVideoMode(m); glfwSetWindowMonitor(win, m, 0, 0, v->width, v->height, v->refreshRate); }
  else glfwSetWindowMonitor(win, nullptr, wx, wy, ww, wh, 0);
  fullscreen = !fullscreen; glfwSwapInterval(vsync ? 1 : 0);
}
static void onKey(GLFWwindow*, int key, int, int action, int mods) {
  if (action == GLFW_RELEASE) return; (void)mods; bool rep = action == GLFW_REPEAT; if (!rep && ui::captureKey(key)) return; if (!rep && key == GLFW_KEY_ESCAPE && ui::optionsOpen()) { ui::closeOptions(); return; }
  int c = key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER ? 13 : key == GLFW_KEY_ESCAPE ? 27 : key == GLFW_KEY_SPACE ? ' ' : (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) ? key : (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) ? key : (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) ? '0' + (key - GLFW_KEY_KP_0) : 0;
  if (key == GLFW_KEY_F11 && !rep) { toggleFullscreen(); return; }
  if (key == GLFW_KEY_F10 && !rep) { vsync = !vsync; glfwSwapInterval(vsync ? 1 : 0); return; }
  if (pl::active()) { if (rep) return; if (key == GLFW_KEY_Q) { gfx::setQuality(gfx::quality + 1); return; } if (c == 27) { if (pl::H.ended) pl::stop(); else pl::stop(); return; } if (pl::H.mode != pl::M_SELECT && key == cfg::S.key[cfg::ACT_ROLL]) rollReq = true; else pl::keyPress(c == 13 ? 13 : key); return; }
  if (rep) return;
  if (key == GLFW_KEY_Q) { gfx::setQuality(gfx::quality + 1); return; }
  if (c == 27 && G.panel == P_NONE && G.state != S_TITLE) { G.panel = P_MENU; return; }
  if (c == 27 && G.state == S_TITLE) { glfwSetWindowShouldClose(win, 1); return; }
  if (ui::key(c)) { audio::play(S_CLICK, 0, 0.5f); return; }
  if (G.state == S_TITLE || G.panel != P_NONE) return;
  if (c == ' ') G.fireFlare(true); else if (c == 'M') { G.panel = P_MENU; G.panelTimer = false; } else if (c == 'C') gfx::closeView = !gfx::closeView;
  else if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD) G.P.speed = G.P.speed == 1 ? 2 : 4; else if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) G.P.speed = G.P.speed == 4 ? 2 : 1;
}
static void onCursor(GLFWwindow*, double x, double y) { if (!locked) return; if (firstMove) { firstMove = false; lastX = x; lastY = y; return; } pl::look((float)(x - lastX), (float)(y - lastY)); lastX = x; lastY = y; }
static void onScroll(GLFWwindow*, double, double dy) { if (pl::active()) pl::scroll((float)dy); }
static void lockCursor(bool on) { locked = on; firstMove = true; glfwSetInputMode(win, GLFW_CURSOR, on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL); if (glfwRawMouseMotionSupported()) glfwSetInputMode(win, GLFW_RAW_MOUSE_MOTION, on ? GLFW_TRUE : GLFW_FALSE); }
static void onMouse(GLFWwindow* w, int button, int action, int) {
  if (pl::active() && pl::H.mode != pl::M_SELECT) return;
  if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return; double x, y; glfwGetCursorPos(w, &x, &y); int ww_, wh_, fw, fh; glfwGetWindowSize(w, &ww_, &wh_); glfwGetFramebufferSize(w, &fw, &fh); float sx = (float)(x * fw / ww_), sy = (float)(y * fh / wh_);
  if (ui::click(sx, sy)) return; if (pl::active() || G.state == S_TITLE) return; if (G.panel != P_NONE || !G.hasRun) return;
  Unit* m = gfx::pickUnit(sx, sy); if (m) { G.setFocus(m); fx.ring(m->x, m->z, 0.3f, m->radius + 0.6f, 0.6f, {1.2f, 0.9f, 0.5f}); fx.floatText(m->x, MON[m->cls].hpY + 0.6f, m->z, "FOCUS", 0xffd27a, 12, 0.5f); return; }
  float gx, gz; if (gfx::pickGround(sx, sy, gx, gz)) { G.setRally(gx, gz); fx.ring(G.rallyX, G.rallyZ, 0.2f, 0.9f, 0.7f, {1.2f, 0.9f, 0.5f}); }
}
// --- tiny PNG writer (stored deflate) for the smoke test ---
static uint32_t crcT[256]; static void crcInit() { for (uint32_t n = 0; n < 256; n++) { uint32_t c = n; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crcT[n] = c; } }
static uint32_t crc(const uint8_t* b, size_t n, uint32_t c = 0xFFFFFFFFu) { for (size_t i = 0; i < n; i++) c = crcT[(c ^ b[i]) & 255] ^ (c >> 8); return c; }
static void be32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 3; i >= 0; i--) v.push_back((x >> (i * 8)) & 255); }
static void chunk(FILE* f, const char* t, const std::vector<uint8_t>& d) { std::vector<uint8_t> o; be32(o, (uint32_t)d.size()); std::vector<uint8_t> td(t, t + 4); td.insert(td.end(), d.begin(), d.end()); o.insert(o.end(), td.begin(), td.end()); be32(o, ~crc(td.data(), td.size())); fwrite(o.data(), 1, o.size(), f); }
static void writePng(const char* path, int w, int h, const std::vector<uint8_t>& rgb) {
  crcInit(); FILE* f = fopen(path, "wb"); if (!f) return; const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10}; fwrite(sig, 1, 8, f); std::vector<uint8_t> ih; be32(ih, w); be32(ih, h); ih.insert(ih.end(), {8, 2, 0, 0, 0}); chunk(f, "IHDR", ih);
  std::vector<uint8_t> raw; raw.reserve((size_t)(w * 3 + 1) * h); for (int y = 0; y < h; y++) { raw.push_back(0); raw.insert(raw.end(), rgb.begin() + (size_t)y * w * 3, rgb.begin() + (size_t)(y + 1) * w * 3); }
  std::vector<uint8_t> z{0x78, 0x01}; size_t pos = 0; uint32_t a = 1, b = 0; for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
  while (pos < raw.size()) { size_t n = std::min<size_t>(65535, raw.size() - pos); z.push_back(pos + n >= raw.size()); z.push_back(n & 255); z.push_back(n >> 8); z.push_back(~n & 255); z.push_back((~n >> 8) & 255); z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n); pos += n; }
  be32(z, (b << 16) | a); chunk(f, "IDAT", z); chunk(f, "IEND", {}); fclose(f);
}
static bool kd(int k) { return glfwGetKey(win, k) == GLFW_PRESS; }
static bool gpPrev[16] = {false}, aimLatch = false, lmbPrev = false;
static bool gpEdge(const GLFWgamepadstate& gs, int b) { bool now = gs.buttons[b] != 0, e = now && !gpPrev[b]; gpPrev[b] = now; return e; }
static pl::Input readInput(float dt) {
  cfg::Settings& S = cfg::S; pl::Input in; auto K = [&](int a) { return kd(S.key[a]); };
  in.mx = ((K(cfg::ACT_RIGHT) || kd(GLFW_KEY_RIGHT)) ? 1.f : 0.f) - ((K(cfg::ACT_LEFT) || kd(GLFW_KEY_LEFT)) ? 1.f : 0.f); in.mz = ((K(cfg::ACT_FWD) || kd(GLFW_KEY_UP)) ? 1.f : 0.f) - ((K(cfg::ACT_BACK) || kd(GLFW_KEY_DOWN)) ? 1.f : 0.f);
  in.sprint = K(cfg::ACT_SPRINT); bool l = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS; in.rmb = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS; bool roll = rollReq || kd(GLFW_KEY_LEFT_CONTROL); rollReq = false;
  GLFWgamepadstate gs; if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1) && glfwGetGamepadState(GLFW_JOYSTICK_1, &gs)) {
    auto dz = [](float v) { float a = std::fabs(v); return a < 0.2f ? 0.f : (v > 0 ? 1.f : -1.f) * (a - 0.2f) / 0.8f; };
    in.mx = std::max(-1.f, std::min(1.f, in.mx + dz(gs.axes[GLFW_GAMEPAD_AXIS_LEFT_X]))); in.mz = std::max(-1.f, std::min(1.f, in.mz - dz(gs.axes[GLFW_GAMEPAD_AXIS_LEFT_Y])));
    float rx = dz(gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]), ry = dz(gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]); if (rx != 0.f || ry != 0.f) pl::look(rx * dt * 760.f * S.sens, ry * dt * 620.f * S.sens);
    l = l || gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > -0.4f; in.rmb = in.rmb || gs.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > -0.4f; in.sprint = in.sprint || gs.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB];
    if (gpEdge(gs, GLFW_GAMEPAD_BUTTON_A)) roll = true; if (gpEdge(gs, GLFW_GAMEPAD_BUTTON_Y)) pl::keyPress(S.key[cfg::ACT_VIEW]); if (gpEdge(gs, GLFW_GAMEPAD_BUTTON_X)) pl::keyPress('E');
  }
  if (S.toggleAim) { if (l && !lmbPrev) aimLatch = !aimLatch; lmbPrev = l; if (aimLatch && !pl::P.drawing && pl::P.relT < 0.1f) aimLatch = false; in.lmb = aimLatch; } else { in.lmb = l; aimLatch = false; }
  in.roll = roll; return in;
}
static void padMenu() {   // hero select: bumpers change, A selects
  GLFWgamepadstate gs; if (!(glfwJoystickIsGamepad(GLFW_JOYSTICK_1) && glfwGetGamepadState(GLFW_JOYSTICK_1, &gs))) return;
  if (gpEdge(gs, GLFW_GAMEPAD_BUTTON_LEFT_BUMPER)) pl::heroStep(-1); if (gpEdge(gs, GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER)) pl::heroStep(1); if (gpEdge(gs, GLFW_GAMEPAD_BUTTON_A)) pl::keyPress(13); if (gpEdge(gs, GLFW_GAMEPAD_BUTTON_B)) pl::stop();
}
static int lastRuns = -1;
namespace gfx { extern const char* gArgv0; }
int main(int argc, char** argv) {
  gfx::gArgv0 = argv[0];
  const char* scene = nullptr; int heroArg = -1, biomeArg = -1; int quality = 2, shotFrames = 0, simSpeed = 1, W0 = 1280, H0 = 720; const char* shot = nullptr; bool demo = false, noAudio = false; int startMode = 0, startView = 0; bool bestiary = false; bool bot = false; float dbgDraw = -1, dbgYaw = 99, dbgPitch = 0; int ff = 0;
  for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i]; else if (!strcmp(argv[i], "--out") && i + 1 < argc) shot = argv[++i]; else if (!strcmp(argv[i], "--scene") && i + 1 < argc) scene = argv[++i]; else if (!strcmp(argv[i], "--hero") && i + 1 < argc) heroArg = atoi(argv[++i]); else if (!strcmp(argv[i], "--biome") && i + 1 < argc) biomeArg = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--frames") && i + 1 < argc) shotFrames = atoi(argv[++i]); else if (!strcmp(argv[i], "--quality") && i + 1 < argc) quality = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--speed") && i + 1 < argc) simSpeed = atoi(argv[++i]); else if (!strcmp(argv[i], "--size") && i + 2 < argc) { W0 = atoi(argv[++i]); H0 = atoi(argv[++i]); } else if (!strcmp(argv[i], "--demo")) demo = true; else if (!strcmp(argv[i], "--mode") && i + 1 < argc) { const char* mm = argv[++i]; startMode = !strcmp(mm, "waves") ? pl::M_WAVES : !strcmp(mm, "select") ? pl::M_SELECT : pl::M_RANGE; } else if (!strcmp(argv[i], "--view") && i + 1 < argc) startView = atoi(argv[++i]); else if (!strcmp(argv[i], "--bot")) bot = true; else if (!strcmp(argv[i], "--ff") && i + 1 < argc) ff = atoi(argv[++i]); else if (!strcmp(argv[i], "--draw") && i + 1 < argc) dbgDraw = (float)atof(argv[++i]); else if (!strcmp(argv[i], "--yaw") && i + 2 < argc) { dbgYaw = (float)atof(argv[++i]); dbgPitch = (float)atof(argv[++i]); } else if (!strcmp(argv[i], "--novsync")) vsync = false; else if (!strcmp(argv[i], "--mute")) noAudio = true;
    else if (!strcmp(argv[i], "--help")) { printf("lanternfall [--quality 0-3] [--size W H] [--novsync] [--mute]\n"); return 0; } }
  cfg::load(); if (heroArg >= 0) cfg::S.hero = heroArg;
  if (scene) { if (!strcmp(scene, "hero")) { startMode = pl::M_SELECT; startView = 1; if (!ff) ff = 190; } else if (!strcmp(scene, "aim")) { startMode = pl::M_RANGE; startView = 1; dbgDraw = 0.92f; if (!ff) ff = 40; } else if (!strcmp(scene, "bestiary")) { startMode = pl::M_RANGE; startView = 1; if (!ff) ff = 50; bestiary = true; } else if (!strcmp(scene, "fpv")) { startMode = pl::M_RANGE; startView = 0; dbgDraw = 0.8f; if (!ff) ff = 40; }
    else if (!strcmp(scene, "moor") || !strcmp(scene, "highlands") || !strcmp(scene, "fen")) { biomeArg = !strcmp(scene, "moor") ? 0 : !strcmp(scene, "highlands") ? 1 : 2; startMode = pl::M_RANGE; startView = 1; if (!ff) ff = 30; } }
  if (biomeArg >= 0) pl::setBiome(biomeArg);
  if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); glfwWindowHint(GLFW_SAMPLES, 0);
  win = glfwCreateWindow(W0, H0, "LANTERNFALL", nullptr, nullptr); if (!win) { fprintf(stderr, "could not create an OpenGL 3.3 window\n"); glfwTerminate(); return 1; }
  glfwMakeContextCurrent(win); glfwSwapInterval(vsync && !shot ? 1 : 0); glLoadAll([](const char* n) { return (void*)glfwGetProcAddress(n); });
  fprintf(stderr, "GL %s | %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
  G.onWaylight = [](float x) { gfx::addWaylight(x, false); }; G.onRelight = []() { gfx::lightLastWaylight(); }; G.onShift = [](float dx) { gfx::shiftWorld(dx); }; G.onReach = [](int r, bool inst) { gfx::setReach(r, inst); };
  G.load(); { int fw, fh; glfwGetFramebufferSize(win, &fw, &fh); gfx::winW = fw; gfx::winH = fh; } if (!gfx::init(quality)) return 1; ui::init(); if (!noAudio && !shot) audio::init(); audio::setVolume(cfg::S.master, G.P.mute);
  G.newRun(); G.state = S_TITLE; gfx::addWaylight(G.anchorX + 2.2f, true); lastRuns = G.P.runs;
  glfwSetKeyCallback(win, onKey); glfwSetMouseButtonCallback(win, onMouse); glfwSetCursorPosCallback(win, onCursor); glfwSetScrollCallback(win, onScroll);
  if (startMode) { pl::start(startMode); pl::setView(startView); pl::H.view = startView; pl::H.bot = bot; if (dbgDraw >= 0) pl::debugDraw(dbgDraw); if (dbgYaw < 90) pl::debugAim(dbgYaw, dbgPitch); if (!shot) lockCursor(true); }
  if (bestiary) pl::debugBestiary();
  if (startMode && ff > 0) { pl::Input in0; for (int i = 0; i < ff; i++) pl::update(1.f / 60.f, in0); }
  if (demo) { G.beginRun(false); G.P.speed = 1; }
  double last = glfwGetTime(), accum = 0; float acc = 0; const float DT = 1.f / 60.f; int frame = 0; float fpsT = 0; int fpsN = 0; char title[128];
  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents(); int fw, fh; glfwGetFramebufferSize(win, &fw, &fh); if (fw < 2 || fh < 2) { glfwWaitEvents(); continue; } if (fw != gfx::winW || fh != gfx::winH) gfx::resize(fw, fh);
    double now = glfwGetTime(); float dtR = shot ? DT : (float)(now - last); last = now; if (!(dtR > 0)) dtR = 0.016f; dtR = std::min(dtR, 0.1f); (void)accum;
    if (pl::pendingMode) { pl::start(pl::pendingMode); lockCursor(pl::pendingMode != pl::M_SELECT); pl::pendingMode = 0; }
    const bool play = pl::active(); if ((!play || pl::H.mode == pl::M_SELECT) && locked) lockCursor(false);
    if (play) { if (pl::H.mode == pl::M_SELECT) padMenu(); pl::Input in = readInput(dtR); pl::update(dtR, in); }
    if (!play) G.tickPanel(dtR); bool paused = !play && (G.panel != P_NONE || G.state == S_TITLE || !G.hasRun); float gdt = 0, adt = 0;
    if (play) adt = dtR * pl::timeScale();
    else if (!paused && (G.hitStop = std::max(0.f, G.hitStop - dtR)) > 0) gdt = 0;
    else if (!paused) { G.slowMo = std::max(0.f, G.slowMo - dtR); float sm = G.slowMo > 0 ? lerpf(1, 0.3f, std::min(1.f, G.slowMo / 0.5f)) : 1.f; adt = dtR * G.P.speed * simSpeed * sm; acc += adt; int n = 0; while (acc >= DT && n < 14 * simSpeed) { G.step(DT); acc -= DT; n++; gdt += DT; if (G.panel != P_NONE || !G.hasRun) break; } if (acc > DT * 4) acc = 0; }
    if (G.panel != P_NONE) { /* the steward's countdown uses real time */ }
    float alpha = (paused || play) ? 1.f : clampf(acc / DT, 0, 1); if (G.P.runs != lastRuns) { lastRuns = G.P.runs; gfx::clearWaylights(); fx.clear(); gfx::resetCamera(); if (G.state == S_RUNEND || G.state == S_WALK) {} }
    for (Unit& h : G.heroes) if (paused) { h.ox = h.x; h.oz = h.z; }
    gfx::update(dtR, paused ? dtR * 0.35f : adt, paused, alpha);
    float danger = play ? (pl::H.mode == pl::M_WAVES ? 0.5f : 0.f) : 0; if (!play && G.hasRun && G.state != S_TITLE) { danger = G.state == S_BOSS ? 1.f : (G.state == S_FIGHT ? 0.55f : 0.1f); if (G.partyHp() < 0.4f) danger = std::min(1.f, danger + 0.3f); }
    audio::update(dtR, danger, G.panel != P_NONE, 0.f); gfx::draw(); ui::draw(fw, fh, dtR);
    if (shot && ++frame >= shotFrames) { std::vector<uint8_t> px((size_t)fw * fh * 4), rgb((size_t)fw * fh * 3); glReadPixels(0, 0, fw, fh, GL_RGBA, GL_UNSIGNED_BYTE, px.data()); for (int y = 0; y < fh; y++) for (int x = 0; x < fw; x++) for (int k = 0; k < 3; k++) rgb[((size_t)y * fw + x) * 3 + k] = px[((size_t)(fh - 1 - y) * fw + x) * 4 + k];
      writePng(shot, fw, fh, rgb); fprintf(stderr, "wrote %s (%dx%d) stage=%d state=%d heroes hp=%.0f%% mons=%d\n", shot, fw, fh, G.run.stage, G.state, G.partyHp() * 100, G.liveMonCount()); break; }
    glfwSwapBuffers(win); fpsN++; fpsT += dtR; if (fpsT >= 1.f) { snprintf(title, 128, "LANTERNFALL - %d fps - %s - %dx%d", (int)(fpsN / fpsT), gfx::qualityName[gfx::quality], gfx::W, gfx::H); glfwSetWindowTitle(win, title); fpsN = 0; fpsT = 0; }
  }
  if (G.hasRun || true) G.save(); audio::shutdown(); glfwDestroyWindow(win); glfwTerminate(); return 0;
}
