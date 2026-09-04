# render_capsules.py — Kaia, rendered natively at all five Steam capsule sizes.
#
# *** RUN FROM THE OPEN EDITOR ***
#   Cmd box:
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/render_capsules.py"
#
# Five PIE windows will open in turn, one per size. That IS the render — let them finish.
# Output lands in Saved/MovieRenders/Capsules/.
#
# ===========================================================================
# WHY NOT JUST CROP A SCREENSHOT.
#
# That was tried first, on 2026-09-03, from Kaia1.jpg (2541x1305). The header came out
# well. The two PORTRAIT capsules did not: cropping a 1.95:1 close-up down to 600x900
# throws away two thirds of the width and cuts the top of her head, which reads as an
# accident rather than a choice. And nothing cropped from that image leaves anywhere to
# put the title, which every capsule needs.
#
# docs/STEAM_PLAN.md already had the better answer: "Kaia's face renders at any
# resolution wanted, lit properly, against black - which is a stronger capsule than most
# indie games manage."
#
# THE USEFUL PART OF RENDERING RATHER THAN CROPPING. A UE camera holds its HORIZONTAL
# field of view, so a taller output does not zoom in - it reveals MORE VERTICALLY. The
# 600x900 render therefore arrives with headroom above her that no crop of a landscape
# frame could ever produce, and that headroom is where the title goes.
#
# Check it anyway: a CineCameraActor with a constrained aspect ratio can behave
# differently, and the whole point of this script is to look at the result.
#
# ---------------------------------------------------------------------------
# ONE FRAME, NOT A MOVIE. render_kaia_intro.py makes an MP4 and needs ffmpeg, a wav
# output and the command-line encoder. A capsule is a still, so this drops all three and
# writes PNGs — nothing external required.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/capsule_render_report.json"
LEVEL = "/Game/Cinematics/L_Cine_KaiaIntro"
SEQ = "/Game/Cinematics/LS_Kaia_Intro"
DEST = "C:/Users/wpark/projects/sibelius-game/Saved/MovieRenders/Capsules/"

# WHICH FRAME OF THE SHOT.
#
# 760, chosen off the contact sheet from render_frame_sheet.py (Walt, 2026-09-03).
# It is near the END on purpose: the kaia_intro audio runs from about frame 0 to 780, so
# almost every earlier frame catches her mid-word with her mouth open. 760 is the tail,
# where her face settles - mouth closed, eyes open, composed, against black.
#
# Frame 60 was the first guess and it was a talking frame. Do not go back to it.
HERO_FRAME = 760

# Valve's sizes, checked 2026-08-26 (docs/STEAM_PLAN.md section 5). Valve raised most of
# these in August 2024; the older dimensions are rejected for new submissions.
CAPSULES = [
    ("header",   920,  430),   # the only one strictly required
    ("small",    462,  174),   # auto-shrinks to 120x45 - the title must survive that
    ("main",     1232, 706),
    ("vertical", 748,  896),   # portrait - this is why we render instead of crop
    ("library",  600,  900),   # portrait
]

r = {"frame": HERO_FRAME, "jobs": []}


def add(config, candidates, label, sink):
    for n in candidates:
        cls = getattr(unreal, n, None)
        if cls is None:
            continue
        try:
            s = config.find_or_add_setting_by_class(cls)
            sink[label] = n
            return s
        except Exception as e:
            sink.setdefault("errors", {})[n] = str(e)
    sink[label] = "MISSING: tried %s" % candidates
    return None


