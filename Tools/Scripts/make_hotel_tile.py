"""Build the architecture-only tile revealed in Grok; run in editor with maps saved."""
import math
import unreal

DEST = '/Game/Maps/L_SibeliusHotel_Generated'
GROK = '/Game/Maps/L_Grok'

# ---------------------------------------------------------------------------
# THE APPROACH AND THE SIGN — the two things Walt saw wrong on the first walk-up.
#
# "i went up the ramp and the bird statues are chopped off and the sibelius hotel sign is
# hard to see."
#
# The landing is a plain Engine cube laid over a vendor scene that was composed without
# it, so where it overlaps DmitriyDryzhak's phoenix statues it simply cuts them in half.
# The numbers below are the ones to move, and the dump at the end of this script prints
# what the statues actually occupy so the next adjustment is measured rather than guessed.
#
# Sizes are the cube's SCALE (the Engine cube is 100 cm), so 4 = 400 cm.
# THE APPROACH RUNS BETWEEN THE STATUES, NOT THROUGH ONE (measured 2026-09-07).
#
# It was at Y=300, and the run printed by this script's own dump says why that shears a
# phoenix in half:
#
#   Landing   X 1400..1800   Y   50..550   Z 235..275
#   Phoenix2  X 1356..1630   Y  -41..226   Z  67..491      <- overlaps on all three
#   Phoenix   X 1356..1630   Y 1185..1452  Z  67..491      <- nowhere near
#
# The two statues flank the entrance, and the middle of the gap between them is Y 705 —
# so the approach was never centred; it was laid over the left-hand one. At 705 the
# landing spans Y 455..955 and clears both by better than two metres.
APPROACH_Y    = 705.0

LANDING_LOC   = unreal.Vector(1600, APPROACH_Y, 255)
LANDING_SCALE = unreal.Vector(4, 5, 0.4)

RAMP_LOC      = unreal.Vector(200, APPROACH_Y, -7.5)
RAMP_RISE     = 525.0      # cm climbed
RAMP_RUN      = 2400.0     # cm travelled
RAMP_WIDTH    = 5          # scale, so 500 cm

# STAIRS INSTEAD OF A RAMP (Walt: "I wish that ramp was stairs instead").
#
# The pack ships its own: SM_Dec_Stair1, SM_Dec_Stair2, SM_Dec_StairRound. Using the
# artist's stone rather than an Engine cube is the whole point — the approach stops being
# a grey slab bolted onto somebody else's composition and becomes part of it.
#
# THE LAYOUT IS MEASURED, NOT GUESSED. Nobody here has seen this mesh, so its size,
# pivot and facing are unknown. The script reads its bounds and tiles it up the slope from
# those numbers, and logs what it found (HOTEL_STAIRMESH) so a wrong orientation is one
# constant away rather than another round of trial and error.
#
# TURNED OFF 2026-09-07, AND THE REASON IS WORTH KEEPING.
#
# SM_Dec_Stair1 measures 475 x 148 x 278 cm: a single tall FLIGHT the artist built for one
# specific place in their composition, not a tileable step. Two of them climb 5.6 m over
# 9.5 m, which is a different slope from the approach and lands nowhere near the entrance
# — Walt got "stairs that go to rock outcroppings and stairs in the air", and the landing
# was dragged up to Z=549 with them, floating over the towers.
#
# The deeper mistake was adding an approach at all. The pack's own Scene mesh includes an
# ornate bridge with steps built into it: the hotel already HAS an entrance walkway, and
# this was a second staircase bolted onto a building that came with one.
#
# Set this back to a mesh path only if a genuinely tileable step turns up.
STAIRS        = None
STAIRS_UNUSED = '/Game/ElvenArchitecture/Meshes/SM_Dec_Stair1'
STAIRS_YAW    = 0.0        # flip to 180 if they climb away from the player
STAIRS_SCALE  = 1.0

