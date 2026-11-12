# LANTERNFALL — Assets Pipeline Status (`assets` branch)

## Agent Ownership (Two Agents on One Shared Notebook / Worktree)

| Resource | Agent B (`assets` branch — THIS AGENT) | Agent A (`main` branch — DO NOT TOUCH) |
|---|---|---|
| Git Branch | `assets` | `main` |
| Status File | `ASSETS_STATUS.md` | `LANTERNFALL_STATUS.md` (untouched) |
| Helper Script | `/tmp/agentB_mx.sh` | `/tmp/agentA_*` (untouched) |
| Run Directory | `/tmp/agentB_run/` | `/tmp/agentA_run/` (untouched) |
| Log Prefix | `/tmp/agentB_<step>.log` | `/tmp/agentA_*.log` (untouched) |
| Blender Temp Dirs | `/tmp/agentB_run/blend/`, `/tmp/agentB_run/raw/`, `assets_build/` | `/tmp/agentA_*` |
| Raw / Build Dirs | `assets_raw/`, `assets_build/` (git-excluded) | `build/` |
| Viewer / QA Shots | `shots/agentB_*.png` (cleaned before final commit) | `shots/` |

**Strict Shared-Environment Rule:**
> never `pkill -f`, never `rm -rf` outside `/tmp/agentB_*` and `assets_build/`, never checkout `main` in the shared worktree.

---

## Style Specification (Extracted from `_ref_js/src/` & `tools/`)

### 1. World Scale & Coordinate Convention
- **Up Axis & Handedness**: Runtime and `.lfm` use `+Y` up, `+Z` forward, right-handed model space; origin `(0, 0, 0)` is planted on the ground plane (`y = 0`).
- **Humanoid Proportions (`humanRig`)**:
  - `root`: `(0, 0, 0)`, `hips`: `y = 0.98m`, `spine`: `y = 1.12m`, `chest`: `y = 1.28m`, `neck`: `y = 1.50m`, `head`: `y = 1.585m` (crown ~`1.81m`).
  - Shoulder half-width `sh`: `0.19m` (Fletcher/Rimebinder) to `0.228m` (Reaver).
  - Hip half-width `hp`: `0.09m` to `0.102m`.
  - Leg length `LEG = 0.845m` (`lThigh` `0.93m` -> `lShin` `0.50m` -> `lFoot` `0.085m`).
  - Arm hang (`bind pose` along `-Y`): `lUp` `1.435m` -> `lFore` `1.14m` -> `lHand` `0.885m`, weapon mounts `wR`/`wL` at `0.835m`.
  - Secondary dynamic chains: 3-bone cloak (`cloakA` `1.46m`, `cloakB` `1.10m`, `cloakC` `0.74m`), absolute pendulum bones `lan` (hand lantern) and `lanB` (back-strapped party lantern at `(-0.14, 2.43, 0.07)`).
- **Unit Scale Multipliers**:
  - Heroes (`scale = 1.0`, `radius = 0.4`, `hpY = 1.8`): `warden`, `reaver`, `fletcher`, `lamplighter`, `apothecary`, `rimebinder`.
  - Monsters:
    - `thornling`: `scale = 0.62`, `radius = 0.28`, `hpY = 1.05`
    - `peatwight`: `scale = 1.04`, `radius = 0.42`, `hpY = 1.85`
    - `gloamhound`: `scale = 1.12`, `radius = 0.40`, `hpY = 1.15` (Quadruped rig `rigQ`)
    - `gloamwisp`: `scale = 1.00`, `radius = 0.35`, `hpY = 2.20`, `float = true` (`WispAnim` rig)
    - `rustbound`: `scale = 1.10`, `radius = 0.42`, `hpY = 2.10`
    - `cairnback`: `scale = 1.50`, `radius = 0.75`, `hpY = 2.70`
    - `barrowking` (Boss): `scale = 1.95`, `radius = 0.85`, `hpY = 3.90`
    - `mossjaw` (Boss): `scale = 2.35`, `radius = 1.15`, `hpY = 4.30`
    - `hollowstag` (Boss): `scale = 2.20`, `radius = 1.00`, `hpY = 3.60` (Quadruped rig `rigQ` with `leg: 1.35, neck: 1.5`)
