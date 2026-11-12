// v2.6 fauna rig: anatomy table, gait generator (walk / trot / gallop with planted feet), two-bone leg IK, emitter of articulated parts.
// Included inside the anonymous namespace of xgame.cpp from xfauna.inc (needs Sp, Fauna, SPD).  Zero per-frame heap allocation.
struct Ana { int body, head, tail; float nY, nZ, tY, tZ, fx, fy, fz, hx, hy, hz, f1, f2, h1, h2, wUf, wUh, wL; bool paw; float bodyY, lieY, hdY, hdZ, hdR, vT, vG, grazeP; };
// +Z forward.  neck pivot / tail pivot in body frame, hip joints (|x|,y,z), segment lengths (upper = hip->knee/hock, lower = knee/hock->ground), widths
const Ana ANA[SP_N] = {
  /* DOG    */ {K_AN_BDOG, K_AN_HDOG, K_AN_TDOG, 0.62f, 0.38f, 0.57f, -0.42f, 0.075f, 0.5f, 0.28f, 0.08f, 0.5f, -0.26f, 0.27f, 0.25f, 0.27f, 0.26f, 0.1f, 0.11f, 0.075f, true, 0.55f, 0.27f, 0.76f, 0.65f, 0.14f, 1.6f, 3.5f, 0.7f},
  /* CAT    */ {K_AN_BCAT, K_AN_HCAT, K_AN_TCAT, 0.3f, 0.17f, 0.26f, -0.22f, 0.045f, 0.25f, 0.14f, 0.05f, 0.25f, -0.15f, 0.12f, 0.13f, 0.12f, 0.13f, 0.06f, 0.065f, 0.045f, true, 0.27f, 0.13f, 0.38f, 0.28f, 0.07f, 1.2f, 2.8f, 0.6f},
  /* HEN    */ {K_AN_BHEN, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0.3f, 0.15f, 0.5f, 0.15f, 0.1f, 1.f, 2.f, 0.f},
  /* SHEEP  */ {K_AN_BSHEEP, K_AN_HSHEEP, -1, 0.74f, 0.4f, 0, 0, 0.1f, 0.62f, 0.3f, 0.11f, 0.62f, -0.3f, 0.3f, 0.28f, 0.3f, 0.3f, 0.14f, 0.15f, 0.09f, false, 0.65f, 0.34f, 0.8f, 0.7f, 0.15f, 1.3f, 2.8f, 1.2f},
  /* COW    */ {K_AN_BCOW, K_AN_HCOW, K_AN_TCOW, 1.14f, 0.74f, 1.38f, -0.9f, 0.2f, 0.98f, 0.45f, 0.22f, 0.98f, -0.5f, 0.52f, 0.46f, 0.52f, 0.48f, 0.2f, 0.22f, 0.17f, false, 0.96f, 0.52f, 1.04f, 1.5f, 0.28f, 1.5f, 3.2f, 0.95f},
  /* HORSE  */ {K_AN_BHORSE, K_AN_HHORSE, K_AN_THORSE, 1.4f, 0.6f, 1.46f, -0.88f, 0.17f, 1.2f, 0.5f, 0.19f, 1.2f, -0.52f, 0.72f, 0.52f, 0.72f, 0.54f, 0.2f, 0.22f, 0.16f, false, 1.2f, 0.68f, 1.9f, 1.45f, 0.26f, 2.4f, 5.8f, 1.35f},
  /* DEER   */ {K_AN_BDEER, K_AN_HDEER, -1, 1.1f, 0.52f, 0, 0, 0.12f, 0.98f, 0.4f, 0.13f, 0.98f, -0.42f, 0.5f, 0.48f, 0.5f, 0.5f, 0.13f, 0.15f, 0.1f, false, 1.0f, 0.5f, 1.6f, 1.0f, 0.17f, 1.8f, 4.5f, 1.2f},
  /* RABBIT */ {K_AN_BRABBIT, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0.12f, 0.06f, 0.2f, 0.15f, 0.07f, 1.f, 2.f, 0.f}};
