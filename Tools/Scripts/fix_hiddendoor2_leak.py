"""
fix_hiddendoor2_leak.py — attic cathedral door was blocking the bedroom hall.

HiddenDoor2 is the Code-Vision gate to L_Cathedral ("Enter the Carousel of Fates").
It sits in the attic at Z=712 with actor scale Z=2.45. The BlockingBox default
extent Z=110 becomes 269 cm in world space, so the invisible wall hangs down
through the ceiling into the 2nd-floor hallway (player-top ~512). You cannot
walk through that door to the bedroom unless you hold V.

DoorMesh is visual-only. Shrink only the box so collision stays in the attic.
World half-height ~103 cm: box bottom ~609, above the hall, still a solid
attic doorway.

Editor-CLOSED (or reload the map after):
    UnrealEditor-Cmd SibeliusGame.uproject -run=pythonscript
        -script="C:/Users/wpark/projects/sibelius-game/Tools/Scripts/fix_hiddendoor2_leak.py"
"""
import json
import unreal

MAP = "/Game/L_Office_v02"
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


door = None
for a in eas.get_all_level_actors():
    if lab(a) == "HiddenDoor2" and a.get_class().get_name() == "HiddenDoor":
        door = a
        break

if door is None:
    notes.append("HiddenDoor2 not found")
else:
    loc = door.get_actor_location()
    scale = door.get_actor_scale3d()
    box = door.get_editor_property("blocking_box")
    before = None
    if box:
        e = box.get_unscaled_box_extent()
        before = [round(e.x, 1), round(e.y, 1), round(e.z, 1)]
        # 42 * scale.z 2.45 = 102.9 cm half-height. Center 712 -> 609..815.
        box.set_box_extent(unreal.Vector(60.0, 15.0, 42.0), True)
        after = box.get_unscaled_box_extent()
        notes.append("HiddenDoor2 at (%.0f, %.0f, %.0f) scale z=%.2f" % (
            loc.x, loc.y, loc.z, scale.z))
        notes.append("BlockingBox extent %s -> [%.1f, %.1f, %.1f]" % (
            before, after.x, after.y, after.z))
        world_half = 42.0 * scale.z
        notes.append("world box Z %.0f .. %.0f (attic floor ~580, hall player-top ~512)" % (
            loc.z - world_half, loc.z + world_half))
        ok = True
    else:
        notes.append("HiddenDoor2 has no blocking_box")

if ok:
    les.save_current_level()
    notes.append("level saved")

payload = {"ok": ok, "notes": notes}
text = json.dumps(payload, indent=2)
out = unreal.Paths.project_saved_dir() + "fix_hiddendoor2_leak.json"
with open(out, "w", encoding="utf-8") as f:
    f.write(text)
print(text)
unreal.log("[fix_hiddendoor2_leak] wrote " + out)
