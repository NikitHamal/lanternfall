// Explorer HUD (included inside namespace ui, after the shared UI helpers).
static void drawExploreHud() {
  using ex::H; const float W_ = (float)SW, H_ = (float)SH; const float M = 8 * S;
  if (H.rain > 0.03f) { const float rw = H.rain; rect(0, 0, W_, H_, {0.30f, 0.36f, 0.46f, 0.10f * rw});
    for (int i = 0; i < 150; i++) { float u = (float)i * 0.61803f; u -= std::floor(u); float v = (float)i * 0.7548776f; v -= std::floor(v); float sp = 1.1f + 0.9f * v; float y = gfx::time_ * sp + (float)i * 0.37f; y -= std::floor(y);
      float len = (16.f + 26.f * v) * S; rect(u * W_, y * (H_ + len) - len, std::max(1.f, 1.1f * S), len, {0.78f, 0.84f, 0.95f, (0.10f + 0.12f * v) * rw}); } }
  if (H.hurt > 0.02f) { float a = std::min(0.45f, H.hurt * 0.5f); rect(0, 0, W_, 18 * S, {0.6f, 0.02f, 0.02f, a * 0.6f}); rect(0, H_ - 18 * S, W_, 18 * S, {0.6f, 0.02f, 0.02f, a * 0.6f}); rect(0, 0, 14 * S, H_, {0.6f, 0.02f, 0.02f, a * 0.5f}); rect(W_ - 14 * S, 0, 14 * S, H_, {0.6f, 0.02f, 0.02f, a * 0.5f}); }
  if (H.hp < H.maxHp * 0.3f && H.hp > 0) { float a = 0.18f + 0.1f * std::sin(gfx::time_ * 6.f); rect(0, 0, W_, 10 * S, {0.5f, 0, 0, a}); rect(0, H_ - 10 * S, W_, 10 * S, {0.5f, 0, 0, a}); }
  if (H.map) {   // full map overlay
    { Col dk{0.02f, 0.03f, 0.05f, 0.82f}; rect(0, 0, W_, H.mapPy, dk); rect(0, H.mapPy + H.mapSize, W_, H_ - H.mapPy - H.mapSize, dk); rect(0, H.mapPy, H.mapPx, H.mapSize, dk); rect(H.mapPx + H.mapSize, H.mapPy, W_ - H.mapPx - H.mapSize, H.mapSize, dk); } text("THE REALM", W_ / 2, 6 * S, S * 2.2f, hexc(0xf1dfb6), 1); char b[64]; snprintf(b, 64, "DISCOVERED %d / %d    M OR ESC TO CLOSE", H.disc, H.discTotal); text(b, W_ / 2, H_ - 12 * S, S * 0.9f, hexc(0xb3a88e), 1);
    frame(H.mapPx - 2, H.mapPy - 2, H.mapSize + 4, H.mapSize + 4, hexc(0x6a5a3c), 2);
    for (int i = 0; i < ex::poiCount(); i++) { if (!ex::poiFound(i)) continue; float x, z, sx, sy; ex::poiPos(i, x, z); if (!ex::mapToScreen(x, z, sx, sy)) continue; text(ex::poiName(i), sx, sy - 12 * S, S * 0.62f, hexc(0xf4ead0), 1); }
    float px, pz, sx, sy; (void)px; (void)pz; (void)sx; (void)sy; text("YOU", H.mapPx + H.mapSize * 0.5f, H.mapPy + H.mapSize * 0.5f - 200 * S, 0.01f, hexc(0xffffff), 1);
    return;
  }
  if (!H.show) { return; }
  // ---- place name + nation + kingdom treasury (top-left)
  { float a = std::min(1.f, 0.55f + H.locT); text(H.place, M, M, S * 1.2f, hexc(0xf1dfb6, a)); text(H.nation, M, M + 13 * S, S * 0.75f, hexc(0xb3a88e, a));
    static char dl[64]; static int ld = -1, lk = -1, lr = -1;
    if (ld != H.disc || lk != H.kills || lr != H.reclaimed) { ld = H.disc; lk = H.kills; lr = H.reclaimed; snprintf(dl, 64, "FOUND %d/%d   SLAIN %d   RIFTS %d/%d", H.disc, H.discTotal, H.kills, H.reclaimed, H.totalRifts); }
    text(dl, M, M + 22 * S, S * 0.68f, hexc(0x9a907c));
    float ry = M + 32 * S; rect(M - 2 * S, ry - 2 * S, 184 * S, 11 * S, {0.03f, 0.04f, 0.06f, 0.52f}); frame(M - 2 * S, ry - 2 * S, 184 * S, 11 * S, hexc(0x5a4c32, 0.7f));
    static char rs[64]; static int rw = -1, rst = -1, rf = -1, rg = -1;
    if (rw != H.wood || rst != H.stone || rf != H.food || rg != H.gold) { rw = H.wood; rst = H.stone; rf = H.food; rg = H.gold; snprintf(rs, 64, "WOOD %d   STONE %d   FOOD %d   GOLD %d", H.wood, H.stone, H.food, H.gold); }
    text(rs, M + 2 * S, ry, S * 0.68f, hexc(0xf2dfb2)); }
  // ---- compass (top-centre)
  { float cx = W_ / 2, cw = 118 * S; rect(cx - cw, M - 2 * S, cw * 2, 11 * S, {0, 0, 0, 0.32f}); float hd = -H.yaw * 57.2958f; hd = std::fmod(std::fmod(hd, 360.f) + 360.f, 360.f);   // bearing: 0 = north (-z), clockwise
    static const char* DIRS[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"}; for (int i = -9; i <= 9; i++) { float b = std::floor(hd / 22.5f + 0.5f) * 22.5f + i * 22.5f; float off = (b - hd) / 22.5f; float x = cx + off * 16 * S; if (std::fabs(x - cx) > cw - 6 * S) continue; int di = ((int)std::floor(b / 22.5f + 0.5f) % 16 + 16) % 16; float al = 1.f - std::fabs(x - cx) / cw; if (di % 2 == 0) text(DIRS[di / 2], x, M, S * (di % 4 == 0 ? 1.1f : 0.7f), di == 0 ? hexc(0xff7a5a, al) : hexc(0xf0dcb0, al), 1); else rect(x - 0.5f, M + 3 * S, 1, 3 * S, {0.9f, 0.85f, 0.7f, al * 0.7f}); }
    rect(cx - 1, M + 9 * S, 2, 3 * S, hexc(0xe0a33c)); }
  // ---- clock + minimap frame
  { float cx = H.mapPx + H.mapSize / 2; int hh = (int)H.hour, mm = (int)((H.hour - hh) * 60); static char cl[24]; static int lm = -1; if (lm != hh * 60 + mm) { lm = hh * 60 + mm; snprintf(cl, 24, "%02d:%02d", hh, mm); } bool day = H.hour > 6.f && H.hour < 18.f; text(cl, cx + 6 * S, 7 * S, S * 1.2f, hexc(day ? 0xf6d58a : 0x9ab4ff), 1); text(day ? "DAY" : "NIGHT", cx - 24 * S, 8 * S, S * 0.7f, hexc(day ? 0xe0a33c : 0x7f9cd6), 1);
    if (H.timeMul > 1.5f) { text("TIME >>", cx + 38 * S, 8 * S, S * 0.7f, hexc(0xff9a5a), 1); } }
  if (!H.inside) { float r = H.mapSize / 2, cx = H.mapPx + r, cy = H.mapPy + r; text("N", cx, H.mapPy + 1 * S, S * 0.8f, hexc(0xff7a5a), 1); (void)cy; }
  // ---- vitals: HP + Stamina + Arcane Mana (bottom-left)
  { float x = M, y = H_ - 48 * S; text(H.hero, x, y - 12 * S, S * 1.0f, hexc(0xe9c98a));
    rect(x - 2, y - 2, 132 * S + 4, 16 * S + 4, {0, 0, 0, 0.48f});
    bar(x, y, 132 * S, 6 * S, H.hp / std::max(1.f, H.maxHp), H.hp < H.maxHp * 0.3f ? hexc(0xe0453a) : hexc(0xc0504a));
    bar(x, y + 7.5f * S, 132 * S, 3 * S, H.st / 100.f, hexc(0x8fb85a));
    bar(x, y + 12 * S, 132 * S, 3.5f * S, H.mana / std::max(1.f, H.maxMana), hexc(0x4aa8ff));
    static char hp[24]; static int lh = -1; int hv = (int)(H.hp + 0.99f); if (lh != hv) { lh = hv; snprintf(hp, 24, "%d", hv); } text(hp, x + 135 * S, y - 1 * S, S * 0.8f, hexc(0xf0dcb0)); }
  // ---- name plates
  for (int i = 0; i < ex::nLabels; i++) { const ex::Label& L = ex::labels[i]; float a = 0.95f; text(L.text, L.x, L.y - 10 * S, S * 0.75f, hexc(L.col, a), 1); if (L.hp >= 0) bar(L.x - 22 * S, L.y - 1 * S, 44 * S, 3 * S, L.hp, hexc(0xd8503f)); }
  // ---- reticle (first person)
  if (H.view == 0 || H.weapon == 1) {
    float cx = W_ / 2, cy = H_ / 2; float spread = H.weapon == 1 ? (1.f - H.draw) * 14.f * S * (H.zoom > 0.5f ? 0.6f : 1.f) + 3.f * S : 0.f; bool full = H.weapon == 1 && H.draw > 0.92f; Col rc = full ? Col{1.f, 0.82f, 0.3f, 0.95f} : Col{1, 1, 1, 0.8f};
    rect(cx - 1, cy - 1, 2, 2, rc); if (H.weapon == 1) { float t = std::max(1.f, S * 0.7f); rect(cx - spread - 5 * S, cy - 0.5f * t, 5 * S, t, rc); rect(cx + spread, cy - 0.5f * t, 5 * S, t, rc); rect(cx - 0.5f * t, cy - spread - 5 * S, t, 5 * S, rc); rect(cx - 0.5f * t, cy + spread, t, 5 * S, rc); }
  }
  if (H.hitT > 0.01f && !H.map) { float cx = W_ / 2, cy = H_ / 2, a = std::min(1.f, H.hitT * 5.f); Col hc = H.hitKill ? Col{1.f, 0.25f, 0.2f, a} : H.hitZone == 1 ? Col{1.f, 0.82f, 0.25f, a} : Col{1, 1, 1, a}; float o = (H.hitZone == 1 ? 9.f : 6.f) * S, l = 4.f * S, t = std::max(1.f, S * 0.8f);
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) for (int k = 0; k < 4; k++) rect(cx + sx * (o + k * 0.7f * S) - t * 0.5f, cy + sy * (o + k * 0.7f * S) - t * 0.5f, t, t, hc); (void)l; }
  if (H.weapon == 1 && H.draw > 0.01f) { float w = 70 * S, x = W_ / 2 - w / 2, y = H_ * 0.62f; rect(x - 2, y - 2, w + 4, 5 * S + 4, {0, 0, 0, 0.4f}); bar(x, y, w, 5 * S, H.draw, H.draw > 0.92f ? hexc(0xffd25a) : hexc(0xd8c090)); }
  // ---- weapon / spell / warband / mount (bottom-left, above vitals)
  { float x = M, y = H_ - 76 * S;
    static const char* SPN[4] = {"FIREBALL", "FROST NOVA", "SOLAR AEGIS", "STORM LANCE"};
    static const char* ORDN[4] = {"FOLLOW", "HOLD LINE", "CHARGE", "GARRISON"};
    static char wl[64]; static int lw = -1, la = -1, lsp = -1;
    static int lpo = -1, lwt = -1, lat = -1; if (lw != H.weapon || la != H.arrows || lsp != H.spell || lpo != H.potions || lwt != H.wpnTier || lat != H.armorTier) { lw = H.weapon; la = H.arrows; lsp = H.spell; lpo = H.potions; lwt = H.wpnTier; lat = H.armorTier; if (H.weapon) snprintf(wl, 64, "BOW (%d) [Q] %s [U] DRAUGHT %d  W%d A%d", H.arrows, SPN[H.spell & 3], H.potions, H.wpnTier, H.armorTier); else snprintf(wl, 64, "BLADE [Q] %s [U] DRAUGHT %d  W%d A%d", SPN[H.spell & 3], H.potions, H.wpnTier, H.armorTier); }
    text(wl, x, y, S * 0.76f, hexc(H.weapon ? 0xe8d49a : 0xd8e6ff));
    static char wb[64]; static int ltr = -1, lor = -1, lmt = -1;
    if (ltr != H.troops || lor != H.order || lmt != (int)H.mounted) { ltr = H.troops; lor = H.order; lmt = (int)H.mounted; snprintf(wb, 64, "WARBAND %d [%s]  %s", H.troops, ORDN[H.order & 3], H.mounted ? "HART MOUNTED" : "[K] MOUNT"); }
    text(wb, x, y - 11 * S, S * 0.72f, hexc(H.troops > 0 ? 0x86d8ff : 0xa8a08e));
    if (H.wanted > 0) { float p = 0.6f + 0.4f * std::sin(gfx::time_ * 8.f); for (int i = 0; i < H.wanted; i++) text("*", x + i * 9 * S, y - 24 * S, S * 1.8f, hexc(0xff4a3a, p)); text("WANTED", x + (H.wanted * 9 + 4) * S, y - 20 * S, S * 0.8f, hexc(0xff7a5a, p)); } }
  if (H.inside) { float cx = H.mapPx + H.mapSize / 2; rect(cx - 42 * S, 24 * S, 84 * S, 13 * S, {0.04f, 0.04f, 0.05f, 0.65f}); frame(cx - 42 * S, 24 * S, 84 * S, 13 * S, hexc(0x6a5a3c)); text("INDOORS", cx, 26 * S, S * 0.85f, hexc(0xe9c98a), 1); }
  // ---- nightly siege indicator & boss bar (top-centre)
  if (H.siegeLeft > 0) {
    float y = 24 * S; static char sb[56]; static int lsl = -1, lnw = -1;
    if (lsl != H.siegeLeft || lnw != H.nightWave) { lsl = H.siegeLeft; lnw = H.nightWave; snprintf(sb, 56, "NIGHT %d DEMON SIEGE  -  %d REMAINING", H.nightWave, H.siegeLeft); }
    rect(W_ / 2 - 118 * S, y - 2 * S, 236 * S, 12 * S, {0.22f, 0.03f, 0.03f, 0.72f}); frame(W_ / 2 - 118 * S, y - 2 * S, 236 * S, 12 * S, hexc(0xd8503f, 0.85f));
    text(sb, W_ / 2, y, S * 0.78f, hexc(0xffb48a), 1);
  }
  if (H.bossHp >= 0) { float w = 220 * S, x = W_ / 2 - w / 2, y = (H.siegeLeft > 0 ? 48 : 28) * S; text(H.bossName, W_ / 2, y - 11 * S, S * 1.1f, hexc(0xffb070), 1); bar(x, y, w, 5 * S, H.bossHp, hexc(0xd8503f)); }
  // ---- Kingdom Build Mode Blueprint Dock (bottom-centre)
  if (H.buildMode) {
    static const char* BN[6] = {"1 WALL", "2 TOWER", "3 GATE", "4 BARRACKS", "5 FARM", "6 BEACON"};
    static const char* BC[6] = {"12W 18S", "24W 28S 10G", "20W 32S 10G", "32W 24S 20G", "18W 10S", "14W 20S 15G"};
    float cw = 48 * S, gap = 2.5f * S, tot = 6 * cw + 5 * gap, x0 = W_ / 2 - tot / 2 + 12 * S, y0 = H_ - 46 * S;
    text("KINGDOM BUILD MODE   -   R ROTATE   LMB PLACE   RMB REPAIR   C EXIT", W_ / 2, y0 - 12 * S, S * 0.72f, hexc(H.ghostOk ? 0x98f0a8 : 0xff9a8a), 1);
    for (int i = 0; i < 6; i++) {
      float cx = x0 + i * (cw + gap); bool sel = (H.bsel == i);
      rect(cx, y0, cw, 22 * S, sel ? Col{0.12f, 0.16f, 0.11f, 0.90f} : Col{0.03f, 0.04f, 0.05f, 0.78f});
      frame(cx, y0, cw, 22 * S, sel ? hexc(H.ghostOk ? 0x6ee88a : 0xff6a5a) : hexc(0x5a4e36), sel ? 2.f : 1.f);
      text(BN[i], cx + cw / 2, y0 + 3 * S, S * 0.68f, hexc(sel ? 0xfff0c4 : 0xd8ceb4), 1);
      text(BC[i], cx + cw / 2, y0 + 12 * S, S * 0.58f, hexc(sel ? 0xbce8a8 : 0x9a927e), 1);
    }
  }
  // ---- banner (discovery / events)
  if (H.bannerT > 0) { float a = std::min(1.f, std::min(H.bannerT * 1.2f, (5.f - H.bannerT + 0.3f) * 2.f)); a = std::max(0.f, std::min(1.f, a)); float y = H_ * 0.2f; rect(W_ / 2 - 150 * S, y - 3 * S, 300 * S, 1, {0.88f, 0.64f, 0.24f, a * 0.8f}); text(H.banner, W_ / 2, y, S * 2.4f, hexc(0xf6d58a, a), 1); text(H.bannerSub, W_ / 2, y + 22 * S, S * 1.1f, hexc(0xe9ddc0, a), 1); rect(W_ / 2 - 150 * S, y + 34 * S, 300 * S, 1, {0.88f, 0.64f, 0.24f, a * 0.8f}); }
  // ---- quest journal tracker (below the minimap)
  if (H.qN > 0 && !H.talkOpen) { float w = 160 * S, x = W_ - w - M, y = H.mapPy + H.mapSize + 8 * S, h = (15 + H.qN * 25) * S; rect(x, y, w, h, {0.03f, 0.035f, 0.05f, 0.55f}); frame(x, y, w, h, hexc(0x6a5a3c, 0.8f)); text("JOURNAL", x + 6 * S, y + 2 * S, S * 0.66f, hexc(0xb3a88e));
    for (int i = 0; i < H.qN; i++) { float yy = y + (11 + i * 25) * S; text(H.qTitle[i], x + 6 * S, yy, S * 0.78f, hexc(H.qReady[i] ? 0x9ae07a : 0xe9c98a)); text(H.qProg[i], x + 6 * S, yy + 10 * S, S * 0.64f, hexc(H.qReady[i] ? 0xb8f0a0 : 0xcfc6ae)); } }
  // ---- talk menu (E): dialogue + options
  if (H.talkOpen) { float w = 470 * S, rowH = 13 * S; char l1[80], l2[80]; { const char* t = H.talkLine; size_t L = std::strlen(t); size_t cut = L; if (L > 64) { cut = 64; while (cut > 20 && t[cut] != ' ') cut--; } snprintf(l1, sizeof l1, "%.*s", (int)cut, t); snprintf(l2, sizeof l2, "%.76s", t + std::min(L, cut + (t[cut] == ' ' ? 1 : 0))); }
    float h = (44 + H.talkN * 12.2f) * S, x = W_ / 2 - w / 2, y = H_ - h - 16 * S; rect(x, y, w, h, {0.035f, 0.04f, 0.055f, 0.93f}); frame(x, y, w, h, hexc(0x8a7447), 2); rect(x, y, w, 15 * S, {0.20f, 0.16f, 0.09f, 0.9f}); text(H.talkName, x + 8 * S, y + 3 * S, S * 1.0f, hexc(0xf1dfb6));
    text(l1, x + 10 * S, y + 18 * S, S * 0.82f, hexc(0xf0e8d4)); text(l2, x + 10 * S, y + 27 * S, S * 0.82f, hexc(0xf0e8d4));
    for (int i = 0; i < H.talkN; i++) { float yy = y + 40 * S + i * 12.2f * S; char k[4] = {'[', (char)('1' + i), ']', 0}; text(k, x + 12 * S, yy, S * 0.8f, hexc(0xe9c98a)); text(H.talkOpt[i], x + 34 * S, yy, S * 0.8f, hexc(H.talkOk[i] ? 0xf4ead0 : 0x8a826e)); }
    text("ESC / E TO LEAVE", x + w - 8 * S, y + 3 * S, S * 0.62f, hexc(0xb3a88e), 2); (void)rowH; }
  // ---- interaction prompt + dialogue
  if (H.prompt[0] && !H.talkOpen) { float y = H_ * 0.66f, pw = std::max(140.f * S, ((float)std::strlen(H.prompt) * 6.2f + 22.f) * S); rect(W_ / 2 - pw * 0.5f, y - 3 * S, pw, 14 * S, {0, 0, 0, 0.58f}); frame(W_ / 2 - pw * 0.5f, y - 3 * S, pw, 14 * S, hexc(0x6a5a3c)); text(H.prompt, W_ / 2, y, S * 1.0f, hexc(0xf0dcb0), 1); }
  if (H.dlgT > 0) { float a = std::min(1.f, H.dlgT * 2.f); float w = 330 * S, h = 40 * S, x = W_ / 2 - w / 2, y = H_ - h - 14 * S; rect(x, y, w, h, {0.04f, 0.045f, 0.06f, 0.9f * a}); frame(x, y, w, h, hexc(0x6a5a3c, a)); text(H.dlgName, x + 8 * S, y + 5 * S, S * 1.0f, hexc(0xe9c98a, a)); text(H.dlgText, x + 8 * S, y + 19 * S, S * 0.95f, hexc(0xf0e8d4, a)); }
  // ---- hints (bottom-right)
  { static const char* hint = "WASD MOVE  B BOW  Q SPELL  Z NEXT SPELL  C BUILD  G GATHER/ENLIST  X ORDERS  K MOUNT  E USE  M MAP  V VIEW"; if (H.discTotal > 0 && H.hour >= 0 && !H.buildMode) text(hint, W_ - M, H_ - 10 * S, S * 0.58f, hexc(0x8a826e), 2); }
  if (H.dead) { rect(0, 0, W_, H_, {0.1f, 0, 0, std::min(0.6f, H.fade * 0.6f)}); text("YOU HAVE FALLEN", W_ / 2, H_ / 2 - 12 * S, S * 3.f, hexc(0xe0a090, std::min(1.f, H.fade)), 1); }
}
