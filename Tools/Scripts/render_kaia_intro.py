# render_kaia_intro.py — LS_Kaia_Intro -> Saved/MovieRenders/kaia_intro.mp4
#
# *** RUN FROM THE OPEN EDITOR ***
#   Cmd box (mode dropdown -> Cmd):
#     py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/render_kaia_intro.py"
#
# A PIE window will open and play the shot through. That IS the render - let it finish.
#
# WHY SCRIPTED. Movie Render Queue needs four settings added by hand through nested
# menus (a render pass, an image output, a .wav output, and the encoder) before it will
# produce a single MP4. Scripting it means the recipe is written down and repeatable
# instead of remembered.
#
# MP4 NEEDS FFMPEG. UE writes image sequences, ProRes and DNxHR natively - never H.264.
# The Command Line Encoder hands frames to an external binary, configured in
# DefaultEngine.ini under MoviePipelineCommandLineEncoderSettings. That path is
# absolute and machine-specific; a fresh clone must re-point it.

import unreal, json, traceback

OUT = "C:/Users/wpark/projects/sibelius-game/Saved/kaia_render_report.json"
LEVEL = "/Game/Cinematics/L_Cine_KaiaIntro"
SEQ = "/Game/Cinematics/LS_Kaia_Intro"
DEST = "C:/Users/wpark/projects/sibelius-game/Saved/MovieRenders/"
NAME = "kaia_intro"
WIDTH, HEIGHT = 1920, 1080

r = {}


def add(config, candidates, label):
    """Add the first setting class that exists under any of these names."""
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

    # Clear anything left from a previous run so we never render a stale job.
    for j in list(queue.get_jobs()):
        queue.delete_job(j)

    job = queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
    job.job_name = NAME
    job.map = unreal.SoftObjectPath(LEVEL)
    job.sequence = unreal.SoftObjectPath(SEQ)
    r["job"] = NAME

    cfg = job.get_configuration()

    add(cfg, ["MoviePipelineDeferredPassBase"], "render_pass")
    add(cfg, ["MoviePipelineImageSequenceOutput_PNG",
              "MoviePipelineImageSequenceOutput_JPG"], "image_output")
    add(cfg, ["MoviePipelineWaveOutput"], "audio_output")
    add(cfg, ["MoviePipelineCommandLineEncoder"], "encoder")

    out = add(cfg, ["MoviePipelineOutputSetting"], "output_setting")
    if out:
        d = unreal.DirectoryPath()
        d.set_editor_property("path", DEST)
        out.set_editor_property("output_directory", d)
        out.set_editor_property("file_name_format", NAME + ".{frame_number}")
        out.set_editor_property("output_resolution", unreal.IntPoint(WIDTH, HEIGHT))
        out.set_editor_property("override_existing_output", True)
        r["output_dir"] = DEST

    r["ready"] = True
    with open(OUT, "w") as fh:
        json.dump(r, fh, indent=2)

    # Renders in a PIE window. Asynchronous - this call returns immediately.
    subsys.render_queue_with_executor(unreal.MoviePipelinePIEExecutor)
    unreal.log("###RENDER### started, watch the PIE window")


try:
    main()
except Exception:
    r["traceback"] = traceback.format_exc()
    with open(OUT, "w") as fh:
        json.dump(r, fh, indent=2)

unreal.log("###RENDER### %s" % json.dumps(r))
