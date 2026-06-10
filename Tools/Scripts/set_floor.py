"""
set_floor.py — give the cathedral floor a glossy reflective surface so it mirrors the
stained glass softly (polished marble, not a literal mirror). Lumen handles the
reflections. Run natively:  py "C:/Users/wpark/Claude/set_floor.py"   then Ctrl+S.
Edit FLOOR_MAT to try the glass-floor variant instead.
"""
import unreal

FLOOR_MAT = "/Game/StainedGlass3D/Materials/M_BlackMarbleFloor"   # or .../M_StainedGlassFloor

mat = unreal.load_asset(FLOOR_MAT)
try:
    AS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = AS.get_all_level_actors()
except Exception:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()

done = 0
for a in actors:
    try:
        if a.get_actor_label() == "Floor":
            c = a.get_component_by_class(unreal.StaticMeshComponent)
            if c and mat:
                for i in range(max(1, c.get_num_materials())):
                    c.set_material(i, mat)
                done += 1
    except Exception as e:
        unreal.log_error("[Floor] %s" % e)

msg = "[Glass] floor material '%s' set on %d actor(s)" % (FLOOR_MAT.split('/')[-1], done)
unreal.log(msg); print(msg)
if not mat:
    unreal.log_error("[Floor] material not found: %s" % FLOOR_MAT)
