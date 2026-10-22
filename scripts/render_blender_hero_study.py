"""Render an already saved/packed .blend; does not rebuild or modify it."""
import bpy,sys
from mathutils import Vector
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build-blender-study/renders';OUT.mkdir(parents=True,exist_ok=True)
original_materials={ob.name:list(ob.data.materials) for ob in bpy.context.scene.objects if ob.type=='MESH'}
def clay(enabled):
    # Preserve intentionally invisible skin faces: a blanket material override
    # would reveal the hidden base's hands through the painted equipment.
    for name,materials in original_materials.items():
        slots=bpy.data.objects[name].data.materials
        for i,material in enumerate(materials):
            slots[i]=bpy.data.materials['Diagnostic clay'] if enabled and material.name!='Body hidden below complete equipment' else material
s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=12
s.render.threads_mode='FIXED';s.render.threads=4
s.render.image_settings.file_format='PNG';s.render.image_settings.color_mode='RGBA'
s.render.film_transparent=True;s.render.resolution_percentage=100
if '--mechanics-only' not in sys.argv:
    s.render.resolution_x=768;s.render.resolution_y=768
    s.camera=bpy.data.objects['CAM_approved_front'];s.frame_set(0)
    s.render.filepath=str(OUT/'front.png');bpy.ops.render.render(write_still=True)
    clay(True)
    s.render.filepath=str(OUT/'clay-front.png');bpy.ops.render.render(write_still=True)
    clay(False);s.camera=bpy.data.objects['CAM_side_diagnostic']
    s.render.filepath=str(OUT/'side-diagnostic.png');bpy.ops.render.render(write_still=True)
    s.camera=bpy.data.objects['CAM_approved_front'];s.render.resolution_x=512;s.render.resolution_y=512
    for frame in range(1,s.frame_end+1):
        s.frame_set(frame);s.render.filepath=str(OUT/f'walk-{frame:02d}.png');bpy.ops.render.render(write_still=True)
s.render.resolution_x=512;s.render.resolution_y=512
s.camera=bpy.data.objects['CAM_walk_mechanics'];base=s.camera.location.copy()
clay(True)
for frame in range(1,s.frame_end+1):
    s.frame_set(frame);s.camera.location=base+Vector((0,-.72*(frame-1)/s.frame_end,0))
    s.render.filepath=str(OUT/f'mechanics-{frame:02d}.png');bpy.ops.render.render(write_still=True)
print('SAVED_BLEND_RENDERED',flush=True)
