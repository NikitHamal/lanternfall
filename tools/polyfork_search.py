#!/usr/bin/env python3
"""Polyfork catalogue search -> compact table (stdlib only). usage: polyfork_search.py term [term...]"""
import sys, json, urllib.request, urllib.parse
def search(q, mt=2000):
    u = 'https://polyfork.dev/api/assets?' + urllib.parse.urlencode({'q': q, 'free': 1, 'max_triangles': mt, 'per_page': 50})
    with urllib.request.urlopen(urllib.request.Request(u, headers={'User-Agent': 'lanternfall-assets/1'}), timeout=30) as r:
        return json.load(r)['assets']
if __name__ == '__main__':
    for q in sys.argv[1:]:
        print('##', q)
        for a in search(q):
            print(f"{a['id']:46s} {str(a['class']):9s} tri={a['triangles'] or 0:5d} rig={int(bool(a['has_rig']))} kit={a['kit']}  {(a.get('size_m') or {}).get('x')}x{(a.get('size_m') or {}).get('y')}x{(a.get('size_m') or {}).get('z')}")
