"""Blender-only front-projection/volume study, not a production eight-view actor.

Run with Blender --background --factory-startup --disable-autoexec --python
this_file.py -- --human-bundle /path/to/human_base_meshes_bundle.blend.
The packed .blend is editable and self-contained. No game files are replaced.
"""
import argparse,hashlib,json,math,os,runpy,sys
from pathlib import Path
import bpy,bmesh
from mathutils import Vector,Matrix
from bpy_extras.object_utils import world_to_camera_view

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'art/blender-hero-study'
args=argparse.ArgumentParser()
args.add_argument('--human-bundle',type=Path,required=True)
args.add_argument('--render-walk',action='store_true',help='Compatibility flag; rendering now includes the full walk by default')
args.add_argument('--no-render',action='store_true')
options=args.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
options.human_bundle=(ROOT/options.human_bundle).resolve()
OUT.mkdir(parents=True,exist_ok=True)
RENDER=ROOT/'build-blender-study/renders';RENDER.mkdir(parents=True,exist_ok=True)

bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
bpy.context.preferences.filepaths.save_version=0
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=12
scene.render.resolution_x=768;scene.render.resolution_y=768;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
scene.render.film_transparent=True;scene.render.fps=24
scene.view_settings.view_transform='Standard';scene.view_settings.look='None'
scene.view_settings.exposure=0;scene.view_settings.gamma=1
scene.world.color=(.18,.18,.18)

# The camera matches the supplied three-quarter illustration framing. Depth
# cannot be recovered from one picture: every depth below is a study estimate.
THETA=math.radians(18);UP=Vector((0,math.sin(THETA),math.cos(THETA)))
FORWARD=Vector((0,math.cos(THETA),-math.sin(THETA)))
CENTER=Vector((0,0,.965));SCALE=630
def point(u,v,d=0):return CENTER+Vector(((u-640)/SCALE,0,0))+UP*((627-v)/SCALE)+FORWARD*d

cam_data=bpy.data.cameras.new('Reference-matched orthographic camera')
camera=bpy.data.objects.new('CAM_approved_front',cam_data);scene.collection.objects.link(camera)
camera.location=point(627,627)-FORWARD*6;camera.rotation_euler=FORWARD.to_track_quat('-Z','Y').to_euler()
cam_data.type='ORTHO';cam_data.ortho_scale=1254/SCALE*1.06;scene.camera=camera
camera_base=camera.location.copy()
diagnostic_camera=camera.copy();diagnostic_camera.data=camera.data.copy()
diagnostic_camera.name='CAM_side_diagnostic';diagnostic_camera.animation_data_clear()
scene.collection.objects.link(diagnostic_camera)
diagnostic_camera.location=CENTER+Vector((4,-4,1.5))
diagnostic_camera.rotation_euler=(CENTER-diagnostic_camera.location).to_track_quat('-Z','Y').to_euler()
mechanics_camera=diagnostic_camera.copy();mechanics_camera.data=diagnostic_camera.data.copy()
mechanics_camera.name='CAM_walk_mechanics';scene.collection.objects.link(mechanics_camera)
mechanics_camera.data.ortho_scale=2.75
mechanics_camera.location=CENTER+Vector((4,-1.2,1.1))
mechanics_camera.rotation_euler=(CENTER-mechanics_camera.location).to_track_quat('-Z','Y').to_euler()

def solid(name,color):
    mat=bpy.data.materials.new(name);mat.diffuse_color=(*color,1);mat.use_nodes=True
    node=mat.node_tree.nodes.get('Principled BSDF');node.inputs['Base Color'].default_value=(*color,1)
    node.inputs['Roughness'].default_value=.78
    return mat
navy=solid('Unpainted reverse - navy',(.022,.038,.070))
skin=solid('Skin volume',(.46,.25,.13));gold=solid('Aged gold volume',(.45,.27,.075))
red=solid('Red cloth reverse',(.32,.016,.029));clay=solid('Diagnostic clay',(.30,.34,.35))
clay.use_fake_user=True
transparent=bpy.data.materials.new('Body hidden below complete equipment');transparent.use_nodes=True
n=transparent.node_tree.nodes;n.clear();a=n.new('ShaderNodeBsdfTransparent');o=n.new('ShaderNodeOutputMaterial')
transparent.node_tree.links.new(a.outputs[0],o.inputs['Surface'])

