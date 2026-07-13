# place_fun_plan_office.py — FUN_PLAN placement for L_Office_v02 (docs/FUN_PLAN_SETUP.md §1-§3).
#
# Idempotent: finds each actor by label and creates it only if missing, so re-running
# is safe. Placeholder engine meshes (sphere/cylinder/cube) make everything visible
# immediately; Walt swaps real meshes + nudges positions by eye afterwards.
#
# Run via the bridge with the editor open on L_Office_v02:
#   Tools\ue_bridge\ue_bridge.cmd exec-python "exec(open(r'Tools/Scripts/place_fun_plan_office.py').read())"

import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
placed = []


def find(label):
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == label:
            return a
    return None


def ensure(cls_path, label, loc, yaw=0.0):
    a = find(label)
    if a:
        placed.append(label + " (already present)")
        return a
    cls = unreal.load_class(None, cls_path)
    a = eas.spawn_actor_from_class(cls, unreal.Vector(loc[0], loc[1], loc[2]),
                                   unreal.Rotator(0.0, 0.0, yaw))
    a.set_actor_label(label)
    placed.append(label + " (spawned)")
    return a


def set_mesh(actor, comp_name, mesh_path, scale):
    mesh = unreal.load_asset(mesh_path)
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        if c.get_name() == comp_name:
            c.set_editor_property("StaticMesh", mesh)
            c.set_editor_property("RelativeScale3D", unreal.Vector(scale[0], scale[1], scale[2]))
            return True
    return False


SPHERE = "/Engine/BasicShapes/Sphere.Sphere"
CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"
CUBE = "/Engine/BasicShapes/Cube.Cube"

# --- §1 The five power shrines, each at its chapter's landmark ---------------
# Floor on the main level is ~z320 (BuildSite); the shrine root is its trigger
# sphere, so ~z420 puts the orb at chest height.
SHRINES = [
    ("PowerGrant_Refactor",  "Refactor",   (-1765.0, 10080.0, 420.0)),  # by the Ch1 hidden door
    ("PowerGrant_Compile",   "Compile",    (-2225.0, 10430.0, 420.0)),  # library, before the books
    ("PowerGrant_TestDrive", "TestDrive",  (-1900.0,  9880.0, 420.0)),  # by the build site
    ("PowerGrant_Deploy",    "Deploy",     (-1950.0,  9680.0, 420.0)),  # below the attic hatch
    ("PowerGrant_Generate",  "Generate",   (-1833.0, 10740.0, 420.0)),  # by the clue terminals
]

VERB = {
    "Refactor": unreal.PowerVerb.REFACTOR,
    "Compile": unreal.PowerVerb.COMPILE,
    "TestDrive": unreal.PowerVerb.TEST_DRIVE,
    "Deploy": unreal.PowerVerb.DEPLOY,
    "Generate": unreal.PowerVerb.GENERATE,
}

for label, verb, loc in SHRINES:
    shrine = ensure("/Script/SibeliusGame.PowerGrant", label, loc)
    shrine.set_editor_property("Power", VERB[verb])
    set_mesh(shrine, "Mesh", SPHERE, (0.45, 0.45, 0.45))

# --- §2 The Sauce Cauldron, in the kitchen near the Many Worlds door ---------
cauldron = ensure("/Script/SibeliusGame.SauceCauldron", "SauceCauldron_Kitchen",
                  (-1620.0, 9300.0, 170.0))
set_mesh(cauldron, "CauldronMesh", CYLINDER, (0.9, 0.9, 0.55))
set_mesh(cauldron, "ContentsMesh", CYLINDER, (0.8, 0.8, 0.05))
for c in cauldron.get_components_by_class(unreal.StaticMeshComponent):
    if c.get_name() == "ContentsMesh":
        c.set_editor_property("RelativeLocation", unreal.Vector(0.0, 0.0, 52.0))

# --- §3 The Carousel door, near the kitchen -----------------------------------
door = ensure("/Script/SibeliusGame.CathedralDoor", "CarouselDoor_Kitchen",
              (-1850.0, 9240.0, 170.0))
door.set_editor_property("TargetLevelName", "L_Carousel_Test")
door.set_editor_property("PromptText", unreal.Text("Ride the Carousel of Fates [E]"))
set_mesh(door, "DoorMesh", CUBE, (0.15, 1.1, 2.1))

print("PLACED: " + "; ".join(placed))