# THE SIGN IS GONE (Walt, 2026-09-07: "the sibelius hotel sign is still barely visible.
# just remove it").
#
# It was tried at world_size 45 in warm cream, then 90 in near-black, and it was unreadable
# both times for the same reason: the facade behind it is pale stone crowded with stained
# windows, gold scrollwork and trees, and flat text has nothing to sit against. Light
# letters dissolved into the stone; dark ones disappeared into the shadowed archway.
#
# The right answer was never a third colour. The building announces itself perfectly well,
# the banner names it out loud when it appears, and the objective line names it again while
# he walks up. A sign was the one piece of this epilogue nobody needed.
#
# Set SIGN_ENABLED = True to bring it back; the sweep below removes any already saved.
# STEPPING CUBES FOR WALT TO DRAG (2026-09-07).
#
# "I am having a hard time putting stair cubes in front of those rock outcroppings in the
# Grok level because the hotel has its own level. Can you put a few cubes in front of the
# outcroppings in the L_SibeliusHotel level? I can nudge them after you do."
#
# Exactly right, and it is why placing them in L_Grok would not have worked: the rocks
# arrive with the hotel tile, so a cube in Grok is in a different level from the thing it
# is meant to climb.
#
# THEY ARE SPAWNED ONCE AND NEVER MOVED AGAIN. This script cannot see the outcroppings, so
# where these land is a guess along the approach line — the point is only that they EXIST,
# somewhere findable, for a person who can see the rocks to drag into place. Every re-run
# after that leaves them exactly where he put them, which is the place_grok_arrival rule:
# a re-run must never throw away a hand-adjusted pose.
#
# Find them in the Outliner as HotelStep_0 .. HotelStep_3. Delete one and it stays deleted.
STEP_CUBES   = 8            # 4 more, asked for once the first four were placed
STEP_SCALE   = unreal.Vector(3, 3, 0.3)     # 300 x 300 x 30 cm
STEP_SPACING = 320.0                        # cm apart along the approach
STEP_RISE    = 90.0                         # cm higher each one

SIGN_ENABLED = False
SIGN_LOC   = unreal.Vector(2100, 200, 600)
SIGN_SIZE  = 90
SIGN_COLOR = unreal.Color(30, 18, 10, 255)

