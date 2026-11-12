# EMBERVALE / LANTERNFALL - fantasy kingdom game roadmap

Vision: a third-person fantasy immersive game (Athran-style kingdom defence + exploration): build and fortify a kingdom, command soldiers,
spells, gather by day, survive demon hordes at night, explore lost kingdoms and reclaim the corrupted world. Every session ships ONE major pillar.

## Pillars and status  (x = shipped, ~ = partial, . = todo)
### P1 Explorer world (v1.6)
- [x] 2 km procedural continent, 3 nations, capitals, villages, forts, camps, ruins, graveyards, roads, bridges, border gates
- [x] rivers, lakes, waterfalls, forests, flowers, mountains, day/night cycle, ~400 NPCs, patrols, guards, monsters, bosses
- [ ] chunk streaming / bigger map, caves, islands, snow biome, desert biome, swamp biome
### P2 Living people  (v1.7 this session)
- [x] daytime: lanterns only carried when dusk/night/indoors
- [~] reputation & crime: NPC factions hostile to the player when attacked, guards escalate, bounty, calm down over time
- [ ] NPC daily schedules (work / market / tavern / sleep), shopkeepers & trade, quests, dialogue trees
- [ ] children, pets (dogs/cats/chickens), livestock, horses & mounts
### P3 Combat  (v1.7 this session)
- [x] bow in explorer (hold to draw, release; arrows with gravity, stick into bodies/ground)
- [x] location damage: head x2.5, torso x1, arms x0.6, legs x0.5 (hitboxes vs. skeleton height bands)
- [x] melee hits hurt people; victims fight back, guards join, villagers flee
- [ ] directional blocking, stagger types, finishers, armour slots, ranged enemies, shields for NPCs, siege
- [ ] spells (fire, frost, heal, summon), mana, ability bar
### P4 Interiors  (v1.7 this session)
- [x] enter houses via the door (E): fade into furnished interior cells (cottage, longhouse, chapel, keep hall)
- [ ] more interiors (smithy, tavern, shop, castle floors), furniture interaction (sit/sleep/cook), loot containers
### P5 Animation  (v1.7 this session)
- [x] new procedural locomotion layer: speed-synced stride, hip bob/sway, counter-rotating spine & arms, lean into acceleration, run lean, landing squash
- [ ] foot IK on slopes, turn-in-place, climbing, swimming strokes, mounted riding, per-weapon locomotion sets
### P6 Kingdom building (Athran loop)  (v1.8+)
- [ ] resources (wood / stone / food / gold), gather nodes (trees, ore, farms)
- [ ] build mode: walls, towers, gates, houses, barracks, farms; placement grid + cost
- [ ] hire / command soldiers (follow, hold, attack, garrison), recruit from villagers
- [ ] nightly demon invasions with pathfinding to the keep, siege units, boss waves, tower defence AI
- [ ] reclaim corrupted lost kingdoms: corruption fog, purify beacons, dungeons
### P7 Art / Blender pipeline
- [~] Polyfork kit (assets/polyfork) in use for buildings/props/figures
- [ ] more citizen / pet / tree / building assets from Polyfork re-built in Blender (tools/blender_build.py) with baked AO
- [ ] audio ambience (wind, river, birds, crickets, town hubbub), music
### P8 Systems
- [ ] save / load, map markers, journal, settings for explorer, gamepad in explorer

## Stats (update every session)
| version | lines of C++ | assets | notes |
|---|---|---|---|
| v1.6.0 | ~5.2 k (dense) | 26 MB | explorer world |