- **Vertex Budget**:
  - Skinned characters: ~4,000–16,000 vertices (`nv < 65535` for 16-bit index buffer in `.lfm`), combining sculpted Polyfork base geometry + kit gear/wardrobe in Blender with baked vertex AO (`Col.a`) and edge cavity (`aBn.w`).
  - Static buildings, terrain props, and weapons: 200–3,500 triangles per asset, origin at ground contact, with baked AO and collision proxy metadata.

### 2. Palette & Surface Material Vocabulary (`10-kit.js` & `20-human.js`)
Every vertex carries `(rough, metal, surfType, grime)` in `aMat` plus linear RGB base color (stored sqrt-encoded in `.lfm`) and `(ao, cavity)`:

| ID | Material Constant | Surface Shader Behavior (`SURF_FMAIN`) | Canonical Hex Palette |
|---|---|---|---|
| `0` | `T_SKIN` | Soft fbm3 mottled skin, subtle pore micro-bump | `skin: #6e5040`, `skinDk: #3a2a22` |
| `1` | `T_CLOTH` | Woven warp/weft (`sin(uv*1570)`), broad cloth folds | `wool: #222a27`, `woolBrown: #3c3026`, `woolGrey: #44474a`, `murrey: #4a1e22`, `linen: #857a60`, `rope: #6a5c40` |
| `2` | `T_QUILT` | Padded gambeson ribs (`abs(sin(uv.x*98))`) + stitch lines | `buff: #6e6148`, `gambeson: #3c3a27`, `jack: #4d4332` |
| `3` | `T_MAIL` | Staggered chainmail rings (`uv*349`), specular glints + rust patches | `mail: #70757a` |
| `4` | `T_LEATHER` | Creased hide grain, lower roughness in worn highlights | `leather: #40291a`, `leatherDk: #271b13`, `leatherTan: #6a4a2c`, `waxed: #25221c` |
| `5` | `T_PLATE` | Brushed/dented forged metal with height-dependent rust oxidation | `plate: #8b8f93`, `iron: #34353a`, `brass: #8a6a34`, `rust: #3d3e42` |
| `6` | `T_FUR` | Anisotropic strand noise (`rp*vec3(210,26,210)`), deep pile occlusion | `fur: #4c3b2c`, `furGrey: #5a5650`, `hair: #2a2018`, `hairGrey: #6a665e` |
| `7` | `T_WOOD` | Elongated wood grain (`uv.x*260, uv.y*9`) | `wood: #5e4a32`, `woodDk: #3a2c1e`, `fence: #3a3027` |
| `8` | `T_STONE` | Multi-octave craggy rock with dark moss (`#090d06`) and pale lichen (`#33332b`) | `stone: #4a4a46`, `standing: #45474a`, `waylight_plinth: #47474a` |
| `9` | `T_PEAT` | Fibrous bog peat / bark ridges, zero mud override | `peat: #1d1711`, `peat2: #2b2218`, `root: #40311f`, `briar: #33261a`, `turf: #26301a` |
| `10` | `T_GLOW` | Emissive lantern/wisp/frost/eye core (`col * grime * uGlow`) | `lantern: #ffb45a`, `gloam: #bfffd0`, `wisp: #d6ffe0`, `rime: #a8d8ff`, `tonic: #7ab04a` |
| `11` | `T_BONE` | Striated bleached bone / horn / thorn with weathered staining | `bone: #6e6857`, `pale: #8f8a72`, `teeth: #b8b29a`, `thorn: #6a5a40` |

- **Ground Mud & Grime**: All surfaces `ty < 9.5` accumulate lower-hem mud (`#0a0806`, `rough = 0.95`) scaled by `grime` (`vMat.w`) from `y = 0.02m` to `0.62m`.

### 3. Full Animation & State Suite (`25-anim.js`, `60-data.js`, `figure.cpp`)
Every rigged character asset includes baked quaternion + translation bone tracks (`nb * 7` values per frame) across the full state suite:
1. **Core Locomotion & Posture**:
   - `idle` (looping breathing, weight shift, cloak/lantern physics, head look)
   - `ready` (combat stance loop)
   - `walk` (periodic gait cycle with 2-bone leg IK for heroes / quadruped trot / wisp glide)
   - `run` (periodic combat sprint / lope cycle)
