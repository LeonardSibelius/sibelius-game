#!/usr/bin/env python3
"""Tools/ue_bridge/ue_bridge.py — Cowork/Code <-> live Unreal Editor bridge (SIB-45).

Runs unreal.* Python inside the RUNNING editor over UE's first-party Python Remote Execution
(UDP multicast 239.0.0.1:6766 discovery + a loopback command channel) and parses a JSON result.

EDITOR-OPEN ONLY. This is the editor-open loop; it never runs during a cook/build/gate (the editor
host is gone by then, and its ports are disjoint from UnrealClaude's TCP 3000 — no PK15 collision).

Run with the engine's bundled Python (no standalone Python needed):
  "C:\\Program Files\\Epic Games\\UE_5.7\\Engine\\Binaries\\ThirdParty\\Python3\\Win64\\python.exe" \\
      Tools\\ue_bridge\\ue_bridge.py <verb> ...
or use the ue_bridge.cmd wrapper. v1 = READ verbs only (list-actors, get-transform,
get-property, get-component-property, read-log). Write/act verbs come next.
"""

import argparse
import json
import os
import sys
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
import remote_execution as rexec  # vendored from the engine's PythonScriptPlugin

# Sentinels delimit our JSON payload inside the editor's (possibly noisy) stdout.
SA = "<<<UEBRIDGE>>>"
SB = "<<<ENDUEBRIDGE>>>"


class BridgeError(Exception):
    pass


def _project_dir():
    return os.path.abspath(os.path.join(_HERE, "..", ".."))


# ---------------------------------------------------------------- remote exec

def _connect(discover_timeout=6.0):
    """Open a command connection to the running editor, or raise BridgeError."""
    conn = rexec.RemoteExecution()
    conn.start()
    deadline = time.time() + discover_timeout
    nodes = []
    while time.time() < deadline:
        nodes = conn.remote_nodes
        if nodes:
            break
        time.sleep(0.1)
    if not nodes:
        conn.stop()
        raise BridgeError(
            "No Unreal Editor found on the Python remote-exec multicast.\n"
            "Confirm: (1) the editor is OPEN, (2) DefaultEngine.ini has bRemoteExecution=True,\n"
            "(3) the editor log shows 'Python Remote Execution' listening on startup.\n"
            "This bridge is editor-open only — never run it during a cook/build.")
    conn.open_command_connection(nodes[0]["node_id"])
    return conn


def _run(conn, py_snippet):
    """Run a Python snippet in the editor; return the parsed sentinel JSON payload."""
    res = conn.run_command(py_snippet, unattended=True,
                           exec_mode=rexec.MODE_EXEC_FILE, raise_on_failure=False)
    out = "".join((o.get("output") or "") for o in (res.get("output") or []))
    if not res.get("success"):
        raise BridgeError("Editor Python raised:\n" + out + "\nresult=" + str(res.get("result")))
    if SA in out and SB in out:
        chunk = out.split(SA, 1)[1].split(SB, 1)[0]
        return json.loads(chunk)
    raise BridgeError("No bridge payload in editor output. Raw output:\n" + out)


# Shared preamble injected into every actor/property snippet: world selection (editor vs PIE),
# label lookup, and a JSON-safe value serializer. Baked in so the AI never re-derives editor-vs-PIE
# or relative-vs-world (the June 15 lessons).
_PREAMBLE = r"""
import unreal, json
def _world():
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    try:
        gw = ues.get_game_world()
    except Exception:
        gw = None
    ew = ues.get_editor_world()
    # During PIE gw is the play world (distinct from the editor world); otherwise read the editor world.
    return gw if (gw and gw != ew) else ew
def _find(label):
    for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.Actor):
        try:
            if a.get_actor_label() == label:
                return a
        except Exception:
            pass
    return None
def _ser(v):
    try:
        if v is None or isinstance(v, (bool, int, float, str)):
            return v
        if isinstance(v, unreal.Vector):
            return {'x': v.x, 'y': v.y, 'z': v.z}
        if isinstance(v, unreal.Vector2D):
            return {'x': v.x, 'y': v.y}
        if isinstance(v, unreal.Rotator):
            return {'pitch': v.pitch, 'yaw': v.yaw, 'roll': v.roll}
        if isinstance(v, unreal.LinearColor):
            return {'r': v.r, 'g': v.g, 'b': v.b, 'a': v.a}
        if isinstance(v, unreal.Name):
            return str(v)
        if isinstance(v, unreal.Object):
            return v.get_path_name()
        if isinstance(v, (unreal.Array, unreal.Set, list, tuple)):
            return [_ser(x) for x in v]
        return str(v)
    except Exception as e:
        return 'unserializable(%s)' % e
"""


def _emit(body):
    """Wrap a snippet body (which must set `payload`) with the preamble + sentinel print."""
    return _PREAMBLE + "\n" + body + "\nprint('%s' + json.dumps(payload) + '%s')\n" % (SA, SB)


# ---------------------------------------------------------------- read verbs

def list_actors(conn, name_filter=None):
    body = (
        "flt = %r\n" % name_filter +
        "out = []\n"
        "for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.Actor):\n"
        "    try:\n"
        "        lbl = a.get_actor_label()\n"
        "    except Exception:\n"
        "        lbl = a.get_name()\n"
        "    if flt and flt.lower() not in lbl.lower():\n"
        "        continue\n"
        "    out.append({'label': lbl, 'class': a.get_class().get_name(), 'path': a.get_path_name()})\n"
        "out.sort(key=lambda d: d['label'])\n"
        "payload = {'world': _world().get_name(), 'count': len(out), 'actors': out}\n"
    )
    return _run(conn, _emit(body))


