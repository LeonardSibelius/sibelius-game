# dump_playerstart_vs_pawn.py — why PlayerStart draws "BAD size".
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

ps = None
for a in eas.get_all_level_actors():
    try:
        if a.get_actor_label() == "PlayerStart":
            ps = a
            break
    except Exception:
        pass

ps_info = None
if ps:
    loc = ps.get_actor_location()
    scale = ps.get_actor_scale3d()
    caps = []
    for c in ps.get_components_by_class(unreal.CapsuleComponent):
        caps.append({
            "name": c.get_name(),
            "r": round(c.get_unscaled_capsule_radius(), 2),
            "h": round(c.get_unscaled_capsule_half_height(), 2),
            "sr": round(c.get_scaled_capsule_radius(), 2),
            "sh": round(c.get_scaled_capsule_half_height(), 2),
        })
    ps_info = {
        "x": round(loc.x, 1), "y": round(loc.y, 1), "z": round(loc.z, 1),
        "scale": [round(scale.x, 3), round(scale.y, 3), round(scale.z, 3)],
        "caps": caps,
    }

# World Settings game mode + default pawn capsule.
pawn_info = None
gm_name = None
try:
    ws = world.get_world_settings()
    gm = ws.get_editor_property("default_game_mode") if ws else None
    gm_name = str(gm)
    gm_cdo = gm.get_default_object() if gm else None
    pawn_cls = gm_cdo.get_editor_property("default_pawn_class") if gm_cdo else None
    if pawn_cls:
        pawn_cdo = pawn_cls.get_default_object()
        pr = ph = None
        try:
            cap = pawn_cdo.get_component_by_class(unreal.CapsuleComponent)
            if cap:
                pr = round(cap.get_unscaled_capsule_radius(), 2)
                ph = round(cap.get_unscaled_capsule_half_height(), 2)
        except Exception as e:
            pr = str(e)
        pawn_info = {"class": pawn_cls.get_name(), "r": pr, "h": ph}
except Exception as e:
    pawn_info = {"error": str(e)}

payload = {"playerstart": ps_info, "gamemode": gm_name, "pawn": pawn_info}