2. **Class & Monster Combat / Skill Clips**:
   - **Warden**: `attack` (arming sword slash), `bash` (Shield Bash), `guard` (Hold the Line / block)
   - **Reaver**: `attack` (greatsword hew), `cleave` (wide sweep), `sunder` (overhead earthshaker slam)
   - **Fletcher**: `attack` (longbow draw & release), `aimed` (Aimed Shot), `volley` (high-angle Volley)
   - **Lamplighter**: `attack` (lantern pole swing), `kindle` (Kindle mend raise), `cinder` (Cinderfall invocation)
   - **Apothecary**: `attack` (flask hurl), `tonic` (Tonic toss), `firepot` (Firepot lob), `quick` (Quicksilver)
   - **Rimebinder**: `attack` (frost shard cast), `hoar` (Hoarfrost ground slam), `shatter` (Shatter burst)
   - **Monsters & Bosses**: `attack` (claw/bite/smash/blade strike), `special` (Barrow King `sweep`, Old Mossjaw `slam`, Hollow Stag `charge`)
   - **Defensive & Reactive Clips (authored on armature in Blender)**: `defend` (block/brace), `dodge` (evasive sidestep/backstep), `hit_f` / `hit_l` / `hit_r` (directional hit-react), `stag` (stagger recoil), `down` (hero kneeling downed state)
3. **Five Death Variants**:
   - `shatter`: Rigid-body bone separation with ballistic bounce & spin (`rustbound`, `cairnback`, `barrowking`, `mossjaw`, `gloamhound`, `hollowstag`, plus procedural clip preview)
   - `dissolve`: Height + 3D noise threshold discard (`uDissolve`) with glowing ember/gloam edge
   - `burn`: Upward fiery dissolve (`disCol = [1.0, 0.45, 0.1]`, `peatwight`)
   - `burst`: Rapid outward spore/thorn rupture (`disCol = [0.6, 1.0, 0.5]`, `thornling`)
   - `gutter`: Shroud collapse and flame guttering out (`disCol = [0.6, 1.0, 0.75]`, `gloamwisp`)

---

## Session 2 notes (Agent B)

### Environment findings
- Polyfork API reachable: `/api/assets?q=…&free=1&max_triangles=2000` returns JSON (`download.glb`/`download.mjs`, `has_rig`, `kit`, `triangles`, `size_m`).
- Blender 4.2.3 unpacked at `/tmp/agentB_run/blender-4.2.3-linux-x64/` (no system Blender).
- No molab/marimo kernel is attached to this sandbox: the viewer is delivered as a marimo notebook file **plus** a self-contained three.js page it serves; heartbeat status is reported at the end.
- Remote `assets` already held a prior ownership/style commit (`4b97885`); this session builds on it (no force push).
- Existing figure clip names (strings in `.lfm`): heroes `ready walk run attack <skills> stag down`; monsters `idle ready walk run attack stag [special]`, plus a baked `shatter` track on shatter-death figures.

