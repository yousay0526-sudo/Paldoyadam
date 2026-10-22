"""Run inside Blender after reopening the saved study, with --python-exit-code 1."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'art/blender-hero-study'
data=json.loads((OUT/'study.json').read_text(encoding='utf8'))
rig=bpy.data.objects['RIG_hero_study']
assert len(rig.data.bones)==19
assert bpy.context.scene.frame_end==24
assert bpy.context.scene.camera.name=='CAM_approved_front'
assert len([im for im in bpy.data.images if im.packed_file])==5
assert all(im.packed_file for im in bpy.data.images if im.source=='FILE')
assert all(im.filepath.startswith('//') for im in bpy.data.images if im.source=='FILE')
assert rig.animation_data.action.name=='Walk_24frames_opposed_stride'
mesh_objects=[ob for ob in bpy.context.scene.objects if ob.type=='MESH']
assert len(mesh_objects)==21
assert bpy.data.objects['WEAPON_Right_hand_hwando'].vertex_groups.get('hand.R')
assert bpy.data.objects['HAND.L'].vertex_groups.get('hand.L')
errors=[];nonfinite=[]
for row in data['contacts']:
    bpy.context.scene.frame_set(row['frame']);bpy.context.view_layer.update()
    actual=rig.matrix_world@rig.pose.bones['foot.'+row['side']].head
    errors.append((actual-Vector(row['ankle'])).length)
    for ob in mesh_objects:
        if any(not math.isfinite(value) for row in ob.matrix_world for value in row):nonfinite.append(ob.name)
result=dict(reopened_blend=True,packed_images=5,meshes=21,bones=19,foot_target_max_error=max(errors),nonfinite_objects=nonfinite)
(OUT/'validation.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
assert not nonfinite
assert max(errors)<.002,('Foot targets drifted from authored contact positions',max(errors))
print('STUDY_VALIDATED',json.dumps(result),flush=True)