def painted(name,path):
    mat=bpy.data.materials.new(name);mat.use_nodes=True;n=mat.node_tree.nodes;n.clear();links=mat.node_tree.links
    image=bpy.data.images.load(str(path),check_existing=True);image.pack()
    tex=n.new('ShaderNodeTexImage');tex.image=image;tex.interpolation='Linear';tex.extension='CLIP'
    emit=n.new('ShaderNodeEmission');links.new(tex.outputs['Color'],emit.inputs['Color'])
    clear=n.new('ShaderNodeBsdfTransparent');mix=n.new('ShaderNodeMixShader')
    links.new(tex.outputs['Alpha'],mix.inputs[0]);links.new(clear.outputs[0],mix.inputs[1]);links.new(emit.outputs[0],mix.inputs[2])
    out=n.new('ShaderNodeOutputMaterial');links.new(mix.outputs[0],out.inputs['Surface'])
    return mat
mats={slot:painted('Approved art projection - '+slot,OUT/f'textures/{slot}.png') for slot in ['upper','lower','belt','weapon']}
mats['skin']=painted('Approved face and hands - projection only',ROOT/'art/hero/warrior-clarity-master.png')

# Bone rest positions are authored against the common source camera. Anatomy
# uses the official CC0 human mesh; garment volume is a first-pass estimate.
targets={
 'root':(Vector((0,0,0)),Vector((0,0,.2)),None),
 'pelvis':(point(640,580,.25),point(640,505,.12),'root'),
 'chest':(point(640,505,.12),point(640,260,.02),'pelvis'),
 'neck':(point(640,260,.02),point(640,235,0),'chest'),
 'head':(point(640,235,0),point(640,105,0),'neck'),
}
for side,coords in {'R':[(490,280,.0),(420,445,.02),(390,590,-.22),(390,630,-.24),(580,580,.25),(550,900,.45),(550,1100,.52),(510,1155,.52)],
                    'L':[(790,295,.04),(866,475,.02),(862,678,-.22),(853,716,-.24),(720,590,.25),(795,930,.10),(805,1170,.15),(810,1225,.15)]}.items():
    q=[point(*p) for p in coords]
    for name,head,tail,parent in [('upper_arm',q[0],q[1],'chest'),('forearm',q[1],q[2],'upper_arm.'+side),('hand',q[2],q[3],'forearm.'+side),('thigh',q[4],q[5],'pelvis'),('shin',q[5],q[6],'thigh.'+side),('foot',q[6],q[7],'shin.'+side),('coat',point(580 if side=='R' else 720,530,.0),point(540 if side=='R' else 760,990,.0),'pelvis')]:
        targets[name+'.'+side]=(head,tail,parent)
rig_data=bpy.data.armatures.new('Adult common skeleton')
rig=bpy.data.objects.new('RIG_hero_study',rig_data);scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active=rig;rig.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
for name,(head,tail,parent) in targets.items():
    bone=rig_data.edit_bones.new(name);bone.head=head;bone.tail=tail
    if parent:bone.parent=rig_data.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT');rig.show_in_front=True;rig.select_set(False)
for p in rig.pose.bones:p.rotation_mode='QUATERNION'
meshes=[]

def source_uv(p):
    return Vector(((640+p.x*SCALE)/1254,1-(627-(p-CENTER).dot(UP)*SCALE)/1254))

def uv_project(ob):
    layer=ob.data.uv_layers.new(name='Fixed approved-camera projection')
    for candidate in ob.data.uv_layers:candidate.active_render=candidate==layer
    ob.data.uv_layers.active=layer
    for poly in ob.data.polygons:
        for loop in poly.loop_indices:
            p=ob.matrix_world@ob.data.vertices[ob.data.loops[loop].vertex_index].co
            layer.data[loop].uv=source_uv(p)

