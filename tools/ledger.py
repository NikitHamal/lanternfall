#!/usr/bin/env python3
"""Rebuild assets/manifest.json + assets/coverage.json (idempotent) and emit per-asset meta.json / preview / thumb for newly baked assets.
Usage: python3 tools/ledger.py [--new assets_raw/manifest.json] [--batch N]   (needs /tmp/agentB_run/pylib for numpy/PIL)"""
import sys, os, json, struct, hashlib, shutil, urllib.request, collections, time
sys.path.insert(0, '/tmp/agentB_run/pylib')
from PIL import Image
UA = {'User-Agent': 'lanternfall-assets/2'}
def get(u): return urllib.request.urlopen(urllib.request.Request(u, headers=UA), timeout=60).read()
def dhash(b):
    import io
    im = Image.open(io.BytesIO(b)).convert('RGBA'); bg = Image.new('RGBA', im.size, (128, 128, 128, 255)); bg.alpha_composite(im)
    g = bg.convert('L').resize((9, 8)); px = list(g.getdata()); h = 0
    for r in range(8):
        for c in range(8): h = (h << 1) | (px[r * 9 + c] > px[r * 9 + c + 1])
    return '%016x' % h
def lfm_info(path):
    b = open(path, 'rb').read(); nv, nt, nb, nc, nfx, _, _ = struct.unpack_from('<7I', b, 4); o = 32 + 16 + 12 + 4 + 12 + 16; o += nb * 12
    o += (nb + 3) // 4 * 4; o += nb * 12 + (nb + 3) // 4 * 4; o += nfx * 28 + nv * 36 + nt * 6; names = []
    for i in range(nc):
        nm = b[o:o + 12].split(b'\0')[0].decode(); fr = struct.unpack_from('<H', b, o + 12)[0]; names.append(nm); o += 32 + fr * nb * 14
    return dict(verts=nv, tris=nt, bones=nb, clips=names)
BIOME = {'p_pine_tall': ['highland'], 'p_pine_young': ['highland'], 'p_round_bush': ['highland', 'fen'], 'p_boulder': ['moor', 'highland'], 'p_fallen_log': ['highland', 'fen'],
 'p_barrel': ['village'], 'p_crate': ['village'], 'p_lamp_post': ['village', 'moor'], 'p_signpost': ['moor', 'highland'], 'p_campfire_tripod': ['highland', 'camp'],
 'p_thorn_bush': ['moor'], 'p_iron_fence': ['graveyard', 'moor'], 'p_cemetery_gate': ['graveyard', 'moor'], 'p_banner_pole': ['village', 'camp'],
 'b_market_stall': ['village'], 'b_timber_barn': ['village'], 'b_watchtower': ['highland', 'village']}
