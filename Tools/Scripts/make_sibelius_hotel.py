"""Run in the editor (not PIE). Rebuilds the hotel from the purchased ElvenArchitecture scene.
The vendor assets and generated map are deliberately excluded from the public repository.
"""
import unreal

SOURCE = '/Game/ElvenArchitecture/Scenes/Scene'
DEST = '/Game/Maps/L_SibeliusHotel'
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
    raise RuntimeError('Stop PIE before preparing the hotel.')
if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
    raise RuntimeError('Save your edited map before preparing the hotel.')
if not unreal.EditorAssetLibrary.does_asset_exist(DEST):
    editor.load_level(SOURCE)
    if not unreal.EditorLoadingAndSavingUtils.save_map(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), DEST):
        raise RuntimeError('Could not save a hotel copy of the purchased scene.')
editor.load_level(DEST)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
all_actors = actors.get_all_level_actors()
start = next(a for a in all_actors if isinstance(a, unreal.PlayerStart))
# Inner entrance walkway: trace BELOW the decorative arch to find the walking surface.
hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(1900,500,700),
    unreal.Vector(1900,500,-2000), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
    False, [start], unreal.DrawDebugTrace.NONE, True)
if not hit:
    raise RuntimeError('Hotel entrance floor was not found.')
floor = hit.to_tuple()[4]
start.set_actor_location(unreal.Vector(1900,500,floor.z+100), False, False)
start.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0), False)
for actor in all_actors:
    if isinstance(actor, unreal.LevelSequenceActor):
        settings = actor.get_editor_property('playback_settings')
        settings.set_editor_property('auto_play', False)
        actor.set_editor_property('playback_settings', settings)
sign = next((a for a in all_actors if a.get_actor_label() == 'SibeliusHotelSign'), None)
if not sign:
    sign = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(2110,900,490),
        unreal.Rotator(pitch=0,yaw=180,roll=0))
sign.set_actor_label('SibeliusHotelSign')
text = sign.get_component_by_class(unreal.TextRenderComponent)
text.set_text('SIBELIUS HOTEL')
text.set_world_size(32)
text.set_text_render_color(unreal.Color(255,220,150,255))
text.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
editor.save_current_level()
unreal.log('SIBELIUS_HOTEL_OK floor=' + str(floor) + ' map=' + DEST)