back_cache={}
def masked_reverse(front,back):
    key=(front.name,back.name)
    if key in back_cache:return back_cache[key]
    mat=back.copy();mat.name=back.name+' / source silhouette '+front.name
    nodes=mat.node_tree.nodes;links=mat.node_tree.links
    image=next(n.image for n in front.node_tree.nodes if n.type=='TEX_IMAGE')
    tex=nodes.new('ShaderNodeTexImage');tex.image=image;tex.extension='CLIP'
    clear=nodes.new('ShaderNodeBsdfTransparent');mix=nodes.new('ShaderNodeMixShader')
    surface=nodes.get('Principled BSDF');output=nodes.get('Material Output')
    links.new(tex.outputs['Alpha'],mix.inputs[0]);links.new(clear.outputs[0],mix.inputs[1]);links.new(surface.outputs[0],mix.inputs[2]);links.new(mix.outputs[0],output.inputs['Surface'])
    back_cache[key]=mat;return mat

def weights(ob,rows):
    for index,w in enumerate(rows):
        for bone,value in w.items():
            group=ob.vertex_groups.get(bone) or ob.vertex_groups.new(name=bone)
            group.add([index],value,'REPLACE')
    mod=ob.modifiers.new('Common skinning','ARMATURE');mod.object=rig
    ob.parent=rig;meshes.append(ob)

def geometry(name,verts,faces,front,back,bone,front_flags=None):
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.recalc_face_normals(bm,faces=bm.faces);bm.to_mesh(mesh);bm.free()
    ob=bpy.data.objects.new(name,mesh);scene.collection.objects.link(ob)
    mesh.materials.append(front);mesh.materials.append(masked_reverse(front,back))
    for i,p in enumerate(mesh.polygons):
        p.use_smooth=True;p.material_index=0 if front_flags is None or front_flags[i] else 1
    uv_project(ob)
    rows=[]
    for v in mesh.vertices:
        if callable(bone):rows.append(bone(v.co))
        else:rows.append({bone:1.0})
    weights(ob,rows)
    return ob

def loft(name,rows,depth,bone,material='upper',back=navy,center_depth=0,N=32):
    verts=[];faces=[];flags=[]
    for v,left,right in rows:
        row_depth=center_depth(v) if callable(center_depth) else center_depth
        for j in range(N):
            phi=2*math.pi*j/N;verts.append(point((left+right)/2+(right-left)/2*math.cos(phi),v,row_depth+depth*math.sin(phi)))
    for i in range(len(rows)-1):
        for j in range(N):
            faces.append((i*N+j,i*N+(j+1)%N,(i+1)*N+(j+1)%N,(i+1)*N+j))
            flags.append(math.sin(2*math.pi*(j+.5)/N)<0)
    faces += [tuple(reversed(range(N))),tuple((len(rows)-1)*N+j for j in range(N))];flags += [True,True]
    return geometry(name,verts,faces,mats[material],back,bone,flags)

def plate(name,outline,depth,bone,material='weapon',back=navy,thickness=.008):
    verts=[point(u,v,depth+z) for z in [-thickness/2,thickness/2] for u,v in outline];n=len(outline)
    # mathutils triangulates a concave polygon without a background card.
    from mathutils.geometry import tessellate_polygon
    polygon=[Vector((u,v,0)) for u,v in outline];tri=tessellate_polygon([polygon])
    index={tuple(p):i for i,p in enumerate(polygon)}
    triangles=[tuple(p if isinstance(p,int) else index[tuple(p)] for p in t) for t in tri]
    faces=list(triangles);flags=[True]*len(faces)
    faces += [tuple(n+p for p in reversed(t)) for t in triangles];flags += [False]*len(triangles)
    for j in range(n):faces.append((j,(j+1)%n,n+(j+1)%n,n+j));flags.append(False)
    return geometry(name,verts,faces,mats[material],back,bone,flags)

def coat_weight(p):
    uv=source_uv(p);v=(1-uv.y)*1254
    strength=max(0,min(1,(v-535)/440));strength=strength*strength*(3-2*strength)
    left=max(0,min(1,(p.x+.15)/.30))
    result={'pelvis':1-strength*.75}
    for side,amount in [('R',1-left),('L',left)]:
        result['thigh.'+side]=strength*.48*amount
        result['coat.'+side]=strength*.27*amount
    return result

