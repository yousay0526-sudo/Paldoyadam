"""Still-art asset checks. Requires Pillow/numpy, as does the preparation tool."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import numpy as np
from PIL import Image,ImageChops

ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'web/equipment-lab/assets'
CAT=json.loads((ASSETS/'catalog.json').read_text(encoding='utf8'))

def expanded(part):
    image=Image.new('RGBA',(1254,1254))
    image.alpha_composite(Image.open(ASSETS/part['file']).convert('RGBA'),(part['x'],part['y']))
    return image

class EquipmentLabTests(unittest.TestCase):
    def test_alpha_and_source_pixels(self):
        parts=[(CAT['body'],ROOT/'art/equipment-lab/body-master.png')]
        for slot,items in CAT['slots'].items():
            for item in items:
                style=item['id'].split('-')[0]
                raw=ROOT/'art/hero/warrior-clarity-master.png' if style=='general' else ROOT/f'art/equipment-lab/{style}-master.png'
                source=ROOT/f'art/equipment-lab/{style}-clean-master.png' if slot=='upper' else ROOT/f'art/equipment-lab/{style}-lower-master.png' if slot=='lower' else raw
                parts.extend((part,source) for part in item['layers'])
        for part,source in parts:
            with self.subTest(part=part['file']):
                im=Image.open(ASSETS/part['file']);self.assertEqual(im.mode,'RGBA')
                arr=np.asarray(im);self.assertGreaterEqual(arr[:,:,3].max(),250)
                # Real empty pixels, not a checkerboard baked into an RGB PNG.
                self.assertGreater((arr[:,:,3]==0).sum(),100)
                src=np.asarray(Image.open(source).convert('RGBA'))[part['y']:part['y']+part['height'],part['x']:part['x']+part['width']]
                visible=arr[:,:,3]>0
                self.assertTrue(np.array_equal(arr[:,:,:3][visible],src[:,:,:3][visible]))

    def test_equipped_body_has_no_underclothes_below_the_neck(self):
        body=expanded(CAT['body']).getchannel('A')
        # All upper/lower variants use these shared body masks. This catches
        # the former exposed white elbows/waist independently of gear colour.
        for slot in ['upper','lower']:
            mask=expanded(CAT['erase'][slot]).getchannel('A')
            body=ImageChops.multiply(body,ImageChops.invert(mask))
        self.assertEqual(body.crop((0,270,1254,1254)).getbbox(),None)

    def test_upper_keeps_complete_coat_instead_of_stopping_at_waist(self):
        samples={'general':[(500,900),(750,950)],'light':[(530,660),(680,690)],'heavy':[(550,900),(750,925)]}
        for item in CAT['slots']['upper']:
            style=item['id'].split('-')[0];alpha=expanded(item['layers'][0]).getchannel('A')
            for point in samples[style]:self.assertGreater(alpha.getpixel(point),200,(style,point))
            self.assertGreater(alpha.getpixel((860,655)),200,(style,'cuff must not be cut at jacket hem'))
            for point in [(560,1070),(800,1140)]:self.assertEqual(alpha.getpixel(point),0,(style,'boots belong to lower',point))

    def test_optional_web_tool_is_copied_without_game_assets(self):
        spec=importlib.util.spec_from_file_location('build_web',ROOT/'scripts/build_web.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        # Keep temporary output in the writable checkout on sandboxed Windows.
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            out=Path(folder);module.copy_equipment_lab(out)
            self.assertEqual((out/'equipment-lab/assets/catalog.json').read_bytes(),(ASSETS/'catalog.json').read_bytes())
            self.assertTrue((out/'equipment-lab/lab.mjs').exists())
            self.assertFalse((out/'assets').exists())

if __name__=='__main__': unittest.main()
