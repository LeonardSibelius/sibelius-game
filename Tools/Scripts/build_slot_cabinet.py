"""
build_slot_cabinet.py — clear the apse altar and stand a PLACEHOLDER slot-machine
cabinet at the end of the nave: the altar of the Church of AI, where Leonard's slot
will live. Primitive blockout (base + body + screen panel) with collision, so you can
walk up to it. Re-runnable (clears the altar + any prior cabinet).

Run NATIVELY (not the bridge), L_Cathedral open, in the editor's bottom Cmd box:
    py "C:/Users/wpark/Claude/build_slot_cabinet.py"     then Ctrl+S.
"""
import unreal

CUBE     = "/Engine/BasicShapes/Cube"
TAG      = "SlotCabinet"
APSE_X   = 3400.0     # end of the nave, where the altar was
APSE_Y   = 0.0
FACE_YAW = 180.0      # cabinet faces back down the nave toward the entrance

def _as():
    try:
        return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    except Exception:
        return None
_AS = _as()

def all_actors():
    return _AS.get_all_level_actors() if _AS else unreal.EditorLevelLibrary.get_all_level_actors()

def spawn(obj, loc, rot):
    fn = _AS.spawn_actor_from_object if _AS else unreal.EditorLevelLibrary.spawn_actor_from_object
    return fn(obj, loc, rot)

def destroy(a):
    (_AS.destroy_actor if _AS else unreal.EditorLevelLibrary.destroy_actor)(a)

# clear the altar + any prior cabinet (idempotent re-run)
removed = 0
for a in list(all_actors()):
    try:
        if a.get_actor_label() == "Altar_Main" or (unreal.Name(TAG) in a.tags):
            destroy(a); removed += 1
    except Exception:
        pass

cube = unreal.load_asset(CUBE)

def block(x, y, z, sx, sy, sz, label):
    a = spawn(cube, unreal.Vector(x, y, z), unreal.Rotator(pitch=0.0, yaw=FACE_YAW, roll=0.0))
    if a:
        a.set_actor_scale3d(unreal.Vector(sx, sy, sz))
        try:
            a.tags = [unreal.Name(TAG)]
        except Exception:
            pass
        a.set_actor_label(label)
        c = a.get_component_by_class(unreal.StaticMeshComponent)
        if c:
            c.set_collision_profile_name("BlockAll")
    return a

# base/plinth: ~90 x 140 x 30 cm, bottom on the floor
block(APSE_X,        APSE_Y, 15.0,  0.9, 1.4, 0.3, "SlotCab_Base")
# body: ~70 x 120 x 170 cm, sitting on the plinth
block(APSE_X,        APSE_Y, 115.0, 0.7, 1.2, 1.7, "SlotCab_Body")
# screen panel: thin, upper-front (-X face, toward the approaching player)
block(APSE_X - 36.0, APSE_Y, 150.0, 0.05, 0.9, 0.8, "SlotCab_Screen")

msg = "[Cathedral] slot cabinet placeholder: removed %d, built 3 (base/body/screen)" % removed
unreal.log(msg); print(msg)