def main():
    subsys = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
    queue = subsys.get_queue()

    # Clear anything left from a previous run so we never render a stale job.
    for j in list(queue.get_jobs()):
        queue.delete_job(j)

    for name, w, h in CAPSULES:
        info = {"name": name, "size": [w, h]}

        job = queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
        job.job_name = "capsule_%s" % name
        job.map = unreal.SoftObjectPath(LEVEL)
        job.sequence = unreal.SoftObjectPath(SEQ)

        cfg = job.get_configuration()
        add(cfg, ["MoviePipelineDeferredPassBase"], "render_pass", info)
        # PNG, and NO wav output and NO encoder: a capsule is a still, so none of the
        # ffmpeg machinery render_kaia_intro.py needs applies here.
        add(cfg, ["MoviePipelineImageSequenceOutput_PNG"], "image_output", info)

        # WARM-UP - the whole reason the first attempt failed.
        #
        # Frame 60 rendered as a BLACK SILHOUETTE ON WHITE: the sky had not streamed in
        # and Kaia's materials had not compiled, so the capture got geometry and nothing
        # else. render_kaia_intro.py never showed this because it renders the WHOLE
        # sequence - its first frames are junk too, and nobody looks at them.
        #
        # A single-frame render has nothing BUT the first frame, so the engine has to be
        # told to run for a while before the shutter opens.
        aa = add(cfg, ["MoviePipelineAntiAliasingSetting"], "anti_aliasing", info)
        if aa:
            # SPATIAL SAMPLES: a still can afford quality that 26 seconds of video
            # cannot. 8 samples per pixel cleans up the aliasing along her hair, which is
            # the first thing that looks cheap when a capsule is seen at full size.
            for prop, val in (("engine_warm_up_count", 64),
                              ("render_warm_up_count", 64),
                              ("use_camera_cut_for_warm_up", False),
                              ("override_anti_aliasing", True),
                              ("spatial_sample_count", 8),
                              ("temporal_sample_count", 1)):
                try:
                    aa.set_editor_property(prop, val)
                except Exception as e:
                    info.setdefault("warmup_errors", {})[prop] = str(e)

        go = add(cfg, ["MoviePipelineGameOverrideSetting"], "game_override", info)
        if go:
            for prop, val in (("flush_grass_streaming", True),
                              ("flush_streaming_managers", True),
                              ("use_high_quality_shadows", True)):
                try:
                    go.set_editor_property(prop, val)
                except Exception as e:
                    info.setdefault("override_errors", {})[prop] = str(e)
            # Textures FULLY LOADED, not streamed in as the camera decides it needs them.
            try:
                go.set_editor_property(
                    "texture_streaming",
                    unreal.MoviePipelineTextureStreamingMethod.FULLY_LOAD)
            except Exception as e:
                info.setdefault("override_errors", {})["texture_streaming"] = str(e)

        out = add(cfg, ["MoviePipelineOutputSetting"], "output_setting", info)
        if out:
            d = unreal.DirectoryPath()
            d.set_editor_property("path", DEST)
            out.set_editor_property("output_directory", d)
            # The size is in the filename, so five PNGs in one folder cannot be confused
            # for one another later - which matters when four of them look alike.
            out.set_editor_property("file_name_format",
                                    "capsule_%s_%dx%d" % (name, w, h))
            out.set_editor_property("output_resolution", unreal.IntPoint(w, h))
            out.set_editor_property("override_existing_output", True)

            # ONE FRAME. Without this it renders the whole sequence at every size, which
            # is five movies' worth of PNGs to find five images in.
            try:
                out.set_editor_property("use_custom_playback_range", True)
                out.set_editor_property("custom_start_frame", HERO_FRAME)
                out.set_editor_property("custom_end_frame", HERO_FRAME + 1)
                info["single_frame"] = True
            except Exception as e:
                # Not fatal: a full-sequence render still produces usable frames, it just
                # produces a great many of them. Say so rather than failing silently.
                info["single_frame"] = "FAILED (%s) - will render the whole sequence" % e

        r["jobs"].append(info)

    r["output_dir"] = DEST
    r["ready"] = True
    with open(OUT, "w") as fh:
        json.dump(r, fh, indent=2)

    # Asynchronous: returns immediately, then works through all five jobs in turn.
    subsys.render_queue_with_executor(unreal.MoviePipelinePIEExecutor)
    unreal.log("###CAPSULES### started %d job(s), watch the PIE windows" % len(CAPSULES))


try:
    main()
except Exception:
    r["traceback"] = traceback.format_exc()
    with open(OUT, "w") as fh:
        json.dump(r, fh, indent=2)

unreal.log("###CAPSULES### %s" % json.dumps(r))
