# city_high_sun.py - put the sun overhead so the street is in full daylight.
#
# *** RUN FROM THE OPEN EDITOR WITH L_City ALREADY OPEN. ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/city_high_sun.py"
#
# It never loads or deletes a level. See make_city_from_downtown.py for why.
#
# ===========================================================================
# THIS IS NOT A BUG, IT IS THE PACK'S TASTE.
#
# Removing Nightime_Lighting got the right TIME OF DAY. It did not get full daylight,
# because Downtown West's Daytime_Lighting puts the sun low - a golden-hour look that
# photographs beautifully in a marketing shot and drags a long shadow off every tree
# across the plaza. Walt: "i sure wish the city was in full daylight. too many shadows."
#
# He is right, and the reason is about the ending rather than about taste. This street is
# the last thing the game shows a man who has just been let out. Long raking shadows read
# as late in the day - as closing time. High sun reads as the middle of one.
#
# ---------------------------------------------------------------------------
# WHY THIS IS FREE, WHERE IT WOULD NORMALLY COST A LIGHTING REBUILD.
#
# DefaultEngine.ini sets r.DynamicGlobalIlluminationMethod=1 - the project runs LUMEN, so
# global illumination is computed dynamically every frame and the baked data is not what
# is lighting this street. That means the sun can be made MOVABLE and swung overhead with
# no Build Lighting pass at all, which on 6,600 actors would otherwise be a long wait for
# a result you might not like.
#
# Two knobs, and the second one matters more than people expect:
#
#   SUN_PITCH  - how high. -90 is straight down and looks flat and dead, because nothing
#                gets a lit side and a shadow side. -55 keeps the modelling on faces and
#                buildings while cutting shadow length to roughly the height of the thing
#                casting it.
#   SKY_LIGHT  - how bright the shadows are. "Too many shadows" is usually really "the
#                shadows are too DARK": ambient skylight is what fills them. Raising this
#                lifts everything the sun cannot reach without washing out the lit side.
#
# ---------------------------------------------------------------------------
# WHERE THE SUN LIVES MATTERS - AND THE SCRIPT TELLS YOU.
#
# If the directional light is in the Downtown_West sublevel rather than in L_City, saving
# it writes to the VENDOR PACK, which CLAUDE.md says not to author into. The readback
# names the package each light belongs to so the choice is visible rather than accidental.
# If it reports a Downtown_West package, the clean fix is to remove that sublevel from
# L_City and let the sun this script can add to L_City itself do the whole job.

import json
import traceback

import unreal

LEVEL = "/Game/Maps/L_City"
OUT = "C:/Users/wpark/projects/sibelius-game/Saved/city_high_sun.json"

SUN_PITCH = -55.0     # degrees below horizontal; -90 is straight down and looks flat
SUN_YAW = -35.0       # which way the shadows fall; taste, not correctness
SUN_INTENSITY = 4.0   # SEE THE NOTE BELOW BEFORE RAISING THIS
SKY_LIGHT = 3.0       # THE SHADOW-LIFTER. Raise this before raising the sun.

# WHY 4.0 AND NOT 8.0.
#
# 8.0 was a number I made up while raising the sun, and between it and the 32 degrees of
# extra height the street went from golden hour to blown out. Walt: "the dancers look hot
# and pale." Skin is the first thing to clip - it sits high in the histogram already, so
# it goes white while the brick is still holding detail, and the palest face goes first.
# That is why Kaia caught it worse than Nyra.
#
# THE ORDER TO TUNE IN, because they are not interchangeable:
#   too dark in the shade   -> raise SKY_LIGHT (fills shadow, barely touches the lit side)
#   too dim overall         -> raise SUN_INTENSITY a HALF STOP at a time (4.0 -> 4.5)
#   faces washing out       -> lower SUN_INTENSITY first, never the sky
#
# Reaching for the sun to fix a shadow is what got us here.

r = {}

