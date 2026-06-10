"""
cathedral_lighting.py — golden-hour pass for L_Cathedral.
  (1) Sun: low raking angle + warm color + volumetric scattering (so it streams
      through the colonnade and feeds the fog).
  (2) Exponential height fog: volumetric fog ON -> the raking sun becomes visible
      light shafts (the real way to do god-rays).
  (3) Unbound Post Process Volume: pull exposure down so it stops blowing out to white.

Run NATIVELY (not the bridge), L_Cathedral open, in the editor's bottom Cmd box:
    py "C:/Users/wpark/Claude/cathedral_lighting.py"
Then Ctrl+S.  Every value is near the top so we can tune + re-run.
"""
import unreal

# ---- tunables ----
SUN_PITCH = -15.0     # low = raking light (more negative = higher sun)
SUN_YAW   = 75.0      # angle across the nave so light comes through the columns
SUN_TEMP  = 4200.0    # warm gold (lower = warmer)
SUN_INTENSITY  = 14.0 # lux; UE default is ~10 — higher = brighter sun through the glass
SUN_VOLSCATTER = 6.0  # how strongly the sun feeds the volumetric fog (the shafts)
EXPOSURE_BIAS  = -1.0 # negative = darker; was -1.5 — raised for a brighter interior
# ------------------

def all_actors():
    try:
        return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    except Exception:
        return unreal.EditorLevelLibrary.get_all_level_actors()

def spawn(cls, label):
    try:
        a = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(cls, unreal.Vector(0,0,0))
    except Exception:
        a = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, unreal.Vector(0,0,0))
    if a:
        a.set_actor_label(label)
    return a

# (1) SUN
try:
    for a in all_actors():
        if isinstance(a, unreal.DirectionalLight):
            a.set_actor_rotation(unreal.Rotator(pitch=SUN_PITCH, yaw=SUN_YAW, roll=0.0), False)
            c = a.get_component_by_class(unreal.DirectionalLightComponent)
            c.set_editor_property("intensity", SUN_INTENSITY)
            c.set_editor_property("use_temperature", True)
            c.set_editor_property("temperature", SUN_TEMP)
            c.set_editor_property("volumetric_scattering_intensity", SUN_VOLSCATTER)
            unreal.log("[Light] sun set")
            break
except Exception as e:
    unreal.log_error("[Light] sun: %s" % e)

# (2) VOLUMETRIC FOG
try:
    for a in all_actors():
        if isinstance(a, unreal.ExponentialHeightFog):
            f = a.get_component_by_class(unreal.ExponentialHeightFogComponent)
            f.set_editor_property("volumetric_fog", True)
            unreal.log("[Light] volumetric fog on")
            break
except Exception as e:
    unreal.log_error("[Light] fog: %s" % e)

# (3) EXPOSURE via unbound post-process volume
try:
    ppv = None
    for a in all_actors():
        if isinstance(a, unreal.PostProcessVolume) and a.get_actor_label() == "PP_Cathedral":
            ppv = a; break
    if ppv is None:
        ppv = spawn(unreal.PostProcessVolume, "PP_Cathedral")
    ppv.set_editor_property("unbound", True)
    s = ppv.get_editor_property("settings")
    s.set_editor_property("override_auto_exposure_bias", True)
    s.set_editor_property("auto_exposure_bias", EXPOSURE_BIAS)
    ppv.set_editor_property("settings", s)
    unreal.log("[Light] exposure volume set")
except Exception as e:
    unreal.log_error("[Light] ppv: %s" % e)

print("[Cathedral] lighting pass applied")
unreal.log("[Cathedral] lighting pass applied")
