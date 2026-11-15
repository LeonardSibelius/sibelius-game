# probe_wrestling_demo.py - where the pack's own demo level stands each pair. Read-only, headless.
#
#   UnrealEditor-Cmd.exe SibeliusGame.uproject -run=pythonscript -script=".../probe_wrestling_demo.py"
#
# The paired moves are authored with each wrestler at her own origin; the demo level places the two
# actors so the hands meet. Those placements are the numbers the match needs. Writes
# Saved/probe_wrestling_demo.json: every actor with an animation, its location, yaw and the anim.

import json
import traceback

import unreal

MAP = "/Game/Wrestler_Finishers_Volume2/Demo/Maps/Wrestler_Finishers_Volume2"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/probe_wrestling_demo.json"

r = {"actors": []}
try:
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    for a in actors:
        for comp in a.get_components_by_class(unreal.SkeletalMeshComponent):
            anim = None
            try:
                data = comp.get_editor_property("animation_data")
                anim = data.get_editor_property("anim_to_play")
            except Exception:
                pass
            t = comp.get_world_transform()
            r["actors"].append({
                "actor": a.get_actor_label() if hasattr(a, "get_actor_label") else a.get_name(),
                "anim": anim.get_name() if anim else None,
                "loc": [round(t.translation.x, 1), round(t.translation.y, 1), round(t.translation.z, 1)],
                "yaw": round(t.rotation.rotator().yaw, 1),
            })
        for txt in a.get_components_by_class(unreal.TextRenderComponent):
            t = txt.get_world_transform()
            r["actors"].append({"text": str(txt.get_editor_property("text")),
                                "loc": [round(t.translation.x, 1), round(t.translation.y, 1), round(t.translation.z, 1)]})
except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=1)
unreal.log("###PROBE DEMO### %d" % len(r["actors"]))
