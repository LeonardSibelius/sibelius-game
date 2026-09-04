# render_frame_sheet.py — a contact sheet of Kaia, to choose a capsule frame from.
#
# *** RUN FROM THE OPEN EDITOR ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/render_frame_sheet.py"
#
# One PIE window. Output: Saved/MovieRenders/FrameSheet/
#
# ===========================================================================
# WHY.  Picking the hero frame means looking at her face at a lot of different times,
# and the Sequencer fills the window so the viewport is hidden behind it. Rather than
# rearranging panels, render the candidates small and look at them all at once.
#
# The speech runs almost the whole sequence (~0 to 780 at 30 fps), so most frames catch
# her mid-word with her mouth open. The frames worth the most attention are the ones
# after the audio stops.
#
# ONE JOB, NOT ONE PER FRAME. A frame STEP inside a single job means one PIE session and
# one warm-up for the whole sheet. Rendering fourteen separate jobs would mean fourteen
# warm-ups — several minutes of waiting to look at some thumbnails.
#
# WARM-UP IS STILL REQUIRED. Without it the first capture is a black silhouette on
# transparency: the sky has not streamed and the materials have not compiled. That cost a
# whole round trip on 2026-09-03 and is the single most important thing in this file.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/frame_sheet_report.json"
LEVEL = "/Game/Cinematics/L_Cine_KaiaIntro"
SEQ = "/Game/Cinematics/LS_Kaia_Intro"
DEST = "C:/Users/wpark/projects/sibelius-game/Saved/MovieRenders/FrameSheet/"

# Small and portrait-ish: big enough to judge a face, small enough to view many at once.
WIDTH, HEIGHT = 400, 500

# The whole sequence, every STEP frames. 30 fps, so 40 is about 1.3 seconds apart.
START_FRAME, END_FRAME, STEP = 0, 865, 40

r = {"range": [START_FRAME, END_FRAME], "step": STEP, "size": [WIDTH, HEIGHT]}


def add(config, candidates, label):
    for n in candidates:
        cls = getattr(unreal, n, None)
        if cls is None:
            continue
        try:
            s = config.find_or_add_setting_by_class(cls)
            r.setdefault("settings", {})[label] = n
            return s
        except Exception as e:
            r.setdefault("setting_errors", {})[n] = str(e)
    r.setdefault("settings", {})[label] = "MISSING: tried %s" % candidates
    return None


def main():
    subsys = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
    queue = subsys.get_queue()
    for j in list(queue.get_jobs()):
        queue.delete_job(j)

    job = queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
    job.job_name = "kaia_frame_sheet"
    job.map = unreal.SoftObjectPath(LEVEL)
    job.sequence = unreal.SoftObjectPath(SEQ)

    cfg = job.get_configuration()
    add(cfg, ["MoviePipelineDeferredPassBase"], "render_pass")
    add(cfg, ["MoviePipelineImageSequenceOutput_JPG",
              "MoviePipelineImageSequenceOutput_PNG"], "image_output")

    aa = add(cfg, ["MoviePipelineAntiAliasingSetting"], "anti_aliasing")
    if aa:
        for prop, val in (("engine_warm_up_count", 64),
                          ("render_warm_up_count", 64),
                          ("use_camera_cut_for_warm_up", False)):
            try:
                aa.set_editor_property(prop, val)
            except Exception as e:
                r.setdefault("warmup_errors", {})[prop] = str(e)

    go = add(cfg, ["MoviePipelineGameOverrideSetting"], "game_override")
    if go:
        try:
            go.set_editor_property(
                "texture_streaming",
                unreal.MoviePipelineTextureStreamingMethod.FULLY_LOAD)
        except Exception as e:
            r.setdefault("override_errors", {})["texture_streaming"] = str(e)

    out = add(cfg, ["MoviePipelineOutputSetting"], "output_setting")
    if out:
        d = unreal.DirectoryPath()
        d.set_editor_property("path", DEST)
        out.set_editor_property("output_directory", d)
        # The frame number IS the answer we are looking for, so it has to be in the name.
        out.set_editor_property("file_name_format", "frame_{frame_number}")
        out.set_editor_property("output_resolution", unreal.IntPoint(WIDTH, HEIGHT))
        out.set_editor_property("override_existing_output", True)
        try:
            out.set_editor_property("use_custom_playback_range", True)
            out.set_editor_property("custom_start_frame", START_FRAME)
            out.set_editor_property("custom_end_frame", END_FRAME)
        except Exception as e:
            r.setdefault("range_errors", {})["playback_range"] = str(e)
        try:
            out.set_editor_property("output_frame_step", STEP)
            r["stepped"] = True
        except Exception as e:
            # Not fatal - it just renders every frame instead of every STEP-th, which is
            # 865 thumbnails rather than 22. Say so rather than quietly filling a folder.
            r["stepped"] = "FAILED (%s) - expect one image PER FRAME" % e

    r["output_dir"] = DEST
    with open(OUT, "w") as fh:
        json.dump(r, fh, indent=2)

    subsys.render_queue_with_executor(unreal.MoviePipelinePIEExecutor)
    unreal.log("###SHEET### started, watch the PIE window")


try:
    main()
except Exception:
    r["traceback"] = traceback.format_exc()
    with open(OUT, "w") as fh:
        json.dump(r, fh, indent=2)

unreal.log("###SHEET### %s" % json.dumps(r))
