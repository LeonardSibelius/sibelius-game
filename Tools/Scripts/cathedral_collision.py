"""
cathedral_collision.py — make the cathedral solid so the player walks BETWEEN the
columns and can't sink into the altar.

  (1) every kit StaticMesh -> "Use Complex Collision As Simple" (render mesh = collision)
  (2) every placed 'CathedralBuild' actor -> mesh component set to BLOCK the player

Run NATIVELY (not the bridge), with L_Cathedral open, in the editor's bottom Cmd box:
    py "C:/Users/wpark/Claude/cathedral_collision.py"
Then press Ctrl+S to save the level.
"""
import unreal

MESH_ROOT = "/Game/UltimateGothicCathedralChurch/Mesh"
TAG       = "CathedralBuild"

# (1) mesh assets -> complex-as-simple
ar = unreal.AssetRegistryHelpers.get_asset_registry()
mesh_set = 0
for a in ar.get_assets_by_path(MESH_ROOT, recursive=True):
    try:
        o = a.get_asset()
        if not isinstance(o, unreal.StaticMesh):
            continue
        bs = o.get_editor_property("body_setup")
        if bs:
            bs.set_editor_property("collision_trace_flag",
                                   unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            unreal.EditorAssetLibrary.save_loaded_asset(o)
            mesh_set += 1
    except Exception as e:
        unreal.log_error("[Collision-mesh] %s: %s" % (a.asset_name, e))

# (2) placed actors -> block the player
try:
    AS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = AS.get_all_level_actors()
except Exception:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()

act_set = 0
for act in actors:
    try:
        if unreal.Name(TAG) not in act.tags:
            continue
        comp = act.get_component_by_class(unreal.StaticMeshComponent)
        if comp:
            comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
            comp.set_collision_profile_name("BlockAll")
            act_set += 1
    except Exception as e:
        unreal.log_error("[Collision-actor] %s: %s" % (act.get_actor_label(), e))

msg = "[Cathedral] collision: %d meshes complex-as-simple, %d actors -> BlockAll" % (mesh_set, act_set)
print(msg); unreal.log(msg)
