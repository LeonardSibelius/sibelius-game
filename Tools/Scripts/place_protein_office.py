"""Editor-closed Python commandlet: author the office and both deli meal grants.

Run with -run=pythonscript -script=<this file>. Re-running preserves the office's
hand-placed transform. No vendor content is changed. Back up maps before first run.
"""
import json
import unreal

REPORT = "C:/Users/wpark/projects/sibelius-game/Saved/protein-office-setup.json"
EAL = unreal.EditorAssetLibrary
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MEL = unreal.MaterialEditingLibrary
FOLDER = "/Game/ProteinMachines"


def material(name, rgb, glow=False):
    path = FOLDER + "/" + name
    existing = EAL.load_asset(path) if EAL.does_asset_exist(path) else None
    if existing:
        return existing
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    color = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, 0)
    color.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    if glow:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.recompile_material(mat)
    assert EAL.save_asset(path)
    return mat


report = {}
try:
    # The commandlet has its own editor world: never switch the user's live map.
    world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/L_Cafe")
    assert world
    meals = []
    for actor in EAS.get_all_level_actors():
        if actor.get_class().get_name() != "CoffeeCup":
            continue
        mesh = actor.get_editor_property("Mesh").get_editor_property("static_mesh")
        words = (actor.get_actor_label() + " " + str(actor.get_editor_property("PromptText"))
                 + " " + (mesh.get_name() if mesh else "")).lower()
        grant = "City.Burger" if "burger" in words else "City.Coffee" if "coffee" in words else None
        if grant:
            actor.set_editor_property("MealGrant", unreal.Name(grant))
            meals.append({"actor": actor.get_actor_label(), "grant": grant})
    assert {m["grant"] for m in meals} == {"City.Burger", "City.Coffee"}, meals
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Maps/L_Cafe")
    report["meals"] = meals

    world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/L_City")
    assert world
    offices = [a for a in EAS.get_all_level_actors() if a.get_class().get_name() == "ProteinMachine"]
    assert len(offices) <= 1, "Multiple protein offices: resolve explicitly"
    office = offices[0] if offices else EAS.spawn_actor_from_class(
        unreal.load_class(None, "/Script/SibeliusGame.ProteinMachine"),
        unreal.Vector(2000, -400, 10), unreal.Rotator(yaw=-90))
    office.set_actor_label("Protein Machines Inc.")
    office.set_editor_property("tags", [unreal.Name("ProteinMachinesOffice")])
    navy = material("M_OfficeNavy", (0.018, 0.034, 0.052))
    light = material("M_OfficeInterior", (0.24, 0.32, 0.37))
    cyan = material("M_ProteinCyan", (0.08, 1.8, 2.3), True)
    gold = material("M_ProteinGold", (2.0, 0.72, 0.08), True)
    bond = material("M_ProteinBond", (0.10, 0.38, 0.46), True)
    for comp in office.get_components_by_class(unreal.StaticMeshComponent):
        name = comp.get_name()
        if name == "ProteinMesh":
            continue  # Preserve any artist-assigned replacement mesh and its material.
        selected = (gold if int(name[-2:]) % 3 == 0 else cyan) if name.startswith("ProteinBead") else (
            bond if name.startswith("ProteinBond") else
            cyan if name.startswith("Accent") or name == "Projector" else
            light if name in ["BackWall", "LeftWall", "RightWall"] else navy)
        comp.set_material(0, selected)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Maps/L_City")
    # Native components must retain their authored overrides across a map reload.
    world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/L_City")
    office = next(a for a in EAS.get_all_level_actors() if a.get_class().get_name() == "ProteinMachine")
    shell = office.get_editor_property("OfficeParts")
    assert len(shell) == 10
    assert all(c.get_material(0) and c.get_material(0).get_path_name().startswith(FOLDER) for c in shell)
    report["shell_materials_survive_reload"] = True
    report["office"] = str(office.get_actor_location())
    report["model_parts"] = len(office.get_editor_property("ModelParts"))
    assert report["model_parts"] == 95
    report["ok"] = True
finally:
    with open(REPORT, "w") as out:
        json.dump(report, out, indent=2)
    print("PROTEIN_OFFICE_SETUP " + json.dumps(report))