def sleeve_weight(side):
    h,k,_=targets['upper_arm.'+side];_,w,_=targets['forearm.'+side]
    axis=(w-h).normalized();elbow=(k-h).dot(axis)
    def skinning(p):
        blend=max(0,min(1,((p-h).dot(axis)-elbow+.09)/.18))
        blend=blend*blend*(3-2*blend)
        return {'upper_arm.'+side:1-blend,'forearm.'+side:blend}
    return skinning

loft('ARMOR_Torso',[(210,560,713),(260,525,755),(330,510,775),(410,515,770),(480,515,758),(535,500,775)],.145,'chest',center_depth=.015)
# Follow the clothing outline below the elbow: the reference hand must not be
# projected on this surface as well as on its separate, wrist-bound volume.
loft('ARMOR_Complete_coat',[(500,510,770),(575,480,795),(650,455,800),(700,450,795),(745,445,805),(780,435,855),(875,410,945),(970,445,915),(1035,500,825)],.20,coat_weight,center_depth=.025)
for side,rows,pauldron,bracer in [
 ('R',[(270,424,548),(350,382,530),(440,347,482),(510,352,452)],[(190,475,535),(240,425,557),(330,385,540),(405,360,508),(435,375,480)],[(445,371,460),(490,359,451),(545,351,435),(594,345,429)]),
 ('L',[(285,750,875),(365,765,915),(450,790,947),(535,812,921)],[(240,760,837),(295,739,880),(375,755,917),(445,776,941),(473,814,915)],[(520,816,915),(580,820,910),(650,815,901),(708,821,897)])]:
    loft('SLEEVE.'+side,rows,.10,sleeve_weight(side),center_depth=.005)
    loft('PAULDRON.'+side,pauldron,.13,lambda p,s=side:{'chest':.35,'upper_arm.'+s:.65},center_depth=-.03)
    loft('BRACER.'+side,bracer,.065,'forearm.'+side,center_depth=-.175)

loft('TROUSERS_BOOT.R',[(570,485,645),(710,470,630),(850,470,628),(920,495,620),(1010,510,612),(1080,485,610),(1135,445,592),(1160,450,550)],.095,'thigh.R','lower',center_depth=lambda v:.25+.27*max(0,min(1,(v-600)/450)))
loft('TROUSERS_BOOT.L',[(570,640,785),(710,645,825),(850,662,855),(950,710,850),(1060,730,854),(1160,750,880),(1220,770,890),(1235,795,870)],.095,'thigh.L','lower',center_depth=lambda v:.25-.10*max(0,min(1,(v-600)/450)))
# Lower vertices bend with the shin/foot, rather than rotating whole legs.
for ob in [o for o in meshes if o.name.startswith('TROUSERS_BOOT')]:
    side=ob.name[-1]
    for g in list(ob.vertex_groups):ob.vertex_groups.remove(g)
    for vert in ob.data.vertices:
        uv=source_uv(vert.co);v=(1-uv.y)*1254
        knee=900 if side=='R' else 930;ankle=1090 if side=='R' else 1150
        shin=max(0,min(1,(v-(knee-65))/130));foot=max(0,min(1,(v-(ankle-35))/90))
        hip=max(0,min(1,(v-540)/140));hip=hip*hip*(3-2*hip)
        data={'pelvis':1-hip,'thigh.'+side:hip*(1-shin),'shin.'+side:hip*shin*(1-foot),'foot.'+side:hip*shin*foot}
        for name,value in data.items():
            if value>0:(ob.vertex_groups.get(name) or ob.vertex_groups.new(name=name)).add([vert.index],value,'REPLACE')