// gait tables (order LF, RF, LH, RH): stand/walk lateral sequence, trot diagonal pairs, transverse gallop
const float G_DUTY[4] = {0.66f, 0.66f, 0.5f, 0.3f}, G_H[4] = {0.f, 0.36f, 0.46f, 0.66f}, G_LIFT[4] = {0.f, 0.075f, 0.1f, 0.2f};
const float G_OFF[4][4] = {{0.25f, 0.75f, 0.f, 0.5f}, {0.25f, 0.75f, 0.f, 0.5f}, {0.f, 0.5f, 0.5f, 0.f}, {0.5f, 0.62f, 0.f, 0.12f}};
// advance the gait of one animal (called from the simulation tick with the animal's actual speed)
void gaitUpdate(Fauna& f, float dt, float spd) {
  const Ana& A = ANA[f.sp]; float Lt = std::max(0.2f, A.f1 + A.f2); f.spdS += (spd - f.spdS) * std::min(1.f, dt * 7.f); float s = f.spdS;
  if (f.sp == SP_HEN || f.sp == SP_RABBIT) { if (spd > 0.15f) f.cyc += dt * (f.sp == SP_HEN ? 3.2f : 2.4f) * std::min(1.8f, 0.6f + spd * 0.25f); else f.cyc += dt * 0.2f; f.cyc -= std::floor(f.cyc); return; }
  int g = s < 0.1f ? 0 : s < A.vT ? 1 : s < A.vG ? 2 : 3; float k = std::min(1.f, dt * (g == 0 ? 6.f : 9.f));
  f.gDuty += (G_DUTY[g] - f.gDuty) * k; f.gH += (G_H[g] - f.gH) * k; f.gLift += (G_LIFT[g] - f.gLift) * k; for (int l = 0; l < 4; l++) { float d = G_OFF[g][l] - f.gOff[l]; f.gOff[l] += d * std::min(1.f, dt * 5.f); }
  float Lc = std::max(0.3f, 2.f * std::max(0.05f, f.gH) * Lt / std::max(0.2f, f.gDuty)); float fc = g == 0 ? 0.f : std::min(4.6f, s / Lc); f.cyc += fc * dt; f.cyc -= std::floor(f.cyc);
  float gal = smooth(A.vG * 0.85f, A.vG * 1.15f, s), trot = smooth(A.vT * 0.8f, A.vT * 1.1f, s) * (1.f - gal); float c2 = std::cos(f.cyc * TAU * 2.f), c1 = std::sin(f.cyc * TAU + 0.9f);
  float moving = smooth(0.1f, 0.6f, s);
  f.bob = Lt * (0.014f * (1.f - gal) * (0.5f + trot) * c2 * moving + 0.055f * gal * c1) * 0.9f; f.pit = 0.012f * std::sin(f.cyc * TAU) * moving + 0.07f * gal * std::cos(f.cyc * TAU + 0.9f);
}
// 2-bone IK in the sagittal plane.  Hip at origin, target (dz forward, dy DOWN).  Angles are measured from straight down, positive = foot swings backward (rotX convention).
void legIK(float L1, float L2, float dz, float dy, bool kneeFwd, float& a1, float& a2) {
  float d = std::sqrt(dz * dz + dy * dy), dm = L1 + L2 - 0.004f; if (d > dm) { dz *= dm / d; dy *= dm / d; d = dm; } d = std::max(d, std::fabs(L1 - L2) + 0.01f);
  float alpha = std::atan2(-dz, dy); float cA = clampf((L1 * L1 + d * d - L2 * L2) / (2.f * L1 * d), -1.f, 1.f); float A = std::acos(cA);
  a1 = kneeFwd ? alpha - A : alpha + A; float kz = -L1 * std::sin(a1), ky = -L1 * std::cos(a1);   // knee (z forward, y up)
  float wz = dz - kz, wy = -dy - ky; a2 = std::atan2(-wz, -wy);
}
// emit all parts of one animal.  f.x/y/z/face/sc are the placement.  saddle: draw tack (ridden horse).
int emitAnimal(xr::Dyn* out, int k, int cap, const Fauna& f, bool saddle, float tAbsNow) {
  const Ana& A = ANA[f.sp]; if (k > cap - 16) return k; const SpDef& sd = SPD[f.sp];
  auto put = [&](int kind, const M4& m, float r, float g, float b) { if (k >= cap) return; xr::Dyn& o = out[k++]; o.kind = kind; o.m = m; o.mode = 0.f; o.tint[0] = r; o.tint[1] = g; o.tint[2] = b; };
  float hit = f.hitT > 0 ? f.hitT * 2.4f : 0.f; float tr = f.tint[0] * (1.f + hit), tg = f.tint[1] * (1.f - hit * 0.3f), tb = f.tint[2] * (1.f - hit * 0.3f);
  float dead = f.alive ? 0.f : smooth(0.f, 0.45f, f.deadT), sink = f.alive ? 0.f : smooth(36.f, 45.f, f.deadT) * 0.6f; float sc = f.sc;
  M4 base = mul(translate(f.x, f.y - sink, f.z), mul(rotY(f.face), scale4(sc, sc, sc)));
  if (dead > 0.f) { float a = dead * 1.5708f, Y = A.bodyY; float c = std::cos(a), s2 = std::sin(a); base = mul(base, mul(translate(0, dead * (Y + Y * 0.25f), 0), mul(translate(Y, 0, 0), mul(rotZ(a), translate(-Y, 0, 0))))); (void)c; (void)s2; }
  float lie = f.alive ? f.lie : 0.f, drop = lie * (A.bodyY - A.lieY); float bob = f.alive ? f.bob : 0.f, pit = f.alive ? f.pit : 0.f;
  if (f.sp == SP_HEN) {   // single mesh body, pecking pitch, two legs
    float peck = f.alive ? f.graze * std::max(0.f, std::sin(tAbsNow * 6.f + f.ph)) * 0.55f : 0.f; float b2 = f.alive && f.spdS > 0.2f ? std::fabs(std::sin(f.cyc * TAU * 2.f)) * 0.012f : 0.f;
    M4 bm = mul(base, mul(translate(0, 0.27f + b2, 0), mul(rotX(peck + (f.spdS > 2.f ? 0.1f : 0.f)), translate(0, -0.27f, 0)))); put(K_AN_BHEN, bm, tr, tg, tb);
    for (int l = 0; l < 2; l++) { float side = l ? 1.f : -1.f; float amp = f.alive ? clampf(f.spdS / 1.2f, 0.f, 1.f) * 0.7f : 0.15f; float sw = std::sin(f.cyc * TAU + l * PI) * amp; if (!f.alive) sw = l ? 0.4f : -0.3f;
      M4 lm = mul(base, mul(translate(side * 0.045f, 0.21f + b2, 0.01f), mul(rotX(sw), scale4(sd.legW, sd.legLen, sd.legW)))); put(K_FA_LEG, lm, 0.95f, 0.72f, 0.3f); } return k; }
  if (f.sp == SP_RABBIT) {
    float hp = f.alive && f.spdS > 0.2f ? std::sin(f.cyc * TAU) : 0.f; float hy = std::max(0.f, hp) * (0.1f + 0.12f * clampf(f.spdS / 6.f, 0.f, 1.f)); float pt = -hp * 0.35f * clampf(f.spdS / 2.f, 0.f, 1.f); float sit = f.alive && f.spdS < 0.2f ? 0.f : 0.f;
    float nib = f.alive && f.spdS < 0.2f ? 0.03f * std::sin(tAbsNow * 9.f + f.ph) : 0.f; float gr = f.graze * 0.3f;
    M4 bm = mul(base, mul(translate(0, hy + sit, 0), mul(rotX(pt + gr + nib), translate(0, 0, 0)))); put(K_AN_BRABBIT, bm, tr, tg, tb); return k; }
  float bodyYnow = A.bodyY + bob - drop; M4 rbody = mul(translate(0, bodyYnow, 0), mul(rotX(pit), translate(0, -A.bodyY, 0))); M4 bm = mul(base, rbody);
  put(A.body, bm, tr, tg, tb);
  // head / neck: look yaw, graze pitch, gait nod, chew
  { float chew = f.graze > 0.85f ? 0.05f * std::sin(tAbsNow * 6.5f + f.ph) * (A.grazeP > 0.8f ? 1.f : 0.5f) : 0.f; float nod = f.alive ? 0.045f * std::sin(f.cyc * TAU + 0.5f) * smooth(0.1f, 1.f, f.spdS) * (1.f - smooth(A.vG * 0.8f, A.vG, f.spdS)) : 0.f;
    float pt = f.headP + f.graze * A.grazeP + chew + nod + dead * 0.6f + lie * 0.18f; M4 hm = mul(bm, mul(translate(0, A.nY, A.nZ), mul(rotY(f.alive ? f.look : 0.f), rotX(pt)))); if (A.head >= 0) put(A.head, hm, tr, tg, tb); }
  if (A.tail >= 0) { float sp = f.spdS; float sw = std::sin(tAbsNow * (f.sp == SP_DOG ? (f.wag > 0.f ? 12.f : 2.f) : 1.7f) + f.ph) * (f.sp == SP_DOG ? (f.wag > 0.f ? 0.55f : 0.12f) : 0.22f) * (1.f + sp * 0.04f);
    float up = f.sp == SP_HORSE || f.sp == SP_COW ? -0.15f - 0.5f * smooth(A.vT, A.vG * 1.2f, sp) : f.sp == SP_DOG ? -0.2f * smooth(1.f, 3.f, sp) : 0.f; M4 tm = mul(bm, mul(translate(0, A.tY, A.tZ), mul(rotY(sw), rotX(up + (f.sp == SP_COW ? 0.05f * std::sin(tAbsNow * 2.3f + f.ph) : 0.f))))); put(A.tail, tm, tr, tg, tb); }
  if (saddle) put(K_AN_SADDLE, bm, 1.f, 1.f, 1.f);
  // legs: gait -> foot targets -> 2-bone IK (feet stay planted through the stance phase)
  float Lt = A.f1 + A.f2, h = f.alive ? f.gH * Lt : 0.f; float cp = std::cos(pit), sp2 = std::sin(pit);
  for (int l = 0; l < 4; l++) { bool fore = l < 2; float side = (l & 1) ? 1.f : -1.f; float hx = (fore ? A.fx : A.hx) * side, hy = fore ? A.fy : A.hy, hz = fore ? A.fz : A.hz; float L1 = fore ? A.f1 : A.h1, L2 = fore ? A.f2 : A.h2, wU = (fore ? A.wUf : A.wUh) * 1.3f;
    float ry = hy - A.bodyY, rz = hz; float wy = A.bodyY + bob - drop + ry * cp - rz * sp2, wz = ry * sp2 + rz * cp;   // hip in the yaw frame (body pitched about its centre)
    float a1, a2;
    if (f.alive) {
      float ph = f.cyc + f.gOff[l]; ph -= std::floor(ph); float zf, yl; if (ph < f.gDuty) { float u = ph / f.gDuty; zf = h * (1.f - 2.f * u); yl = 0.f; } else { float s = (ph - f.gDuty) / (1.f - f.gDuty); float e = s * s * (3.f - 2.f * s); zf = -h + 2.f * h * e; yl = f.gLift * Lt * std::pow(std::sin(PI * s), 0.85f); }
      float tz = wz + zf + (fore ? 0.f : -0.02f * Lt), ty = yl;
      if (lie > 0.01f) { tz += lie * (fore ? 0.2f : -0.14f) * Lt; ty += lie * (fore ? 0.06f : 0.08f) * Lt; }
      float graze = f.graze * (fore && f.sp != SP_HEN && A.grazeP > 0.9f && f.spdS < 0.3f ? 1.f : 0.f); if (graze > 0.01f) { tz += graze * ((l == 0) ? 0.22f : -0.04f) * Lt; ty += graze * ((l == 0) ? 0.04f : 0.f); }
      legIK(L1, L2, tz - wz, wy - ty, fore, a1, a2);
    } else { a1 = (l & 1) ? 0.55f : -0.35f; a2 = (l & 1) ? 0.3f : -0.5f; wy = A.bodyY; }
    float ux = std::sin(a1) * 0.f; (void)ux; float kz = wz - L1 * std::sin(a1), ky = wy - L1 * std::cos(a1);
    M4 um = mul(base, mul(translate(hx, wy, wz), mul(rotX(a1), scale4(wU, L1, wU)))); put(fore ? K_AN_UPF : K_AN_UPH, um, tr * 0.97f, tg * 0.97f, tb * 0.97f);
    M4 lm = mul(base, mul(translate(hx, ky, kz), mul(rotX(a2), scale4(A.wL * 1.22f, L2, A.wL * 1.22f)))); put(A.paw ? K_AN_LOP : K_AN_LOH, lm, tr, tg, tb); }
  return k;
}
