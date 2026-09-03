# place_supply_counter.py - the checkout that provisions Leonard for the planet Grok.
#
# *** OPEN /Game/Maps/L_uFoods FIRST, THEN RUN THIS ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/place_supply_counter.py"
#
# Refuses unless L_uFoods is already open, and never changes levels itself.
#
# ===========================================================================
# WHAT IT MAKES.  One ASupplyCounter - the [E] that buys every supply at once.
#
# Walt, after walking the aisles for the first time: "Instead of picking from all that
# stuff, maybe just [E] to Buy all Supplies at once." The room has 1,956 meshes and 286
# price tags and none of that is a shopping system; the beat is "he went and got what he
# needed", and an inventory screen would add clicks without adding that.
#
# WHERE, AND WHY IT IS NOT A NUMBER I INVENTED.
#
# The pack models a checkout - SM_Cashier_Table_01 and friends - so the counter goes
# where a person would actually pay, and the level is asked where that is. Of the several
# tables, it picks the one NEAREST THE ENTRANCE: he walks in, and the first till he meets
# is the one that serves him. Anything else makes him hunt through a supermarket for the
# only interactive object in it.
#
# The PlayerStart is the entrance (move_ufoods_playerstart.py put it among the trolleys),
# so "nearest the entrance" is measured from there rather than guessed.
#
# AND IT LEAVES AN EXISTING COUNTER ALONE. Hand placement wins - drag it and re-running
# will not put it back. That is the place_cafe_doors lesson, made structural.

import json
import traceback

import unreal

LEVEL = "/Game/Maps/L_uFoods"
TAG = "SupplyCounter"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/place_supply_counter.json"

# Mesh-name fragments that mean "this is the till".
TILL_WORDS = ("cashier_table", "cash_register")

# Lifted off the table top so the reach sphere sits in the air a person aims at, not
# inside the furniture. The sphere's own ReachHeight adds to this.
LIFT = 40.0

r = {}


def find_by_tag(eas, tag):
    for a in eas.get_all_level_actors():
        try:
            if tag in [str(t) for t in a.get_editor_property("tags")]:
                return a
        except Exception:
            pass
    return None


try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    open_level = ues.get_editor_world().get_path_name().split(".")[0]
    r["open_level"] = open_level

    if open_level != LEVEL:
        r["refused"] = "Open %s by hand and run this again." % LEVEL
    else:
        counter_cls = unreal.load_class(None, "/Script/SibeliusGame.SupplyCounter")
        if not counter_cls:
            raise Exception("no ASupplyCounter class - did the editor start after the "
                            "build that added it? A new UCLASS needs a full build with "
                            "the editor CLOSED; Live Coding will not do it.")

        existing = find_by_tag(eas, TAG)
        if existing:
            p = existing.get_actor_location()
            r["counter"] = "ALREADY EXISTS - not moved"
            r["counter_at"] = [round(p.x, 1), round(p.y, 1), round(p.z, 1)]
            r["note"] = "Hand placement wins. Delete it and re-run to re-derive."
        else:
            actors = eas.get_all_level_actors()

            # The entrance, so "nearest" means something.
            starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
            if not starts:
                raise Exception("no PlayerStart in L_uFoods - run "
                                "move_ufoods_playerstart.py first; it defines the entrance.")
            door = starts[0].get_actor_location()
            r["entrance_at"] = [round(door.x, 1), round(door.y, 1), round(door.z, 1)]

            tills = []
            for a in actors:
                for c in a.get_components_by_class(unreal.StaticMeshComponent):
                    m = c.get_editor_property("static_mesh")
                    if not m:
                        continue
                    name = m.get_path_name().split(".")[-1].lower()
                    if any(w in name for w in TILL_WORDS):
                        t = c.get_world_transform().translation
                        d = ((t.x - door.x) ** 2 + (t.y - door.y) ** 2) ** 0.5
                        tills.append((d, name, t))

            r["tills_found"] = len(tills)
            if not tills:
                raise Exception("no cashier table or till found in this level. Place the "
                                "SupplyCounter by hand and tag it %s." % TAG)

            tills.sort(key=lambda e: e[0])
            dist, name, t = tills[0]
            r["chosen_till"] = name
            r["till_distance_cm"] = round(dist, 1)
            r["all_tills_cm"] = [round(e[0], 1) for e in tills]

            spot = unreal.Vector(t.x, t.y, t.z + LIFT)
            counter = eas.spawn_actor_from_class(counter_cls, spot)
            counter.set_actor_label("uFoods_SupplyCounter")
            counter.set_editor_property("tags", [unreal.Name(TAG)])

            r["counter"] = "created"
            r["counter_at"] = [round(spot.x, 1), round(spot.y, 1), round(spot.z, 1)]
            # Read back rather than assume - a price that silently failed to read is a
            # shop that charges something nobody chose.
            try:
                r["price"] = int(counter.get_editor_property("Price"))
            except Exception as e:
                r["price_read_error"] = str(e)
            r["CHECK_IT"] = ("Walk to the till and look for '[E] Buy all supplies'. If the "
                             "prompt does not appear, the reach sphere is inside the "
                             "furniture - raise LIFT or drag the counter out a little.")

        les.save_current_level()
        r["saved"] = True
        r["next"] = ("Play: go into uFoods, walk to the till, press E. Then check the "
                     "Sauce counter went down and the prompt changed.")

    unreal.SystemLibrary.collect_garbage()

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###SUPPLYCOUNTER### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