loft('SASH_Wrapped_red_cloth',[(448,510,735),(475,505,735),(510,510,735),(533,530,725)],.17,'pelvis','belt',red,center_depth=-.10)
plate('SASH_Front_tail',[(677,512),(708,520),(716,570),(733,650),(758,731),(781,796),(797,844),(741,891),(714,824),(687,744),(672,656),(662,574)],-.31,'coat.L','belt',red,.005)
loft('SASH_Lion_buckle',[(466,570,612),(482,548,635),(510,552,634),(533,585,610)],.018,'pelvis','belt',gold,center_depth=-.31,N=24)
plate('WEAPON_Right_hand_hwando',[(188,1090),(208,1090),(399,704),(422,677),(425,646),(444,615),(446,583),(428,567),(389,573),(369,597),(362,623),(344,642),(344,668),(360,698),(302,832)],-.30,'hand.R','weapon',gold,.012)
loft('HAND.L',[(672,835,891),(690,807,889),(712,810,881),(733,831,869),(740,840,855)],.032,'hand.L','upper',skin,center_depth=-.235,N=24)

# Load only the official human and its eyeballs. Strip source transforms and
# optional sculpt modifiers; do not execute anything from the source .blend.
with bpy.data.libraries.load(str(options.human_bundle),link=False) as (source,dest):
    names=['GEO-body_male_realistic','GEO-body_male_realistic.eye.R','GEO-body_male_realistic.eye.L','realistic_body_male']
    dest.objects=[name for name in names if name in source.objects]
asset=bpy.data.objects.get('realistic_body_male')
asset_license={k:getattr(asset.asset_data,k,None) for k in ['author','description','license']} if asset and asset.asset_data else {'license':'CC0; official human base meshes bundle'}
if asset and asset.asset_data:assert asset.asset_data.license=='CC0'

source_bones={'pelvis':((0,0,.84),(0,0,1.03)),'chest':((0,0,1.03),(0,0,1.44)), 'neck':((0,0,1.44),(0,0,1.48)), 'head':((0,0,1.48),(0,0,1.68))}
for side,sign in [('R',-1),('L',1)]:
    for name,h,t in [('upper_arm',(.18*sign,0,1.39),(.315*sign,0,1.16)),('forearm',(.315*sign,0,1.16),(.405*sign,0,.96)),('hand',(.405*sign,0,.96),(.425*sign,-.015,.84)),('thigh',(.095*sign,0,.86),(.12*sign,-.015,.46)),('shin',(.12*sign,-.015,.46),(.145*sign,0,.09)),('foot',(.145*sign,0,.09),(.15*sign,-.13,.03))]:source_bones[name+'.'+side]=(h,t)

def segment_distance(p,a,b):
    t=max(0,min(1,(p-a).dot(b-a)/(b-a).length_squared));return (p-(a+(b-a)*t)).length
def map_vertex(p,name):
    h,t=[Vector(q) for q in source_bones[name]];a,b,_=targets[name]
    rotation=(t-h).rotation_difference(b-a)
    return a+rotation@((p-h)*((b-a).length/(t-h).length))

for name in names[:3]:
    body=bpy.data.objects.get(name)
    if body is None:continue
    body.parent=None;body.matrix_world=Matrix.Identity(4)
    for mod in list(body.modifiers):body.modifiers.remove(mod)
    scene.collection.objects.link(body)
    data=[];visible=[]
    for vert in body.data.vertices:
        p=vert.co.copy();rank=sorted((segment_distance(p,Vector(h),Vector(t)),bone) for bone,(h,t) in source_bones.items())[:3]
        scores=[1/(distance+.012)**5 for distance,bone in rank];total=sum(scores);w={rank[i][1]:scores[i]/total for i in range(len(rank))}
        new=sum((map_vertex(p,bone)*value for bone,value in w.items()),Vector())
        if p.z>1.48:new.x*=1.16
        vert.co=new;data.append(w);visible.append(p.z>1.45)
    body.data.materials.clear();body.data.materials.append(mats['skin']);body.data.materials.append(transparent)
    for face in body.data.polygons:
        face.material_index=0 if all(visible[v] for v in face.vertices) else 1;face.use_smooth=True
    uv_project(body);weights(body,data);body.name='BODY_CC0_adult' if name==names[0] else 'EYE.'+name[-1]

loft('HAIR_Volume',[(65,605,673),(100,567,707),(140,550,718),(185,570,705),(205,588,690)],.075,'head','skin',navy,center_depth=-.055)
loft('HAIR_Topknot',[(0,628,650),(15,610,669),(35,609,672),(58,630,650)],.045,'head','skin',navy,center_depth=-.015,N=24)
plate('HAIR_Gold_pin',[(544,37),(564,31),(600,38),(703,39),(724,44),(722,48),(602,47),(562,47)],-.13,'head','skin',gold,.008)