def get_actor_transform(conn, label):
    body = (
        "a = _find(%r)\n" % label +
        "if a is None:\n"
        "    payload = {'error': 'actor not found', 'label': %r}\n" % label +
        "else:\n"
        "    wl = a.get_actor_location(); wr = a.get_actor_rotation(); ws = a.get_actor_scale3d()\n"
        "    root = a.root_component\n"
        "    rl = root.get_editor_property('relative_location') if root else wl\n"
        "    rr = root.get_editor_property('relative_rotation') if root else wr\n"
        "    rs = root.get_editor_property('relative_scale3d') if root else ws\n"
        "    payload = {'label': a.get_actor_label(), 'class': a.get_class().get_name(),\n"
        "        'world': {'location': _ser(wl), 'rotation': _ser(wr), 'scale': _ser(ws)},\n"
        "        'relative': {'location': _ser(rl), 'rotation': _ser(rr), 'scale': _ser(rs)}}\n"
    )
    return _run(conn, _emit(body))


def get_property(conn, label, prop):
    body = (
        "a = _find(%r)\n" % label +
        "if a is None:\n"
        "    payload = {'error': 'actor not found', 'label': %r}\n" % label +
        "else:\n"
        "    try:\n"
        "        payload = {'label': a.get_actor_label(), 'property': %r, 'value': _ser(a.get_editor_property(%r))}\n" % (prop, prop) +
        "    except Exception as e:\n"
        "        payload = {'label': a.get_actor_label(), 'property': %r, 'error': str(e)}\n" % prop
    )
    return _run(conn, _emit(body))


def get_component_property(conn, label, component, prop):
    body = (
        "a = _find(%r)\n" % label +
        "if a is None:\n"
        "    payload = {'error': 'actor not found', 'label': %r}\n" % label +
        "else:\n"
        "    comp = None\n"
        "    for c in a.get_components_by_class(unreal.ActorComponent):\n"
        "        if c.get_name() == %r:\n" % component +
        "            comp = c; break\n"
        "    if comp is None:\n"
        "        payload = {'error': 'component not found', 'label': a.get_actor_label(), 'component': %r,\n" % component +
        "            'available': [c.get_name() for c in a.get_components_by_class(unreal.ActorComponent)]}\n"
        "    else:\n"
        "        try:\n"
        "            payload = {'label': a.get_actor_label(), 'component': %r, 'property': %r, 'value': _ser(comp.get_editor_property(%r))}\n" % (component, prop, prop) +
        "        except Exception as e:\n"
        "            payload = {'label': a.get_actor_label(), 'component': %r, 'property': %r, 'error': str(e)}\n" % (component, prop)
    )
    return _run(conn, _emit(body))


def read_log(lines=200, grep=None):
    """Tail the editor's log — pure file read, works regardless of remote-exec state."""
    log_path = os.path.join(_project_dir(), "Saved", "Logs", "SibeliusGame.log")
    if not os.path.isfile(log_path):
        return {"error": "log not found", "path": log_path}
    with open(log_path, "r", encoding="utf-8", errors="replace") as f:
        rows = f.readlines()
    if grep:
        rows = [r for r in rows if grep in r]
    tail = [r.rstrip("\n") for r in rows[-lines:]]
    return {"path": log_path, "grep": grep, "returned": len(tail), "tail": tail}


# ---------------------------------------------------------------- CLI

def main(argv=None):
    p = argparse.ArgumentParser(prog="ue_bridge", description="Live Unreal Editor bridge (read verbs). EDITOR-OPEN ONLY.")
    sub = p.add_subparsers(dest="verb", required=True)

    s = sub.add_parser("list-actors", help="list actors in the live world")
    s.add_argument("--filter", default=None, help="case-insensitive label substring")

    s = sub.add_parser("get-transform", help="world + relative transform of an actor by label")
    s.add_argument("label")

    s = sub.add_parser("get-property", help="read an actor UPROPERTY by label")
    s.add_argument("label")
    s.add_argument("prop")

    s = sub.add_parser("get-component-property", help="read a component UPROPERTY")
    s.add_argument("label")
    s.add_argument("component")
    s.add_argument("prop")

    s = sub.add_parser("read-log", help="tail Saved/Logs/SibeliusGame.log")
    s.add_argument("--lines", type=int, default=200)
    s.add_argument("--grep", default=None)

    args = p.parse_args(argv)

    try:
        if args.verb == "read-log":
            result = read_log(lines=args.lines, grep=args.grep)
        else:
            conn = _connect()
            try:
                if args.verb == "list-actors":
                    result = list_actors(conn, name_filter=args.filter)
                elif args.verb == "get-transform":
                    result = get_actor_transform(conn, args.label)
                elif args.verb == "get-property":
                    result = get_property(conn, args.label, args.prop)
                elif args.verb == "get-component-property":
                    result = get_component_property(conn, args.label, args.component, args.prop)
            finally:
                conn.close_command_connection()
                conn.stop()
    except BridgeError as e:
        print(json.dumps({"ok": False, "error": str(e)}, indent=2))
        return 1

    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
