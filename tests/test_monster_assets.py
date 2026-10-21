"""Independent checks of the committed art candidates; requires Pillow."""
from pathlib import Path
import hashlib
import json
import unittest
from PIL import Image,ImageChops,ImageOps

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/monsters'
SPECS={'boar-idle':8,'boar-walk':32,'boar-attack':32,
       'butcher-idle':8,'butcher-walk':32,'butcher-attack':32,'butcher-skill':32}

class MonsterAssets(unittest.TestCase):
    def test_installed_runtime_contract(self):
        runtime=json.loads((ART/'runtime.json').read_text(encoding='utf-8'))
        report=json.loads((ART/'preparation.json').read_text(encoding='utf-8'))
        names=['boar-idle','boar-walk','boar-attack','butcher-idle','butcher-walk','butcher-skill']
        self.assertEqual([e['name'] for e in runtime['installed']],names)
        header=(ROOT/'src/monster_atlas_data.h').read_text(encoding='utf-8')
        for entry in runtime['installed']:
            name=entry['name'];path=ROOT/entry['path'];source=ART/'prepared'/path.name
            self.assertEqual(path.read_bytes(),source.read_bytes())
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),entry['sha256'])
            frames=report['atlases'][name]['frames']
            scales=[f"{f['draw_scale_compensation']:.9f}f" for f in frames[::1 if name.endswith('idle') else 4]]
            self.assertIn('{'+', '.join(scales)+'}, /* '+name+' */',header)
        self.assertFalse((ROOT/'assets/monsters/butcher-attack-8dir-v1.png').exists())

    def test_reviewed_sources_and_cells(self):
        report=json.loads((ART/'preparation.json').read_text(encoding='utf-8'))
        self.assertEqual(report['direction_order'],['n','ne','e','se','s','sw','w','nw'])
        self.assertEqual(set(report['atlases']),set(SPECS))
        seen_sources=set();seen_atlases=set();poses=0
        for name,count in SPECS.items():
            entry=report['atlases'][name]
            path=ART/'prepared'/f'{name}-8dir-v1.png'
            digest=hashlib.sha256(path.read_bytes()).hexdigest()
            self.assertEqual(digest,entry['runtime_sha256']);self.assertNotIn(digest,seen_atlases);seen_atlases.add(digest)
            im=Image.open(path);self.assertEqual(im.mode,'RGBA')
            self.assertEqual(im.size,(768,384 if count==8 else 1536))
            self.assertEqual(len(entry['frames']),count)
            cells=[]
            for i in range(count):
                col,row=(i%4,i//4)
                cell=im.crop((col*192,row*192,(col+1)*192,(row+1)*192))
                alpha=cell.getchannel('A');self.assertGreater(alpha.histogram()[0]/(192*192),.25)
                self.assertEqual(alpha.getextrema()[0],0);self.assertGreater(alpha.getextrema()[1],128)
                for border in [(0,0,192,4),(0,188,192,192),(0,0,4,192),(188,0,192,192)]:
                    self.assertEqual(alpha.crop(border).getextrema(),(0,0))
                self.assertEqual(hashlib.sha256(cell.tobytes()).hexdigest(),entry['frames'][i]['cell_sha256'])
                self.assertFalse(entry['frames'][i]['mirrored']);cells.append(cell);poses+=1
            for d in range(8):
                view=cells[d:d+1] if count==8 else cells[d*4:(d+1)*4]
                self.assertEqual(len({c.tobytes() for c in view}),len(view))
            # Opposite views cannot be literal reflected copies of the same art.
            for d in range(4):
                a=cells[d] if count==8 else cells[d*4]
                b=cells[d+4] if count==8 else cells[(d+4)*4]
                self.assertIsNotNone(ImageChops.difference(a,ImageOps.mirror(b)).getbbox())
            for source in entry['sources']:
                path=ROOT/source['path'];self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),source['sha256'])
                master=Image.open(path);self.assertEqual(master.mode,'RGBA');self.assertEqual(list(master.size),source['size'])
                self.assertEqual(master.getchannel('A').getextrema()[0],0)
                seen_sources.add(source['path'])
        self.assertEqual(poses,176);self.assertEqual(len(seen_sources),12)

if __name__=='__main__':unittest.main()
