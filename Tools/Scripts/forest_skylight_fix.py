import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
for a in eas.get_all_level_actors():
    if a.get_actor_label() == "SkyLight":
        c = a.get_component_by_class(unreal.SkyLightComponent)
        c.set_mobility(unreal.ComponentMobility.MOVABLE)
        c.set_editor_property("real_time_capture", True)
        unreal.log("###FIX### skylight real-time capture set on " + a.get_name())
les.save_current_level()
unreal.log("###FIX### saved")
