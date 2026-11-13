// v3.4 skinned fauna: Sketchfab sculpts (decimated + re-rigged by tools/glb_animals.py) drawn through the figure pipeline
// (GPU skinning, baked quadruped clips) instead of the procedural lofted parts.  Included in the anonymous namespace of
// xgame.cpp right after xfauna.inc (needs Fauna, ANA, FA, P, K, rideH/rideF, FAV/nFAV).  Zero per-frame heap allocation:
// 40 pooled FigAnim slots keyed by fauna index, recycled least-recently-drawn.
static FigAsset FAF[7];
// desired overall height (m, head up, at sc = 1) per species; hen / rabbit stay procedural
const float FAH[SP_N] = {0.66f, 0.34f, 0.f, 0.86f, 1.52f, 2.05f, 1.78f, 0.f};
void faunaFigLoad() {
  memset(FAV, 0, sizeof FAV); memset(nFAV, 0, sizeof nFAV);
  static const struct { int sp; const char* key; } L[7] = {{SP_DOG, "animals/husky"}, {SP_CAT, "animals/cat"}, {SP_SHEEP, "animals/sheep"}, {SP_COW, "animals/cow"},
    {SP_HORSE, "animals/horse_bay"}, {SP_HORSE, "animals/horse_black"}, {SP_DEER, "animals/stag"}};
  for (int i = 0; i < 7; i++) if (figLoad(FAF[i], L[i].key)) { int s = L[i].sp; if (nFAV[s] < 2) FAV[s][nFAV[s]++] = &FAF[i]; if (getenv("LF_MEM")) printf("fauna fig %-20s h=%.2f nb=%d clips=%d\n", L[i].key, FAF[i].height, FAF[i].nb, FAF[i].nclips); }
}
// per-instance colour variation from one sculpt (hue / value tints in the shader replace separate Blender copies)
static inline float fr1(float x) { return x - std::floor(x); }
static void faTint(const Fauna& f, int variant, float* t) {
  float h = fr1(f.ph * 91.7f + 0.13f), j = 0.94f + 0.12f * fr1(f.ph * 53.9f); float r = 1, g = 1, b = 1;
  switch (f.sp) {
    case SP_DOG: if (h < .25f) { r = 1.15f; g = .95f; b = .8f; } else if (h < .45f) { r = .62f; g = .62f; b = .66f; } else if (h < .6f) { r = 1.3f; g = 1.28f; b = 1.25f; } break;
    case SP_CAT: if (h < .25f) { r = .45f; g = .45f; b = .47f; } else if (h < .5f) { r = 1.3f; g = 1.0f; b = .7f; } else if (h < .65f) { r = 1.25f; g = 1.25f; b = 1.25f; } break;
    case SP_SHEEP: if (h < .12f) { r = .32f; g = .3f; b = .3f; } else if (h < .3f) { r = .82f; g = .7f; b = .56f; } break;
    case SP_COW: if (h < .25f) { r = 1.35f; g = 1.3f; b = 1.25f; } else if (h < .45f) { r = .55f; g = .5f; b = .48f; } else if (h < .6f) { r = 1.2f; g = .95f; b = .8f; } break;
    case SP_HORSE: if (variant == 0) { if (h < .3f) { r = 1.25f; g = 1.1f; b = .9f; } else if (h < .5f) { r = .7f; g = .68f; b = .66f; } } else { if (h < .3f) { r = 1.8f; g = 1.8f; b = 1.85f; } else if (h < .45f) { r = 1.3f; g = 1.2f; b = 1.1f; } } break;
    case SP_DEER: if (h < .35f) { r = 1.1f; g = .95f; b = .8f; } else if (h < .6f) { r = .85f; g = .85f; b = .8f; } break;
    default: break; }
  t[0] = r * j; t[1] = g * j; t[2] = b * j;
}
static inline float faScale(const Fauna& f, const FigAsset& a) { return FAH[f.sp] * f.sc / std::max(0.2f, a.height); }
// returns the new figure count.  `ride` draws the ridden horse copy (rideF), pool index MAXF.
int faunaFigs(FigDraw* fd, int nfd, int cap, float dt) {
  if (CI.in) return nfd;
  static FigAnim FAN[40]; static int owner[40], slotOf[MAXF + 1]; static unsigned stamp[40], frame = 0; static bool init = false;
  if (!init) { init = true; for (int i = 0; i < 40; i++) owner[i] = -1; for (int i = 0; i <= MAXF; i++) slotOf[i] = -1; }
  frame++; int n = 0; V3 cf = norm(P.tgt - P.eye);
  auto one = [&](const Fauna& f, int idx, float d) {
    int nv = nFAV[f.sp]; if (!nv || nfd >= cap) return; int variant = nv > 1 ? (int)(fr1(f.ph * 13.37f) * nv * 0.999f) : 0; const FigAsset* a = FAV[f.sp][variant]; if (!a || !a->ok) return;
    int s = slotOf[idx]; if (s < 0 || owner[s] != idx) { unsigned best = ~0u; s = -1; for (int k = 0; k < 40; k++) if (stamp[k] < best) { best = stamp[k]; s = k; } if (s < 0 || best == frame) return; FAN[s] = FigAnim(); FAN[s].t = fr1(f.ph) * 9.f; FAN[s].id = (uint32_t)(7000 + idx); owner[s] = idx; slotOf[idx] = s; }
    stamp[s] = frame; FigAnim& an = FAN[s]; const Ana& A = ANA[f.sp]; float S = faScale(f, *a);
    figAnimUpdate(an, *a, dt, f.alive ? f.spdS / S : 0.f, false, -1, -1.f, f.hitT > 0.05f ? 1.f : 0.f, false, 0.f, -1.f, 0.f);
    int nk = figBone(*a, "neck", -1), rt = figBone(*a, "root", 0); float lie = f.alive ? f.lie : 0.f;
    if (lie > 0.01f) figAddMove(an, *a, rt, 0.f, -lie * (A.bodyY - A.lieY) * f.sc / S * 0.9f, 0.f);
    if (nk >= 0) {
      if (f.alive) { float pt = f.headP + f.graze * A.grazeP * 0.85f + lie * 0.18f; figAddRot(an, *a, nk, 1, 0, 0, pt, a->head[nk]); if (std::fabs(f.look) > 1e-3f) figAddRot(an, *a, nk, 0, 1, 0, f.look, a->head[nk]); }
      else figAddRot(an, *a, nk, 1, 0, 0, 0.5f, a->head[nk]); }
    figAnimPose(an, *a);
    float dead = f.alive ? 0.f : smooth(0.f, 0.45f, f.deadT), sink = f.alive ? 0.f : smooth(36.f, 45.f, f.deadT) * 0.6f;
    M4 base = mul(translate(f.x, f.y - sink, f.z), mul(rotY(f.face), scale4(S, S, S)));
    if (dead > 0.f) { float ang = dead * 1.5708f, Y = A.bodyY * f.sc / S; base = mul(base, mul(translate(0, dead * Y * 1.25f, 0), mul(translate(Y, 0, 0), mul(rotZ(ang), translate(-Y, 0, 0))))); }
    FigDraw& o = fd[nfd++]; o.a = a; o.mat = an.mat; o.model = base; o.dissolve = 0.f; o.disH = 1.f; o.glow = 1.f; o.shadow = d < 45.f; o.dim = 1.f; o.lantern = false; o.vm = false; o.noBaked = false;
    o.disCol[0] = o.disCol[1] = o.disCol[2] = 0.f; float hit = f.hitT > 0 ? f.hitT * 2.4f : 0.f; o.flash[0] = hit * 1.2f; o.flash[1] = hit * 0.5f; o.flash[2] = hit * 0.3f; faTint(f, variant, o.tint); n++; };
  if (rideH && K.mounted && P.view != 0 && nFAV[rideF.sp]) one(rideF, MAXF, 0.f);
  for (int pass = 0; pass < 2; pass++) for (int i = 0; i < nFA; i++) {
    const Fauna& f = FA[i]; if (!f.used || !nFAV[f.sp]) continue; float dx = f.x - P.eye.x, dz = f.z - P.eye.z, d2 = dx * dx + dz * dz; float lim = SPD[f.sp].legLen < 0.3f ? 70.f : 125.f;
    if (d2 > lim * lim) continue; float d = std::sqrt(d2) + 1e-3f; if (d > 14.f && (dx * cf.x + dz * cf.z) / d < -0.05f) continue; if ((pass == 0) != (d < 48.f)) continue; if (n >= 44) return nfd; one(f, i, d); }
  return nfd;
}
