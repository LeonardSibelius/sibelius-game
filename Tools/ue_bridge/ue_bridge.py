#!/usr/bin/env python3
"""Tools/ue_bridge/ue_bridge.py — Cowork/Code <-> live Unreal Editor bridge (SIB-45).

Runs unreal.* Python inside the RUNNING editor over UE's first-party Python Remote Execution
(UDP multicast 239.0.0.1:6766 discovery + a loopback command channel) and parses a JSON result.

EDITOR-OPEN ONLY. This is the editor-open loop; it never runs during a cook/build/gate (the editor
host is gone by then, and its ports are disjoint from UnrealClaude's TCP 3000 — no PK15 collision).

Run with the engine's bundled Python (no standalone Python needed):
  "C:\\Program Files\\Epic Games\\UE_5.7\\Engine\\Binaries\\ThirdParty\\Python3\\Win64\\python.exe" \\
      Tools\\ue_bridge\\ue_bridge.py <verb> ...
or use the ue_bridge.cmd wrapper.

READ:  list-actors, get-transform, get-property, get-component-property, read-log
WRITE: set-actor-transform, set-property, set-component-property, place-actor, duplicate-actor,
       delete-actor (--confirm)
ACT:   run-pie, stop-pie, exec-console, screenshot, save-level (--confirm), save-asset (--confirm),
       exec-python

Writes target the EDITOR world (persistent placement); reads resolve the PIE world while playing.
delete-actor / save-* require --confirm — Walt's hand-placed work is precious.
"""

import argparse
import glob
import json
import os
import sys
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
import remote_execution as rexec  # vendored from the engine's PythonScriptPlugin

SA = "<<<UEBRIDGE>>>"
SB = "<<<ENDUEBRIDGE>>>"


class BridgeError(Exception):
    pass


def _project_dir():
    return os.path.abspath(os.path.join(_HERE, "..", ".."))


# ---------------------------------------------------------------- remote exec

def _connect(discover_timeout=6.0):
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
            "(3) the editor restarted after enabling it. EDITOR-OPEN ONLY — never during a cook/build.")
    conn.open_command_connection(nodes[0]["node_id"])
    return conn


def _run(conn, py_snippet):
    res = conn.run_command(py_snippet, unattended=True,
                           exec_mode=rexec.MODE_EXEC_FILE, raise_on_failure=False)
    out = "".join((o.get("output") or "") for o in (res.get("output") or []))
    if not res.get("success"):
        raise BridgeError("Editor Python raised:\n" + out + "\nresult=" + str(res.get("result")))
    if SA in out and SB in out:
        return json.loads(out.split(SA, 1)[1].split(SB, 1)[0])
    raise BridgeError("No bridge payload in editor output. Raw output:\n" + out)


