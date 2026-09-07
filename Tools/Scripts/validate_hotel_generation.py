"""Run in Grok PIE after PreviewHotel: validates same-world reveal and approach clearance."""
import math
import unreal

def main():
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world and unreal.GameplayStatics.get_current_level_name(world,True)=='L_Grok'
    subsystem=unreal.find_object(world,'GrokEndingSubsystem_0')
    status=subsystem.hotel_status()
    assert 'generated=1' in status and 'visible=1' in status, status
    mesh_actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor)
    hotels=[a for a in mesh_actors if a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_name()=='SM_Scene']
    assert len(hotels)==1, 'Hotel duplicated or absent'
    angle=math.radians(294.6)
    c,s=math.cos(angle),math.sin(angle)
    entrance=unreal.Vector(193891.4+4000*c,149016+4000*s,-1000)
    def point(x,y,z):
        return entrance+unreal.Vector((x-1900)*c-(y-500)*s,(x-1900)*s+(y-500)*c,z-275.36388)
    blocked=[]
    for x in range(-1000,1751,50):
        z=min(275,-250+(x+1000)*525/2400)
        next_z=min(275,-250+(x+1050)*525/2400)
        hit=unreal.SystemLibrary.capsule_trace_single(world,point(x,300,z+110),point(x+50,300,next_z+110),42,96,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True)
        if hit: blocked.append(x)
    print('HOTEL_STATE',status)
    print('APPROACH_BLOCKED_SAMPLES',blocked)
    assert not blocked, 'Approach has a capsule obstruction'
    print('HOTEL_GENERATION_VALIDATED: same Grok world, one hotel, clear approach to forecourt')

main()
