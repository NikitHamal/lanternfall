#!/usr/bin/env python3
"""Download the curated Polyfork picks (GLB + ES module + detail JSON) into assets_raw/ and write assets_raw/manifest.json.
Free models only, no key. Usage: python3 tools/polyfork_fetch.py"""
import json, os, urllib.request
UA = {'User-Agent': 'lanternfall-assets/1'}
# game_key -> (polyfork id, category, role)
PICKS = {
 # heroes (humanoid Mixamo-named rig)
 'warden':      ('knight-of-the-forge-7e2bee', 'hero', 'TANK'),
 'reaver':      ('varka-5a6264', 'hero', 'HEAVY MELEE'),
 'fletcher':    ('ashen-assassin-1ab976', 'hero', 'RANGED'),
 'lamplighter': ('fire-cultist-05e52e', 'hero', 'FIRE CASTER'),
 'apothecary':  ('forest-witch-63e5f9', 'hero', 'SUPPORT'),
 'rimebinder':  ('order-judge-3db47a', 'hero', 'FROST CASTER'),
 # monsters + bosses
 'thornling':   ('bloom-elemental-bbe733', 'monster', 'swarm'),
 'peatwight':   ('bog-fiend-cfd54c', 'monster', 'bruiser'),
 'gloamhound':  ('flesh-eater-8401c3', 'monster', 'flanker (quadruped)'),
 'gloamwisp':   ('hanging-cheesecloth-ghosts-c569cf', 'monster', 'ranged floater'),
 'rustbound':   ('bell-warden-838915', 'monster', 'elite'),
 'cairnback':   ('cathedral-statue-36eaad', 'monster', 'elite (stone)'),
 'barrowking':  ('bone-warrior-3a0f95', 'boss', 'sweep'),
 'mossjaw':     ('the-drowned-behemoth-889bd3', 'boss', 'slam'),
 'hollowstag':  ('spectral-stag-d19dc1', 'boss', 'charge (quadruped)'),
 'emberhound':  ('ember-dog-50b92d', 'bonus', 'bestiary extra (quadruped)'),
 'thornboar':   ('thorn-boar-669ac9', 'bonus', 'bestiary extra (quadruped)'),
 'rootcolossus':('root-colossus-2b995e', 'bonus', 'bestiary extra'),
 # weapons
 'w_arming_sword': ('arming-sword-ab8738', 'weapon', 'warden main hand'),
 'w_greatsword':   ('greatsword-a956f3', 'weapon', 'reaver'),
 'w_bow':          ('recurve-hunting-bow-2903c8', 'weapon', 'fletcher'),
 'w_kite_shield':  ('kite-shield-343e62', 'weapon', 'warden off hand'),
 'w_round_shield': ('round-wooden-shield-b81fe3', 'weapon', 'spare'),
 'w_staff':        ('living-wood-druid-staff-fd83c1', 'weapon', 'rimebinder / apothecary'),
 'w_cleaver':      ('orcish-cleaver-7117ab', 'weapon', 'monster weapon'),
 'w_lantern':      ('camping-lantern-21d14b', 'weapon', 'lamplighter lantern'),
 # terrain props
 'p_cairn':        ('stone-cairn-a9bf48', 'prop', 'cairn'),
 'p_dead_tree':    ('dead-tree-6795fa', 'prop', 'tree'),
 'p_twisted_oak':  ('twisted-dead-oak-49457b', 'prop', 'tree'),
 'p_headstone':    ('carved-headstone-09d9f8', 'prop', 'standing stone'),
 'p_grave_mound':  ('grave-mound-169ec3', 'prop', 'barrow mound'),
 'p_rune_stone':   ('glowing-rune-stone-5a5558', 'prop', 'waylight plinth'),
 'p_split_fence':  ('split-rail-fence-run-87da39', 'prop', 'fence'),
 'p_low_wall':     ('low-stone-wall-8a8ea3', 'prop', 'wall'),
 'p_field_wall':   ('cemetery-fieldstone-wall-run-ab9077', 'prop', 'wall'),
 'p_stone_well':   ('stone-well-2b4717', 'prop', 'well'),
 'p_brazier':      ('standing-brazier-c9e4ee', 'prop', 'brazier'),
 'p_wall_torch':   ('wall-torch-bracket-f5ad14', 'prop', 'torch'),
 'p_plank_bridge': ('log-plank-bridge-a5f74f', 'prop', 'fen bridge'),
 # buildings
 'b_stone_chapel': ('stone-chapel-03e774', 'building', 'chapel'),
 'b_thatch_cottage':('thatched-cottage-0e63f5', 'building', 'cottage'),
 'b_ruined_wall':  ('ruined-lair-wall-176561', 'building', 'ruin'),
 'b_beacon_tower': ('signal-beacon-tower-9c0d1a', 'building', 'waylight tower'),
 'b_stone_tower':  ('tupa-stone-tower-801c13', 'building', 'tower'),
 'b_ruined_gable': ('ruined-gable-end-834895', 'building', 'ruin'),
}
def get(u, binary=False):
    with urllib.request.urlopen(urllib.request.Request(u, headers=UA), timeout=60) as r:
        b = r.read(); return b if binary else json.loads(b)
if __name__ == '__main__':
    os.makedirs('assets_raw/glb', exist_ok=True); os.makedirs('assets_raw/mjs', exist_ok=True)
    man = {'source': 'https://polyfork.dev', 'license': 'free low-poly models: commercial use, no attribution', 'assets': {}}
    for key, (pid, cat, role) in PICKS.items():
        try:
            d = get('https://polyfork.dev/api/assets/' + pid); a = d.get('asset', d)
            for ext, sub in (('glb', 'glb'), ('mjs', 'mjs')):
                p = f'assets_raw/{sub}/{pid}.{ext}'
                if not os.path.exists(p): open(p, 'wb').write(get(f'https://polyfork.dev/cdn/{pid}.{ext}', True))
            man['assets'][key] = {'polyfork_id': pid, 'category': cat, 'role': role, 'kit': a.get('kit'), 'triangles': a.get('triangles'),
              'class': a.get('class'), 'has_rig': a.get('has_rig'), 'size_m': a.get('size_m'), 'palette': a.get('palette'),
              'page': f'https://polyfork.dev/asset/{pid}', 'glb_url': f'https://polyfork.dev/cdn/{pid}.glb', 'mjs_url': f'https://polyfork.dev/cdn/{pid}.mjs',
              'glb': f'assets_raw/glb/{pid}.glb'}
            print('ok', key, pid, a.get('triangles'))
        except Exception as e:
            print('FAIL', key, pid, e)
    json.dump(man, open('assets_raw/manifest.json', 'w'), indent=1)
    print(len(man['assets']), 'assets')
