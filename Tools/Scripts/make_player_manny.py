# make_player_manny.py - Leonard's everyday body becomes Manny instead of Greystone.
#
# Walt, 2026-10-15, on the basketball court: "i would rather the player be Manny than
# Greystone, Greystone looks silly standing on the court with his sword". And of the
# meadow: "Greystone still fights 150 refusers after the cathedral is done and he looks
# fine there".
#
# WHAT CHANGES. BP_FirstPersonCharacter's Mesh (CharacterMesh0) - the full body hidden from
# his own eyes - goes from Greystone (fbe807a8, July: the attic mirror's "weird pale
# monster" was the old mannequin; that mirror is parked) to SKM_Manny_Simple driven by the
# third-person template's ABP_Unarmed. It is the body his shadow comes from, and the one
# every camera that is not his eyes shows: the court, the tables, the spaceport.
#
# WHAT DOES NOT. Battle form (the meadow, Kaia's home) builds its own avatar component and
# wears Greystone there (UBattleFormComponent::AvatarMesh); his sword swings play on that
# avatar. Greystone stays always-cooked (DefaultGame.ini).
#
# *** RUN FROM THE OPEN EDITOR, PIE stopped, any level ***
#   py "C:/Users/wpark/projects/sibelius-game/Tools/Scripts/make_player_manny.py"
# Saves the Blueprint and nothing else. Safe to re-run. To go back, set BODY and ANIM to
# Greystone's and run it again.

import unreal

TAG = "###PLAYERMANNY###"
PLAYER_BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
BODY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
ANIM = "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"

eal = unreal.EditorAssetLibrary


def say(text):
    print("%s %s" % (TAG, text))


def describe(body):
    mesh = body.get_editor_property("skeletal_mesh_asset")
    anim = body.get_editor_property("anim_class")
    return "%s, animated by %s" % (mesh.get_path_name() if mesh else None, anim.get_path_name() if anim else None)


def main():
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        say("REFUSING: stop Play first. Nothing changed.")
        return
    bp = eal.load_asset(PLAYER_BP)
    mesh = eal.load_asset(BODY)
    anim = unreal.load_class(None, ANIM)
    if not bp or not mesh or not anim:
        say("missing: blueprint %s, body %s, anim %s. Nothing changed." % (bool(bp), bool(mesh), bool(anim)))
        return
    body = unreal.get_default_object(bp.generated_class()).get_editor_property("mesh")
    say("before: %s" % describe(body))
    body.set_editor_property("skeletal_mesh_asset", mesh)
    body.set_editor_property("anim_class", anim)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_asset(PLAYER_BP, only_if_is_dirty=False)
    body = unreal.get_default_object(bp.generated_class()).get_editor_property("mesh")
    say("after:  %s" % describe(body))
    say("saved %s" % PLAYER_BP)


main()
