# dump_fp_pawn_capsule2.py
import unreal

eal = unreal.EditorAssetLibrary

def inspect(path):
    out = {"path": path}
    if not eal.does_asset_exist(path):
        out["missing"] = True
        return out
    bp = eal.load_asset(path)
    out["type"] = bp.get_class().get_name()
    gen = None
    try:
        gen = bp.generated_class()
    except Exception as e:
        out["gen_err"] = str(e)
    if gen is None:
        return out
    out["gen"] = gen.get_name()
    cdo = gen.get_default_object()
    out["cdo"] = cdo.get_class().get_name()
    try:
        cap = cdo.get_component_by_class(unreal.CapsuleComponent.static_class())
        if cap:
            out["r"] = round(cap.get_unscaled_capsule_radius(), 2)
            out["h"] = round(cap.get_unscaled_capsule_half_height(), 2)
    except Exception as e:
        out["cap_err"] = str(e)
    for prop in ("default_pawn_class", "hud_class"):
        try:
            v = cdo.get_editor_property(prop)
            out[prop] = v.get_name() if v else None
        except Exception as e:
            out[prop + "_err"] = str(e)
    return out

payload = {
    "mode": inspect("/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode"),
    "char": inspect("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter"),
}
