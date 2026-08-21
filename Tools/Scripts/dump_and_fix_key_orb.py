"""
dump_and_fix_key_orb.py — one alcove orb, books take the attic key.

The little sphere Walt walks up to in the library alcove is the KeyItem BuildSite.
COMPILE is locked, so C never spends the books. PowerGrant_AtticKey was moved to a
bookshelf and is a second, easy-to-miss sphere.

This script:
  * dumps every BuildSite and PowerGrant
  * forces the KeyItem site to consume-on-build + ghost-orb + 8 books
  * hides PowerGrant_AtticKey so only the alcove orb remains
  * if no KeyItem site exists, parks PowerGrant_AtticKey back in the alcove

Editor-CLOSED:
    UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript
        -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/dump_and_fix_key_orb.py"
"""
import json
import unreal

MAP = "/Game/L_Office_v02"
ALCOVE = unreal.Vector(-2225.0, 10423.0, 420.0)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(MAP)

notes = []
ok = False


def lab(a):
    try:
        return a.get_actor_label()
    except Exception:
        return a.get_name()


def prop(a, name, default=None):
    try:
        return a.get_editor_property(name)
    except Exception:
        return default


def setp(a, name, value):
    try:
        a.set_editor_property(name, value)
        return True
    except Exception as e:
        notes.append("could not set %s.%s: %s" % (lab(a), name, e))
        return False


def loc_of(a):
    p = a.get_actor_location()
    return [round(p.x), round(p.y), round(p.z)]


def hide_grant(g):
    for c in g.get_components_by_class(unreal.StaticMeshComponent):
        c.set_visibility(False)
        c.set_hidden_in_game(True)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    for c in g.get_components_by_class(unreal.PointLightComponent):
        c.set_visibility(False)
        c.set_hidden_in_game(True)
    trig = prop(g, "trigger")
    if trig:
        trig.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        try:
            trig.set_editor_property("generate_overlap_events", False)
        except Exception:
            pass


sites = []
grants = []
key_sites = []
attic_grant = None

for a in eas.get_all_level_actors():
    cls = a.get_class().get_name()
    if cls == "BuildSite":
        info = {
            "label": lab(a),
            "cls": cls,
            "loc": loc_of(a),
            "output": str(prop(a, "output")),
            "cost": prop(a, "cost"),
            "consume_on_build": str(prop(a, "consume_on_build")),
            "ghost_as_orb": str(prop(a, "ghost_as_orb")),
            "interact_radius": prop(a, "interact_radius"),
        }
        sites.append(info)
        out = str(prop(a, "output")).lower()
        if "key" in out:
            key_sites.append(a)
    elif cls == "PowerGrant":
        info = {
            "label": lab(a),
            "loc": loc_of(a),
            "grants_power": str(prop(a, "grants_power")),
            "grants_key": str(prop(a, "grants_key")),
            "book_cost": prop(a, "book_cost"),
            "slot_trial": str(prop(a, "slot_trial")),
            "show_beacon": str(prop(a, "show_beacon")),
            "grant_key": str(prop(a, "grant_key")),
        }
        grants.append(info)
        if lab(a) == "PowerGrant_AtticKey":
            attic_grant = a

notes.append("BuildSites: %d (KeyItem: %d)" % (len(sites), len(key_sites)))
notes.append("PowerGrants: %d" % len(grants))

key_enum = None
try:
    key_enum = unreal.BuildOutput.KEY_ITEM
except Exception:
    key_enum = None

for site in key_sites:
    if key_enum is not None:
        setp(site, "output", key_enum)
    setp(site, "consume_on_build", True)
    setp(site, "ghost_as_orb", True)
    setp(site, "interact_radius", 350.0)
    # Authored cost is 2. A previous pass forced 8 and broke C4
    # (13 pickups vs staircase 8 + key 8).
    if prop(site, "cost") == 8:
        setp(site, "cost", 2)
        notes.append("restored %s cost to 2 (book budget)" % lab(site))
    notes.append("armed KeyItem site %s at %s" % (lab(site), loc_of(site)))
    ok = True

if attic_grant:
    # Second sphere at the bookshelf. The alcove BuildSite is what Walt walks to.
    # Keep the grant in the level (Elise/Compile claim keys must not collide) but
    # stand it down so it cannot steal the beat or hang in the stairs.
    hide_grant(attic_grant)
    setp(attic_grant, "grants_power", False)
    setp(attic_grant, "slot_trial", False)
    setp(attic_grant, "show_beacon", False)
    setp(attic_grant, "summon_refusers_on_trial", False)
    for key in ("grants_key", "b_grants_key"):
        if setp(attic_grant, key, False):
            break
    notes.append("hid PowerGrant_AtticKey at %s" % loc_of(attic_grant))
    ok = True

if not key_sites and attic_grant:
    # Recovery: no KeyItem site, so the PowerGrant IS the alcove pickup.
    attic_grant.set_actor_location(ALCOVE, False, False)
    for c in attic_grant.get_components_by_class(unreal.StaticMeshComponent):
        if "beacon" in c.get_name().lower():
            c.set_visibility(False)
            c.set_hidden_in_game(True)
            continue
        c.set_visibility(True)
        c.set_hidden_in_game(False)
    trig = prop(attic_grant, "trigger")
    if trig:
        trig.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
        try:
            trig.set_editor_property("generate_overlap_events", True)
        except Exception:
            pass
    setp(attic_grant, "grants_power", False)
    setp(attic_grant, "slot_trial", False)
    setp(attic_grant, "show_beacon", False)
    setp(attic_grant, "sauce_reward", 0)
    setp(attic_grant, "grant_key", "Attic.Key")
    for key in ("grants_key", "b_grants_key"):
        if setp(attic_grant, key, True):
            break
    setp(attic_grant, "book_cost", 8)
    notes.append("no KeyItem site — PowerGrant_AtticKey restored to alcove")
    ok = True

if ok:
    les.save_current_level()
    notes.append("level saved")

payload = {
    "ok": ok,
    "notes": notes,
    "sites": sites,
    "grants": grants,
}
text = json.dumps(payload, indent=2)
out = unreal.Paths.project_saved_dir() + "dump_and_fix_key_orb.json"
with open(out, "w", encoding="utf-8") as f:
    f.write(text)
unreal.log("[dump_and_fix_key_orb] wrote " + out)
print(text)
