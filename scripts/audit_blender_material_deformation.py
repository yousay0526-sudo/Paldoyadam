"""Measure material deformation in the saved study; diagnostic, never art approval."""
import bpy,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
scene=bpy.context.scene
names=['WEAPON_Right_hand_hwando','PAULDRON.R','PAULDRON.L','ARMOR_Complete_coat','SLEEVE.R','SLEEVE.L','TROUSERS_BOOT.R','TROUSERS_BOOT.L']
objects={}
for name in names:
    ob=bpy.data.objects[name]
    objects[name]=dict(vertices=len(ob.data.vertices),faces=len(ob.data.polygons),modifiers=[m.type for m in ob.modifiers],shape_keys=bool(ob.data.shape_keys),edge_ratio_min=1e9,edge_ratio_max=0)
frames=[scene.frame_start+i*.5 for i in range((scene.frame_end-scene.frame_start+1)*2+1)]
for frame in frames:
    scene.frame_set(int(frame),subframe=frame-int(frame));bpy.context.view_layer.update()
    deps=bpy.context.evaluated_depsgraph_get()
    for name in names:
        ob=bpy.data.objects[name];posed=ob.evaluated_get(deps);result=objects[name]
        for edge in ob.data.edges:
            a,b=edge.vertices;rest=(ob.data.vertices[a].co-ob.data.vertices[b].co).length
            if rest<1e-5:continue
            length=((posed.matrix_world@posed.data.vertices[a].co)-(posed.matrix_world@posed.data.vertices[b].co)).length
            ratio=length/rest
            if ratio<result['edge_ratio_min']:result.update(edge_ratio_min=ratio,min_frame=frame)
            if ratio>result['edge_ratio_max']:result.update(edge_ratio_max=ratio,max_frame=frame)
report=dict(art_accepted=False,diagnostic_only=True,source_commit='cf548e6e6f72a757914b97234efddec82d6e64a6',blend_sha256=hashlib.sha256(Path(bpy.data.filepath).read_bytes()).hexdigest(),frames=frames,measurement='Evaluated mesh edge length divided by rest edge length; local extremes, not whole-garment size.',objects=objects)
output=ROOT/'art/blender-hero-study/material-audit.json'
output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print('MATERIAL_AUDIT',json.dumps(objects),flush=True)
