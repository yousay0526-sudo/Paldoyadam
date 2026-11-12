"""Phase 4: templates (built-in from the repository and user files) and 자동 보정 (no Qt)."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

from paldo_studio import autofix  # noqa: E402
from paldo_studio.checks import check_asset  # noqa: E402
from paldo_studio.game_specs import find_repo, load_specs  # noqa: E402
from paldo_studio.imaging import compose, measure, open_png  # noqa: E402
from paldo_studio.model import Project  # noqa: E402
from paldo_studio.templates import (Template, asset_from_template, builtin_templates, load_template,  # noqa: E402
                                    project_templates, save_template)

from test_core import Base, png, sha  # noqa: E402

REPO = find_repo(HERE)


def painted(path: Path, size=(64, 64), bg=(250, 250, 245), figure=(30, 60, 140), box=(20, 10, 44, 58)) -> Path:
    """A GPT-style artwork: opaque flat background, a figure in the middle (with a hole of bg colour)."""
    image = Image.new("RGBA", size, bg + (255,))
    image.paste(Image.new("RGBA", (box[2] - box[0], box[3] - box[1]), figure + (255,)), box[:2])
    image.paste(Image.new("RGBA", (4, 4), bg + (255,)), (box[0] + 8, box[1] + 10))  # same colour inside
    image.save(path)
    return path


@unittest.skipIf(REPO is None, "game repository not found")
class BuiltinTemplateTests(unittest.TestCase):
    def setUp(self):
        self.specs = load_specs(REPO)
        self.templates, self.notes = builtin_templates(self.specs)
        self.by_key = {t.key: t for t in self.templates}

    def test_values_come_from_the_repository(self):
        idle = self.by_key["hero-idle-8dir"]
        self.assertEqual((idle.width, idle.anchor, idle.directions, idle.sheet_layout),
                         (192, [96.0, 176.0], self.specs.direction_order, "views_in_cells"))
        walk = self.by_key["hero-walk-8dir"]
        self.assertEqual((walk.frames, walk.sheet_layout, walk.sheet_columns, len(walk.reference)),
                         (8, "per_view", 4, 64))
        attack = self.by_key["hero-attack"]
        self.assertEqual(attack.frame_anchors["5"], list(self.specs.preset("hero-attack-5").anchor))
        boar_walk = self.by_key["monster-boar-walk"]
        self.assertEqual((boar_walk.kind, boar_walk.frames, boar_walk.sheet_layout, boar_walk.anchor),
                         ("monster", 4, "rows_by_view", [96.0, 176.0]))
        self.assertEqual(self.by_key["monster-butcher-idle"].sheet_layout, "views_in_cells")
        self.assertIn("monster-butcher-skill", self.by_key)
        self.assertFalse([t for t in self.templates if t.kind == "npc"])
        self.assertTrue(any("NPC" in n for n in self.notes))
        self.assertEqual(len(self.notes), 1, self.notes)  # only the NPC note: everything else was read

    def test_template_with_reference_rebuilds_runtime_sheets(self):
        """A template's reference cells sit exactly on the runtime cells: turning them into body layers
        exports the same sheet again (and the game file is untouched)."""
        for key, rel in (("monster-boar-attack", "assets/monsters/boar-attack-8dir-v1.png"),
                         ("hero-attack", "assets/warrior-attack-v1.png")):
            with tempfile.TemporaryDirectory() as tmp:
                project = Project("p", root=Path(tmp))
                project.save()
                before = sha(REPO / rel)
                a = asset_from_template(project, self.by_key[key], "x", REPO, with_reference=True)
                cache = autofix.ImageCache()
                self.assertTrue(all(l.slot == "reference" for l in a.layers))
                errors = [r for r in check_asset(project, a, cache, self.specs) if r["level"] == "error"]
                self.assertTrue(errors)  # only reference art: nothing to export yet
                for layer in a.layers:
                    layer.slot = "body"
                from paldo_studio.imaging import build_sheets
                sheet = build_sheets(project, a, cache)[0][1]
                want = np.asarray(open_png(REPO / rel).convert("RGBA")).copy()
                got = np.asarray(sheet).copy()
                want[want[..., 3] == 0] = 0
                self.assertTrue(np.array_equal(got, want), key)
                self.assertEqual(sha(REPO / rel), before)


class UserTemplateTests(Base):
    def test_save_load_and_make_asset(self):
        a = self.project.add_asset("마을 사람", "npc", 160, 200, (80, 190), directions=["S", "E"])
        a.frames, a.sheet_layout, a.sheet_columns, a.fps = 3, "rows_by_view", 3, 6.0
        a.frame_anchors = {"2": [82.0, 188.0]}
        a.tolerances = {"axis": 30.0}
        self.add(a, png(self.dir / "x.png", (10, 10), (255, 0, 0, 255), (0, 0, 10, 10)), view="S")
        t = Template.from_asset(a, "NPC 2방향 걷기")
        path = save_template(t, self.project.root / "templates" / "npc-walk")
        self.assertTrue(path.name.endswith(".paldo-template.json"))
        loaded = project_templates(self.project)
        self.assertEqual([x.label for _, x in loaded], ["NPC 2방향 걷기"])
        b = asset_from_template(self.project, load_template(path), "주막 주인")
        self.assertEqual((b.kind, b.width, b.height, b.anchor, b.directions, b.frames, b.sheet_layout,
                          b.sheet_columns, b.fps, b.tolerances, b.layers),
                         ("npc", 160, 200, [80.0, 190.0], ["S", "E"], 3, "rows_by_view", 3, 6.0, {"axis": 30.0}, []))
        self.assertEqual(b.anchor_for(2), [82.0, 188.0])
        b.frame_anchors["2"][0] = 1  # independent copy
        self.assertEqual(a.frame_anchors["2"][0], 82.0)

    def test_broken_template_files_are_refused_or_skipped(self):
        folder = self.project.root / "templates"
        folder.mkdir()
        (folder / "bad.paldo-template.json").write_text('{"format": "other"}', encoding="utf-8")
        self.assertEqual(project_templates(self.project), [])
        with self.assertRaises(ValueError):
            load_template(folder / "bad.paldo-template.json")


class AutoFixTests(Base):
    def test_background_removal_keeps_inner_same_colour_and_original(self):
        path = painted(self.dir / "gpt.png")
        before = sha(path)
        a = self.project.add_asset("a", "character", 64, 64, (32, 58))
        layer = self.add(a, path)
        copy_before = sha(self.project.source_path(self.project.source(layer.source)))
        result = autofix.remove_background(open_png(path), tolerance=12, feather=False)
        self.assertEqual(result.colour, (250, 250, 245))
        alpha = np.asarray(result.image)[..., 3]
        self.assertEqual(alpha[0, 0], 0)
        self.assertEqual(alpha[20, 30], 255)       # figure
        self.assertEqual(alpha[22, 29], 255)       # bg-coloured hole inside the figure stays
        self.assertEqual(result.removed, 64 * 64 - 24 * 48)
        old_source = layer.source
        new = autofix.add_derived_source(self.project, result.image, layer, "background",
                                         {"tolerance": 12})
        self.assertNotEqual(layer.source, old_source)
        self.assertEqual(new.derived["from"], old_source)
        self.assertEqual(new.derived["operation"], "background")
        self.assertEqual(sha(path), before)
        self.assertEqual(sha(self.project.source_path(self.project.source(old_source))), copy_before)
        self.project.save()
        self.assertEqual(Project.load(self.project.root).source(new.id).derived["params"], {"tolerance": 12})

    def test_background_removal_refuses_unclear_background(self):
        transparent = png(self.dir / "t.png", (16, 16), (255, 0, 0, 255), (4, 4, 12, 12))
        result = autofix.remove_background(open_png(transparent))
        self.assertIsNone(result.colour)
        self.assertEqual(result.removed, 0)
        self.assertTrue(result.reason)

    def test_feather_softens_only_the_rim(self):
        image = Image.new("RGBA", (20, 20), (255, 255, 255, 255))
        image.paste(Image.new("RGBA", (8, 8), (240, 240, 240, 255)), (6, 6))   # close to bg: rim
        image.paste(Image.new("RGBA", (4, 4), (0, 0, 0, 255)), (8, 8))
        out = np.asarray(autofix.remove_background(image, tolerance=10).image)[..., 3]
        self.assertEqual(out[0, 0], 0)
        self.assertTrue(0 < out[6, 10] < 255)
        self.assertEqual(out[10, 10], 255)

    def test_clean_alpha_noise(self):
        image = Image.new("RGBA", (4, 1))
        image.putdata([(1, 1, 1, 3), (1, 1, 1, 4), (1, 1, 1, 5), (1, 1, 1, 255)])
        out, changed = autofix.clean_alpha_noise(image)
        self.assertEqual(changed, 2)
        self.assertEqual(np.asarray(out)[0, :, 3].tolist(), [0, 0, 5, 255])

    def test_snap_to_foot_per_cell_and_all(self):
        a = self.project.add_asset("a", "character", 64, 64, (32, 56), directions=["N", "E"])
        src = png(self.dir / "b.png", (16, 32), (40, 120, 200, 255), (0, 0, 16, 32))
        layers = {}
        for v in ("N", "E"):
            for f in range(2):
                layers[(v, f)] = self.add(a, src, view=v, frame=f)
        layers[("E", 1)].x += 5
        layers[("E", 1)].y -= 7
        a.frame_anchors["1"] = [30.0, 50.0]
        moved = autofix.snap_all_to_foot(self.project, a, self.cache)
        self.assertIn(("E", 1), moved)
        for (v, f) in layers:
            m = measure(compose(self.project, a, self.cache, view=v, frame=f))
            ax, ay = a.anchor_for(f)
            self.assertEqual(m.contact_y, ay, (v, f))
            self.assertLessEqual(abs(m.foot_x - ax), 0.5, (v, f))
        self.assertEqual(autofix.snap_all_to_foot(self.project, a, self.cache), {})  # already aligned

    def test_match_height_keeps_the_foot(self):
        a = self.project.add_asset("a", "character", 128, 128, (64, 120))
        big = self.add(a, png(self.dir / "big.png", (40, 100), (200, 30, 30, 255), (0, 0, 40, 100)))
        foot_before = (big.x + 20, big.y + 100)
        scale = autofix.match_height(self.project, big, self.cache, 50)
        self.assertEqual(scale, 0.5)
        image = self.cache.layer_image(self.project, big)
        bbox = image.getbbox()
        self.assertEqual(bbox[3] - bbox[1], 50)
        self.assertEqual((big.x + (bbox[0] + bbox[2]) / 2, big.y + bbox[3]), foot_before)

    def test_reference_height(self):
        a = self.project.add_asset("a", "character", 64, 64, (32, 56))
        ref = self.add(a, png(self.dir / "r.png", (10, 30), (9, 9, 9, 255), (0, 0, 10, 30)), slot="reference")
        self.add(a, png(self.dir / "k.png", (10, 60), (9, 9, 9, 255), (0, 0, 10, 60)))
        self.assertEqual(autofix.reference_height(self.project, a, self.cache, "", 0), 30)
        ref.visible = False
        self.assertIsNone(autofix.reference_height(self.project, a, self.cache, "", 0))


if __name__ == "__main__":
    unittest.main()
