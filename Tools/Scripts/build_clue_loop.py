# build_clue_loop.py — SIB-43, the clue loop (see docs/sib-43-clue-loop-notes.md).
#
# TWO-MAP script: detects which map is open and does that map's work.
# ORDER: Build.bat FIRST (spawns AAIClueTerminal / ACathedralDoor classes).
#
#   L_Office_v02 open:
#     - places an AIClueTerminal shell over each oracle computer (label
#       CONTAINS match: Monitor_A1, Laptop_A3, Keyboard_A1 — CL4: the QuadArt
#       actors themselves are never modified), shell sized to the mesh bounds
#       + margin, Clue1Voice=S_ai_intro, Clue2Voice=S_ai_clue2 (if imported;
#       silent ceremony until then, CL7). Idempotent: old terminals cleared.
#     - imports Tools/Audio/ai_clue2.wav as /Game/AIApparition/S_ai_clue2 if
#       the wav exists (same pipeline as the intro).
#
#   L_Cathedral open:
#     - spawns the RETURN door (ACathedralDoor, TargetLevelName=L_Office_v02,
#       PromptText "Return to the office [E]") 350cm behind the PlayerStart,
#       facing it, with the attic door's mesh + scale. Idempotent by label.
#
# RUN NATIVELY (editor Cmd box, dropdown = Cmd):
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/build_clue_loop.py"

import os
import unreal

TAG = "###CLUELOOP###"
ORACLES = ["Monitor_A1", "Laptop_A3", "Keyboard_A1"]
CLUE2_WAV = "C:/Users/wpark/projects/sibelius-game/Tools/Audio/ai_clue2.wav"
SND_DIR = "/Game/AIApparition"
DOOR_MESH = "/Game/UltimateGothicCathedralChurch/Mesh/SM_Door_Cathedral_Huge_00001__6336"  # the attic door's mesh
SHELL_MARGIN = 12.0  # cm of forgiveness around the mesh bounds


def import_clue2(asset_tools, eal):
    snd = "%s/S_ai_clue2" % SND_DIR
    if not os.path.isfile(CLUE2_WAV):
        unreal.log_warning("%s %s not found — clue 2 ceremony will be SILENT until recorded+re-run (CL7)." % (TAG, CLUE2_WAV))
        return None
    if eal.does_asset_exist(snd):
        eal.delete_asset(snd)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", CLUE2_WAV)
    task.set_editor_property("destination_path", SND_DIR)
    task.set_editor_property("destination_name", "S_ai_clue2")
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])
    if eal.does_asset_exist(snd):
        unreal.log("%s S_ai_clue2 imported." % TAG)
        return unreal.load_asset(snd)
    unreal.log_error("%s clue-2 wav import FAILED — check the Output Log." % TAG)
    return None


def do_office(eas, asset_tools, eal):
    clue2 = import_clue2(asset_tools, eal)
    clue1 = unreal.load_asset("%s/S_ai_intro" % SND_DIR) if eal.does_asset_exist("%s/S_ai_intro" % SND_DIR) else None

    cls = unreal.load_class(None, "/Script/SibeliusGame.AIClueTerminal")
    if not cls:
        unreal.log_error("%s AIClueTerminal class not found — Build.bat first." % TAG)
        return

    # idempotent: clear previous terminals
    for a in list(eas.get_all_level_actors()):
        try:
            if a.get_class().get_name() == "AIClueTerminal":
                eas.destroy_actor(a)
        except Exception:
            pass

    actors = eas.get_all_level_actors()
    placed = 0
    for key in ORACLES:
        target = None
        for a in actors:
            if key.lower() in a.get_actor_label().lower():
                target = a
                break
        if not target:
            unreal.log_error("%s no actor label contains '%s' — Outliner-search it and tell Cowork the real label (CL4)." % (TAG, key))
            continue

        origin, extent = target.get_actor_bounds(False)
        term = eas.spawn_actor_from_class(cls, origin, unreal.Rotator(roll=0, pitch=0, yaw=0))
        term.set_actor_label("ClueTerminal_%s" % key)
        shell = term.get_component_by_class(unreal.BoxComponent)
        if shell:
            shell.set_box_extent(unreal.Vector(extent.x + SHELL_MARGIN, extent.y + SHELL_MARGIN, extent.z + SHELL_MARGIN))
        if clue1:
            term.set_editor_property("clue1_voice", clue1)
        if clue2:
            term.set_editor_property("clue2_voice", clue2)
        placed += 1
        unreal.log("%s terminal over %s at (%.0f, %.0f, %.0f), extent (%.0f, %.0f, %.0f)" %
                   (TAG, target.get_actor_label(), origin.x, origin.y, origin.z,
                    extent.x + SHELL_MARGIN, extent.y + SHELL_MARGIN, extent.z + SHELL_MARGIN))

    unreal.log("%s OFFICE done: %d/3 oracle terminals. Ctrl+S." % (TAG, placed))


def do_cathedral(eas):
    cls = unreal.load_class(None, "/Script/SibeliusGame.CathedralDoor")
    if not cls:
        unreal.log_error("%s CathedralDoor class not found — Build.bat first." % TAG)
        return

    # idempotent by label
    for a in list(eas.get_all_level_actors()):
        if a.get_actor_label() == "ReturnDoor_Office":
            eas.destroy_actor(a)

    start = None
    for a in eas.get_all_level_actors():
        if isinstance(a, unreal.PlayerStart):
            start = a
            break
    if not start:
        unreal.log_error("%s no PlayerStart in this level." % TAG)
        return

    fwd = start.get_actor_forward_vector()
    loc = start.get_actor_location() - fwd * 350.0
    loc.z = start.get_actor_location().z
    yaw = start.get_actor_rotation().yaw  # face the same way the player spawns; flip by hand if it shows its back

    door = eas.spawn_actor_from_class(cls, loc, unreal.Rotator(roll=0, pitch=0, yaw=yaw))
    door.set_actor_label("ReturnDoor_Office")
    door.set_editor_property("target_level_name", "L_Office_v02")
    door.set_editor_property("prompt_text", unreal.Text("Return to the office [E]"))

    mesh_comp = None
    for c in door.get_components_by_class(unreal.StaticMeshComponent):
        mesh_comp = c
        break
    mesh = unreal.load_asset(DOOR_MESH)
    if mesh_comp and mesh:
        mesh_comp.set_static_mesh(mesh)
        mesh_comp.set_relative_scale3d(unreal.Vector(0.3, 0.4, 0.6))  # the attic door's look
        unreal.log("%s return door mesh set." % TAG)
    else:
        unreal.log_warning("%s door mesh %s not found — assign DoorMesh by hand (CL8); the E-logic works regardless." % (TAG, DOOR_MESH))

    unreal.log("%s CATHEDRAL done: ReturnDoor_Office at (%.0f, %.0f, %.0f) -> L_Office_v02. Ctrl+S." % (TAG, loc.x, loc.y, loc.z))


def main():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    name = world.get_name() if world else ""
    if "Office" in name:
        do_office(eas, asset_tools, eal)
    elif "Cathedral" in name:
        do_cathedral(eas)
    else:
        unreal.log_error("%s open L_Office_v02 or L_Cathedral first (current: %s)." % (TAG, name))


main()
