# forest_sculpt.py — gentle hills via a procedural heightmap drawn to a render target, imported
# into the landscape. Smooth grayscale (0.5 +/- small sin bumps); on RTF_RGBA8 the R high-byte
# dominates so it reads as smooth low relief. Safety net: report Z-bounds before/after so I can
# revert if it goes haywire. Reseats the PlayerStart above the terrain.
import unreal, json
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
out = {}

land = None
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.Landscape):
        land = a
        break
_, ext0 = land.get_actor_bounds(False)
out["z_before_uu"] = round(ext0.z, 1)

# --- height material: emissive = 0.5 + 0.010*(sin(U*15.7) + sin(V*12.5)) ---
if not unreal.EditorAssetLibrary.does_asset_exist("/Game/Forest/M_HeightGen"):
    hm = at.create_asset("M_HeightGen", "/Game/Forest", unreal.Material, unreal.MaterialFactoryNew())
    hm.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    def mk(cls, x, y):
        return mel.create_material_expression(hm, cls, x, y)
    tc = mk(unreal.MaterialExpressionTextureCoordinate, -1000, 0)
    mu = mk(unreal.MaterialExpressionComponentMask, -820, -120); mu.set_editor_property("r", True); mu.set_editor_property("g", False); mu.set_editor_property("b", False); mu.set_editor_property("a", False)
    mv = mk(unreal.MaterialExpressionComponentMask, -820, 120); mv.set_editor_property("r", False); mv.set_editor_property("g", True); mv.set_editor_property("b", False); mv.set_editor_property("a", False)
    mel.connect_material_expressions(tc, "", mu, "")
    mel.connect_material_expressions(tc, "", mv, "")
    ku = mk(unreal.MaterialExpressionConstant, -820, -260); ku.set_editor_property("r", 15.7)
    kv = mk(unreal.MaterialExpressionConstant, -820, 260); kv.set_editor_property("r", 12.5)
    xu = mk(unreal.MaterialExpressionMultiply, -640, -120); mel.connect_material_expressions(mu, "", xu, "A"); mel.connect_material_expressions(ku, "", xu, "B")
    xv = mk(unreal.MaterialExpressionMultiply, -640, 120); mel.connect_material_expressions(mv, "", xv, "A"); mel.connect_material_expressions(kv, "", xv, "B")
    su = mk(unreal.MaterialExpressionSine, -460, -120); mel.connect_material_expressions(xu, "", su, "")
    sv = mk(unreal.MaterialExpressionSine, -460, 120); mel.connect_material_expressions(xv, "", sv, "")
    add = mk(unreal.MaterialExpressionAdd, -300, 0); mel.connect_material_expressions(su, "", add, "A"); mel.connect_material_expressions(sv, "", add, "B")
    ka = mk(unreal.MaterialExpressionConstant, -300, 200); ka.set_editor_property("r", 0.010)
    amp = mk(unreal.MaterialExpressionMultiply, -140, 0); mel.connect_material_expressions(add, "", amp, "A"); mel.connect_material_expressions(ka, "", amp, "B")
    kb = mk(unreal.MaterialExpressionConstant, -140, 200); kb.set_editor_property("r", 0.5)
    h = mk(unreal.MaterialExpressionAdd, 40, 0); mel.connect_material_expressions(amp, "", h, "A"); mel.connect_material_expressions(kb, "", h, "B")
    mel.connect_material_property(h, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(hm)
    unreal.EditorAssetLibrary.save_asset("/Game/Forest/M_HeightGen")
else:
    hm = unreal.EditorAssetLibrary.load_asset("/Game/Forest/M_HeightGen")

rt = unreal.RenderingLibrary.create_render_target2d(world, 512, 512, unreal.TextureRenderTargetFormat.RTF_RGBA8)
unreal.RenderingLibrary.draw_material_to_render_target(world, rt, hm)
try:
    ok = land.landscape_import_heightmap_from_render_target(rt)
    out["import_ok"] = str(ok)
except Exception as e:
    out["import_err"] = repr(e)

_, ext1 = land.get_actor_bounds(False)
out["z_after_uu"] = round(ext1.z, 1)

# reseat PlayerStart well above the (now sculpted) terrain so spawn always lands on ground
for a in eas.get_all_level_actors():
    if a.get_actor_label() == "PlayerStart":
        a.set_actor_location(unreal.Vector(0.0, 0.0, 1200.0), False, False)

les.save_current_level()
open(r"C:/Users/wpark/projects/sibelius-game/forest-sculpt.json", "w").write(json.dumps(out, indent=1))
unreal.log("###SCULPT### z_before=%s z_after=%s import=%s" % (out["z_before_uu"], out["z_after_uu"], out.get("import_ok") or out.get("import_err")))
