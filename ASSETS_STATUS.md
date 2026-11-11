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

### Progress log
- [x] Bootstrap, branch `assets`, reference clone, style spec
- [ ] Polyfork sourcing → `assets_raw/manifest.json`
- [ ] Blender build (`tools/blender_build_assets.py`)
- [ ] Viewer notebook
- [ ] CI result
