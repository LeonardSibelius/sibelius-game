# dump_fp_pawn_capsule.py — capsule on BP_FirstPersonCharacter / game mode pawn.
import unreal

eal = unreal.EditorAssetLibrary

def capsule_of(path):
    if not eal.does_asset_exist(path):
        return {"missing": path}
    obj = eal.load_asset(path)
    cdo = obj.get_default_object() if hasattr(obj, "get_default_object") else obj
    # Generated class
    try:
        gen = obj.generated_class() if hasattr(obj, "generated_class") else None
        if gen:
            cdo = gen.get_default_object()
    except Exception:
        pass
    info = {"path": path, "cdo": str(type(cdo))}
    try:
        cap = cdo.get_component_by_class(unreal.CapsuleComponent)
        if cap:
            info["r"] = round(cap.get_unscaled_capsule_radius(), 2)
            info["h"] = round(cap.get_unscaled_capsule_half_height(), 2)
            info["cap"] = cap.get_name()
    except Exception as e:
        info["err"] = str(e)
    # GameMode: try both property names
    for prop in ("default_pawn_class", "DefaultPawnClass"):
        try:
            info[prop] = str(cdo.get_editor_property(prop))
        except Exception:
            pass
    return info

payload = {
    "mode": capsule_of("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode"),
    "char": capsule_of("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter"),
    "sib": capsule_of("/Script/SibeliusGame.SibeliusGameCharacter"),
}
