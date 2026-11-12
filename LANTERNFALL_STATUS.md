# LANTERNFALL native port - status & architecture decisions

Reference: https://github.com/loktar00/lanternfall (single-page three.js idle battler, ~350 KB of JS + 2.5 MB embedded music).

## Audit summary (what the original contains)
- `00-util` hash/noise/mulberry, road `pathZ`, terrain `terrH` (periodic over WORLD_L=4096)
- `30-world` procedural ground shader (road/mud/heath/water/pebbles), instanced grass (8600 clumps, wind), sky shader (clouds, stars, moon),
  prop generators (rock, standing stone, tree, wall, cairn, fence, waylight), 5 "reaches" (palettes, fog, props), moon+hemi+fill+lantern point light
- `40-post` HDR scene -> AO + exp-height fog + analytic point-light in-scatter -> 4-level bloom -> DOF + AgX tonemap + colour grade + grain
- `50-fx` particles (embers/sparks/dust/chips/frost/flash lights), ground marks (ring/tele/fire/frost), projectiles, floating text, loot
- `60-data` 6 hero classes x 3 skills, 9 monsters (3 bosses), cards, camp upgrades, boons (meta), gear grades
- `61-game` fixed 60 Hz sim: walk/fight/boss/breather/relight/choice/wipe state machine, hero & monster AI, skills, flare, steward auto-play, offline
- `90-main` spring camera (fov 30), shake, hit-stop, slow-mo, input (Enter/Space/M/C/+/-/click focus & rally)
- `70-ui`, `79-sfx`, `80-audio`, `78-music` DOM HUD, WebAudio synth SFX, embedded music

## Native architecture (C++17, OpenGL 3.3 core, GLFW only)
- own GL loader (`gl.h`, via glfwGetProcAddress), own math (`mathx.h`), own 5x7 bitmap font; miniaudio (single header, vendored) for audio
- everything GPU-instanced (`glDrawElementsInstanced`): props, characters (primitive parts), grass blades, particles, marks, UI quads
- zero heap allocation per frame: fixed pools & preallocated vectors
- shaders embedded in the executable (`shaders.h`) -> single portable binary
- post chain ported 1:1 from `40-post.js` (HDR RGBA16F, MSAA scene, AO+fog+in-scatter, 4 bloom mips, DOF, AgX, grade, grain)

