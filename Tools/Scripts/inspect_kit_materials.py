# inspect_kit_materials.py — SIB-47 floor-material quick win. Prints the material slots of the
# floor/wall kit meshes and whether their base materials carry bUsedWithInstancedStaticMeshes
# (the flag that decides whether a runtime ISM renders the real material or the engine "checker").
# READ-ONLY on the gitignored kit. Run editor-CLOSED via -run=pythonscript; writes to Saved/.
import unreal

OUT = "%s/Saved/kit_materials.txt" % unreal.Paths.project_dir().rstrip("/")
TAG = "###KITMAT###"

MESHES = [
    "/Game/ModularSciFiEnv_K/Meshes/Floors/SM_Floor_A_4x4m.SM_Floor_A_4x4m",
    "/Game/ModularSciFiEnv_K/Meshes/Walls/SM_Wall_A_Mid_4x4m.SM_Wall_A_Mid_4x4m",
    "/Game/ModularSciFiEnv_K/Meshes/Ceilings/SM_Ceiling_A_4x4m.SM_Ceiling_A_4x4m",
]
MATS = [
    "/Game/ModularSciFiEnv_K/Materials/Instances/MI_Floor_A_Base.MI_Floor_A_Base",
    "/Game/ModularSciFiEnv_K/Materials/Instances/MI_Floor_A_Border.MI_Floor_A_Border",
]

def used_with_ism(mat_iface):
    try:
        base = mat_iface.get_base_material()
        return base.get_editor_property("used_with_instanced_static_meshes")
    except Exception as e:
        return "??(%r)" % e

lines = []
for mp in MESHES:
    sm = unreal.load_asset(mp)
    name = mp.split("/")[-1].split(".")[0]
    if not isinstance(sm, unreal.StaticMesh):
        lines.append("%s : NOT A STATIC MESH" % name); continue
    sms = sm.get_editor_property("static_materials")
    lines.append("MESH %s (%d slots):" % (name, len(sms)))
    for i, slot in enumerate(sms):
        mi = slot.get_editor_property("material_interface")
        slot_name = slot.get_editor_property("material_slot_name")
        mi_path = mi.get_path_name() if mi else "<none>"
        lines.append("   slot %d '%s' -> %s | usedWithISM=%s" % (i, slot_name, mi_path, used_with_ism(mi) if mi else "n/a"))

for mp in MATS:
    mi = unreal.load_asset(mp)
    name = mp.split("/")[-1].split(".")[0]
    if mi is None:
        lines.append("MAT %s : NOT FOUND" % name); continue
    lines.append("MAT %s | usedWithISM=%s | base=%s" % (
        name, used_with_ism(mi), (mi.get_base_material().get_name() if mi.get_base_material() else "?")))

with open(OUT, "w") as fh:
    fh.write("\n".join(lines) + "\n")
for L in lines:
    unreal.log("%s %s" % (TAG, L))
unreal.log("%s DONE -> %s" % (TAG, OUT))
