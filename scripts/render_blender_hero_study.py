"""Render an already saved/packed .blend; does not rebuild or modify it."""
import bpy
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build-blender-study/renders';OUT.mkdir(parents=True,exist_ok=True)
s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=12
s.render.image_settings.file_format='PNG';s.render.image_settings.color_mode='RGBA'
s.render.film_transparent=True;s.render.resolution_percentage=100
s.render.resolution_x=768;s.render.resolution_y=768
s.camera=bpy.data.objects['CAM_approved_front'];s.frame_set(1)
s.render.filepath=str(OUT/'front.png');bpy.ops.render.render(write_still=True)
s.view_layers[0].material_override=bpy.data.materials['Diagnostic clay']
s.render.filepath=str(OUT/'clay-front.png');bpy.ops.render.render(write_still=True)
s.view_layers[0].material_override=None;s.camera=bpy.data.objects['CAM_side_diagnostic']
s.render.filepath=str(OUT/'side-diagnostic.png');bpy.ops.render.render(write_still=True)
s.camera=bpy.data.objects['CAM_approved_front'];s.render.resolution_x=512;s.render.resolution_y=512
for frame in range(1,17,2):
    s.frame_set(frame);s.render.filepath=str(OUT/f'walk-{frame:02d}.png');bpy.ops.render.render(write_still=True)
print('SAVED_BLEND_RENDERED',flush=True)
