#!/usr/bin/env python3
"""Fetch a gap-driven batch (tools/batches/<name>.json) -> assets_raw/{glb,mjs,thumb} + assets_raw/manifest.json (build input).
Skips ids/sha256 already in assets/manifest.json. Usage: python3 tools/polyfork_fetch_batch.py tools/batches/batch3.json"""
import json, os, sys, hashlib, urllib.request
UA = {'User-Agent': 'lanternfall-assets/2'}
def get(u, binary=False):
    with urllib.request.urlopen(urllib.request.Request(u, headers=UA), timeout=60) as r:
        b = r.read(); return b if binary else json.loads(b)
if __name__ == '__main__':
    picks = json.load(open(sys.argv[1]))
    led = json.load(open('assets/manifest.json')) if os.path.exists('assets/manifest.json') else {'assets': []}
    ids = {a['polyfork_id'] for a in led['assets']}; shas = {a.get('glb_sha256') for a in led['assets']}
    for d in ('glb', 'mjs', 'thumb'): os.makedirs('assets_raw/' + d, exist_ok=True)
    man = {'source': 'https://polyfork.dev', 'assets': {}}
    for key, p in picks.items():
        pid = p['id']
        if pid in ids: print('SKIP covered', pid); continue
        d = get('https://polyfork.dev/api/assets/' + pid); a = d.get('asset', d)
        glb = get(f'https://polyfork.dev/cdn/{pid}.glb', True); sha = hashlib.sha256(glb).hexdigest()
        if sha in shas: print('SKIP sha covered', pid); continue
        open(f'assets_raw/glb/{pid}.glb', 'wb').write(glb)
        try: open(f'assets_raw/thumb/{pid}.png', 'wb').write(get(a.get('thumbnail') or f'https://polyfork.dev/files/{pid}/renders/hero.png', True))
        except Exception as e: print('thumb fail', pid, e)
        man['assets'][key] = {'polyfork_id': pid, 'name': a.get('title'), 'category': p['cat'], 'role': p['role'], 'theme': p.get('theme'), 'kit': a.get('kit'),
            'triangles': a.get('triangles'), 'class': a.get('class'), 'has_rig': a.get('has_rig'), 'ik_type': a.get('ik_type'), 'size_m': a.get('size_m'),
            'palette': a.get('palette'), 'licence': 'Polyfork free low-poly: commercial use, no attribution', 'page': f'https://polyfork.dev/asset/{pid}',
            'glb_url': f'https://polyfork.dev/cdn/{pid}.glb', 'glb': f'assets_raw/glb/{pid}.glb', 'glb_sha256': sha}
        print('ok', key, pid, a.get('triangles'))
    json.dump(man, open('assets_raw/manifest.json', 'w'), indent=1); print(len(man['assets']), 'assets')
