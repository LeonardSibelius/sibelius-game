# forest_pcg_generate.py — place a PCG volume in L_Elsewhere_Forest running PCG_ForestScatter,
# seed it, set generate-on-load, generate now, save. (Run AFTER build_pcg_forest_graph.py.)
import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
graph = unreal.load_asset("/Game/PCG/PCG_ForestScatter")

for a in list(eas.get_all_level_actors()):
    if a.get_actor_label() == "ForestPCG":
        eas.destroy_actor(a)

vol = eas.spawn_actor_from_class(unreal.PCGVolume, unreal.Vector(0.0, 0.0, 0.0))
vol.set_actor_label("ForestPCG")
vol.set_actor_scale3d(unreal.Vector(300.0, 300.0, 40.0))   # generous backup; unbounded sampler makes size moot
pcg = vol.get_component_by_class(unreal.PCGComponent)

try:
    pcg.set_graph(graph)
except Exception:
    pcg.set_editor_property("graph_instance", graph)
pcg.set_editor_property("seed", 1337)
try:
    pcg.set_editor_property("generation_trigger", unreal.PCGComponentGenerationTrigger.GENERATE_ON_LOAD)
except Exception as e:
    unreal.log("###FORESTGEN### gen-trigger skip %r" % e)

res = None
for fn in ("generate", "generate_local"):
    try:
        res = getattr(pcg, fn)(True)
        unreal.log("###FORESTGEN### %s(True) -> %s" % (fn, res))
        break
    except Exception as e:
        unreal.log("###FORESTGEN### %s err %r" % (fn, e))

les.save_current_level()
unreal.log("###FORESTGEN### placed ForestPCG + generate requested + saved")
