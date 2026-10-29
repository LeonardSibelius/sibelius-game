"""Small original crown/jewel meshes and materials. Run with PIE stopped.
Creates missing assets only. Sources are generated under Saved, so this script
is the reproducible source of the meshes. Never modifies the vendor pack.
"""
import math
from pathlib import Path
import unreal

FOLDER = "/Game/DragonsWager"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
scratch = Path(unreal.Paths.project_saved_dir()) / "DragonsWagerAssetSource"
scratch.mkdir(parents=True, exist_ok=True)

def mesh(name, vertices, faces):
    if eal.does_asset_exist(FOLDER + "/" + name):
        return
    source = scratch / (name + ".obj")
    source.write_text("o " + name + "\ns off\n" + "".join("v %.6f %.6f %.6f\n" % v for v in vertices)
                      + "".join("f " + " ".join(str(i+1) for i in f) + "\n" for f in faces), encoding="ascii")
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = FOLDER
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = False
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes = True
    # OBJ is Z-up here; tell the importer to preserve source coordinates.
    options.static_mesh_import_data.convert_scene = False
    task.options = options
    tools.import_asset_tasks([task])
    assert eal.does_asset_exist(FOLDER + "/" + name), task.imported_object_paths

def ring_mesh():
    vertices, faces = [], []
    count = 64
    # A hollow band with an undulating eight-point upper rim.
    for i in range(count):
        a = 2*math.pi*i/count
        top = 10 + 25*max(0, math.cos(8*a))**2
        for radius,z in [(30,0),(30,top),(26,top),(26,0)]:
            vertices.append((radius*math.cos(a),radius*math.sin(a),z))
    for i in range(count):
        j=(i+1)%count
        for k in range(4):
            n=(k+1)%4
            faces.append((4*i+k,4*j+k,4*j+n,4*i+n))
    return vertices,faces

def gem_mesh():
    vertices,faces=[],[]
    for radius,z,offset in [(9,-13,0),(17,0,0),(11,10,0),(6,15,0)]:
        for i in range(8):
            a=2*math.pi*i/8+offset
            vertices.append((radius*math.cos(a),radius*math.sin(a),z))
    for layer in range(3):
        for i in range(8):
            j=(i+1)%8
            faces.append((layer*8+i,layer*8+j,(layer+1)*8+j,(layer+1)*8+i))
    faces.append(tuple(reversed(range(8))))
    faces.append(tuple(range(24,32)))
    return vertices,faces

def const(mat, value):
    n=mel.create_material_expression(mat,unreal.MaterialExpressionConstant,0,0)
    n.set_editor_property("r",value)
    return n

def material(name, instanced=False):
    path=FOLDER+"/"+name
    if eal.does_asset_exist(path): return
    mat=tools.create_asset(name,FOLDER,unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_instanced_static_meshes",True)
    # Gold also gilds the caught player's skeletal meshes. A cooked build cannot add
    # the usage flag on the fly and would draw the default material instead.
    mat.set_editor_property("used_with_skeletal_mesh",not instanced)
    if instanced:
        channels=[]
        for i in range(3):
            n=mel.create_material_expression(mat,unreal.MaterialExpressionPerInstanceCustomData,-500,i*100)
            n.set_editor_property("data_index",i)
            channels.append(n)
        rg=mel.create_material_expression(mat,unreal.MaterialExpressionAppendVector,-300,0)
        rgb=mel.create_material_expression(mat,unreal.MaterialExpressionAppendVector,-100,0)
        mel.connect_material_expressions(channels[0],"",rg,"A")
        mel.connect_material_expressions(channels[1],"",rg,"B")
        mel.connect_material_expressions(rg,"",rgb,"A")
        mel.connect_material_expressions(channels[2],"",rgb,"B")
        glow=mel.create_material_expression(mat,unreal.MaterialExpressionMultiply,100,200)
        mel.connect_material_expressions(rgb,"",glow,"A")
        mel.connect_material_expressions(const(mat,0.045),"",glow,"B")
        mel.connect_material_property(glow,"",unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        rgb=mel.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-100,0)
        rgb.set_editor_property("constant",unreal.LinearColor(0.55,0.32,0.07,1))
    mel.connect_material_property(rgb,"",unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(const(mat,0.5 if instanced else 0.88),"",unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(const(mat,0.17 if instanced else 0.25),"",unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_asset(path)

def skeletal_usage(name):
    """Assets made before the flag was added get it here, without rebuilding them."""
    path=FOLDER+"/"+name
    mat=eal.load_asset(path)
    if not mat.get_editor_property("used_with_skeletal_mesh"):
        mat.set_editor_property("used_with_skeletal_mesh",True)
        mel.recompile_material(mat)
    eal.save_asset(path,only_if_is_dirty=False)

material("M_WagerGold")
material("M_WagerGem",True)
skeletal_usage("M_WagerGold")
mesh("SM_WagerCrown",*ring_mesh())
mesh("SM_WagerGem",*gem_mesh())
print("Dragon's Wager: crown, faceted gem, gold and jewel materials ready.")