## Progress
- [x] audit of reference (all src/*.js except embedded music data)
- [x] GL loader, math, data tables, simulation (game.cpp), fx, procedural audio, meshes, renderer + post chain, HUD
- [x] local build + xvfb smoke test (title + gameplay frames render, level-up panel works)
- [x] release workflow (Windows / Linux / macOS universal) -> GitHub Release v1.0.0
- [x] v1.1 character overhaul (below)
- [ ] follow-ups: point-light shadows, procedural music, offline progress, DOM-faithful HUD layout (panels/cards), portraits

## Decisions
- No GLAD/GLM/stb: own loader & math keep the binary ~0.8 MB. Font is a built-in 5x7 bitmap atlas.
- (v1, superseded) characters were instanced primitives. v1.1 uses real skinned meshes, see below.
- Steward panels use real-time countdowns (cards 6 s, choice 7 s, run end 8 s) exactly like the original.

## v1.1 - Blender character pipeline (this task)
Goal: characters identical to the three.js reference (`_ref_js/src/10-kit..22-monsters, 25-anim`), not approximations.
Pipeline (all scripts in `tools/`, reproducible on the molab notebook: Blender 4.3.2 + node):
1. `tools/bake.js <ref> <out>` runs the ORIGINAL generators (Geo/loft/tube kit, HW human, wardrobe, 6 heroes, 9 monsters, rigs) headless in node and dumps the exact
   skinned meshes (P,N,C,UV,M,BA,BB,W,I), rigs, attachment points, and **bakes the reference animation**: HumanAnim/QuadAnim/WispAnim including the two-bone leg IK with
   foot pinning, spring cloak and lantern pendulum -> world-space bone pose (quat+pos) per frame: idle, ready, walk@1.6, run@3.2, every action clip, stagger, downed.
2. `tools/blender_build.py` (one Blender process per figure, 9 in parallel on 20 cores, ~3-20 s each): builds a real mesh datablock with custom normals, UV, colour/material
   attributes, a real armature (28/21/9 bones, hierarchy from the original rig) and vertex groups from the skin weights; bakes per-vertex ambient occlusion (24 BVH rays/vertex
   incl. ground contact) and cavity (edge curvature); poses the armature from the baked frames and renders Cycles previews (`docs/previews/blender_*.jpg`); saves `.blend`;
   reads everything back out of Blender and writes the runtime asset `assets/<name>.lfm` (36 B/vertex, u16 indices, int16 quantised clips).
3. Native: `src/figure.cpp` loads `.lfm`, GPU skinning (two-bone blend like `Figure.skin()`), baked clip player (idle/ready/locomotion cadence follows real speed, action
   clips driven by the sim's windup/recover, stagger, downed, shatter rigid-body death + dissolve, burn/burst/gutter deaths), and the ported **surface material shader**
   (`SURF_FMAIN`/`SURF_FNORMAL`: skin, cloth weave, quilt, mail rings, leather, rusted plate, fur, wood, lichen stone, peat, glow, bone; derivative bump, mud, dissolve).
   Back-lantern geometry is stored once as an index range per hero (drawn only for the carrier).
4. UI: serif glyph atlas (`assets/font.bin`, Liberation Serif Bold rendered with PIL) replaces the 5x7 bitmap font (original uses Georgia).

Metrics: 15 figures, 124 k vertices, 7.2 MB assets (not embedded; `assets/` must sit next to the executable, searched in `./assets`, `../assets`, `../Resources/assets`); executable ~0.9 MB.
Caveats: no GPU in the sandbox - frames captured with Mesa llvmpipe under Xvfb (speckled terrain in captures is software-rasteriser derivative noise from the v1 ground shader, to be checked on hardware).
Where Blender "customisation" differs from the reference: AO + cavity baked into vertices (softened 0.4..1) and applied in the surface shader; everything else is 1:1.

## v1.2.0 - 3D gameplay & feel overhaul (this session)
Done:
- `src/play.cpp/.h` (new): archer play modes. Views: first person (pointer-lock, procedural bow view-model with limb bend / string / nocked arrow / hands, idle sway, look lag, recoil + string vibration), third person (shoulder camera, critically-damped spring, ray-vs-props/terrain clipping guard, aim zoom), free-look orbit showcase (auto-archer demo, wheel zoom). `V` cycles with a 0.22 s blended transition (no hitch, same scene).
- Archery: hold-to-draw with force curve `1-(1-p)^2(1-.35p)`, sweet spot (p>=.93, <0.9 s hold), over-hold => noise sway + stamina drain, reticle tightens/gold, HUD force-curve bars, arc preview (toggle G, same integrator), swept collisions, zone scoring, head x2, hit markers, damage numbers, kill feed, hit-stop + slow-mo on clean shots >= 32 m, combo.
- Practice range (11 targets: static/slider/bobber/sweeper) and wave mode (all 9 monsters shoot projectiles or lunge, bosses every 5th wave, 3 flames, downed loop + revive).
- Animation: additive procedural layer (`figAddRot/figAddMove`) on top of the baked clips: draw/release phases driven through the baked `aimed`/`attack` clip (`hit` frame = release), roll (somersault + crouch, i-frames), directional hit lean, raised-bow block, downed breathing. Foes reuse the baked `stag`/`attack` clips + reference death variants (shatter/burn/burst/gutter) with extra FX.
- Renderer: camera override, circle colliders from prop chunks (`rayWorld`/`pushOut`), instanced primitive batches (world + depth-ranged view-model); no per-frame heap allocs, strings formatted on events only.
- UI: title menu (carried defect (a) fixed: big buttons, label/sub layout), play HUD.
- CLI test hooks: `--mode range|waves --view 0..2 --bot --draw f --yaw y p --ff N`.
Metrics (llvmpipe, quality 0, 960x540): executable 941 KB, assets 7.4 MB, peak RSS ~210 MB (Mesa software rasteriser included).
QA shots Read this session: title, fpv_idle, fpv_aim, tpv_idle, tpv_aim, free, title2, fpv_aim2, range_bot (9/10 bullseyes by auto-archer), combat_tpv, combat_fpv, fpv_aim3.
Deferred / known: monster sheets (b) still not inspected individually (only seen in combat frames); FPV bow string is slightly over-bright under bloom; black speckle particles near horizon are carried from v1.1 (mote particles on llvmpipe); real-GPU frame time not measured; offline/music items unchanged.

CI: run 43092334901 (commit 86d49c9) - Windows/Linux/macOS all success, release v1.2.0 published with 3 assets. (Earlier runs a5c3e5e / c2b294f failed on a double->float narrowing in ui.cpp, fixed in 86d49c9.)
Combat capture: done (combat_tpv / combat_fpv via --bot --ff).

## v1.3.0 - integration & feel (this session)
Merged `origin/assets` (8e8dc2c) first (clean merge, Polyfork kit under `assets/polyfork/`, `assets/manifest.json` is the catalogue; there is no `catalog.json` and no entry is flagged `playable`, so "playable" = rigged biped).
Done:
- `src/settings.{h,cpp}`: sensitivity, invert Y, FOV, arc assist, reticle style (cross/dot/ring/brackets), camera shake, reduce motion, colourblind reticle (blue/orange), hold-vs-toggle aim, subtitles, master volume, last hero, 8 rebindable actions -> `save/settings.json` (written on every change). Options screen on the title (OPTIONS / key O) with click-to-rebind.
- `src/roster.{h,cpp}`: 18 characters (6 native + 6 Polyfork heroes + 6 Polyfork bipeds: Bloom Elemental, Bog Fiend, Bell Warden, Cathedral Statue, Root Colossus ...) with role, weapon, six stats, one active (E) + one passive. Only current + next figure resident, previous released (`figLoad/figFree`). Bones resolved by name so the 28-bone native and 25-bone Polyfork rigs both work; rigs lacking an `aimed` clip fall back to `attack`. Quadrupeds / wisp excluded (no hands for a bow).
- Hero select (title CHOOSE HERO / key 4): real 3D model on a pedestal, turntable, idle -> draw/release -> victory loop, stat bars, keyboard / mouse / wheel / gamepad bumpers, persisted.
- FPV view-model rewritten: bow with limb flex + string, arrow resting at the grip, gloved bow hand, draw hand hooked on the string (arc, release finger-open, string snap overshoot), over-hold tremble. Stuck arrows now embed (tip sunk 0.12-0.34 m, sink out at end of life) - fixes the "floating arrow".
- HUD: new reticle (4 styles, tightens + recolours at sweet spot), draw strength moved to a slim bar at bottom centre, per-view density (FPV minimal, TPV full + compass, free = INSPECT), quiver pips, ability cooldown, kill feed capped at 4, 8px grid + 3% safe margin.
- Gameplay identity: draw speed, stability, damage, move speed, HP, quiver (regen 1/1.1 s), 7 abilities (volley, bulwark, surge, ember burst, tonic, frost nova, quake), passives.
- Gamepad (sticks, triggers, A/Y/X), `--scene title|hero|fpv|aim|moor|highlands|fen`, `--out`, `--hero N`.
- Worlds: three biomes (ASHEN MOOR, PINE HIGHLANDS, DROWNED FEN) are mapped onto the existing five reach palettes (fog, ground, grass, props, drizzle); `T` cycles them in play, `1/2/3` in hero select.
Deferred (not done): 64 m chunked 4-LOD terrain, heightfield stitching, water-depth plane, GPU-instanced Polyfork foliage/buildings (the assets exist but are not placed; there is no terrain kit in the merged branch), 15/30/50 m lanes + wind flags, cover/verticality, day/night toggle, foot IK / turn-in-place / coyote time, directional hit-react clips from the Polyfork `hit_f/l/r` and `dodge` clips, five-variant death for the player, combat capture.
QA shots read: title, hero, fpv, aim, moor, highlands, fen (1280x720, llvmpipe via molab Xvfb).

## v1.4.0 - every hero plays its own kit (this session)
Root causes fixed: (1) the previous in-progress commit had corrupted `n2()` in the figure fragment shader (a dropped `h12(...)` argument) so *all skinned figures failed to link and were invisible* under Mesa; fixed (735ed3e). (2) `play.cpp` hard-wired the archer kit for every hero; (3) socketed Polyfork weapons were loaded but never drawn.
Done:
- 6 combat archetypes driven by `roster::Hero.style`, in Practice Range, Wave Hunt and steward-independent play: Marksman bow (unchanged), Sword & Shield (3-hit combo, shield bash when guarding, 0.3 s parry window -> stun + x2 riposte), 2H Heavy (hold to charge: tap = wide cleave, release = ground slam shockwave, hyper-armour while winding), Fire (firebolt / charged cinder orb with AoE + burn DoT, flame ward ignites attackers), Alchemist (lobbed flask / charged firepot leaving a lingering acid pool, mend ward), Frost (3-shard fan piercing + chill / charged frost lance with shatter bonus, ice ward chills attackers).
- Animation: authored baked clips (`attack/quick/cleave/sunder/bash/kindle/cinder/firepot/lance`) driven by the sim's wind-up -> hit -> recovery (`figActClip`), charge = held anticipation frame, authored `guard` blend while blocking, hit-direction reaction, melee lunge + aim-assist yaw, weapon swing trails, charge orbs.
- Weapons: `.lfm` weapons socketed to hand bones for Polyfork heroes in third person; FPV viewmodels for non-bow styles use the same 3D weapon meshes with a depth-range hack (`FigDraw.vm`).
- Practice range gains 4 live training dummies (Thornling, Peatwight, Rustbound, Cairnback) that regen and respawn; every spell/melee hits boards too. Bestiary scene (`--scene bestiary`) shows all 12 monsters. FD AI table extended to the 3 new monsters (was reading out of range).
- UI: hero-select stats/labels per style (no more "CHOOSE YOUR ARCHER"), per-style control hints and banners, charge bar replaces quiver for non-archers; default view is third person for non-archers; camera distance scales with hero size.
- CLI: `--bot` plays every style (approach, swing/charge pattern); `--scene bestiary`.
QA shots read (1280x720 llvmpipe via molab Xvfb, JPEG): k_hero7/k_tpv9 (invisible-figure regression), h_hero7/h_tpv9/h_tpv12, c_h7 (knight sword+shield + Peatwight dummy), c_h8 (Varka greatsword), c_h9 (cultist staff+lantern), c_h10 (witch acid), c_h11/c_h5 (frost), c_sel8, c_w1 (wave combo, CRIT), b_best (all 12 monsters), b_fpv7/b_fpv8 -> d_fpv7/d_fpv8 (FPV sword+kite, greatsword), d_fpv9/d_fpv11/f_fpv9 (staff + orb), d_w16/f_w9/f_w4 (waves), d_hero16/f_hero16 (Root Colossus select, camera too close - addressed by TITAN view scale).
Deferred: Polyfork monster alt-skins in waves, chunked terrain / biome prop placement, authored dodge clip for non-archers, foot IK, caster flash intensities in FPV are still hot, camera can clip into standing stones (rayWorld guard only covers the boom).

## v1.5.0 - monster-mode overhaul (in progress)
Plan: spear style + Spearman heroes, FPV viewmodels rewritten (low-profile casters that never cover the reticle, real melee swing arcs + trails), new FX library (slashes, impacts, casts), combat controls (lock-on, input buffer, dodge, sprint-attack), rival duel mode, HUD rewrite. See git log for what landed.