if __name__ == '__main__':
    os.makedirs('assets/polyfork/meta', exist_ok=True); os.makedirs('assets/polyfork/preview', exist_ok=True); os.makedirs('assets/polyfork/thumbs', exist_ok=True)
    led = json.load(open('assets/manifest.json')) if os.path.exists('assets/manifest.json') else {'schema': 2, 'assets': []}
    have = {a['key']: a for a in led['assets']}
    if not have:   # bootstrap ledger from the session-2 manifest
        old = json.load(open('assets/polyfork/manifest.json'))['assets']
        for k, a in old.items():
            e = dict(key=k, polyfork_id=a['polyfork_id'], name=a['polyfork_id'].rsplit('-', 1)[0].replace('-', ' ').title(), category=a['category'], role=a.get('role'), kit=a.get('kit'),
              triangles=a.get('triangles'), size_m=a.get('size_m'), has_rig=a.get('has_rig'), origin='polyfork-session2', version=1, variant_of=None, licence='Polyfork free low-poly: commercial use, no attribution',
              page=a.get('page'), lfm=f'assets/polyfork/{k}.lfm')
            try: e['glb_sha256'] = hashlib.sha256(get(a['glb_url'])).hexdigest()
            except Exception as ex: e['glb_sha256'] = None; print('sha fail', k, ex)
            try: e['thumb_dhash'] = dhash(get(f"https://polyfork.dev/files/{a['polyfork_id']}/renders/hero.png"))
            except Exception as ex: e['thumb_dhash'] = None; print('dh fail', k, ex)
            p = f'assets/polyfork/{k}.lfm'
            if os.path.exists(p):
                li = lfm_info(p); e['states'] = li['clips'] or ['static']; e['verts_baked'] = li['verts']; e['bones'] = li['bones']
            led['assets'].append(e); have[k] = e
    newp = sys.argv[sys.argv.index('--new') + 1] if '--new' in sys.argv else None
    bn = int(sys.argv[sys.argv.index('--batch') + 1]) if '--batch' in sys.argv else 3
    if newp:
        for k, a in json.load(open(newp))['assets'].items():
            if k in have: continue
            li = lfm_info(f'assets/polyfork/{k}.lfm'); col = json.load(open(f'assets/polyfork/{k}.col.json'))
            shutil.copy(f'assets_build/glb/{k}.glb', f'assets/polyfork/preview/{k}.glb')
            th = f'assets_raw/thumb/{a["polyfork_id"]}.png'; shutil.copy(f'shots/agentB_{k}.png', f'assets/polyfork/thumbs/{k}.png')
            e = dict(key=k, polyfork_id=a['polyfork_id'], name=a['name'], category=a['category'], class_=a['class'], role=a['role'], kit=a['kit'], triangles=a['triangles'], size_m=a['size_m'],
              palette_src=a.get('palette'), has_rig=a['has_rig'], ik_type=a['ik_type'], licence=a['licence'], page=a['page'], glb_url=a['glb_url'], glb_sha256=a['glb_sha256'],
              thumb_dhash=dhash(open(th, 'rb').read()), origin='polyfork-batch%d' % bn, version=1, variant_of=None, states=['static'], verts_baked=li['verts'], tris_baked=li['tris'],
              palette_theme=a.get('theme') or 'BUILD', lum_scale=a.get('lum_scale', 1.0), biomes=BIOME.get(k, []), collision=dict(aabb=col['aabb'], radius=col['radius'], height=col['height'], footprint_pts=len(col['footprint'])),
              lfm=f'assets/polyfork/{k}.lfm', col=f'assets/polyfork/{k}.col.json', preview_glb=f'assets/polyfork/preview/{k}.glb', thumb=f'assets/polyfork/thumbs/{k}.png', meta=f'assets/polyfork/meta/{k}.json',
              baked=dict(ao='vertex Col.a', cavity='aBn.w', origin='ground y=0', up='+Y', front='+Z', units='metres'), added=time.strftime('%Y-%m-%d'))
            e['class'] = e.pop('class_'); json.dump(e, open(f'assets/polyfork/meta/{k}.json', 'w'), indent=1)
            led['assets'].append(e); have[k] = e
    A = led['assets']
    for f in ('polyfork_id', 'glb_sha256', 'thumb_dhash'):
        c = collections.Counter(a.get(f) for a in A if a.get(f)); d = [k for k, v in c.items() if v > 1]
        if d: print('DUPLICATE', f, d)
    led['count'] = len(A); json.dump(led, open('assets/manifest.json', 'w'), indent=1)
    TGT = dict(character=30, building=50, prop=150, animal=15, terrain=6, weapon=40)
    def cls(a):
        c = a['category']; return {'hero': 'character', 'monster': 'character', 'boss': 'character', 'bonus': 'animal'}.get(c, c)
    by = collections.Counter(cls(a) for a in A); byk = collections.Counter(a.get('kit') or '-' for a in A); byb = collections.Counter(b for a in A for b in (a.get('biomes') or ['moor']))
    byr = collections.Counter((a.get('role') or '?').split(' (')[0] for a in A)
    cov = dict(targets=TGT, by_class=dict(by), remaining={k: max(0, v - by.get(k, 0)) for k, v in TGT.items()}, by_kit=dict(byk), by_biome=dict(byb), by_role=dict(byr),
      total_triangles_source=sum(a.get('triangles') or 0 for a in A),
      role_gaps=['archer', 'guard', 'civilian', 'caster (extra)', 'bridge (fen, long)', 'shrine', 'cart', 'ground tile', 'cliff', 'water prop', 'reeds', 'house (more)'],
      search_gaps={'handcart': 'poor (Polyfork search returned coverage: poor; only a barrel-organ cart)'}, updated=time.strftime('%Y-%m-%d'))
    json.dump(cov, open('assets/coverage.json', 'w'), indent=1); print('assets', len(A), dict(by))
