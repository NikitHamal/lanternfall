# EMBERVALE / LANTERNFALL - fantasy kingdom game roadmap

Vision: a third-person fantasy immersive game (Athran-style kingdom defence + exploration): build and fortify a kingdom, command soldiers,
spells, gather by day, survive demon hordes at night, explore lost kingdoms and reclaim the corrupted world. Every session ships ONE major pillar.

## Pillars and status  (x = shipped, ~ = partial, . = todo)
### P1 Explorer world (v1.6)
- [x] 2 km procedural continent, 3 nations, capitals, villages, forts, camps, ruins, graveyards, roads, bridges, border gates
- [x] rivers, lakes, waterfalls, forests, flowers, mountains, day/night cycle, ~400 NPCs, patrols, guards, monsters, bosses
- [ ] chunk streaming / bigger map, caves, islands, snow biome, desert biome, swamp biome
### P2 Living people  (v1.7-v1.8)
- [x] daytime: lanterns only carried when dusk/night/indoors
- [x] reputation & crime: victims fight back or flee, witnesses panic, guards hunt you, per-nation bounty (WANTED stars) that decays
- [x] summonable Noble Hart mount (`K`) with seated riding pose, trample charge, mounted archery & spellcasting (v1.8)
- [x] NPC daily schedules: morning at home, work at stalls / wells / mills / barns, evening on the plaza, asleep indoors 20:30-05:30 (v2.1)
- [x] talk menu (E), shopkeepers & trade (arrows, healing draughts, weapon tiers 1-3, armour tiers 1-3, sell food), quest journal (slay / hunt / gather wood / gather stone / scout) with report-back reward + crime pardon (v2.1)
- [ ] tavern interiors, dialogue trees with branching consequences, escort / delivery quests, merchant stock rotation
- [x] pets (dogs befriend + guard), cats, hens, sheep, cows, horses (rideable, E), songbirds, crows, butterflies (v1.9)
- [ ] children, NPC daily schedules, animal species sounds, hunting drops beyond food
### P3 Combat  (v1.7-v1.8)
- [x] bow in explorer (hold to draw, release; arrows with gravity, stick into bodies/ground; camera-space FPV longbow viewmodel fixed in v1.8)
- [x] location damage: head x2.6, torso x1, arms x0.6, legs x0.5 (bone-capsule hitboxes vs. skeleton)
- [x] melee hits hurt people; victims fight back, guards join, villagers flee
- [x] spells (`Q` cast, `Z` cycle): Solar Fireball (AoE + dynamic light), Glacial Frost Nova (ring of 10 shards + freeze/hobble), Solar Aegis (heal + buff + mend walls), Storm Lance (piercing bolt), regenerating Arcane Mana bar (v1.8)
- [ ] directional blocking, stagger types, finishers, armour slots, ranged enemies, shields for NPCs
### P4 Interiors  (v1.7-v1.8)
- [x] enter houses via the door (E): fade into furnished interior cells (cottage, longhouse/great hall, chapel, keep hall) + indoor HUD badge (v1.8)
- [ ] more interiors (smithy, tavern, shop, castle floors), furniture interaction (sit/sleep/cook), loot containers
### P5 Animation  (v1.7-v1.8)
- [x] procedural locomotion layer: speed-synced stride, 2-bone leg IK, hip bob/sway, counter-rotating spine & arms, lean into acceleration
- [x] seated mounted riding leg pose (`lThigh`/`rThigh`/`lShin`/`rShin`) synced to quadruped gallop (v1.8)
- [ ] foot IK on steep slopes, turn-in-place, climbing, swimming strokes, per-weapon locomotion sets
### P6 Kingdom building (Athran loop)  (v1.8 SHIPPED)
- [x] resources (wood / stone / food / gold) + spatial 64m harvest grid (`G` or melee hit on trees, boulders, 85 procedural iron/gold ore veins, hay/bushes)
- [x] real-time 3D Kingdom Build Mode (`C`, keys `1-6` / wheel, `R` rotate, `LMB` place, `RMB` repair): Stone Wall (end-to-end auto-snap), Archer Watchtower, Fortified Gatehouse (allies pass, enemies blocked), Royal Barracks (+2 sworn troops), Homestead Farm (passive food/gold + heal), Sunfire Beacon (dynamic light + holy burn)
- [x] hire / command Sworn Warband troops (`G` enlist villagers/soldiers/knights or train at Barracks; `X` cycles `FOLLOW`, `HOLD LINE`, `CHARGE`, `GARRISON`)
- [x] nightly demon siege waves (`NIGHT N DEMON SIEGE`): siege bosses (Barrow King / Cairnback Siegebreakers), monsters besiege & batter player-built walls/gates/towers, autonomous Watchtower ballista/archer defense AI leads & shoots monsters, Dawn Victory tax & auto-mend
- [x] reclaim corrupted lost kingdoms: 19 Corrupted Rift monoliths (`K_RIFT` -> `K_PURIFIED`) guarded by lair monsters; purifying grants resources, full restore & a sworn Knight
- [ ] multi-tier castle upgrades, siege catapults/rams, corruption fog shader, underground dungeons
### P7 Art / Blender pipeline
- [~] Polyfork kit (`assets/polyfork`) in use for buildings/props/figures + procedural `.lfm`-layout meshes (`K_ORE`, `K_RIFT`, `K_PURIFIED`, `K_SPELL_FIRE`, `K_SPELL_FROST`)
- [ ] more citizen / pet / tree / building assets from Polyfork re-built in Blender (`tools/blender_build_assets.py`) with baked AO
- [x] audio ambience: birdsong by day, crickets at night, running water, daylight drone fade (v1.9)
- [x] biome flora: cherry blossom, birch, jungle, willow, palm, ferns, mushrooms, reeds, lilies, rock spires, flowering shrubs, falling petals / leaves (v1.9)
- [ ] town hubbub, procedural exploration music, snow / desert / swamp biomes
### P8 Systems
- [x] persistent explorer save (`save/explorer.sav`: gold, ammo, resources, tiers, draughts, clock, position, discovered POIs, quests; autosave 45 s + on exit) (v2.1)
- [ ] persist built fortifications / warband, custom map markers, gamepad bindings in explorer