def main():
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError('Stop PIE first.')
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError('Save edited maps first.')
    editor.load_level('/Game/ElvenArchitecture/Scenes/Scene')
    def collect():
        records = []
        for a in actors.get_all_level_actors():
            name = a.get_actor_label()
            mesh = name in ['Scene','Moon','Grass','Flowers','Phoenix','Phoenix2','EntranceFog']
            if not mesh and not isinstance(a,unreal.PointLight):
                continue
            r = dict(name=name, transform=a.get_actor_transform(), mesh=mesh)
            if mesh:
                c=a.static_mesh_component
                r['asset']=c.static_mesh.get_path_name()
                r['materials']=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())]
            else:
                c=a.get_component_by_class(unreal.PointLightComponent)
                r['props']={p:c.get_editor_property(p) for p in ['intensity','attenuation_radius','light_color','intensity_units']}
            records.append(r)
        return records
    records=collect()
    if unreal.EditorAssetLibrary.does_asset_exist(DEST): editor.load_level(DEST)
    else: editor.new_level(DEST)
    existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
    def get(name,cls):
        a=existing.get(name) or actors.spawn_actor_from_class(cls,unreal.Vector())
        a.set_actor_label(name)
        return a
    for r in records:
        a=get(r['name'],unreal.StaticMeshActor if r['mesh'] else unreal.PointLight)
        a.set_actor_transform(r['transform'],False,False)
        if r['mesh']:
            c=a.static_mesh_component
            c.set_static_mesh(unreal.load_asset(r['asset']))
            for i,m in enumerate(r['materials']):
                if m: c.set_material(i,unreal.load_asset(m))
            c.set_collision_profile_name('BlockAll' if r['name']=='Scene' else 'NoCollision')
        else:
            c=a.get_component_by_class(unreal.PointLightComponent)
            c.set_mobility(unreal.ComponentMobility.MOVABLE)
            for p,v in r['props'].items(): c.set_editor_property(p,v)
            c.set_cast_shadows(False)
    ramp=get('HotelApproach',unreal.StaticMeshActor)
    ramp.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    ramp.static_mesh_component.set_collision_profile_name('BlockAll')
    # SWEEP UP LAST RUN'S STAIRCASES.
    #
    # get() only ever creates or moves, so turning STAIRS off would otherwise leave the
    # previous run's flights saved in the tile forever — floating in mid-air, which is
    # exactly what Walt would have seen on the next play. Anything named HotelStair_* was
    # made by this script and is this script's to remove; nothing authored by hand is
    # touched, which is the line that matters.
    stale = [a for a in actors.get_all_level_actors()
             if a.get_actor_label().startswith('HotelStair_')]
    for a in stale:
        actors.destroy_actor(a)
    if stale:
        unreal.log('HOTEL_STAIRS removed %d stale flight(s) from the previous run' % len(stale))

    stair_mesh = unreal.load_asset(STAIRS) if STAIRS else None
    if stair_mesh:
        # SM_Dec_Stair1 measured 474.7 x 147.7 x 278.2 cm — a whole FLIGHT, not one step.
        b = stair_mesh.get_bounds().box_extent
        unreal.log('HOTEL_STAIRMESH %s extent=%s' % (STAIRS, b))
        run  = max(b.x * 2.0 * STAIRS_SCALE, 1.0)
        rise = max(b.z * 2.0 * STAIRS_SCALE, 1.0)

        # THE STONE DEFINES THE SLOPE; THE INVISIBLE RAMP FOLLOWS IT.
        #
        # First attempt sized the ramp from RAMP_RISE/RAMP_RUN (525 over 2400) and the
        # stairs from their own mesh (556 over 949). Two different slopes from the same
        # origin: the walking surface went one way, the stone another, and Walt saw stairs
        # with no floor under them. Whatever the flights actually measure is now the truth,
        # and the ramp and the landing are both derived from it so they cannot disagree.
        steps = max(1, int(math.ceil(RAMP_RISE / rise)))
        span_run  = run  * steps
        span_rise = rise * steps

        for i in range(steps):
            s = get('HotelStair_%d' % i, unreal.StaticMeshActor)
            s.static_mesh_component.set_static_mesh(stair_mesh)
            s.static_mesh_component.set_collision_profile_name('NoCollision')
            s.set_actor_location(unreal.Vector(RAMP_LOC.x + run * i,
                                               RAMP_LOC.y,
                                               RAMP_LOC.z + rise * i), False, False)
            s.set_actor_rotation(unreal.Rotator(pitch=0, yaw=STAIRS_YAW, roll=0), False)
            s.set_actor_scale3d(unreal.Vector(STAIRS_SCALE, STAIRS_SCALE, STAIRS_SCALE))

        ramp.static_mesh_component.set_visibility(False)
        ramp.set_actor_location(unreal.Vector(RAMP_LOC.x, RAMP_LOC.y, RAMP_LOC.z), False, False)
        ramp.set_actor_rotation(unreal.Rotator(pitch=math.degrees(math.atan2(span_rise,span_run)),yaw=0,roll=0),False)
        ramp.set_actor_scale3d(unreal.Vector(math.hypot(span_run,span_rise)/100,
                                             max(b.y*2.0*STAIRS_SCALE,1.0)/100, 0.4))

        LANDING_LOC.x = RAMP_LOC.x + span_run
        LANDING_LOC.z = RAMP_LOC.z + span_rise
        unreal.log('HOTEL_STAIRS placed=%d run=%.1f rise=%.1f span=%.1f/%.1f landing=%s'
                   % (steps, run, rise, span_run, span_rise, LANDING_LOC))
    else:
        ramp.set_actor_location(RAMP_LOC,False,False)
        ramp.set_actor_rotation(unreal.Rotator(pitch=math.degrees(math.atan2(RAMP_RISE,RAMP_RUN)),yaw=0,roll=0),False)
        ramp.set_actor_scale3d(unreal.Vector(math.hypot(RAMP_RUN,RAMP_RISE)/100,RAMP_WIDTH,0.4))
    landing=get('HotelApproachLanding',unreal.StaticMeshActor)
    landing.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    landing.static_mesh_component.set_collision_profile_name('BlockAll')
    landing.set_actor_location(LANDING_LOC,False,False)
    landing.set_actor_scale3d(LANDING_SCALE)

    # CUBES FOR WALT TO DRAG. See STEP_CUBES above for why they are spawned once and then
    # never touched again: this script cannot see the outcroppings, so their first position
    # is a guess and every position after it is his.
    existing_steps = {a.get_actor_label(): a for a in actors.get_all_level_actors()
                      if a.get_actor_label().startswith('HotelStep_')}

    # NEW CUBES ARRIVE BESIDE THE OLD ONES, NOT BACK AT THE ORIGINAL GUESS.
    #
    # The first four were spawned on a line this script picked blind and then dragged into
    # place by hand. Spawning the next four on that same blind line would put them wherever
    # the guess was, not where the staircase is being built — so they continue from the
    # highest-numbered cube that already exists, in the direction the run was going.
    anchor = None
    for i in reversed(range(STEP_CUBES)):
        a = existing_steps.get('HotelStep_%d' % i)
        if a:
            anchor = (i, a.get_actor_location())
            break

    spawned = 0
    for i in range(STEP_CUBES):
        label = 'HotelStep_%d' % i
        if label in existing_steps:
            continue          # already there, wherever he has since put it — leave it alone
        if anchor:
            n = i - anchor[0]
            where = unreal.Vector(anchor[1].x + STEP_SPACING * n,
                                  anchor[1].y,
                                  anchor[1].z + STEP_RISE * n)
        else:
            where = unreal.Vector(RAMP_LOC.x + STEP_SPACING * i,
                                  APPROACH_Y,
                                  RAMP_LOC.z + STEP_RISE * i)
        c = actors.spawn_actor_from_class(unreal.StaticMeshActor, where)
        c.set_actor_label(label)
        c.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
        c.static_mesh_component.set_collision_profile_name('BlockAll')
        c.set_actor_scale3d(STEP_SCALE)
        c.set_mobility(unreal.ComponentMobility.MOVABLE)   # so he can drag it
        spawned += 1
    unreal.log('HOTEL_STEPS spawned=%d kept=%d (drag HotelStep_* into place, then Ctrl+S)'
               % (spawned, STEP_CUBES - spawned))
    if SIGN_ENABLED:
        sign=get('SibeliusHotelSign',unreal.TextRenderActor)
        sign.set_actor_location(SIGN_LOC,False,False)
        sign.set_actor_rotation(unreal.Rotator(pitch=0,yaw=180,roll=0),False)
        text=sign.get_component_by_class(unreal.TextRenderComponent)
        text.set_text('SIBELIUS HOTEL')
        text.set_world_size(SIGN_SIZE)
        text.set_text_render_color(SIGN_COLOR)
        text.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    else:
        # Sweep the one already saved in the tile, the same way the stale staircases go.
        # Turning a thing off in a generator has to REMOVE it, or the last run's copy
        # simply lives on forever and the setting looks broken.
        for a in [x for x in actors.get_all_level_actors()
                  if x.get_actor_label() == 'SibeliusHotelSign']:
            actors.destroy_actor(a)
            unreal.log('HOTEL_SIGN removed')

    # MEASURE WHAT THE SLAB IS CUTTING. The statues were composed by their artist without
    # a grey cube in the scene; the landing overlaps them and shears the wings. These lines
    # are the numbers needed to move it clear, printed rather than guessed at.
    origin,extent = landing.get_actor_bounds(False)
    unreal.log('HOTEL_LANDING centre=%s extent=%s' % (origin,extent))
    for a in actors.get_all_level_actors():
        if a.get_actor_label().startswith('Phoenix'):
            o,e = a.get_actor_bounds(False)
            unreal.log('HOTEL_PHOENIX %s centre=%s extent=%s' % (a.get_actor_label(),o,e))

    editor.save_current_level()
    unreal.log('SIBELIUS_HOTEL_TILE_OK actors='+str(len(records)+2))

    # AND LEAVE HIM IN GROK, NOT STANDING IN THE TILE.
    #
    # This used to finish with L_SibeliusHotel_Generated open. That tile is architecture
    # only — no landscape, no sky, no PlayerStart — because it is streamed additively INTO
    # Grok and never entered on its own. Walt pressed Play on it twice and fell into
    # blackness twice, then asked where the Grok terrain had gone. It had not gone
    # anywhere; he was never in Grok.
    #
    # A warning in the log did not help, and could not: nobody reads a log before pressing
    # Play. The fix is to make the wrong thing impossible rather than documented, so the
    # script hands the editor back with the level you actually want to play already open.
    if unreal.EditorAssetLibrary.does_asset_exist(GROK):
        editor.load_level(GROK)
        unreal.log('SIBELIUS_HOTEL: %s is open. Press Play, hear Nyra out, then G.' % GROK)
    else:
        unreal.log_warning('SIBELIUS_HOTEL: %s not found - the hotel tile is still the open '
                           'level, and it is NOT playable on its own (no PlayerStart, no '
                           'ground). Open Grok before pressing Play.' % GROK)

main()
