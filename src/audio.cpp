// Procedural audio: every sound effect, the wind bed, campfire crackle and the low drone are synthesised in the audio callback
// (no samples shipped). Replaces the WebAudio synth + embedded music of the original with ~0 bytes of assets.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include "../third_party/miniaudio.h"
#include "audio.h"
#include "mathx.h"
#include <atomic>
namespace {
enum W { SINE, BAND, LOW, HIGH, TRI };
struct Layer { int w; float f0, f1, dur, vol, delay; };
struct Def { Layer l[5]; int n; };
#define L(w, f0, f1, dur, vol, delay) {w, f0, f1, dur, vol, delay}
Def DEFS[S_COUNT];
void def(int s, std::initializer_list<Layer> ls) { Def& d = DEFS[s]; d.n = 0; for (auto& l : ls) if (d.n < 5) d.l[d.n++] = l; }
void arp(int s, float base, float vol, int steps, float gap) { Def& d = DEFS[s]; d.n = 0; static const float r[5] = {1, 1.25f, 1.5f, 2.f, 2.5f}; for (int i = 0; i < steps && i < 5; i++) d.l[d.n++] = L(SINE, base * r[i], base * r[i], 0.28f, vol, i * gap); }
void buildDefs() {
  def(S_SWING, {L(BAND, 900, 300, .15f, .35f, 0)}); def(S_SWINGHEAVY, {L(BAND, 600, 150, .25f, .45f, 0), L(SINE, 90, 50, .2f, .2f, 0.05f)});
  def(S_BOW, {L(HIGH, 3000, 600, .08f, .25f, 0), L(TRI, 220, 110, .1f, .15f, 0)}); def(S_BOWHEAVY, {L(HIGH, 3000, 500, .1f, .3f, 0), L(TRI, 140, 60, .25f, .25f, 0)});
  def(S_BOWVOLLEY, {L(HIGH, 3000, 600, .08f, .25f, 0), L(HIGH, 3200, 700, .08f, .22f, .07f), L(HIGH, 2800, 500, .08f, .2f, .14f), L(TRI, 180, 90, .3f, .15f, 0)});
  def(S_ARROWHIT, {L(BAND, 1500, 800, .06f, .3f, 0), L(SINE, 160, 80, .08f, .25f, 0)});
  def(S_FIRESMALL, {L(BAND, 1200, 400, .2f, .25f, 0)}); def(S_FIRECAST, {L(BAND, 800, 200, .4f, .35f, 0), L(SINE, 200, 500, .3f, .12f, 0)});
  def(S_FIREBURST, {L(LOW, 2500, 200, .5f, .6f, 0), L(SINE, 110, 40, .4f, .4f, 0)});
  def(S_FROSTSMALL, {L(SINE, 1800, 2600, .15f, .12f, 0), L(HIGH, 4000, 3000, .1f, .12f, 0)}); def(S_FROSTCAST, {L(SINE, 1200, 3000, .3f, .14f, 0), L(SINE, 1800, 2400, .25f, .1f, .05f)});
  def(S_FROSTFIELD, {L(HIGH, 2500, 1500, .5f, .15f, 0), L(SINE, 600, 300, .5f, .12f, 0)}); def(S_SHATTER, {L(HIGH, 5000, 2500, .25f, .35f, 0), L(SINE, 3200, 1800, .2f, .15f, 0), L(SINE, 4100, 2300, .2f, .1f, .04f)});
  def(S_THROW, {L(BAND, 600, 300, .12f, .2f, 0)}); def(S_GLASS, {L(SINE, 2400, 2300, .15f, .12f, 0), L(SINE, 3100, 3000, .12f, .09f, .01f), L(HIGH, 5000, 3000, .06f, .2f, 0)});
  def(S_HEAL, {L(SINE, 523, 784, .3f, .13f, 0), L(SINE, 659, 988, .3f, .1f, .08f)});
  def(S_BASH, {L(SINE, 120, 45, .25f, .55f, 0), L(LOW, 1500, 300, .1f, .35f, 0)}); def(S_HOLD, {L(SINE, 98, 196, .5f, .25f, 0), L(SINE, 147, 294, .5f, .18f, .05f), L(HIGH, 3000, 3000, .08f, .15f, 0)});
  def(S_CLEAVE, {L(BAND, 700, 150, .3f, .45f, 0), L(SINE, 100, 50, .25f, .2f, .08f)}); def(S_SUNDER, {L(SINE, 70, 30, .6f, .6f, 0), L(LOW, 800, 60, .5f, .5f, 0)});
  def(S_KINDLE, {L(SINE, 330, 660, .6f, .13f, 0), L(SINE, 495, 990, .6f, .1f, .07f)}); def(S_QUICK, {L(SINE, 800, 1600, .25f, .13f, 0), L(SINE, 1200, 2400, .25f, .1f, .04f)});
  def(S_HITMETAL, {L(SINE, 900, 700, .14f, .16f, 0), L(SINE, 1350, 1100, .1f, .1f, 0), L(HIGH, 4000, 2000, .06f, .3f, 0)}); def(S_HITSHIELD, {L(SINE, 600, 300, .15f, .22f, 0), L(HIGH, 3000, 1500, .05f, .3f, 0)});
  def(S_HITSTONE, {L(LOW, 1200, 400, .1f, .4f, 0), L(SINE, 140, 70, .1f, .3f, 0)}); def(S_HITWOOD, {L(SINE, 220, 110, .08f, .35f, 0), L(BAND, 1500, 600, .05f, .25f, 0)});
  def(S_HITSOFT, {L(LOW, 700, 200, .08f, .4f, 0), L(SINE, 100, 60, .1f, .25f, 0)}); def(S_HITFIRE, {L(BAND, 900, 300, .15f, .22f, 0)});
  def(S_HITFROST, {L(SINE, 2400, 1200, .1f, .1f, 0), L(HIGH, 4500, 3000, .06f, .12f, 0)}); def(S_HITGLOAM, {L(SINE, 300, 150, .15f, .15f, 0), L(SINE, 450, 200, .15f, .1f, 0)});
  def(S_DIE, {L(LOW, 900, 150, .3f, .4f, 0), L(SINE, 150, 50, .4f, .25f, 0)}); def(S_DIEBIG, {L(SINE, 80, 30, .8f, .55f, 0), L(LOW, 700, 100, .7f, .5f, 0)});
  def(S_MONATK, {L(BAND, 700, 250, .15f, .22f, 0), L(SINE, 140, 70, .15f, .18f, 0)}); def(S_MONHEAVY, {L(SINE, 70, 35, .3f, .45f, 0), L(LOW, 900, 150, .25f, .35f, 0)});
  def(S_GLOAMCAST, {L(SINE, 200, 600, .3f, .12f, 0), L(SINE, 205, 615, .3f, .12f, 0)});
  def(S_BOSSINTRO, {L(SINE, 55, 40, 1.8f, .5f, 0), L(SINE, 82, 60, 1.8f, .3f, 0), L(LOW, 400, 120, 1.5f, .25f, 0)}); def(S_BOSSWIND, {L(SINE, 80, 300, 1.6f, .22f, 0), L(BAND, 300, 1500, 1.6f, .12f, 0)});
  def(S_BOSSSLAM, {L(SINE, 60, 25, .8f, .65f, 0), L(LOW, 1500, 150, .6f, .55f, 0)}); def(S_ENRAGE, {L(SINE, 110, 80, 1.0f, .35f, 0), L(TRI, 220, 160, 1.0f, .2f, 0)});
  def(S_COIN, {L(SINE, 1568, 1568, .08f, .1f, 0), L(SINE, 2093, 2093, .12f, .1f, .06f)}); arp(S_GEAR, 392, .11f, 2, .07f); arp(S_GEAR2, 440, .12f, 3, .07f); arp(S_RELIC, 330, .14f, 5, .09f); arp(S_LEVELUP, 523, .12f, 4, .08f);
  def(S_HERODOWN, {L(SINE, 220, 90, .8f, .3f, 0), L(LOW, 600, 100, .3f, .25f, 0)}); def(S_REVIVE, {L(SINE, 220, 440, .5f, .2f, 0), L(SINE, 330, 660, .5f, .12f, .05f)});
  def(S_FLAMEOUT, {L(LOW, 900, 80, 1.2f, .5f, 0), L(SINE, 110, 40, 1.2f, .35f, 0)});
  def(S_FLARE, {L(LOW, 300, 3000, .8f, .4f, 0), L(SINE, 100, 400, .9f, .45f, 0), L(SINE, 800, 1600, .5f, .1f, .05f)});
  def(S_WAYLIGHT, {L(SINE, 262, 262, .9f, .15f, 0), L(SINE, 330, 330, .9f, .13f, .1f), L(SINE, 392, 392, .9f, .12f, .2f), L(LOW, 1200, 200, .4f, .25f, 0)});
  def(S_BOSSDOWN, {L(SINE, 110, 55, 1.2f, .4f, 0), L(SINE, 330, 330, .8f, .12f, .3f), L(SINE, 392, 392, .8f, .1f, .45f)});
  def(S_PUSH, {L(SINE, 330, 440, .25f, .12f, 0), L(SINE, 440, 660, .3f, .12f, .1f)}); def(S_HOLDCH, {L(SINE, 440, 330, .3f, .12f, 0)});
  def(S_CARD, {L(SINE, 880, 1320, .15f, .1f, 0), L(SINE, 1320, 1320, .2f, .1f, .08f)}); def(S_BUY, {L(SINE, 1175, 1175, .08f, .1f, 0), L(SINE, 1568, 1568, .12f, .1f, .05f)});
  def(S_CLICK, {L(SINE, 700, 500, .04f, .14f, 0)}); def(S_LANTERNFALL, {L(SINE, 330, 110, 2.0f, .3f, 0), L(SINE, 247, 82, 2.0f, .25f, 0), L(LOW, 800, 80, 1.6f, .3f, 0)});
  def(S_CINDERLAND, {L(LOW, 1500, 150, .3f, .3f, 0), L(SINE, 120, 60, .3f, .2f, 0)}); arp(S_UNLOCK, 392, .13f, 5, .1f);
}
struct Voice { bool on = false; int w = 0; float t = 0, delay = 0, dur = 0, f0 = 0, f1 = 0, vol = 0, ph = 0, lp = 0, bp = 0, pan = 0; };
constexpr int NV = 40; Voice voices[NV];
struct Req { int snd; float pan, vol; }; constexpr int QN = 128; Req queue[QN]; std::atomic<unsigned> qh{0}, qt{0};
std::atomic<float> aDanger{0}, aFire{0}, aVol{0.8f}, aDay{0.f}, aWater{0.f}, aForest{0.3f}; std::atomic<bool> aMute{false}, aPanel{false};
uint32_t rs = 0x1234567; float rnd() { rs = rs * 1664525u + 1013904223u; return ((rs >> 8) & 0xFFFF) / 32768.f - 1.f; }
struct Chirp { float t = 0, wait = 0.5f, f0 = 3000, f1 = 3000, dur = 0.1f, ph = 0; int notes = 0; } chirps[3]; float daySm = 0, watSm = 0, forSm = 0.3f, wl1 = 0, wl2 = 0, crPh = 0;
float sr = 48000; float wphase = 0, dph1 = 0, dph2 = 0, windLp = 0, windLp2 = 0, windT = 0, fireEnv = 0, fireLp = 0, duck = 1, danger = 0, fireSm = 0, hb = 0;
ma_device dev; bool started = false;
void spawn(const Req& r) {
  const Def& d = DEFS[r.snd]; for (int i = 0; i < d.n; i++) { Voice* v = nullptr; for (auto& q : voices) if (!q.on) { v = &q; break; } if (!v) return; const Layer& l = d.l[i];
    *v = Voice(); v->on = true; v->w = l.w; v->f0 = l.f0; v->f1 = l.f1; v->dur = l.dur; v->vol = l.vol * r.vol; v->delay = l.delay; v->pan = r.pan; }
}
void cb(ma_device*, void* out, const void*, ma_uint32 frames) {
  float* o = (float*)out; unsigned h = qh.load(), t = qt.load(); while (t != h) { spawn(queue[t % QN]); t++; } qt.store(t);
  float day = aDay.load(), wat = aWater.load(), forT = aForest.load();
  float vol = aMute.load() ? 0.f : aVol.load(), dg = aDanger.load(), fr = aFire.load(); float dt = 1.f / sr; float tgtDuck = aPanel.load() ? 0.55f : 1.f;
  for (ma_uint32 i = 0; i < frames; i++) {
    float L_ = 0, R_ = 0; duck += (tgtDuck - duck) * 0.0002f; danger += (dg - danger) * 0.00005f; fireSm += (fr - fireSm) * 0.0003f;
    // wind bed: two filtered noise bands with slow gusts
    windT += dt; float n = rnd(); windLp += (n - windLp) * 0.012f; windLp2 += (n - windLp2) * 0.15f; float gust = 0.55f + 0.35f * std::sin(windT * 0.23f) + 0.2f * std::sin(windT * 0.071f + 1.f);
    float wind = (windLp * 3.2f * gust + (windLp2 - windLp) * 0.35f * gust * gust) * 0.07f;
    // drone: low open fifth with slow beating; swells in danger
    dph1 += TAU * 55.f * dt; dph2 += TAU * 82.4f * dt; if (dph1 > TAU) dph1 -= TAU; if (dph2 > TAU) dph2 -= TAU; float swell = 0.5f + 0.5f * std::sin(windT * 0.11f);
    float drone = (std::sin(dph1) * 0.5f + std::sin(dph2 + 0.3f * std::sin(windT * 0.2f)) * 0.3f + std::sin(dph1 * 2.003f) * 0.12f) * (0.018f + 0.015f * swell + 0.03f * danger);
    // campfire crackle near the waylights
    float cr = 0; if (fireSm > 0.01f) { fireEnv *= 0.9992f; if (rnd() > 0.9985f) fireEnv = 0.4f + 0.6f * (rnd() * 0.5f + 0.5f); fireLp += (rnd() - fireLp) * 0.35f; cr = (fireLp * fireEnv * 0.35f + rnd() * 0.01f) * fireSm * 0.4f; }
    // danger pulse
    hb += dt * (1.1f + danger * 0.9f); if (hb > 1) hb -= 1; float pulse = danger > 0.2f ? std::exp(-hb * 18.f) * std::sin(hb * 300.f) * 0.05f * danger : 0.f;
    // ambience (explorer): birdsong by day, crickets at night, running water near rivers / falls / the sea
    daySm += (day - daySm) * 0.00003f; watSm += (wat - watSm) * 0.00005f; forSm += (forT - forSm) * 0.00003f; drone *= 1.f - 0.82f * daySm;
    float amb = 0;
    for (int c = 0; c < 3; c++) { Chirp& k = chirps[c];
      if (k.notes <= 0) { k.wait -= dt; if (k.wait <= 0 && daySm > 0.25f) { k.notes = 2 + (int)((rnd() * 0.5f + 0.5f) * 4.f); k.dur = 0.07f + (rnd() * 0.5f + 0.5f) * 0.1f; k.f0 = 2300.f + (rnd() * 0.5f + 0.5f) * 2200.f; k.f1 = k.f0 * (rnd() > 0 ? 1.35f : 0.72f); k.t = 0; } }
      if (k.notes > 0) { k.t += dt; float u = k.t / k.dur; if (u >= 1.f) { k.notes--; k.t = 0; if (k.notes <= 0) k.wait = (1.5f + (rnd() * 0.5f + 0.5f) * 7.f) / (0.35f + forSm); }
        else { float f = k.f0 * std::pow(k.f1 / k.f0, u) * (1.f + 0.02f * std::sin(k.t * 90.f)); k.ph += TAU * f * dt; if (k.ph > TAU) k.ph -= TAU; amb += std::sin(k.ph) * std::sin(u * PI) * 0.04f * daySm * (0.4f + forSm); } } }
    if (daySm < 0.7f) { crPh += TAU * 4300.f * dt; if (crPh > TAU) crPh -= TAU; float g = 0.5f + 0.5f * std::sin(windT * TAU * 17.f); g = g * g * g; float sl = 0.5f + 0.5f * std::sin(windT * 0.9f); amb += std::sin(crPh) * g * sl * 0.010f * (1.f - daySm / 0.7f); }
    wl1 += (n - wl1) * 0.3f; wl2 += (wl1 - wl2) * 0.06f; amb += (wl1 - wl2) * 0.5f * watSm * (0.8f + 0.2f * std::sin(windT * 3.1f));
    float bed = wind + drone + cr + pulse + amb; L_ += bed; R_ += bed;
    for (auto& v : voices) { if (!v.on) continue; if (v.delay > 0) { v.delay -= dt; continue; } float u = v.t / v.dur; if (u >= 1) { v.on = false; continue; }
      float f = v.f0 * std::pow(v.f1 / v.f0, u), env = std::exp(-4.5f * u) * std::min(1.f, v.t / 0.004f) * (u > 0.9f ? (1 - u) * 10.f : 1.f), s = 0;
      if (v.w == SINE) { v.ph += TAU * f * dt; if (v.ph > TAU) v.ph -= TAU; s = std::sin(v.ph); }
      else if (v.w == TRI) { v.ph += f * dt; if (v.ph > 1) v.ph -= 1; s = 4 * std::fabs(v.ph - 0.5f) - 1; }
      else { float x = rnd(), k = clampf(TAU * f * dt, 0.001f, 0.95f); v.lp += (x - v.lp) * k; if (v.w == LOW) s = v.lp * 2.f; else if (v.w == HIGH) s = x - v.lp; else { v.bp += (v.lp - v.bp) * k; s = (v.lp - v.bp) * 5.f; } }
      s *= env * v.vol; float pl = 0.5f - v.pan * 0.4f; L_ += s * (1 - pl) * 1.4f; R_ += s * pl * 1.4f; v.t += dt; }
    L_ *= vol * duck; R_ *= vol * duck; o[i * 2] = std::tanh(L_); o[i * 2 + 1] = std::tanh(R_);
  }
}
}  // namespace
namespace audio {
bool init() {
  buildDefs(); ma_device_config c = ma_device_config_init(ma_device_type_playback); c.playback.format = ma_format_f32; c.playback.channels = 2; c.sampleRate = 48000; c.dataCallback = cb; c.periodSizeInMilliseconds = 20;
  if (ma_device_init(nullptr, &c, &dev) != MA_SUCCESS) return false; sr = (float)dev.sampleRate; if (ma_device_start(&dev) != MA_SUCCESS) { ma_device_uninit(&dev); return false; } started = true; return true;
}
void shutdown() { if (started) { ma_device_uninit(&dev); started = false; } }
void play(int snd, float pan, float vol) { if (!started || snd <= 0 || snd >= S_COUNT) return; unsigned h = qh.load(); if (h - qt.load() >= QN) return; queue[h % QN] = {snd, pan, vol}; qh.store(h + 1); }
void update(float, float danger, bool panel, float fire) { aDanger.store(danger); aPanel.store(panel); aFire.store(fire); }
void ambience(float day, float water, float forest) { aDay.store(day); aWater.store(water); aForest.store(forest); }
void setVolume(float v, bool mute) { aVol.store(v); aMute.store(mute); }
}  // namespace audio