## Stats (update every session)
| version | lines of C++ | assets | binary | notes |
|---|---|---|---|---|
| v1.6.0 | ~5.2 k (dense) | 26 MB | 1.4 MB | explorer world |
| v1.7.0 | ~5.6 k (dense) + 3 new modules | 26 MB | 1.5 MB | gait IK, body-part combat, bow, crime, interiors, horde |
| v1.9.0 | ~7.0 k + xfauna.inc + xmesh_new.inc | 26 MB | ~1.7 MB | biome flora (11 kinds), fauna + ride horses, ambience, explore-only title |
| v1.8.0 | ~6.5 k (dense) + `src/xkingdom.inc` | 26 MB | 1.6 MB | Athran kingdom build/siege loop, tower defense AI, warband orders, 4 spells + mana, rifts, Noble Hart mount, FPV bow fix |

| v2.1.0 | ~7.4 k + xtown.inc | 26 MB | ~1.8 MB | schedules, talk menu, merchants, quests, tiers, save |

## Next session candidates (pick ONE major pillar)
1. Persistent Kingdom Save/Load (`save/kingdom.json`) + Multi-Tier Structure Upgrades (Timber -> Stone -> Citadel) & Siege Catapults
2. Blender rebuild of new Polyfork citizens, village pets (dogs, cats, chickens, horses), and lush biome foliage placed as instanced fauna
3. Living Towns II: Smithy, Tavern & Merchant interiors with equipment/arrow/potion shops, NPC day/night indoor schedules, and quests
4. Ambient Soundscape & Procedural Fantasy Soundtrack (rivers, waterfalls, wind in pines, birdsong by day, crickets & war-horns at night)