### Results (session 2)
Pipeline: `tools/polyfork_search.py` / `tools/polyfork_fetch.py` (catalogue → `assets_raw/`, git-ignored) → `tools/blender_build_assets.py` (Blender 4.2.3 headless) → `assets/polyfork/<key>.lfm` (+ `<key>.col.json` for static assets) and preview GLBs in `assets_build/` (git-ignored) → `tools/lfm_sheet.py` (QA sheet rendered from the shipped .lfm bytes with the runtime's two-bone skinning).
Output lives in `assets/polyfork/` so the v1.1.0 `assets/*.lfm` are untouched; no loader/gameplay code was changed (no additive loader commit was needed).

| Category | Count | Source tris | Baked verts |
|---|---|---|---|
| hero (warden, reaver, fletcher, lamplighter, apothecary, rimebinder) | 6 | 29,840 | 89,520 |
| monster (thornling, peatwight, gloamhound, gloamwisp, rustbound, cairnback) | 6 | 24,158 | 71,202 |
| boss (barrowking, mossjaw, hollowstag) | 3 | 12,706 | 38,118 |
| bonus bestiary (emberhound, thornboar, rootcolossus) | 3 | 12,637 | 37,911 |
| weapon (8) / terrain prop (13) / building (6) | 27 | 22,470 | 67,410 |
| **total** | **45** | **101,811 (baked 101,387)** | **304,161** |

Kit sources: Grimlight Denizens (figures), The Ultimate RPG Character, Dungeon Crypt, Halloween Night, Medieval Village, Nature Forest, Three Kingdoms, Easter Island (see `assets/polyfork/manifest.json` for every Polyfork id, URL, triangle count).
Per figure: 25 bones (humanoid: root + 22 Mixamo-derived bones renamed to the game's `hips/spine/chest/lUp/lFore/lHand/lThigh...` + `wR/wL` sockets), 27 (quadrupeds), 5 (wisp). 246 clips are baked into the 18 .lfm (runtime cap is 16 per file). The preview GLBs carry the full suite (up to 19-20 clips): idle, ready, walk, run (Polyfork's own CC0 clips, resampled for the 15 Mixamo/animal rigs), attack + class skills (`bash guard cleave sunder aimed volley kindle cinder tonic firepot quick lance hoar special`), `defend`, `dodge`, `hit_f/hit_l/hit_r`, `stag`, `down` and the five death variants `dissolve burn burst gutter shatter` (shatter = independent ballistic shards). Authored clips are written as real Blender Actions (NLA tracks) on the rebuilt armature and exported in the GLB.
Palette: every vertex colour is snapped to a per-asset subset of the game's palette (`THEME` in the build script) with luminance preserved, and carries game surf type / rough / metal / grime; glow entries only where the source is emissive/saturated (wisp forced to gloam). Baked: AO in the hang/ready pose incl. ground contact (alpha of colour), edge cavity (`b.w`), smoothed normals (40 deg), triplanar UV.
Triangle budget: Polyfork figures are 2.7K-5K tris / 8K-15K verts (flat-shaded, split verts), i.e. inside the 10-18K vertex budget but with about a third of the v1.1.0 triangle density (smooth lofted originals have 18-30K tris) - faceted by design.

### Viewer (QA only)
`viewer/lanternfall_viewer.py` is a marimo notebook (marimo 0.25.2 imported OK; notebook compiles) that starts `viewer/serve.py` (stdlib) and embeds `viewer/index.html` (three.js 0.160 from unpkg) at 1280x720: kit dropdown, asset dropdown, one button per clip, turntable, wireframe, loop toggle, frame counter. It polls `/index.json` every 2 s, so a `.glb` that lands in or changes under `assets_build/viewer/` appears/reloads automatically (clip selection kept). The viewer server was running on :8765 during the session and served 45 GLBs (`/index.json` verified with curl).

### QA screenshots (shots/ is git-ignored)
No headless browser exists in this sandbox, so the three.js page itself could not be screenshotted. QA used `tools/lfm_sheet.py` sheets rendered from the shipped .lfm: warden (v2/v3), reaver (v3), hollowstag (v3), peatwight (v3/v4/v5), gloamwisp (v3), rimebinder (v4), b_stone_chapel (v3/v4) and p_cairn / w_arming_sword (v3, generated, not all viewed). See the final report for what each showed.

### CI
`.github/workflows/release.yml` triggers only on pushes to `main`, `v*` tags and manual dispatch, so pushing `assets` starts no run (`/actions/runs?branch=assets` returned total_count 0). No tag or release was created and the workflow was not dispatched (it can publish a release).

### Deferred / known issues
- Authored attack/skill poses are first-pass: arm-forward poses (e.g. warden `attack` hit frame) read too high; needs a tuning pass with the viewer.
- Colour fidelity: gloamhound / hollowstag / reaver collapse to few palette entries (pale bone/cream); peatwight needed `bone` removed from its theme.
- Wisp uses a single cheesecloth ghost (175 tris) with a 5-bone chain instead of the original 9-bone arm rig; no hand-lantern/back-lantern geometry or `lan/lanB` bones on heroes; weapons are static .lfm, not attached to hands; no `cloak` chains.
- Deaths other than the figure's own are only in the GLB (lfm cap of 16 clips); `fx` points (`tip/base/off/head`) are estimates.
- Collision proxies are AABB + circle + <=12-point footprint (`*.col.json`), not meshes.
- Terrain rocks/trees of the game's `gRock/gTree` family have only partial analogues in the Polyfork catalogue (cairn, dead trees, headstones, walls); no heather/bracken/fern.
- Keep-alive: no molab kernel exists here; a 30 s heartbeat (`/tmp/agentB_hb.sh` pinging the local viewer server, plus a heartbeat thread cell in the notebook) ran instead.


---

## Session 3 (Agent B) — ledger bootstrap + batch 3 (nature / village / graveyard gap fill)

### Agent ownership (unchanged, restated)
Run dirs `/tmp/agentB_run/` (Blender 4.2.3 at `/tmp/agentB_run/blender-4.2.3-linux-x64`, X libs extracted to `/tmp/agentB_run/libs`, numpy/PIL in `/tmp/agentB_run/pylib`); logs `/tmp/agentB_<step>.log`; viewer port file `/tmp/agentB_viewer.port` (8791), pid `/tmp/agentB_viewer.pid`; build dirs `assets_raw/`, `assets_build/`. Rules: never `pkill -f`; never `rm -rf` outside `/tmp/agentB_*` and `assets_build/`; never checkout `main` in the shared worktree.

### Environment
- **molab notebook is gone**: `https://sb-29b1005f8560826c.sb.molab.run/` answers `HTTP 410` (sandbox retired), so no marimo execute endpoint and no keep-alive was possible. Token stored in `/tmp/marimo.env` (600, untracked). The viewer runs standalone: `python3 viewer/serve.py 8791` (auto-refresh by polling `/index.json`, `Cache-Control: no-store`); `viewer/lanternfall_viewer.py` embeds it when a kernel exists.
- Blender needed `libXrender/libXi/libGL...`: debs fetched from deb.debian.org and extracted locally (no root), run via `LD_LIBRARY_PATH` wrapper `/tmp/agentB_run/blender.sh`.

### Ledger (new)
`assets/manifest.json` (62 entries: 45 session-2 bootstrapped with glb_sha256 + thumb_dhash + states from `.lfm` clips, plus 17 new), `assets/coverage.json` (counts by class/kit/biome/role, targets, role gaps). Dedupe rule enforced by `tools/ledger.py` (prints DUPLICATE for polyfork_id / glb_sha256 / thumb_dhash — none found) and `tools/polyfork_fetch_batch.py` (skips covered id/sha).
Tools added: `tools/polyfork_batch.py` (gap search), `tools/polyfork_fetch_batch.py`, `tools/ledger.py`, `tools/contact_sheet.py`, `tools/batches/batch3.json`; `blender_build_assets.py` now reads `theme` and `lum_scale` per asset from the batch manifest.

### Kit shortlist (recorded before download)
Nature & Forest (`nature-forest-kit-f29d6a`), Medieval Village (`medieval-village-kit-ca0b43`), Halloween Night (`halloween-night-46ade4`), Dungeon & Crypt (`dungeon-crypt-c27d93`), Three Kingdoms Settlements, Campground & Lake, Cozy Farm, Alpine & Snow. Thumbnails of 22 candidates were inspected (`shots/shortlist_sheet.png`); rejected: reed-bed (sparse), oak-barrel (duplicate role), cattail, stump, haystack (colour/role weak). Search gap: `handcart` → `coverage: poor`, nothing substituted.

### Batch 3 results (17 assets, 11740 baked triangles, all statics, 1 bone)
| key | polyfork_id | class | kit | tris | role |
|---|---|---|---|---|---|
| `p_pine_tall` | tall-pine-tree-ab4108 | prop | nature-forest-kit | 350 | tree (pine highland) |
| `p_pine_young` | young-pine-0d7695 | prop | nature-forest-kit | 270 | tree (pine highland) |
| `p_round_bush` | round-bush-cd2ac0 | prop | nature-forest-kit | 573 | bush |
| `p_boulder` | large-boulder-a29b99 | prop | nature-forest-kit | 566 | rock |
| `p_fallen_log` | fallen-log-1685fb | prop | nature-forest-kit | 484 | log (fen/pine) |
| `p_barrel` | wooden-barrel-ab50ca | prop | nature-forest-kit | 540 | barrel |
| `p_crate` | timber-crate-ba0739 | prop | dungeon-crypt | 588 | crate |
| `p_lamp_post` | lamp-post-c9e409 | prop | medieval-village-kit | 478 | lantern post |
| `p_signpost` | trail-signpost-bd29a7 | prop | nature-forest-kit | 420 | signpost |
| `p_campfire_tripod` | campfire-cook-tripod-d95a1e | prop | nature-forest-kit | 470 | campfire |
| `p_thorn_bush` | bare-thorn-bush-91a238 | prop | halloween-night | 813 | bush (moor thorn) |
| `p_iron_fence` | wrought-iron-cemetery-fence-f1a113 | prop | halloween-night | 808 | fence |
| `p_cemetery_gate` | arched-cemetery-gate-bdca7c | prop | halloween-night | 904 | gate |
| `p_banner_pole` | faction-banner-pole-39835b | prop | three-kingdoms-settlements-a | 766 | banner |
| `b_market_stall` | market-stall-94937d | building | medieval-village-kit | 592 | market stall |
| `b_timber_barn` | timber-barn-90b3ce | building | medieval-village-kit | 1382 | barn |
| `b_watchtower` | watchtower-140f78 | building | medieval-village-kit | 1736 | tower |

Per asset: `.lfm` + `.col.json` (AABB/radius/<=12-pt footprint) in `assets/polyfork/`, preview GLB in `assets/polyfork/preview/`, thumbnail (runtime-`.lfm` render) in `assets/polyfork/thumbs/`, `meta/<key>.json`. Origin on the ground (y=0), +Y up, real metres. Palette: every vertex snapped to the game palette subset in `THEME`; foliage uses `woodDk/turf/lichen/peat` with `lum_scale 0.12` so pines read dark green instead of khaki; other assets use `lum_scale` 0.6–0.8.
QA screenshots read this session: `shots/shortlist_sheet.png` (22 candidate thumbs), `shots/viewer_batch3.png` (first bake: geometry on ground, silhouettes right, colours too light/khaki), `shots/viewer_batch3b.png` (after `lum_scale`: gate, barn, boulder, thorn bush OK; foliage still khaki), `shots/viewer_nature.png` (final dark-green pines/bush, OK). Screenshots come from `tools/lfm_sheet.py` (shipped bytes), not from the three.js page (no headless browser).

### Next batch plan
1. Characters: archer / guard / civilian / caster humanoids from Grimlight Denizens + Medieval Village (has_rig) — retarget idle/walk/run from the rig, author the full state suite in Blender (strafe, turn, aim, draw, release, block, dodge, hit x4, get-up, victory, taunt, spawn).
2. Fen set: reeds, water props, jetty/boardwalk (Campground & Lake, Coast & Boardwalk), shrine (Dia de Muertos / Japanese shrine hall only if palette fits), cart (gap: none found).
3. Terrain kits: 64 m tiles with `terrain.json` (`at(x,z)`, `flat_areas`, `scatter`) from Alpine & Snow / Prehistoric Valley / Easter Island terrain class (only 21 free terrain assets exist).
4. Bake LOD1 for buildings; re-bake preview GLBs for the 45 session-2 assets into `assets/polyfork/preview/` (they are only in git-ignored `assets_build/` today).

### Deferred
- LOD1 for `b_*` not produced; characters/state suite untouched this batch (statics only); old 45 assets have no committed preview GLB / thumbnail / meta.json; marimo card not reachable (410); CI: `release.yml` only runs on `main`/tags, so no run exists for `assets`.

### CI (session 3)
`/actions/runs?branch=assets` -> total_count 0 (workflow only triggers on `main`/tags); no run ID exists. No tag created, nothing merged to `main`.
