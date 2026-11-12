// xanim.inc - procedural personality layer (work / talk / look-around / posture) and player air + landing layer.
// Applied additively on top of baked idle + procedural gait. No heap allocation; bone lookups are cached per rig.
struct FxBones { const FigAsset* key; int lU, lF, rU, rF, hd, nk, sp, hp, lT, rT, lS, rS; float side; };
static FxBones fxCache[64]; static int fxN = 0;
static const FxBones* fxBones(const FigAsset& a) {
  for (int i = 0; i < fxN; i++) if (fxCache[i].key == &a) return &fxCache[i];
  if (fxN >= 64) return nullptr; FxBones& b = fxCache[fxN++]; b.key = &a;
  b.lU = figBone(a, "lUp", -1); b.lF = figBone(a, "lFore", -1); b.rU = figBone(a, "rUp", -1); b.rF = figBone(a, "rFore", -1); b.hd = figBone(a, "head", -1); b.nk = figBone(a, "neck", -1); b.sp = figBone(a, "spine", -1); b.hp = figBone(a, "hips", -1);
  b.lT = figBone(a, "lThigh", -1); b.rT = figBone(a, "rThigh", -1); b.lS = figBone(a, "lShin", -1); b.rS = figBone(a, "rShin", -1);
  b.side = (b.lT >= 0 && a.head[b.lT][0] < 0) ? -1.f : 1.f; if (b.nk < 0) b.nk = b.hd;
  if (b.lU < 0 || b.rU < 0 || b.lF < 0 || b.rF < 0 || b.hd < 0 || b.sp < 0 || b.hp < 0) b.key = nullptr;
  return &b;
}
static inline void fxRot(FigAnim& an, const FigAsset& a, int b, float ax, float ay, float az, float ang) {
  if (b < 0 || std::fabs(ang) < 1e-4f) return; float p[3] = {an.pose[b * 7 + 4], an.pose[b * 7 + 5], an.pose[b * 7 + 6]}; figAddRot(an, a, b, ax, ay, az, ang, p);
}
enum FxMode { FX_NONE, FX_HAMMER, FX_TEND, FX_TALK, FX_LOOK, FX_CROSS, FX_ATTN, FX_DRINK, FX_STRETCH };
// mode: activity, t: absolute time, w: blend weight (0..1), seed: per-NPC phase offset
static void fxApply(FigAnim& an, const FigAsset& a, int mode, float t, float w, float seed) {
  const FxBones* B = fxBones(a); if (!B || !B->key || w < 0.01f || mode == FX_NONE) return; const float sd = B->side; float s1 = std::sin(t * 5.4f + seed), c1 = std::cos(t * 5.4f + seed);
  auto R = [&](int b, float ax, float ay, float az, float ang) { fxRot(an, a, b, ax, ay, az, ang * w); };
  switch (mode) {
    case FX_HAMMER: {   // strike: right arm raised, snaps down; left hand steadies; torso follows
      float k = 0.5f + 0.5f * s1, snap = std::pow(k, 3.f);
      R(B->sp, 1, 0, 0, 0.16f + 0.09f * snap); R(B->hd, 1, 0, 0, 0.22f); R(B->rU, 1, 0, 0, -(0.55f + 0.95f * k)); R(B->rF, 1, 0, 0, -(0.9f + 0.7f * k - 0.5f * snap));
      R(B->lU, 1, 0, 0, -0.65f); R(B->lF, 1, 0, 0, -1.25f); R(B->lU, 0, 0, 1, -sd * 0.18f); figAddMove(an, a, B->hp, 0, -0.02f * w * snap, 0); break; }
    case FX_TEND: { // sweeping / raking: both hands forward and low, sway side to side
      float sw = std::sin(t * 1.7f + seed);
      R(B->sp, 1, 0, 0, 0.30f); R(B->sp, 0, 1, 0, 0.22f * sw); R(B->hd, 1, 0, 0, 0.18f);
      R(B->rU, 1, 0, 0, -0.62f); R(B->rF, 1, 0, 0, -0.55f); R(B->lU, 1, 0, 0, -0.55f); R(B->lF, 1, 0, 0, -0.7f); R(B->rU, 0, 1, 0, 0.22f * sw); R(B->lU, 0, 1, 0, 0.22f * sw); break; }
    case FX_TALK: { // expressive gesturing + head nods
      float g1 = std::sin(t * 2.1f + seed), g2 = std::sin(t * 3.3f + seed * 1.7f), g3 = std::sin(t * 1.3f + seed * 0.6f);
      R(B->rU, 1, 0, 0, -(0.55f + 0.22f * g1)); R(B->rU, 0, 0, 1, -sd * (0.28f + 0.14f * g2)); R(B->rF, 1, 0, 0, -(1.05f + 0.45f * g2));
      R(B->lU, 1, 0, 0, -(0.12f + 0.12f * std::max(0.f, g3))); R(B->lF, 1, 0, 0, -(0.35f + 0.5f * std::max(0.f, g3)));
      R(B->nk, 1, 0, 0, 0.06f + 0.07f * std::sin(t * 2.6f + seed)); R(B->hd, 0, 1, 0, 0.22f * std::sin(t * 0.7f + seed)); R(B->sp, 0, 1, 0, 0.06f * g1); break; }
    case FX_LOOK: { // weight shift + slow scanning glance
      float g = std::sin(t * 0.55f + seed), g2 = std::sin(t * 0.9f + seed * 2.f);
      R(B->nk, 0, 1, 0, 0.45f * g); R(B->hd, 0, 1, 0, 0.35f * g); R(B->hd, 1, 0, 0, 0.05f * g2); R(B->sp, 0, 0, 1, sd * 0.03f * std::sin(t * 0.5f + seed)); figAddMove(an, a, B->hp, sd * 0.03f * std::sin(t * 0.5f + seed), 0, 0); break; }
    case FX_CROSS: { // folded arms
      R(B->lU, 1, 0, 0, -0.62f); R(B->lU, 0, 0, 1, -sd * 0.34f); R(B->lF, 1, 0, 0, -2.0f); R(B->rU, 1, 0, 0, -0.62f); R(B->rU, 0, 0, 1, sd * 0.34f); R(B->rF, 1, 0, 0, -2.0f);
      R(B->nk, 0, 1, 0, 0.18f * std::sin(t * 0.4f + seed)); R(B->sp, 1, 0, 0, -0.03f); break; }
    case FX_ATTN: { // at attention: chest out, arms tight, tiny breathing
      R(B->sp, 1, 0, 0, -0.07f + 0.015f * std::sin(t * 1.5f)); R(B->lU, 1, 0, 0, 0.1f); R(B->rU, 1, 0, 0, 0.1f); R(B->lU, 0, 0, 1, -sd * 0.04f); R(B->rU, 0, 0, 1, sd * 0.04f); R(B->nk, 0, 1, 0, 0.08f * std::sin(t * 0.3f + seed)); break; }
    case FX_DRINK: { float k = 0.5f - 0.5f * std::cos(t * 0.8f + seed); R(B->rU, 1, 0, 0, -(0.4f + 0.7f * k)); R(B->rF, 1, 0, 0, -(0.8f + 1.0f * k)); R(B->nk, 1, 0, 0, -0.18f * k); break; }
    case FX_STRETCH: { float k = 0.5f - 0.5f * std::cos(t * 0.9f + seed); R(B->lU, 1, 0, 0, -2.6f * k); R(B->rU, 1, 0, 0, -2.6f * k); R(B->lU, 0, 0, 1, sd * 0.35f * k); R(B->rU, 0, 0, 1, -sd * 0.35f * k); R(B->sp, 1, 0, 0, -0.18f * k); R(B->nk, 1, 0, 0, -0.2f * k); break; }
  }
}
// pick a personality for an idle NPC (index hash + slow time slicing; schedule phase 1 = working hours, 2 = plaza)
static int fxPick(const Npc& n, int idx, int phase, float t) {
  if (n.talking) return FX_TALK; if (n.role >= R_MON) return FX_NONE;
  int cyc = ((int)(t * 0.09f + idx * 0.37f)) % 4; unsigned h = (unsigned)idx * 2654435761u >> 7;
  if (n.role == R_VILL) { if (phase == 1) return cyc == 3 ? FX_LOOK : ((h & 1) ? FX_HAMMER : FX_TEND); if (phase == 2) { static const int P2[4] = {FX_TALK, FX_LOOK, FX_CROSS, FX_DRINK}; return P2[(cyc + (int)(h & 3)) & 3]; } static const int P0[4] = {FX_LOOK, FX_STRETCH, FX_CROSS, FX_LOOK}; return P0[(cyc + (int)(h & 3)) & 3]; }
  static const int PG[4] = {FX_ATTN, FX_ATTN, FX_LOOK, FX_CROSS}; return PG[(cyc + (int)(h & 3)) & 3];
}
// player: airborne tuck / fall spread and landing crouch (additive, after locomotion)
struct AirState { float airW = 0, landK = 0, airT = 0; bool wasGround = true; } AIR;
static void fxPlayerAir(FigAnim& an, const FigAsset& a, bool ground, float vy, bool blocked, float dt) {
  const FxBones* B = fxBones(a); if (!B || !B->key) return;
  if (!ground && !blocked) { AIR.airT += dt; AIR.wasGround = false; } else { if (!AIR.wasGround && AIR.airT > 0.28f) AIR.landK = std::min(1.f, 0.35f + AIR.airT * 0.9f); AIR.wasGround = true; AIR.airT = 0; }
  float target = (!ground && !blocked && AIR.airT > 0.12f) ? 1.f : 0.f; AIR.airW += (target - AIR.airW) * (1.f - std::exp(-dt * (target > 0 ? 12.f : 9.f))); AIR.landK = std::max(0.f, AIR.landK - dt * 3.4f);
  float w = AIR.airW, sd = B->side;
  if (w > 0.01f) { float fall = clampf((-vy - 1.f) / 8.f, 0.f, 1.f);   // rising = tucked knees; falling = limbs spread
    fxRot(an, a, B->lT, 1, 0, 0, -(0.85f - 0.4f * fall) * w); fxRot(an, a, B->lS, 1, 0, 0, (1.05f - 0.5f * fall) * w); fxRot(an, a, B->rT, 1, 0, 0, (0.2f + 0.25f * fall) * w); fxRot(an, a, B->rS, 1, 0, 0, (0.55f) * w);
    fxRot(an, a, B->lU, 0, 0, 1, sd * (0.5f + 0.6f * fall) * w); fxRot(an, a, B->rU, 0, 0, 1, -sd * (0.5f + 0.6f * fall) * w); fxRot(an, a, B->lU, 1, 0, 0, -0.5f * w); fxRot(an, a, B->rU, 1, 0, 0, -0.5f * w); fxRot(an, a, B->sp, 1, 0, 0, 0.1f * w); }
  float k = AIR.landK; if (k > 0.01f) { float e = std::sin(k * 3.14159f * 0.5f); figAddMove(an, a, B->hp, 0, -0.26f * e, 0); fxRot(an, a, B->lT, 1, 0, 0, -0.9f * e); fxRot(an, a, B->rT, 1, 0, 0, -0.9f * e); fxRot(an, a, B->lS, 1, 0, 0, 1.2f * e); fxRot(an, a, B->rS, 1, 0, 0, 1.2f * e); fxRot(an, a, B->sp, 1, 0, 0, 0.3f * e); fxRot(an, a, B->lU, 0, 0, 1, sd * 0.4f * e); fxRot(an, a, B->rU, 0, 0, 1, -sd * 0.4f * e); }
}
