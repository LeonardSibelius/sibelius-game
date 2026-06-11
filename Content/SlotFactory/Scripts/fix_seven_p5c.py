# fix_seven_p5c.py — un-mirror the seven + rebake its sequence
#
# The P5B render showed the BACK of the glyph (mirrored 7): yaw=-90 faces the
# text away from the render camera. Fix = yaw +90. The actor rotation alone is
# NOT enough — SEQ_seven's transform track keys the old yaw at frame 0 and
# overrides the actor during MRQ. So: rotate the actor, then recreate the
# sequence from the corrected transform.
#
# RUN AFTER the by-hand material fix (M_seven on all four Text3D slots).
#   py "C:/Users/wpark/projects/sibelius-game/Content/SlotFactory/Scripts/fix_seven_p5c.py"

import unreal

ROOT = "/Game/SlotFactory"
TAG = "###P5C###"
FPS = 30
DURATION = 120


def main():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    eal = unreal.EditorAssetLibrary

    actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
    seven = actors.get("seven_glyph")
    cam = actors.get("CAM_seven")
    if not seven or not cam:
        unreal.log_error("%s seven_glyph or CAM_seven not found" % TAG)
        return

    # face the glyph toward the camera (front, not back)
    seven.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0), False)
    unreal.log("%s seven yaw -> 90 (front faces camera)" % TAG)

    # recentre after the flip (bounds shift slightly with extrusion direction)
    origin, extent = seven.get_actor_bounds(False)
    tgt = unreal.Vector(7200.0, 0.0, 140.0)
    seven.add_actor_world_offset(unreal.Vector(tgt.x - origin.x, tgt.y - origin.y, tgt.z - origin.z), False, False)
    unreal.log("%s seven recentred" % TAG)

    # recreate SEQ_seven from the corrected transform
    full = "%s/SEQ_seven" % ROOT
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    seq = asset_tools.create_asset("SEQ_seven", ROOT, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(unreal.FrameRate(FPS, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(DURATION)

    cam_binding = seq.add_possessable(cam)
    track = seq.add_track(unreal.MovieSceneCameraCutTrack) if hasattr(seq, "add_track") else seq.add_master_track(unreal.MovieSceneCameraCutTrack)
    cut = track.add_section()
    cut.set_start_frame_seconds(0.0)
    cut.set_end_frame_seconds(DURATION / float(FPS))
    bid = unreal.MovieSceneObjectBindingID()
    bid.set_editor_property("guid", cam_binding.get_id())
    cut.set_camera_binding_id(bid)

    b = seq.add_possessable(seven)
    ts = b.add_track(unreal.MovieScene3DTransformTrack).add_section()
    ts.set_start_frame_seconds(0.0)
    ts.set_end_frame_seconds(DURATION / float(FPS))
    loc, rot, scl = seven.get_actor_location(), seven.get_actor_rotation(), seven.get_actor_scale3d()
    ch = ts.get_all_channels()
    f0, fN = unreal.FrameNumber(0), unreal.FrameNumber(DURATION)
    for i, v in enumerate([loc.x, loc.y, loc.z, rot.roll, rot.pitch, rot.yaw, scl.x, scl.y, scl.z]):
        ch[i].add_key(f0, float(v))
    ch[5].add_key(fN, float(rot.yaw) + 360.0)
    eal.save_asset(full)
    unreal.log("%s recreated SEQ_seven" % TAG)

    try:
        les.save_current_level()
    except Exception:
        pass
    unreal.log("%s DONE — MRQ: delete all jobs, add SEQ_seven only, SymbolStill, render." % TAG)


main()
