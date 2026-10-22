"""Check evaluated gait mechanics, including half frames; --report-only measures an old study."""
import bpy,json,math,sys
from pathlib import Path
from mathutils import Vector,Quaternion
ROOT=Path(__file__).resolve().parents[1]
scene=bpy.context.scene;rig=bpy.data.objects['RIG_hero_study']
baseline='--report-only' in sys.argv
count=scene.frame_end;stride=.18 if baseline else .72;stance=.5 if baseline else .6
samples=[];joint_error=0;foot_targets={};clothes={};cycle={};knee_forward=[]
for frame in [1+i*.5 for i in range(count*2+1)]:
    scene.frame_set(int(frame),subframe=frame-int(frame));bpy.context.view_layer.update()
    root=rig.pose.bones['root'].head.copy()
    row={'frame':frame,'root':list(root),'chest_z':rig.pose.bones['chest'].tail.z}
    for side,offset in [('R',0),('L',.5)]:
        bones=rig.pose.bones
        ankle=bones['foot.'+side].head.copy();wrist=bones['hand.'+side].head.copy()
        hip=bones['thigh.'+side].head;knee=bones['shin.'+side].head
        axis=(ankle-hip).normalized();bend=knee-hip-axis*(knee-hip).dot(axis)
        knee_forward.append(-bend.y)
        row[side]=dict(ankle=list(ankle),ankle_relative_y=ankle.y-root.y,wrist_relative_y=wrist.y-root.y,phase=((frame-1)/count+offset)%1)
        for parent,child in [('thigh','shin'),('shin','foot'),('upper_arm','forearm'),('forearm','hand')]:
            joint_error=max(joint_error,(bones[parent+'.'+side].tail-bones[child+'.'+side].head).length)
        # Real support contacts, independent of the targets saved by the builder.
        q=row[side]['phase']
        if q<stance-1e-6:
            group=(side,math.floor((frame-1)/count+offset+1e-7))
            foot_targets.setdefault(group,[]).append(ankle)
    if frame in [1,count+1]:
        cycle[frame]={b.name:dict(head=list(b.head-root),rotation=list(b.matrix.to_quaternion())) for b in rig.pose.bones}
    # Inspect evaluated clothing surfaces, not only vertex-group declarations.
    if frame in range(1,count+1):
        deps=bpy.context.evaluated_depsgraph_get()
        pelvis_inverse=rig.pose.bones['pelvis'].matrix.inverted()
        for name in ['SLEEVE.R','SLEEVE.L','TROUSERS_BOOT.R','TROUSERS_BOOT.L','ARMOR_Complete_coat']:
            ob=bpy.data.objects[name];evaluated=ob.evaluated_get(deps)
            indices=[int(len(ob.data.vertices)*f) for f in [.25,.5,.75,.9]]
            clothes.setdefault(name,[]).append([list(pelvis_inverse@(evaluated.matrix_world@evaluated.data.vertices[i].co)) for i in indices])
    samples.append(row)

def span(values):return max(values)-min(values)
def correlation(x,y):
    mx=sum(x)/len(x);my=sum(y)/len(y)
    return sum((a-mx)*(b-my) for a,b in zip(x,y))/math.sqrt(sum((a-mx)**2 for a in x)*sum((b-my)**2 for b in y))
drift=max((p-positions[0]).length for positions in foot_targets.values() for p in positions)
metrics={}
for side in ['R','L']:
    ankle=[s[side]['ankle_relative_y'] for s in samples[:-1]];wrist=[s[side]['wrist_relative_y'] for s in samples[:-1]]
    metrics[side]=dict(ankle_fore_aft_range_m=span(ankle),wrist_fore_aft_range_m=span(wrist),same_side_arm_leg_correlation=correlation(ankle,wrist))
deformation={}
for name,poses in clothes.items():
    deformation[name]=max((Vector(point)-Vector(poses[0][i])).length for pose in poses for i,point in enumerate(pose))
loop_error=max((Vector(cycle[1][name]['head'])-Vector(cycle[count+1][name]['head'])).length for name in cycle[1])
loop_angle=max(Quaternion(cycle[1][name]['rotation']).rotation_difference(Quaternion(cycle[count+1][name]['rotation'])).angle for name in cycle[1])
travel=samples[0]['root'][1]-samples[-1]['root'][1]
result=dict(frame_count=count,half_frame_samples=len(samples),forward_travel_m=travel,minimum_knee_forward_bend_m=min(knee_forward),joint_connection_max_error_m=joint_error,stance_foot_world_drift_m=drift,chest_vertical_range_m=span([s['chest_z'] for s in samples]),cycle_relative_position_error_m=loop_error,cycle_rotation_error_radians=loop_angle,sides=metrics,garment_motion_relative_to_pelvis_m=deformation)
output=ROOT/('build-blender-study/before/walk-validation.json' if baseline else 'art/blender-hero-study/walk-validation.json')
output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
print('EVALUATED_GAIT',json.dumps(result),flush=True)
if not baseline:
    assert joint_error<.002,('Disconnected elbows/knees/ankles',joint_error)
    assert drift<.002,('Support foot slides',drift)
    assert result['chest_vertical_range_m']<.012
    assert loop_error<.002
    assert loop_angle<.002
    assert abs(travel-stride)<.005
    assert min(knee_forward)>.01
    for side,data in metrics.items():
        assert data['ankle_fore_aft_range_m']>.40,(side,data)
        assert data['wrist_fore_aft_range_m']>.25,(side,data)
        assert data['same_side_arm_leg_correlation']<-.65,(side,data)
    for name,amount in deformation.items():assert amount>.035,(name,amount)
    expected={'SLEEVE.R':{'upper_arm.R','forearm.R'},'SLEEVE.L':{'upper_arm.L','forearm.L'},'ARMOR_Complete_coat':{'pelvis','thigh.R','thigh.L','coat.R','coat.L'}}
    for side in ['R','L']:expected['TROUSERS_BOOT.'+side]={'pelvis','thigh.'+side,'shin.'+side,'foot.'+side}
    for name,groups in expected.items():
        ob=bpy.data.objects[name]
        assert groups.issubset({g.name for g in ob.vertex_groups})
        for v in ob.data.vertices:assert abs(sum(g.weight for g in v.groups)-1)<1e-5
    print('GAIT_MECHANICS_PASSED',flush=True)
