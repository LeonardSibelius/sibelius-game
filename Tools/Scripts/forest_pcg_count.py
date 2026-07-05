import unreal, json
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
counts, total = {}, 0
for a in eas.get_all_level_actors():
    for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
        m = c.get_editor_property("static_mesh")
        name = m.get_name() if m else "None"
        n = c.get_instance_count()
        counts[name] = counts.get(name, 0) + n
        total += n
open(r"C:/Users/wpark/projects/sibelius-game/forest-gen-count.json", "w").write(json.dumps({"total": total, "by_mesh": counts}, indent=1))
unreal.log("###FORESTCNT### total=%d distinct_meshes=%d" % (total, len(counts)))
