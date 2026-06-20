# forest_flatten.py — undo the broken scripted heightmap. Import a UNIFORM render target
# (no variation -> no 8-bit terracing -> dead flat) to overwrite the jagged heightmap.
# Phase 1 ships flat; Walt hand-brushes gentle hills later with the Sculpt tool.
import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

land = None
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.Landscape):
        land = a
        break

rt = unreal.RenderingLibrary.create_render_target2d(world, 512, 512, unreal.TextureRenderTargetFormat.RTF_RGBA8)
# R=0.5 (high byte ~128), G=0 -> uniform height16 ~32768 = the landscape's original flat plane.
unreal.RenderingLibrary.clear_render_target2d(world, rt, unreal.LinearColor(0.5, 0.0, 0.0, 1.0))
ok = land.landscape_import_heightmap_from_render_target(rt)

for a in eas.get_all_level_actors():
    if a.get_actor_label() == "PlayerStart":
        a.set_actor_location(unreal.Vector(0.0, 0.0, 200.0), False, False)

les.save_current_level()
unreal.log("###FLATTEN### uniform import=%s -> landscape flat, PlayerStart=200, saved" % ok)
