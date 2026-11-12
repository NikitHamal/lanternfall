// Explorer HUD (included inside namespace ui, after the shared UI helpers).
static void drawExploreHud() {
  using ex::H; const float W_ = (float)SW, H_ = (float)SH; const float M = 8 * S;
  if (H.hurt > 0.02f) { float a = std::min(0.45f, H.hurt * 0.5f); rect(0, 0, W_, 18 * S, {0.6f, 0.02f, 0.02f, a * 0.6f}); rect(0, H_ - 18 * S, W_, 18 * S, {0.6f, 0.02f, 0.02f, a * 0.6f}); rect(0, 0, 14 * S, H_, {0.6f, 0.02f, 0.02f, a * 0.5f}); rect(W_ - 14 * S, 0, 14 * S, H_, {0.6f, 0.02f, 0.02f, a * 0.5f}); }
  if (H.hp < H.maxHp * 0.3f && H.hp > 0) { float a = 0.18f + 0.1f * std::sin(gfx::time_ * 6.f); rect(0, 0, W_, 10 * S, {0.5f, 0, 0, a}); rect(0, H_ - 10 * S, W_, 10 * S, {0.5f, 0, 0, a}); }
  if (H.map) {   // full map overlay
    rect(0, 0, W_, H_, {0.02f, 0.03f, 0.05f, 0.78f}); text("THE REALM", W_ / 2, 6 * S, S * 2.2f, hexc(0xf1dfb6), 1); char b[64]; snprintf(b, 64, "DISCOVERED %d / %d    M OR ESC TO CLOSE", H.disc, H.discTotal); text(b, W_ / 2, H_ - 12 * S, S * 0.9f, hexc(0xb3a88e), 1);
    frame(H.mapPx - 2, H.mapPy - 2, H.mapSize + 4, H.mapSize + 4, hexc(0x6a5a3c), 2);
    for (int i = 0; i < ex::poiCount(); i++) { if (!ex::poiFound(i)) continue; float x, z, sx, sy; ex::poiPos(i, x, z); if (!ex::mapToScreen(x, z, sx, sy)) continue; text(ex::poiName(i), sx, sy - 12 * S, S * 0.62f, hexc(0xf4ead0), 1); }
    float px, pz, sx, sy; (void)px; (void)pz; (void)sx; (void)sy; text("YOU", H.mapPx + H.mapSize * 0.5f, H.mapPy + H.mapSize * 0.5f - 200 * S, 0.01f, hexc(0xffffff), 1);
    return;
  }
  if (!H.show) { return; }
  // ---- place name + nation (top-left)
  { float a = std::min(1.f, 0.55f + H.locT); text(H.place, M, M, S * 1.2f, hexc(0xf1dfb6, a)); text(H.nation, M, M + 13 * S, S * 0.75f, hexc(0xb3a88e, a)); static char dl[48]; static int ld = -1, lk = -1; if (ld != H.disc || lk != H.kills) { ld = H.disc; lk = H.kills; snprintf(dl, 48, "DISCOVERED %d/%d   SLAIN %d", H.disc, H.discTotal, H.kills); } text(dl, M, M + 22 * S, S * 0.7f, hexc(0x8f8672)); }
  // ---- compass (top-centre)
  { float cx = W_ / 2, cw = 118 * S; rect(cx - cw, M - 2 * S, cw * 2, 11 * S, {0, 0, 0, 0.32f}); float hd = -H.yaw * 57.2958f; hd = std::fmod(std::fmod(hd, 360.f) + 360.f, 360.f);   // bearing: 0 = north (-z), clockwise
    static const char* DIRS[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"}; for (int i = -9; i <= 9; i++) { float b = std::floor(hd / 22.5f + 0.5f) * 22.5f + i * 22.5f; float off = (b - hd) / 22.5f; float x = cx + off * 16 * S; if (std::fabs(x - cx) > cw - 6 * S) continue; int di = ((int)std::floor(b / 22.5f + 0.5f) % 16 + 16) % 16; float al = 1.f - std::fabs(x - cx) / cw; if (di % 2 == 0) text(DIRS[di / 2], x, M, S * (di % 4 == 0 ? 1.1f : 0.7f), di == 0 ? hexc(0xff7a5a, al) : hexc(0xf0dcb0, al), 1); else rect(x - 0.5f, M + 3 * S, 1, 3 * S, {0.9f, 0.85f, 0.7f, al * 0.7f}); }
    rect(cx - 1, M + 9 * S, 2, 3 * S, hexc(0xe0a33c)); }
  // ---- clock + minimap frame
  { float cx = H.mapPx + H.mapSize / 2; int hh = (int)H.hour, mm = (int)((H.hour - hh) * 60); static char cl[24]; static int lm = -1; if (lm != hh * 60 + mm) { lm = hh * 60 + mm; snprintf(cl, 24, "%02d:%02d", hh, mm); } bool day = H.hour > 6.f && H.hour < 18.f; text(cl, cx + 6 * S, 7 * S, S * 1.2f, hexc(day ? 0xf6d58a : 0x9ab4ff), 1); text(day ? "DAY" : "NIGHT", cx - 24 * S, 8 * S, S * 0.7f, hexc(day ? 0xe0a33c : 0x7f9cd6), 1);
    if (H.timeMul > 1.5f) { text("TIME >>", cx + 38 * S, 8 * S, S * 0.7f, hexc(0xff9a5a), 1); } }
  { float r = H.mapSize / 2, cx = H.mapPx + r, cy = H.mapPy + r; text("N", cx, H.mapPy + 1 * S, S * 0.8f, hexc(0xff7a5a), 1); (void)cy; }
  // ---- vitals (bottom-left)
  { float x = M, y = H_ - 44 * S; text(H.hero, x, y - 12 * S, S * 1.0f, hexc(0xe9c98a)); rect(x - 2, y - 2, 130 * S + 4, 11 * S + 4, {0, 0, 0, 0.45f}); bar(x, y, 130 * S, 6 * S, H.hp / std::max(1.f, H.maxHp), H.hp < H.maxHp * 0.3f ? hexc(0xe0453a) : hexc(0xc0504a)); bar(x, y + 8 * S, 130 * S, 3 * S, H.st / 100.f, hexc(0x8fb85a)); static char hp[24]; static int lh = -1; int hv = (int)(H.hp + 0.99f); if (lh != hv) { lh = hv; snprintf(hp, 24, "%d", hv); } text(hp, x + 132 * S, y - 1 * S, S * 0.8f, hexc(0xf0dcb0)); }
  // ---- name plates
  for (int i = 0; i < ex::nLabels; i++) { const ex::Label& L = ex::labels[i]; float a = 0.95f; text(L.text, L.x, L.y - 10 * S, S * 0.75f, hexc(L.col, a), 1); if (L.hp >= 0) bar(L.x - 22 * S, L.y - 1 * S, 44 * S, 3 * S, L.hp, hexc(0xd8503f)); }
  // ---- reticle (first person)
  if (H.view == 0) { rect(W_ / 2 - 1, H_ / 2 - 1, 2, 2, {1, 1, 1, 0.8f}); }
  // ---- boss bar
  if (H.bossHp >= 0) { float w = 220 * S, x = W_ / 2 - w / 2, y = 26 * S; text(H.bossName, W_ / 2, y - 11 * S, S * 1.1f, hexc(0xffb070), 1); bar(x, y, w, 5 * S, H.bossHp, hexc(0xd8503f)); }
  // ---- banner (discovery / events)
  if (H.bannerT > 0) { float a = std::min(1.f, std::min(H.bannerT * 1.2f, (5.f - H.bannerT + 0.3f) * 2.f)); a = std::max(0.f, std::min(1.f, a)); float y = H_ * 0.2f; rect(W_ / 2 - 150 * S, y - 3 * S, 300 * S, 1, {0.88f, 0.64f, 0.24f, a * 0.8f}); text(H.banner, W_ / 2, y, S * 2.4f, hexc(0xf6d58a, a), 1); text(H.bannerSub, W_ / 2, y + 22 * S, S * 1.1f, hexc(0xe9ddc0, a), 1); rect(W_ / 2 - 150 * S, y + 34 * S, 300 * S, 1, {0.88f, 0.64f, 0.24f, a * 0.8f}); }
  // ---- interaction prompt + dialogue
  if (H.prompt[0]) { float y = H_ * 0.66f; rect(W_ / 2 - 70 * S, y - 3 * S, 140 * S, 14 * S, {0, 0, 0, 0.5f}); frame(W_ / 2 - 70 * S, y - 3 * S, 140 * S, 14 * S, hexc(0x6a5a3c)); text(H.prompt, W_ / 2, y, S * 1.0f, hexc(0xf0dcb0), 1); }
  if (H.dlgT > 0) { float a = std::min(1.f, H.dlgT * 2.f); float w = 330 * S, h = 40 * S, x = W_ / 2 - w / 2, y = H_ - h - 14 * S; rect(x, y, w, h, {0.04f, 0.045f, 0.06f, 0.9f * a}); frame(x, y, w, h, hexc(0x6a5a3c, a)); text(H.dlgName, x + 8 * S, y + 5 * S, S * 1.0f, hexc(0xe9c98a, a)); text(H.dlgText, x + 8 * S, y + 19 * S, S * 0.95f, hexc(0xf0e8d4, a)); }
  // ---- hints (bottom-right)
  { static const char* hint = "WASD MOVE  SHIFT RUN  SPACE JUMP  LMB ATTACK  RMB GUARD  E TALK/REST  M MAP  V VIEW  T TIME  N SKIP  F FLY  H HUD  ESC EXIT"; if (H.discTotal > 0 && H.hour >= 0) text(hint, W_ - M, H_ - 10 * S, S * 0.62f, hexc(0x7f7866), 2); }
  if (H.dead) { rect(0, 0, W_, H_, {0.1f, 0, 0, std::min(0.6f, H.fade * 0.6f)}); text("YOU HAVE FALLEN", W_ / 2, H_ / 2 - 12 * S, S * 3.f, hexc(0xe0a090, std::min(1.f, H.fade)), 1); }
}
