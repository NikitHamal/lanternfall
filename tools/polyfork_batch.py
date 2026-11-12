#!/usr/bin/env python3
"""Gap-driven Polyfork shortlist. Usage: polyfork_batch.py search <q> [class] [max_tris]  |  thumbs id... """
import json, sys, urllib.request, urllib.parse, os
UA = {'User-Agent': 'lanternfall-assets/2'}
def get(u, binary=False):
    with urllib.request.urlopen(urllib.request.Request(u, headers=UA), timeout=60) as r:
        b = r.read(); return b if binary else json.loads(b)
def covered():
    ids = set()
    for p in ('assets/manifest.json',):
        if os.path.exists(p):
            m = json.load(open(p))
            for a in m['assets']: ids.add(a['polyfork_id']); 
    return ids
def search(q, cls='', mt=1500):
    u = 'https://polyfork.dev/api/assets?free=1&fields=compact&per_page=12&max_triangles=%d&q=%s' % (mt, urllib.parse.quote(q))
    if cls: u += '&class=' + cls
    d = get(u); cov = covered()
    return d.get('coverage'), [a for a in d['assets'] if a['id'] not in cov]
if __name__ == '__main__':
    cmd = sys.argv[1]
    if cmd == 'search':
        cv, items = search(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else '', int(sys.argv[4]) if len(sys.argv) > 4 else 1500)
        print('##', sys.argv[2], 'coverage:', cv)
        for a in items: print(' ', a['id'], '|', a['class'], a['triangles'], (a.get('kit') or '-')[:28], a['size_m'])
    elif cmd == 'thumbs':
        os.makedirs('shots/shortlist', exist_ok=True)
        for i in sys.argv[2:]:
            for url in ('https://polyfork.dev/cdn/%s.png' % i, 'https://polyfork.dev/cdn/%s-thumb.png' % i):
                try: open('shots/shortlist/%s.png' % i, 'wb').write(get(url, True)); print('ok', i); break
                except Exception as e: print('fail', url, e)