# Light sources matter in the clay diagnosis; the front paint itself remains
# emission-based, with its original illustration shading visibly baked in.
for name,loc,power,size in [('Key',(-3,-4,5),700,4),('Fill',(3,-1,3),350,3)]:
    light=bpy.data.lights.new(name,'AREA');light.energy=power;light.shape='DISK';light.size=size
    ob=bpy.data.objects.new(name,light);scene.collection.objects.link(ob);ob.location=loc;ob.rotation_euler=(CENTER-ob.location).to_track_quat('-Z','Y').to_euler()

def matrix_for(name,head,tail):
    rh,rt,_=targets[name];rotation=(rt-rh).rotation_difference(tail-head).to_matrix().to_4x4()
    return Matrix.Translation(head)@rotation@Matrix.Translation(-rh)@rig_data.bones[name].matrix_local
def solve_knee(hip,ankle,rest_h,rest_k,rest_a):
    a=(rest_k-rest_h).length;b=(rest_a-rest_k).length;axis=ankle-hip;length=axis.length;axis.normalize()
    assert abs(a-b)+.0001<length<a+b-.0001,('Unreachable leg target',length,a+b)
    d=(a*a-b*b+length*length)/(2*length);height=math.sqrt(max(0,a*a-d*d))
    # Both knees bend toward the walking direction, never from the asymmetric
    # illustration's accidental depth estimate (which bent one knee backward).
    pole=Vector((0,-1,0));pole-=axis*pole.dot(axis)
    return hip+axis*d+pole.normalized()*height

FRAMES=24;STRIDE=.72;STANCE=.60
def foot_path(q):
    front=STRIDE*STANCE/2;back=-front
    if q<STANCE:return front-STRIDE*q,0,0
    u=(q-STANCE)/(1-STANCE)
    # Hermite tangents give zero world velocity at lift-off and landing.
    relative=back+(front-back)*(3*u*u-2*u*u*u)-STRIDE*(1-STANCE)*(2*u*u*u-3*u*u+u)
    return relative,.080*math.sin(math.pi*u)**2,-math.radians(10)*math.sin(math.pi*u)

def rotate_from_parent(name,angle,axis='X'):
    bone=rig.pose.bones[name];h,_,parent=targets[name]
    deform=rig.pose.bones[parent].matrix@rig_data.bones[parent].matrix_local.inverted()
    bone.matrix=deform@Matrix.Translation(h)@Matrix.Rotation(angle,4,axis)@Matrix.Translation(-h)@rig_data.bones[name].matrix_local
    bpy.context.view_layer.update()

