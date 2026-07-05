import unreal, json
G = unreal.load_asset("/Game/PCG/PCG_ForestScatter")

def lbl(p):
    return str(p.get_editor_property("properties").get_editor_property("label"))

res = {"per_node": []}
for n in G.nodes:
    sname = n.get_settings().get_class().get_name()
    ie = sum(len(p.edges) for p in n.input_pins)
    oe = sum(len(p.edges) for p in n.output_pins)
    res["per_node"].append("%s in=%d out=%d" % (sname, ie, oe))

io = G.get_output_node()
res["output_node_in"] = sum(len(p.edges) for p in io.input_pins)
res["output_in_by_pin"] = ["%s:%d" % (lbl(p), len(p.edges)) for p in io.input_pins if len(p.edges)]
open(r"C:/Users/wpark/projects/sibelius-game/forest-pcg-check.json", "w").write(json.dumps(res, indent=1))
unreal.log("###PCGCHK### outnode_in=%d" % res["output_node_in"])