# Shared preamble: world selection (editor vs PIE), editor-world find (writes), label find (reads),
# value (de)serialization, and string->typed coercion driven by the property's existing type.
_PREAMBLE = r"""
import unreal, json
def _world():
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    try:
        gw = ues.get_game_world()
    except Exception:
        gw = None
    ew = ues.get_editor_world()
    return gw if (gw and gw != ew) else ew
def _find(label):
    for a in unreal.GameplayStatics.get_all_actors_of_class(_world(), unreal.Actor):
        try:
            if a.get_actor_label() == label:
                return a
        except Exception:
            pass
    return None
def _efind(label):
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        try:
            if a.get_actor_label() == label:
                return a
        except Exception:
            pass
    return None
def _nums(s):
    s = str(s).replace('(', '').replace(')', '').replace('[', '').replace(']', '')
    return [float(x) for x in s.split(',') if x.strip() != '']
def _coerce(template, s):
    if isinstance(template, bool):
        return str(s).strip().lower() in ('true', '1', 'yes', 'on')
    if isinstance(template, int):
        return int(float(s))
    if isinstance(template, float):
        return float(s)
    if isinstance(template, unreal.Vector):
        n = _nums(s); return unreal.Vector(n[0], n[1], n[2])
    if isinstance(template, unreal.Vector2D):
        n = _nums(s); return unreal.Vector2D(n[0], n[1])
    if isinstance(template, unreal.Rotator):
        n = _nums(s); return unreal.Rotator(n[0], n[1], n[2])
    if isinstance(template, str):
        return s
    return json.loads(s)
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
        "    comp = next((c for c in a.get_components_by_class(unreal.ActorComponent) if c.get_name() == %r), None)\n" % component +
        "    if comp is None:\n"
        "        payload = {'error': 'component not found', 'component': %r,\n" % component +
        "            'available': [c.get_name() for c in a.get_components_by_class(unreal.ActorComponent)]}\n"
        "    else:\n"
        "        try:\n"
        "            payload = {'label': a.get_actor_label(), 'component': %r, 'property': %r, 'value': _ser(comp.get_editor_property(%r))}\n" % (component, prop, prop) +
        "        except Exception as e:\n"
        "            payload = {'component': %r, 'property': %r, 'error': str(e)}\n" % (component, prop)
    )
    return _run(conn, _emit(body))


def read_log(lines=200, grep=None):
    log_path = os.path.join(_project_dir(), "Saved", "Logs", "SibeliusGame.log")
    if not os.path.isfile(log_path):
        return {"error": "log not found", "path": log_path}
    with open(log_path, "r", encoding="utf-8", errors="replace") as f:
        rows = f.readlines()
    if grep:
        rows = [r for r in rows if grep in r]
    tail = [r.rstrip("\n") for r in rows[-lines:]]
    return {"path": log_path, "grep": grep, "returned": len(tail), "tail": tail}


# ---------------------------------------------------------------- write verbs

def set_actor_transform(conn, label, location=None, rotation=None, scale=None):
    body = (
        "a = _efind(%r)\n" % label +
        "loc=%r; rot=%r; scl=%r\n" % (location, rotation, scale) +
        "if a is None:\n"
        "    payload = {'error': 'actor not found (editor world)', 'label': %r}\n" % label +
        "else:\n"
        "    if loc is not None:\n        n=_nums(loc); a.set_actor_location(unreal.Vector(n[0],n[1],n[2]), False, False)\n"
        "    if rot is not None:\n        n=_nums(rot); a.set_actor_rotation(unreal.Rotator(n[0],n[1],n[2]), False)\n"
        "    if scl is not None:\n        n=_nums(scl); a.set_actor_scale3d(unreal.Vector(n[0],n[1],n[2]))\n"
        "    wl=a.get_actor_location(); wr=a.get_actor_rotation(); ws=a.get_actor_scale3d()\n"
        "    payload = {'label': a.get_actor_label(), 'world': {'location': _ser(wl), 'rotation': _ser(wr), 'scale': _ser(ws)}}\n"
    )
    return _run(conn, _emit(body))


def set_property(conn, label, prop, value):
    body = (
        "a = _efind(%r)\n" % label +
        "prop=%r; val=%r\n" % (prop, value) +
        "if a is None:\n"
        "    payload = {'error': 'actor not found (editor world)', 'label': %r}\n" % label +
        "else:\n"
        "    try:\n"
        "        if prop.endswith(']') and '[' in prop:\n"
        "            base=prop[:prop.index('[')]; idx=int(prop[prop.index('[')+1:-1])\n"
        "            arr=a.get_editor_property(base)\n"
        "            tmpl=arr[idx] if idx < len(arr) else (arr[0] if len(arr)>0 else unreal.Vector())\n"
        "            arr[idx]=_coerce(tmpl, val); a.set_editor_property(base, arr)\n"
        "            payload={'label': a.get_actor_label(), 'property': prop, 'value': _ser(a.get_editor_property(base))}\n"
        "        else:\n"
        "            cur=a.get_editor_property(prop); a.set_editor_property(prop, _coerce(cur, val))\n"
        "            payload={'label': a.get_actor_label(), 'property': prop, 'value': _ser(a.get_editor_property(prop))}\n"
        "    except Exception as e:\n"
        "        payload={'label': a.get_actor_label(), 'property': prop, 'error': str(e)}\n"
    )
    return _run(conn, _emit(body))


def set_component_property(conn, label, component, prop, value):
    body = (
        "a = _efind(%r)\n" % label +
        "prop=%r; val=%r\n" % (prop, value) +
        "if a is None:\n"
        "    payload = {'error': 'actor not found (editor world)', 'label': %r}\n" % label +
        "else:\n"
        "    comp = next((c for c in a.get_components_by_class(unreal.ActorComponent) if c.get_name() == %r), None)\n" % component +
        "    if comp is None:\n"
        "        payload = {'error': 'component not found', 'component': %r}\n" % component +
        "    else:\n"
        "        try:\n"
        "            cur=comp.get_editor_property(prop); comp.set_editor_property(prop, _coerce(cur, val))\n"
        "            payload={'label': a.get_actor_label(), 'component': %r, 'property': prop, 'value': _ser(comp.get_editor_property(prop))}\n" % component +
        "        except Exception as e:\n"
        "            payload={'component': %r, 'property': prop, 'error': str(e)}\n" % component
    )
    return _run(conn, _emit(body))


def place_actor(conn, class_path, location=None, rotation=None, scale=None, label=None):
    body = (
        "cp=%r; lbl=%r\n" % (class_path, label) +
        "cls = unreal.load_class(None, cp)\n"
        "obj = None if cls else unreal.load_object(None, cp)\n"
        "if cls is None and obj is None:\n"
        "    payload = {'error': 'class/asset not found', 'class_path': cp}\n"
        "else:\n"
        "    nl=_nums(%r) if %r else [0.0,0.0,0.0]\n" % (location, location) +
        "    nr=_nums(%r) if %r else [0.0,0.0,0.0]\n" % (rotation, rotation) +
        "    eas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\n"
        "    loc=unreal.Vector(nl[0],nl[1],nl[2]); rot=unreal.Rotator(nr[0],nr[1],nr[2])\n"
        "    act = eas.spawn_actor_from_class(cls, loc, rot) if cls else eas.spawn_actor_from_object(obj, loc, rot)\n"
        "    if act is None:\n        payload={'error':'spawn failed','class_path':cp}\n"
        "    else:\n"
        "        if %r:\n            n=_nums(%r); act.set_actor_scale3d(unreal.Vector(n[0],n[1],n[2]))\n" % (scale, scale) +
        "        if lbl:\n            act.set_actor_label(lbl)\n"
        "        payload={'label': act.get_actor_label(), 'class': act.get_class().get_name(), 'path': act.get_path_name(), 'location': _ser(act.get_actor_location())}\n"
    )
    return _run(conn, _emit(body))


def duplicate_actor(conn, label):
    body = (
        "a = _efind(%r)\n" % label +
        "if a is None:\n"
        "    payload = {'error': 'actor not found (editor world)', 'label': %r}\n" % label +
        "else:\n"
        "    eas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\n"
        "    dup = eas.duplicate_actor(a)\n"
        "    payload = {'error':'duplicate failed'} if dup is None else {'source': a.get_actor_label(), 'label': dup.get_actor_label(), 'path': dup.get_path_name()}\n"
    )
    return _run(conn, _emit(body))


def delete_actor(conn, label):
    body = (
        "a = _efind(%r)\n" % label +
        "if a is None:\n"
        "    payload = {'error': 'actor not found (editor world)', 'label': %r}\n" % label +
        "else:\n"
        "    pn = a.get_path_name()\n"
        "    eas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\n"
        "    ok = eas.destroy_actor(a)\n"
        "    payload = {'deleted': bool(ok), 'label': %r, 'path': pn}\n" % label
    )
    return _run(conn, _emit(body))


# ---------------------------------------------------------------- act verbs

def run_pie(conn):
    body = (
        "les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n"
        "les.editor_play_simulate()\n"
        "payload = {'pie': 'started (simulate)'}\n"
    )
    return _run(conn, _emit(body))


def stop_pie(conn):
    body = (
        "les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n"
        "les.editor_request_end_play()\n"
        "payload = {'pie': 'stop requested'}\n"
    )
    return _run(conn, _emit(body))


def exec_console(conn, command):
    body = (
        "unreal.SystemLibrary.execute_console_command(_world(), %r)\n" % command +
        "payload = {'console': %r}\n" % command
    )
    return _run(conn, _emit(body))


def screenshot(conn, res="1920x1080", timeout=20.0):
    shot_dir = os.path.join(_project_dir(), "Saved", "Screenshots", "WindowsEditor")
    before = set(glob.glob(os.path.join(shot_dir, "*.png"))) if os.path.isdir(shot_dir) else set()
    exec_console(conn, "HighResShot %s" % res)
    deadline = time.time() + timeout
    newest = None
    while time.time() < deadline:
        now = set(glob.glob(os.path.join(shot_dir, "*.png"))) if os.path.isdir(shot_dir) else set()
        fresh = now - before
        if fresh:
            newest = max(fresh, key=os.path.getmtime)
            # wait until the file size settles (async write)
            s1 = os.path.getsize(newest); time.sleep(0.4); s2 = os.path.getsize(newest)
            if s1 == s2:
                break
        time.sleep(0.3)
    if newest:
        return {"screenshot": newest, "res": res}
    return {"error": "no new screenshot appeared", "dir": shot_dir, "res": res,
            "hint": "HighResShot writes after a tick; raise --timeout or check the dir"}


def save_level(conn):
    body = (
        "les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n"
        "ok = les.save_current_level()\n"
        "payload = {'saved_level': bool(ok)}\n"
    )
    return _run(conn, _emit(body))


def save_asset(conn, asset_path):
    body = (
        "ok = unreal.EditorAssetLibrary.save_asset(%r, only_if_is_dirty=False)\n" % asset_path +
        "payload = {'saved_asset': bool(ok), 'path': %r}\n" % asset_path
    )
    return _run(conn, _emit(body))


def exec_python(conn, snippet):
    """Raw escape hatch: run a snippet; the snippet itself must set `payload` (or print)."""
    if "payload" in snippet:
        return _run(conn, _emit(snippet))
    # No payload set: run as-is and echo captured stdout.
    res = conn.run_command(snippet, unattended=True, exec_mode=rexec.MODE_EXEC_FILE, raise_on_failure=False)
    out = "".join((o.get("output") or "") for o in (res.get("output") or []))
    return {"success": bool(res.get("success")), "output": out, "result": res.get("result")}


# ---------------------------------------------------------------- CLI

def main(argv=None):
    p = argparse.ArgumentParser(prog="ue_bridge", description="Live Unreal Editor bridge. EDITOR-OPEN ONLY.")
    sub = p.add_subparsers(dest="verb", required=True)

    s = sub.add_parser("list-actors"); s.add_argument("--filter", default=None)
    s = sub.add_parser("get-transform"); s.add_argument("label")
    s = sub.add_parser("get-property"); s.add_argument("label"); s.add_argument("prop")
    s = sub.add_parser("get-component-property"); s.add_argument("label"); s.add_argument("component"); s.add_argument("prop")
    s = sub.add_parser("read-log"); s.add_argument("--lines", type=int, default=200); s.add_argument("--grep", default=None)

    s = sub.add_parser("set-actor-transform"); s.add_argument("label")
    s.add_argument("--location"); s.add_argument("--rotation"); s.add_argument("--scale")
    s = sub.add_parser("set-property"); s.add_argument("label"); s.add_argument("prop"); s.add_argument("value")
    s = sub.add_parser("set-component-property"); s.add_argument("label"); s.add_argument("component"); s.add_argument("prop"); s.add_argument("value")
    s = sub.add_parser("place-actor"); s.add_argument("class_path")
    s.add_argument("--location"); s.add_argument("--rotation"); s.add_argument("--scale"); s.add_argument("--label")
    s = sub.add_parser("duplicate-actor"); s.add_argument("label")
    s = sub.add_parser("delete-actor"); s.add_argument("label"); s.add_argument("--confirm", action="store_true")

    sub.add_parser("run-pie")
    sub.add_parser("stop-pie")
    s = sub.add_parser("exec-console"); s.add_argument("command")
    s = sub.add_parser("screenshot"); s.add_argument("--res", default="1920x1080"); s.add_argument("--timeout", type=float, default=20.0)
    sub.add_parser("save-level").add_argument("--confirm", action="store_true")
    s = sub.add_parser("save-asset"); s.add_argument("asset_path"); s.add_argument("--confirm", action="store_true")
    s = sub.add_parser("exec-python"); s.add_argument("snippet")

    args = p.parse_args(argv)

    # Guard the destructive/persisting verbs behind --confirm.
    if args.verb in ("delete-actor", "save-level", "save-asset") and not getattr(args, "confirm", False):
        print(json.dumps({"ok": False, "error": "%s requires --confirm (destructive/persisting)." % args.verb}, indent=2))
        return 2

    needs_editor = args.verb != "read-log"
    conn = None
    try:
        if needs_editor:
            conn = _connect()
        if args.verb == "read-log":
            result = read_log(lines=args.lines, grep=args.grep)
        elif args.verb == "list-actors":
            result = list_actors(conn, name_filter=args.filter)
        elif args.verb == "get-transform":
            result = get_actor_transform(conn, args.label)
        elif args.verb == "get-property":
            result = get_property(conn, args.label, args.prop)
        elif args.verb == "get-component-property":
            result = get_component_property(conn, args.label, args.component, args.prop)
        elif args.verb == "set-actor-transform":
            result = set_actor_transform(conn, args.label, args.location, args.rotation, args.scale)
        elif args.verb == "set-property":
            result = set_property(conn, args.label, args.prop, args.value)
        elif args.verb == "set-component-property":
            result = set_component_property(conn, args.label, args.component, args.prop, args.value)
        elif args.verb == "place-actor":
            result = place_actor(conn, args.class_path, args.location, args.rotation, args.scale, args.label)
        elif args.verb == "duplicate-actor":
            result = duplicate_actor(conn, args.label)
        elif args.verb == "delete-actor":
            result = delete_actor(conn, args.label)
        elif args.verb == "run-pie":
            result = run_pie(conn)
        elif args.verb == "stop-pie":
            result = stop_pie(conn)
        elif args.verb == "exec-console":
            result = exec_console(conn, args.command)
        elif args.verb == "screenshot":
            result = screenshot(conn, res=args.res, timeout=args.timeout)
        elif args.verb == "save-level":
            result = save_level(conn)
        elif args.verb == "save-asset":
            result = save_asset(conn, args.asset_path)
        elif args.verb == "exec-python":
            result = exec_python(conn, args.snippet)
    except BridgeError as e:
        print(json.dumps({"ok": False, "error": str(e)}, indent=2))
        return 1
    finally:
        if conn is not None:
            conn.close_command_connection()
            conn.stop()

    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
