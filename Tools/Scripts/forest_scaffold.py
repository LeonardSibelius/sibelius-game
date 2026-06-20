# forest_scaffold.py — SIB Forest Phase 1 (run via the live-editor bridge, editor OPEN).
# Creates L_Elsewhere_Forest with basic outdoor lighting + sky + fog + a PlayerStart, and saves.
# The Landscape grid itself is the one Slate-only step Walt does (Landscape Mode -> New); this
# scaffold is everything around it. Re-runnable-ish (new_level overwrites the level asset).
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def log(s): unreal.log("###FORESTSCAFFOLD### " + str(s))

LEVEL = "/Game/Maps/L_Elsewhere_Forest"
les.new_level(LEVEL)
log("new level: " + LEVEL)

def spawn(cls, loc=(0.0, 0.0, 0.0), rot=(0.0, 0.0, 0.0), label=None):
    a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
    if a and label:
        a.set_actor_label(label)
    return a

# --- Sun (drives the SkyAtmosphere) ---
sun = spawn(unreal.DirectionalLight, (0, 0, 2000), (-42.0, -35.0, 0.0), "Sun")  # Rotator(pitch,yaw,roll)
try:
    sc = sun.light_component
    sc.set_mobility(unreal.ComponentMobility.MOVABLE)
    sc.set_editor_property("intensity", 6.0)
    try:
        sc.set_editor_property("atmosphere_sun_light", True)
    except Exception as e2:
        log("atmosphere_sun_light skip %r" % e2)
    log("sun ok")
except Exception as e:
    log("sun props ERR %r" % e)

# --- Sky atmosphere + sky light (real-time capture) + height fog ---
spawn(unreal.SkyAtmosphere, label="SkyAtmosphere")
log("sky atmosphere ok")
sl = spawn(unreal.SkyLight, (0, 0, 2000), label="SkyLight")
try:
    slc = sl.sky_light_component
    slc.set_mobility(unreal.ComponentMobility.MOVABLE)
    slc.set_editor_property("real_time_capture", True)
    log("skylight ok")
except Exception as e:
    log("skylight props ERR %r" % e)
spawn(unreal.ExponentialHeightFog, (0, 0, 200), label="HeightFog")
log("fog ok")

# --- PlayerStart (above the future terrain; reseated after the landscape exists) ---
spawn(unreal.PlayerStart, (0, 0, 300), label="PlayerStart")
log("playerstart ok")

les.save_current_level()
log("SAVED " + LEVEL + " — ready for Walt's New Landscape step")