rig.animation_data_create();action=bpy.data.actions.new('Walk_24frames_opposed_stride');rig.animation_data.action=action
contact=[]
for frame in range(1,FRAMES+2):
    scene.frame_set(frame)
    t=(frame-1)/FRAMES;phase=t*2*math.pi
    root_delta=Vector((.009*math.sin(phase),-STRIDE*t,-.055+.002*(1-math.cos(phase*2))))
    for name,(h,tail,parent) in targets.items():
        rig.pose.bones[name].matrix=Matrix.Translation(root_delta)@rig_data.bones[name].matrix_local
        bpy.context.view_layer.update()
    bpy.context.view_layer.update()
    rotate_from_parent('pelvis',math.radians(2.5)*math.sin(phase),'Z')
    rotate_from_parent('chest',-math.radians(4)*math.sin(phase),'Z')
    for side,offset in [('R',0),('L',.5)]:
        ph=(t+offset)%1
        relative,lift,pitch=foot_path(ph)
        h,k,_=targets['thigh.'+side];a,toe,_=targets['foot.'+side]
        ankle=Vector((a.x,h.y-.03-STRIDE*t-relative,a.z+lift))
        pelvis_deform=rig.pose.bones['pelvis'].matrix@rig_data.bones['pelvis'].matrix_local.inverted()
        hip=pelvis_deform@h;knee=solve_knee(hip,ankle,h,k,a)
        for name,head,tail in [('thigh.'+side,hip,knee),('shin.'+side,knee,ankle),('foot.'+side,ankle,ankle+Matrix.Rotation(pitch,3,'X')@(toe-a))]:
            rig.pose.bones[name].matrix=matrix_for(name,head,tail)
            bpy.context.view_layer.update()
        contact.append(dict(frame=frame,side=side,phase=ph,stance=ph<STANCE,ankle=list(ankle)))
    for side,offset in [('R',0),('L',.5)]:
        arm_phase=phase+offset*2*math.pi
        rotate_from_parent('upper_arm.'+side,math.radians(21 if side=='R' else 24)*math.cos(arm_phase))
        rotate_from_parent('forearm.'+side,-math.radians(5+5*(1-math.cos(arm_phase))))
        # The hem follows its thigh through skin weights, plus a smaller delayed
        # swing. Its waist stays on the pelvis; sleeves bend at the elbow.
        rotate_from_parent('coat.'+side,-math.radians(6)*math.cos(arm_phase-.25))
    bpy.context.view_layer.update()
    for bone in rig.pose.bones:
        for path in ['location','rotation_quaternion','scale']:bone.keyframe_insert(data_path=path,frame=frame,group=bone.name)
    camera.location=camera_base+Vector((0,-STRIDE*t,0));camera.keyframe_insert(data_path='location',frame=frame)

# Keep an explicit unanimated reference pose, outside the walking playback.
scene.frame_set(0)
for bone in rig.pose.bones:
    bone.matrix_basis=Matrix.Identity(4)
    for path in ['location','rotation_quaternion','scale']:bone.keyframe_insert(data_path=path,frame=0,group=bone.name)
camera.location=camera_base;camera.keyframe_insert(data_path='location',frame=0)
for curve in action.fcurves:
    for key in curve.keyframe_points:key.interpolation='LINEAR'
scene.frame_start=1;scene.frame_end=FRAMES;scene.frame_set(1)
rig['study_limitations']='Front-projected reference paint; reverse surfaces intentionally unpainted. Not an eight-direction production character. Cloth, hidden textures and hand grips need artist correction.'
scene['source_art']='Approved warrior-clarity-master and PR40 still equipment textures'
scene['render_provenance']='Actual Blender Cycles CPU renders; no AI-generated render preview'
for ob in meshes:
    for modifier in ob.modifiers:
        if modifier.type=='ARMATURE':modifier.use_deform_preserve_volume=True
for image in bpy.data.images:
    if image.source=='FILE':
        if not image.packed_file:image.pack()
        image.filepath_raw='//'+os.path.relpath(bpy.path.abspath(image.filepath),OUT).replace('\\','/')
bpy.context.view_layer.objects.active=rig
for ob in bpy.context.selected_objects:ob.select_set(False)
rig.select_set(True)
for area in bpy.context.screen.areas if bpy.context.screen else []:
    if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA'
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'hero-front-study.blend'),compress=True)

stats=dict(blender=bpy.app.version_string,source_license=asset_license,source_mesh_bundle_sha256=hashlib.sha256(options.human_bundle.read_bytes()).hexdigest(),
    objects=[dict(name=o.name,vertices=len(o.data.vertices),faces=len(o.data.polygons),depth=round(float(o.dimensions.y),5)) for o in meshes],
    bones=len(rig_data.bones),frame_start=1,frame_end=FRAMES,fps=24,stride_m=STRIDE,stance_fraction=STANCE,packed_images=[im.name for im in bpy.data.images if im.packed_file],
    projection_only=True,game_assets_modified=False,contacts=contact)
(OUT/'study.json').write_text(json.dumps(stats,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
if options.no_render:
    print('STUDY_GENERATED_WITHOUT_RENDER',flush=True)
    sys.exit(0)
runpy.run_path(str(ROOT/'scripts/render_blender_hero_study.py'),run_name='__main__')
print('STUDY_READY',json.dumps({'blend':str(OUT/'hero-front-study.blend'),'objects':len(meshes),'bones':len(rig_data.bones)}),flush=True)
