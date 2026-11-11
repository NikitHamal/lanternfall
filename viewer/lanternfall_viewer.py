import marimo

__generated_with = "0.9.0"
app = marimo.App(width="full", app_title="LANTERNFALL asset viewer")


@app.cell
def _():
    import marimo as mo
    import os, sys, threading, time, urllib.request
    return mo, os, sys, threading, time, urllib


@app.cell
def _(os, sys):
    # repo root = parent of this notebook's folder; works from molab (cwd = repo) or locally
    HERE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.path.join(os.getcwd(), "viewer")
    REPO = os.path.dirname(HERE)
    sys.path.insert(0, HERE)
    import serve
    PORT = int(os.environ.get("LF_VIEWER_PORT", "8765"))
    try:
        SRV = serve.serve(PORT)          # no-op if a previous run already owns the port
        status = f"viewer server started on :{PORT}"
    except OSError:
        SRV = None
        status = f"viewer server already running on :{PORT}"
    return HERE, PORT, REPO, SRV, serve, status


@app.cell
def _(mo, status):
    mo.md(f"""
    # LANTERNFALL · Polyfork → Blender asset viewer  <small>(QA surface only, not part of the game)</small>
    {status}. The card below is a three.js scene (1280×720): kit dropdown, asset dropdown, one button per
    animation/state, turntable + wireframe toggles and a frame counter. It **polls `/index.json` every 2 s**:
    when a `.glb` lands in or changes under `assets_build/viewer/`, the model list and the current model
    reload by themselves, keeping the selected clip.
    """)
    return


@app.cell
def _(PORT, mo):
    mo.Html(f'<iframe src="http://127.0.0.1:{PORT}/" width="1290" height="730" style="border:1px solid #333;border-radius:4px;background:#0b0f16" allow="autoplay"></iframe>')
    return


@app.cell
def _(mo):
    # auto-refreshing inventory card (every 5 s) – same data the viewer polls
    refresh = mo.ui.refresh(options=["5s"], default_interval="5s")
    refresh
    return (refresh,)


@app.cell
def _(PORT, mo, refresh, urllib):
    import json as _json
    refresh.value
    try:
        rows = _json.load(urllib.request.urlopen(f"http://127.0.0.1:{PORT}/index.json", timeout=3))
    except Exception as e:  # server not reachable (e.g. remote kernel)
        rows = []
    mo.ui.table([{"kit": r["kit"], "asset": r["key"], "polyfork": r["title"], "tris": r["tris"], "KB": r["size"] // 1024,
                  "mtime": __import__("time").strftime("%H:%M:%S", __import__("time").localtime(r["mtime"] / 1000))} for r in rows],
                selection=None, page_size=12, label=f"{len(rows)} GLB(s) in assets_build/viewer/")
    return


@app.cell
def _(mo, threading, time):
    # keep-alive: no-op tick every 30 s so an idle sandbox is not retired
    if not any(t.name == "lf-heartbeat" for t in threading.enumerate()):
        def _hb():
            while True:
                time.sleep(30)
        threading.Thread(target=_hb, name="lf-heartbeat", daemon=True).start()
    mo.md("heartbeat thread: **running** (30 s tick)")
    return


if __name__ == "__main__":
    app.run()
