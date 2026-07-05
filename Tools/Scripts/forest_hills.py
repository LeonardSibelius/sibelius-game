# forest_hills.py — SIB Forest: gentle rolling hills via a PROPER 16-bit heightmap (RTF_RG16f, so
# no 8-bit terracing like the first attempt). Height = 0.5 + amp*(weighted low-frequency sines) —
# large-wavelength smooth swells, a few bigger than others (hero hills). ONE attempt: if it reads
# wrong in PIE, Walt hand-brushes instead (no loop). Run via the bridge (editor open).
import unreal, json
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
les.load_level("/Game/Maps/L_Elsewhere_Forest")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()

land = None
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.Landscape):
        land = a
        break

if unreal.EditorAssetLibrary.does_asset_exist("/Game/Forest/M_Hills16"):
    unreal.EditorAssetLibrary.delete_asset("/Game/Forest/M_Hills16")
hm = at.create_asset("M_Hills16", "/Game/Forest", unreal.Material, unreal.MaterialFactoryNew())
hm.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

def mk(c, x, y):
    return mel.create_material_expression(hm, c, x, y)

def const(v, x, y):
    k = mk(unreal.MaterialExpressionConstant, x, y)
    k.set_editor_property("r", float(v))
    return k

tc = mk(unreal.MaterialExpressionTextureCoordinate, -1300, 0)
mu = mk(unreal.MaterialExpressionComponentMask, -1120, -180)
mu.set_editor_property("r", True); mu.set_editor_property("g", False); mu.set_editor_property("b", False); mu.set_editor_property("a", False)
mv = mk(unreal.MaterialExpressionComponentMask, -1120, 180)
mv.set_editor_property("r", False); mv.set_editor_property("g", True); mv.set_editor_property("b", False); mv.set_editor_property("a", False)
mel.connect_material_expressions(tc, "", mu, "")
mel.connect_material_expressions(tc, "", mv, "")

def sine(mask, k, w, x, y):
    # w * sin(mask * k). k = 2*pi*cycles (small cycles = large wavelength = smooth).
    ml = mk(unreal.MaterialExpressionMultiply, x, y)
    mel.connect_material_expressions(mask, "", ml, "A")
    mel.connect_material_expressions(const(k, x - 170, y - 60), "", ml, "B")
    s = mk(unreal.MaterialExpressionSine, x + 150, y)
    mel.connect_material_expressions(ml, "", s, "")
    sw = mk(unreal.MaterialExpressionMultiply, x + 320, y)
    mel.connect_material_expressions(s, "", sw, "A")
    mel.connect_material_expressions(const(w, x + 150, y + 70), "", sw, "B")
    return sw

# Hero swells (1.2-1.5 cycles over 508 m = ~340-420 m wavelength) + a gentler finer roll.
terms = [sine(mu, 7.5, 0.6, -780, -260), sine(mv, 9.4, 0.5, -780, -60),
         sine(mu, 15.7, 0.18, -780, 160), sine(mv, 12.5, 0.16, -780, 360)]
acc = terms[0]
for t in terms[1:]:
    nxt = mk(unreal.MaterialExpressionAdd, -120, 0)
    mel.connect_material_expressions(acc, "", nxt, "A")
    mel.connect_material_expressions(t, "", nxt, "B")
    acc = nxt
amp = mk(unreal.MaterialExpressionMultiply, 80, 0)
mel.connect_material_expressions(acc, "", amp, "A")
mel.connect_material_expressions(const(0.030, -120, 200), "", amp, "B")   # ~ +/- 15 m relief
h = mk(unreal.MaterialExpressionAdd, 260, 0)
mel.connect_material_expressions(amp, "", h, "A")
mel.connect_material_expressions(const(0.5, 80, 200), "", h, "B")
mel.connect_material_property(h, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(hm)

rt = unreal.RenderingLibrary.create_render_target2d(world, 1009, 1009, unreal.TextureRenderTargetFormat.RTF_RG16F)
unreal.RenderingLibrary.draw_material_to_render_target(world, rt, hm)
ok = land.landscape_import_heightmap_from_render_target(rt)
_, ext = land.get_actor_bounds(False)
les.save_current_level()
out = {"import": str(ok), "z_half_uu": round(ext.z, 1), "relief_m": round(ext.z * 2 / 100.0, 1)}
open(r"C:/Users/wpark/projects/sibelius-game/forest-hills.json", "w").write(json.dumps(out))
unreal.log("###HILLS### import=%s z_half=%.0f uu (~%.1f m relief)" % (ok, ext.z, ext.z * 2 / 100.0))