try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    world = ues.get_editor_world()
    open_level = world.get_path_name().split(".")[0]
    r["open_level"] = open_level
    if open_level != LEVEL:
        raise Exception("open %s first (File > Open Level)." % LEVEL)

    def home_of(actor):
        """Which package this actor will be SAVED into - vendor or ours."""
        try:
            return actor.get_outer().get_outer().get_path_name().split(".")[0]
        except Exception:
            return "unknown"

    suns, skies = [], []

    for a in eas.get_all_level_actors():
        if isinstance(a, unreal.DirectionalLight):
            c = a.get_component_by_class(unreal.DirectionalLightComponent)
            if not c:
                continue
            before = a.get_actor_rotation()
            # RECORD BEFORE YOU OVERWRITE. The first version captured pitch_was and not
            # intensity_was, so when 8.0 turned out to be too hot there was no record of
            # what the pack had shipped and the original was simply gone. A readback that
            # only reports the value you just wrote is not a readback.
            intensity_before = float(c.get_editor_property("intensity"))
            # MOVABLE FIRST. A Static or Stationary light will not accept a new angle at
            # runtime and its baked shadows would keep pointing the old way; under Lumen
            # there is no reason for it to be anything else.
            c.set_mobility(unreal.ComponentMobility.MOVABLE)
            a.set_actor_rotation(unreal.Rotator(0.0, SUN_PITCH, SUN_YAW), False)
            c.set_editor_property("intensity", SUN_INTENSITY)
            after = a.get_actor_rotation()
            suns.append({
                "label": a.get_actor_label(),
                "lives_in": home_of(a),
                "pitch_was": round(before.pitch, 1),
                "pitch_now": round(after.pitch, 1),
                "intensity_was": round(intensity_before, 2),
                "intensity_now": round(float(c.get_editor_property("intensity")), 2),
                "moved": abs(after.pitch - SUN_PITCH) < 0.5,
            })

        elif isinstance(a, unreal.SkyLight):
            c = a.get_component_by_class(unreal.SkyLightComponent)
            if not c:
                continue
            was = float(c.get_editor_property("intensity"))
            c.set_mobility(unreal.ComponentMobility.MOVABLE)
            c.set_editor_property("intensity", SKY_LIGHT)
            # REAL TIME CAPTURE OFF. Turning it on was a reflex and it was wrong here:
            # Downtown West ships no SkyAtmosphere, so a real-time skylight has nothing
            # valid to capture and the viewport says so in red - "otherwise it will be
            # black". The pack's own captured sky is correct for this street; the
            # skylight was already at 3.0 and needed nothing.
            try:
                c.set_editor_property("real_time_capture", False)
            except Exception:
                pass
            skies.append({
                "label": a.get_actor_label(),
                "lives_in": home_of(a),
                "intensity_was": round(was, 2),
                "intensity_now": round(float(c.get_editor_property("intensity")), 2),
            })

    r["suns"] = suns
    r["sky_lights"] = skies
    r["sun_count"] = len(suns)

    # DID WE JUST EDIT THE VENDOR PACK? Say it out loud rather than letting it happen.
    touched = {s["lives_in"] for s in suns} | {s["lives_in"] for s in skies}
    r["packages_touched"] = sorted(touched)
    r["vendor_touched"] = [p for p in touched if "Downtown_West" in p]

    # Persistent level plus any dirtied sublevels - save_current_level would miss the
    # sublevel the sun probably lives in.
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    r["saved"] = True
    r["next"] = ("Travel there with [>] and look at the shadows. Too dark still? Raise "
                 "SKY_LIGHT before touching SUN_PITCH - dark shadows are an ambient "
                 "problem, not a sun-angle one.")

    del world, suns, skies
    unreal.SystemLibrary.collect_garbage()

except Exception:
    r["traceback"] = traceback.format_exc()

with open(OUT, "w") as fh:
    json.dump(r, fh, indent=2)
unreal.log("###SUN### %s" % json.dumps(r))
print(json.dumps(r, indent=2))
