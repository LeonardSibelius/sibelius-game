# dump_fp_pawn_capsule3.py
import unreal

cls = unreal.load_class(None, "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C")
out = {"cls": str(cls)}
if cls:
    cdo = unreal.get_default_object(cls)
    out["cdo"] = str(cdo)
    out["cdo_class"] = cdo.get_class().get_name() if cdo else None
    if cdo:
        try:
            cap = cdo.get_component_by_class(unreal.CapsuleComponent.static_class())
            out["cap"] = cap.get_name() if cap else None
            if cap:
                out["r"] = round(cap.get_unscaled_capsule_radius(), 3)
                out["h"] = round(cap.get_unscaled_capsule_half_height(), 3)
        except Exception as e:
            out["err"] = str(e)
        # SCS components on BP
        try:
            gen = cdo.get_class()
            out["gen"] = gen.get_name()
        except Exception as e:
            out["gen_err"] = str(e)

gm_cls = unreal.load_class(None, "/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode_C")
if gm_cls:
    gm = unreal.get_default_object(gm_cls)
    try:
        pawn = gm.get_editor_property("default_pawn_class")
        out["gm_pawn"] = pawn.get_name() if pawn else None
    except Exception as e:
        out["gm_err"] = str(e)

payload = out
