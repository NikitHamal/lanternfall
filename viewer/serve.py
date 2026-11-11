#!/usr/bin/env python3
"""Tiny QA server for the LANTERNFALL asset viewer (stdlib only).
  /              -> viewer/index.html
  /index.json    -> live list of every .glb in assets_build/viewer/ (key, kit, mtime, tris), rebuilt on every request => viewer auto-refreshes
  /m/<key>.glb   -> assets_build/viewer/<key>.glb
usage: python3 viewer/serve.py [port]"""
import json, os, sys, http.server, socketserver, threading
HERE = os.path.dirname(os.path.abspath(__file__)); REPO = os.path.dirname(HERE)
GLB = os.environ.get('LF_VIEWER_DIR', os.path.join(REPO, 'assets_build', 'viewer'))
KITS = {'hero': '1 Heroes', 'monster': '2 Monsters', 'boss': '3 Bosses', 'bonus': '4 Bestiary extras', 'weapon': '5 Weapons', 'prop': '6 Terrain props', 'building': '7 Buildings'}

def index():
    try: man = json.load(open(os.path.join(REPO, 'assets_raw', 'manifest.json')))['assets']
    except Exception: man = {}
    out = []
    for f in sorted(os.listdir(GLB)) if os.path.isdir(GLB) else []:
        if not f.endswith('.glb'): continue
        k = f[:-4]; m = man.get(k, {}); p = os.path.join(GLB, f)
        out.append(dict(key=k, kit=KITS.get(m.get('category'), '0 Other'), title=m.get('polyfork_id', k), tris=m.get('triangles'), mtime=int(os.path.getmtime(p) * 1000), size=os.path.getsize(p)))
    return sorted(out, key=lambda x: (x['kit'], x['key']))

class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def _send(self, code, body, ctype):
        self.send_response(code); self.send_header('Content-Type', ctype); self.send_header('Content-Length', str(len(body))); self.send_header('Cache-Control', 'no-store'); self.send_header('Access-Control-Allow-Origin', '*'); self.end_headers(); self.wfile.write(body)
    def do_GET(self):
        p = self.path.split('?')[0]
        if p in ('/', '/index.html'): return self._send(200, open(os.path.join(HERE, 'index.html'), 'rb').read(), 'text/html; charset=utf-8')
        if p == '/index.json': return self._send(200, json.dumps(index()).encode(), 'application/json')
        if p.startswith('/m/') and p.endswith('.glb') and '/' not in p[3:] and '..' not in p:
            f = os.path.join(GLB, p[3:])
            if os.path.exists(f): return self._send(200, open(f, 'rb').read(), 'model/gltf-binary')
        self._send(404, b'not found', 'text/plain')

class S(socketserver.ThreadingMixIn, http.server.HTTPServer): daemon_threads = True; allow_reuse_address = True

def serve(port=8765):
    srv = S(('0.0.0.0', port), H); t = threading.Thread(target=srv.serve_forever, daemon=True); t.start(); return srv

if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765; print('viewer on http://127.0.0.1:%d  (glb dir: %s)' % (port, GLB), flush=True)
    S(('0.0.0.0', port), H).serve_forever()
