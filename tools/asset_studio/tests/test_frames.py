"""Phase 3: frames, per-frame foot roots, sheet layouts and the frame-jump check (no Qt)."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

from paldo_studio.checks import check_asset, has_errors  # noqa: E402
from paldo_studio.game_specs import find_repo, load_specs  # noqa: E402
from paldo_studio.imaging import ImageCache, build_sheets, compose, export_asset, open_png  # noqa: E402
from paldo_studio.model import Project  # noqa: E402

from test_core import Base, png, sha  # noqa: E402

REPO = find_repo(HERE)


class FrameModelTests(Base):
    def make(self, frames=3):
        a = self.project.add_asset("anim", "character", 32, 32, (16, 28))
        src = png(self.dir / "dot.png", (8, 8), (200, 40, 40, 255), (0, 0, 8, 8))
        for f in range(frames):
            layer = self.add(a, src, frame=f)
            layer.name = f"f{f}"
        return a

    def test_add_layer_grows_frames_and_cell_layers_filter(self):
        a = self.make(3)
        self.assertEqual(a.frames, 3)
        self.assertEqual([l.name for l in a.cell_layers("", 1)], ["f1"])
        with self.assertRaises(ValueError):
            self.add(a, self.dir / "dot.png", frame=-1)

    def test_insert_duplicate_delete_move_keep_layers_and_anchors(self):
        a = self.make(3)
        a.frame_anchors = {"0": [10.0, 20.0], "2": [12.0, 22.0]}
        made = a.insert_frame(1, copy_from=0)  # duplicate frame 0 right after it
        self.assertEqual(a.frames, 4)
        self.assertEqual([l.name for l in made], ["f0"])
        self.assertNotEqual(made[0].id, a.cell_layers("", 0)[0].id)
        self.assertEqual([a.cell_layers("", f)[0].name for f in range(4)], ["f0", "f0", "f1", "f2"])
        self.assertEqual(a.frame_anchors, {"0": [10.0, 20.0], "1": [10.0, 20.0], "3": [12.0, 22.0]})
        a.insert_frame(4)  # empty frame at the end
        self.assertEqual((a.frames, a.cell_layers("", 4)), (5, []))
        self.assertTrue(a.move_frame(3, -1))
        self.assertEqual([[l.name for l in a.cell_layers("", f)] for f in range(5)],
                         [["f0"], ["f0"], ["f2"], ["f1"], []])
        self.assertEqual(a.frame_anchors["2"], [12.0, 22.0])
        self.assertFalse(a.move_frame(0, -1))
        self.assertTrue(a.delete_frame(0))
        self.assertEqual(a.frames, 4)
        self.assertEqual(a.frame_anchors, {"0": [10.0, 20.0], "1": [12.0, 22.0]})
        for _ in range(3):
            a.delete_frame(0)
        self.assertFalse(a.delete_frame(0))  # the last frame stays
        self.assertEqual(a.frames, 1)

    def test_frames_survive_save_and_load(self):
        a = self.make(2)
        a.frame_anchors = {"1": [15.0, 27.0]}
        a.fps, a.sheet_layout, a.sheet_columns = 12.0, "per_view", 2
        self.project.save()
        b = Project.load(self.project.root).assets[0]
        self.assertEqual((b.frames, b.fps, b.sheet_layout, b.sheet_columns), (2, 12.0, "per_view", 2))
        self.assertEqual(b.anchor_for(1), [15.0, 27.0])
        self.assertEqual(b.anchor_for(0), [16, 28])
        self.assertEqual([l.frame for l in b.layers], [0, 1])

    def test_compose_is_per_frame_and_pivot_snaps_to_frame_anchor(self):
        a = self.make(2)
        for layer in a.layers:
            layer.x, layer.y = 0, 0
        a.layers[1].x = 20
        left = compose(self.project, a, self.cache, frame=0)
        right = compose(self.project, a, self.cache, frame=1)
        self.assertEqual(left.getbbox(), (0, 0, 8, 8))
        self.assertEqual(right.getbbox(), (20, 0, 28, 8))
        a.frame_anchors = {"1": [24.0, 30.0]}
        layer = a.layers[1]
        layer.pivot = [4, 8]
        self.project.snap_pivot_to_anchor(a, layer)
        self.assertEqual((layer.x, layer.y), (20, 22))


class FrameCheckTests(Base):
    def standing(self, frames, positions):
        a = self.project.add_asset("walk", "character", 64, 64, (32, 56))
        src = png(self.dir / "body.png", (16, 32), (40, 120, 200, 255), (0, 0, 16, 32))
        for f in range(frames):
            layer = self.add(a, src, frame=f)
            layer.x, layer.y = positions[f], 56 - 32 + 1  # bottom row at y=56
        return a

    def test_jump_between_frames_is_warned_with_frame_number(self):
        a = self.standing(3, [24, 26, 24])
        self.assertEqual([r["level"] for r in check_asset(self.project, a, self.cache)], ["ok"])
        a.layers[2].x = 24 + 40  # frame 3 leaps 40 px
        a.tolerances["axis"] = 64
        a.width = 128
        report = check_asset(self.project, a, self.cache)
        jumps = [r for r in report if "그림 중심이" in r["message"]]
        self.assertEqual([(r["frame"], r["level"]) for r in jumps], [(2, "warn")], report)
        self.assertIn("프레임 2→3", jumps[0]["message"])
        a.tolerances["jump"] = 50
        self.assertFalse([r for r in check_asset(self.project, a, self.cache) if "그림 중심이" in r["message"]])

    def test_ground_uses_each_frames_own_anchor(self):
        a = self.standing(2, [24, 24])
        a.layers[1].y -= 10  # frame 2 lifted ...
        lifted = [r for r in check_asset(self.project, a, self.cache) if r["message"].startswith("발 접지")]
        self.assertEqual([r["frame"] for r in lifted], [1])
        a.frame_anchors["1"] = [32.0, 46.0]  # ... which is right when that frame's foot root is higher
        lifted = [r for r in check_asset(self.project, a, self.cache) if r["message"].startswith("발 접지")]
        self.assertEqual(lifted, [])

    def test_missing_frame_in_one_direction_is_an_error(self):
        a = self.project.add_asset("walk8", "character", 64, 64, (32, 56), directions=["N", "E"])
        src = png(self.dir / "body.png", (16, 32), (40, 120, 200, 255), (0, 0, 16, 32))
        for v in ("N", "E"):
            for f in range(2):
                if (v, f) != ("E", 1):
                    layer = self.add(a, src, view=v, frame=f)
                    layer.x, layer.y = 24, 25
        report = check_asset(self.project, a, self.cache)
        errors = [(r["view"], r["frame"]) for r in report if r["level"] == "error"]
        self.assertEqual(errors, [("E", 1)], report)

    def test_layout_rules(self):
        a = self.standing(2, [24, 24])
        a.sheet_layout = "views_in_cells"
        self.assertTrue(has_errors(check_asset(self.project, a, self.cache)))
        with self.assertRaises(ValueError):
            build_sheets(self.project, a, self.cache)
        a.sheet_layout = "rows_by_view"
        self.assertTrue(has_errors(check_asset(self.project, a, self.cache)))
        a.sheet_layout = "cells"
        names = [p.name for p in export_asset(self.project, a, self.cache, [])[0]]
        self.assertEqual(names, ["walk-f0.png", "walk-f1.png"])


@unittest.skipIf(REPO is None, "game repository not found")
class RuntimeSheetRoundTripTests(unittest.TestCase):
    """Cut a runtime sheet into frames, re-export it in its layout: same pixels, original untouched."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.project = Project("p", root=Path(self.tmp.name))
        self.project.save()
        self.cache = ImageCache()
        self.specs = load_specs(REPO)

    def tearDown(self):
        self.tmp.cleanup()

    def source(self, rel):
        path = REPO / rel
        image = open_png(path)
        return path, image, self.project.import_png(path, image.width, image.height, image.mode)

    def assertSame(self, out: Path, original: Image.Image):
        """Every visible pixel identical. Fully transparent pixels count as equal whatever their hidden
        colour: warrior-attack-v1.png keeps RGB under alpha 0, which the composite writes as 0,0,0,0."""
        with Image.open(out) as im:
            self.assertEqual(im.size, original.size)
            got, want = np.asarray(im.convert("RGBA")).copy(), np.asarray(original.convert("RGBA")).copy()
        self.assertTrue(np.array_equal(got[..., 3], want[..., 3]))
        got[got[..., 3] == 0] = 0
        want[want[..., 3] == 0] = 0
        self.assertTrue(np.array_equal(got, want))

    def test_walk_sheet_per_view_4x2(self):
        path, image, source = self.source("assets/warrior-walk-e-v1.png")
        before = sha(path)
        a = self.project.add_asset("warrior-walk-e-v1", "character", 192, 192, (96, 176), "hero-move")
        for f in range(8):
            self.project.add_layer(a, source, "걷기", [f % 4 * 192, f // 4 * 192, 192, 192], "body",
                                   (96, 176), frame=f)
        a.sheet_layout, a.sheet_columns = "per_view", 4
        report = check_asset(self.project, a, self.cache, self.specs)
        self.assertEqual([r["level"] for r in report], ["ok"], report)
        out, meta = export_asset(self.project, a, self.cache, report)
        self.assertEqual([p.name for p in out], ["warrior-walk-e-v1.png"])
        self.assertSame(out[0], image)
        data = json.loads(meta.read_text(encoding="utf-8"))
        self.assertEqual((data["asset"]["frames"], data["asset"]["layout"]), (8, "per_view"))
        self.assertEqual(data["outputs"][0]["size"], [768, 384])
        self.assertEqual(sha(path), before)

    def test_attack_sheet_3x2_with_draw_c_foot_roots(self):
        path, image, source = self.source("assets/warrior-attack-v1.png")
        a = self.project.add_asset("warrior-attack-v1", "character", 192, 192,
                                   self.specs.preset("hero-attack-0").anchor, "hero-attack-0")
        for f in range(6):
            a.frame_anchors[str(f)] = list(self.specs.preset(f"hero-attack-{f}").anchor)
            # whole cell: its foot is already at this frame's own root (as the sheet import does it)
            self.project.add_layer(a, source, "공격", [f % 3 * 192, f // 3 * 192, 192, 192], "body",
                                   tuple(a.anchor_for(f)), frame=f)
        self.assertTrue(all((l.x, l.y) == (0, 0) for l in a.layers))  # whole cells stay in place
        a.sheet_layout, a.sheet_columns = "per_view", 3
        report = check_asset(self.project, a, self.cache, self.specs)
        self.assertFalse(has_errors(report), report)
        out, _ = export_asset(self.project, a, self.cache, report)
        self.assertSame(out[0], image)

    def test_idle_sheet_views_in_cells_4x2(self):
        path, image, source = self.source("assets/warrior-idle-8dir-v1.png")
        a = self.project.add_asset("warrior-idle-8dir-v1", "character", 192, 192, (96, 176), "hero-move",
                                   self.specs.direction_order)
        for i, v in enumerate(a.directions):
            self.project.add_layer(a, source, v, [i % 4 * 192, i // 4 * 192, 192, 192], "body", (96, 176), view=v)
        a.sheet_layout, a.sheet_columns = "views_in_cells", 4
        out, _ = export_asset(self.project, a, self.cache, check_asset(self.project, a, self.cache, self.specs))
        self.assertEqual(len(out), 1)
        self.assertSame(out[0], image)

    def test_monster_walk_rows_by_view(self):
        path, image, source = self.source("assets/monsters/boar-walk-8dir-v1.png")
        cols, rows = 4, 8
        w, h = image.width // cols, image.height // rows
        a = self.project.add_asset("boar-walk-8dir-v1", "monster", w, h, (w / 2, h * 0.9), "free",
                                   self.specs.direction_order)
        for r, v in enumerate(a.directions):
            for f in range(cols):
                self.project.add_layer(a, source, v, [f * w, r * h, w, h], "body", tuple(a.anchor),
                                       view=v, frame=f)
        a.sheet_layout = "rows_by_view"
        report = check_asset(self.project, a, self.cache, self.specs)
        self.assertFalse(has_errors(report), report)
        out, _ = export_asset(self.project, a, self.cache, report)
        self.assertSame(out[0], image)

    def test_runtime_motion_sheets_pass_the_jump_tolerance(self):
        """The default jump tolerance (30 px) was measured so every current hero motion passes."""
        from paldo_studio.imaging import measure
        from paldo_studio.model import DEFAULT_TOLERANCES
        import math
        sheets = [(f"warrior-walk-{d}-v1.png", 4, 8) for d in ("n", "ne", "e", "se", "s", "sw", "w", "nw")]
        sheets += [(f"warrior-{t}-v1.png", 3, 6) for t in ("attack", "cleave", "sweep", "thrust", "retreat")]
        worst = 0.0
        for name, cols, frames in sheets:
            sheet = open_png(REPO / "assets" / name).convert("RGBA")
            centres = [measure(sheet.crop((f % cols * 192, f // cols * 192, f % cols * 192 + 192,
                                           f // cols * 192 + 192))).centre for f in range(frames)]
            for p, q in zip(centres, centres[1:]):
                worst = max(worst, math.hypot(q[0] - p[0], q[1] - p[1]))
        self.assertLessEqual(worst, DEFAULT_TOLERANCES["jump"])


if __name__ == "__main__":
    unittest.main()
