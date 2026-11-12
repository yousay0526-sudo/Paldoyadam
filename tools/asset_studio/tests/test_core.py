"""Model, compositing, checks and export without Qt.  python -m unittest discover tools/asset_studio/tests"""

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

from paldo_studio.checks import check_asset, has_errors  # noqa: E402
from paldo_studio.game_specs import find_repo, load_specs, parse_attack_roots  # noqa: E402
from paldo_studio.imaging import ImageCache, compose, export_asset, inspect, open_png  # noqa: E402
from paldo_studio.model import History, Project  # noqa: E402

REPO = find_repo(HERE)


def png(path: Path, size, color, box=None, mode="RGBA") -> Path:
    image = Image.new(mode, size, (0, 0, 0, 0) if mode == "RGBA" else (0, 0, 0))
    if box:
        image.paste(Image.new(mode, (box[2] - box[0], box[3] - box[1]), color), box[:2])
    image.save(path)
    return path


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.project = Project("시험", root=self.dir / "proj")
        self.project.save()
        self.cache = ImageCache()

    def tearDown(self):
        self.tmp.cleanup()

    def add(self, asset, path, **kw):
        image = open_png(path)
        source = self.project.import_png(path, image.width, image.height, image.mode)
        return self.project.add_layer(asset, source, path.stem, **kw)


class ProjectTests(Base):
    def test_save_load_roundtrip(self):
        a = self.project.add_asset("주인공", "character", 192, 192, (96, 176), "hero-move")
        layer = self.add(a, png(self.dir / "a.png", (10, 10), (255, 0, 0, 255), (0, 0, 10, 10)))
        layer.pivot, layer.opacity, layer.scale = [5, 9], 0.5, 0.5
        self.project.save()
        loaded = Project.load(self.dir / "proj")
        self.assertEqual(loaded.to_dict(), self.project.to_dict())
        self.assertEqual(json.loads((self.dir / "proj/project.paldo.json").read_text("utf-8"))["format"],
                         "paldo-asset-studio/1")

    def test_import_copies_and_never_touches_original(self):
        original = png(self.dir / "orig.png", (8, 8), (0, 255, 0, 255), (2, 2, 6, 6))
        before = sha(original)
        a = self.project.add_asset("x", "item", 16, 16, (8, 15))
        layer = self.add(a, original)
        layer.x += 3
        export_asset(self.project, a, self.cache, [])
        self.assertEqual(sha(original), before)
        copy = self.project.root / self.project.source(layer.source).file
        self.assertTrue(copy.is_file())
        self.assertEqual(sha(copy), before)
        # Importing the same bytes again reuses the stored copy.
        again = self.project.import_png(original, 8, 8, "RGBA")
        self.assertEqual(again.id, layer.source)
        self.assertEqual(len(list((self.project.root / "sources").iterdir())), 1)

    def test_save_as_brings_sources(self):
        a = self.project.add_asset("x", "item", 16, 16, (8, 15))
        self.add(a, png(self.dir / "s.png", (4, 4), (1, 2, 3, 255), (0, 0, 4, 4)))
        self.project.save(self.dir / "other")
        loaded = Project.load(self.dir / "other")
        self.assertIsNotNone(self.cache.get(loaded, loaded.assets[0].layers[0].source))

    def test_unknown_format_is_rejected(self):
        with self.assertRaises(ValueError):
            Project.from_dict({"format": "something/9", "name": "x"})

    def test_layer_order_and_pivot_snap(self):
        a = self.project.add_asset("x", "weapon", 64, 64, (32, 60))
        l1 = self.add(a, png(self.dir / "1.png", (4, 4), (255, 0, 0, 255), (0, 0, 4, 4)))
        l2 = self.add(a, png(self.dir / "2.png", (6, 6), (0, 0, 255, 255), (0, 0, 6, 6)))
        self.assertTrue(Project.move_layer(a, l2.id, -1))
        self.assertEqual([l.id for l in a.layers], [l2.id, l1.id])
        self.assertFalse(Project.move_layer(a, l2.id, -1))
        l1.pivot, l1.scale = [2, 3], 2.0
        self.project.snap_pivot_to_anchor(a, l1)
        self.assertEqual((l1.x, l1.y), (28, 54))  # anchor - pivot * scale

    def test_history_undo_redo_and_merge(self):
        a = self.project.add_asset("x", "item", 16, 16, (8, 15))
        layer = self.add(a, png(self.dir / "h.png", (4, 4), (9, 9, 9, 255), (0, 0, 4, 4)))
        history = History(self.project)
        for value in (1, 2, 3):          # one spin box: three edits, one undo step
            history.checkpoint("x-field")
            self.project.assets[0].layers[0].x = value
        history.checkpoint()
        self.project.assets[0].layers[0].visible = False
        self.assertTrue(history.undo())
        self.assertTrue(self.project.assets[0].layers[0].visible)
        self.assertEqual(self.project.assets[0].layers[0].x, 3)
        self.assertTrue(history.undo())
        self.assertEqual(self.project.assets[0].layers[0].x, 6)  # import placed at anchor 8 - width 4 / 2
        self.assertFalse(history.undo())
        self.assertTrue(history.redo())
        self.assertEqual(self.project.assets[0].layers[0].x, 3)


class ImagingTests(Base):
    def test_compose_order_offset_opacity_and_clipping(self):
        a = self.project.add_asset("x", "item", 8, 8, (4, 7))
        red = self.add(a, png(self.dir / "r.png", (4, 4), (255, 0, 0, 255), (0, 0, 4, 4)))
        blue = self.add(a, png(self.dir / "b.png", (4, 4), (0, 0, 255, 255), (0, 0, 4, 4)))
        red.x, red.y, blue.x, blue.y = 0, 0, 2, 2
        out = np.asarray(compose(self.project, a, self.cache))
        self.assertEqual(tuple(out[0, 0]), (255, 0, 0, 255))
        self.assertEqual(tuple(out[3, 3]), (0, 0, 255, 255))   # top layer wins
        self.assertEqual(tuple(out[7, 0]), (0, 0, 0, 0))
        blue.opacity = 0.5
        out = np.asarray(compose(self.project, a, self.cache))
        r, g, b, alpha = out[3, 3]
        self.assertEqual(alpha, 255)
        self.assertTrue(120 <= r <= 135 and 120 <= b <= 135)
        blue.x, blue.y = 6, -2                                 # hangs outside: clipped, no crash
        out = np.asarray(compose(self.project, a, self.cache))
        self.assertEqual(tuple(out[0, 7]), (0, 0, 255, 128))
        red.visible = False
        self.assertEqual(int(np.asarray(compose(self.project, a, self.cache))[0, 0, 3]), 0)

    def test_reference_layers_are_not_exported(self):
        a = self.project.add_asset("x", "item", 4, 4, (2, 3))
        layer = self.add(a, png(self.dir / "r.png", (4, 4), (255, 0, 0, 255), (0, 0, 4, 4)))
        layer.x = layer.y = 0
        layer.slot = "reference"
        self.assertIsNone(compose(self.project, a, self.cache).getbbox())
        self.assertIsNotNone(compose(self.project, a, self.cache, include_reference=True).getbbox())

    def test_crop_and_scale(self):
        sheet = Image.new("RGBA", (8, 4), (0, 0, 0, 0))
        sheet.paste(Image.new("RGBA", (4, 4), (0, 255, 0, 255)), (4, 0))
        sheet.save(self.dir / "sheet.png")
        a = self.project.add_asset("x", "item", 8, 8, (4, 7))
        layer = self.add(a, self.dir / "sheet.png", crop=[4, 0, 4, 4])
        self.assertEqual(self.cache.layer_image(self.project, layer).size, (4, 4))
        self.assertEqual(self.cache.layer_image(self.project, layer).getpixel((0, 0)), (0, 255, 0, 255))
        layer.scale = 0.5
        small = self.cache.layer_image(self.project, layer)
        self.assertEqual(small.size, (2, 2))
        self.assertEqual(small.getpixel((1, 1)), (0, 255, 0, 255))

    def test_premultiplied_scaling_has_no_dark_fringe(self):
        image = Image.new("RGBA", (8, 8), (255, 255, 255, 0))
        image.paste(Image.new("RGBA", (4, 8), (255, 255, 255, 255)), (0, 0))
        image.save(self.dir / "white.png")
        a = self.project.add_asset("x", "item", 8, 8, (4, 7))
        layer = self.add(a, self.dir / "white.png")
        layer.scale = 0.37
        arr = np.asarray(self.cache.layer_image(self.project, layer))
        visible = arr[arr[..., 3] > 8]
        self.assertGreaterEqual(int(visible[..., :3].min()), 250)

    def test_inspect(self):
        info = inspect(open_png(png(self.dir / "i.png", (10, 10), (1, 1, 1, 255), (2, 3, 5, 7))))
        self.assertTrue(info.has_alpha)
        self.assertEqual(info.bbox, (2, 3, 5, 7))
        self.assertAlmostEqual(info.transparent_ratio, 0.88)
        rgb = inspect(open_png(png(self.dir / "rgb.png", (4, 4), (1, 1, 1), None, "RGB")))
        self.assertFalse(rgb.has_alpha)

    def test_open_png_rejects_other_formats(self):
        Image.new("RGB", (2, 2)).save(self.dir / "x.jpg")
        with self.assertRaises(ValueError):
            open_png(self.dir / "x.jpg")


class CheckAndExportTests(Base):
    def test_levels(self):
        a = self.project.add_asset("hero", "character", 32, 32, (16, 28))
        report = check_asset(self.project, a, self.cache)
        self.assertTrue(has_errors(report))                   # no layers
        layer = self.add(a, png(self.dir / "ok.png", (4, 4), (5, 5, 5, 255), (0, 0, 4, 4)))
        report = check_asset(self.project, a, self.cache)
        self.assertEqual([i["level"] for i in report], ["ok"])
        layer.x = 30                                          # visible art leaves the canvas
        self.assertIn("warn", [i["level"] for i in check_asset(self.project, a, self.cache)])
        layer.x = 14
        layer.y = 2                                           # floats above the ground line
        self.assertTrue(any("발 기준선" in i["message"] for i in check_asset(self.project, a, self.cache)))
        (self.project.root / self.project.source(layer.source).file).unlink()
        self.cache.clear()
        self.assertTrue(has_errors(check_asset(self.project, a, self.cache)))

    def test_rgb_and_name_and_weapon_pivot_warnings(self):
        a = self.project.add_asset("무기 시안", "weapon", 16, 16, (8, 15))
        layer = self.add(a, png(self.dir / "rgb.png", (16, 16), (5, 5, 5), (0, 0, 16, 16), "RGB"), slot="weapon")
        layer.x = layer.y = 0
        messages = " ".join(i["message"] for i in check_asset(self.project, a, self.cache))
        self.assertIn("알파 채널이 없습니다", messages)
        self.assertIn("기준점", messages)
        self.assertIn("투명 픽셀이 없습니다", messages)
        a.export_name = "무기"
        self.assertIn("파일명", " ".join(i["message"] for i in check_asset(self.project, a, self.cache)))
        self.assertFalse(has_errors(check_asset(self.project, a, self.cache)))

    def test_export_writes_png_and_metadata_and_refuses_overwrite(self):
        a = self.project.add_asset("hero", "character", 16, 16, (8, 15))
        self.add(a, png(self.dir / "e.png", (4, 4), (200, 10, 10, 255), (0, 0, 4, 4)))
        report = check_asset(self.project, a, self.cache)
        out, meta = export_asset(self.project, a, self.cache, report)
        self.assertEqual(out, [self.project.root / "exports" / "hero.png"])
        with Image.open(out[0]) as im:
            self.assertEqual((im.size, im.mode), ((16, 16), "RGBA"))
        data = json.loads(meta.read_text("utf-8"))
        self.assertEqual(data["outputs"][0]["sha256"], sha(out[0]))
        self.assertEqual(data["asset"]["anchor"], [8.0, 15.0])
        self.assertEqual(len(data["sources"]), 1)
        with self.assertRaises(FileExistsError):
            export_asset(self.project, a, self.cache, report)
        export_asset(self.project, a, self.cache, report, overwrite=True)
        a.export_name = "../escape"
        with self.assertRaises(ValueError):
            export_asset(self.project, a, self.cache, report)


class DirectionAndGroupTests(Base):
    DIRS = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]

    def make(self, groups=("",)):
        """8-direction 32x32 asset: one 4x6 'body' block per view standing on (16, 28)."""
        a = self.project.add_asset("hero", "character", 32, 32, (16, 28), directions=self.DIRS)
        body = png(self.dir / "body.png", (4, 6), (90, 60, 30, 255), (0, 0, 4, 6))
        for v in self.DIRS:
            for g in groups:
                path = body if not g else png(self.dir / f"{g}.png", (4, 2), (20, 20, 200, 255), (0, 0, 4, 2))
                image = open_png(path)
                source = self.project.import_png(path, image.width, image.height, image.mode)
                layer = self.project.add_layer(a, source, g or "몸", view=v, group=g)
                if g:
                    layer.y = 26 - 2
        return a

    def test_views_compose_and_export_per_direction(self):
        a = self.make()
        a.layers[2].x += 3        # E only
        self.assertNotEqual(compose(self.project, a, self.cache, view="E").getbbox(),
                            compose(self.project, a, self.cache, view="N").getbbox())
        with self.assertRaises(ValueError):
            self.project.add_layer(a, self.project.sources[0], "x", view="UP")
        report = check_asset(self.project, a, self.cache)
        self.assertFalse(has_errors(report), report)
        pngs, meta = export_asset(self.project, a, self.cache, report)
        self.assertEqual([p.name for p in pngs], [f"hero-{d.lower()}.png" for d in self.DIRS])
        data = json.loads(meta.read_text("utf-8"))
        self.assertEqual([o["view"] for o in data["outputs"]], self.DIRS)
        self.assertEqual(data["asset"]["directions"], self.DIRS)

    def test_missing_direction_is_an_error_naming_the_view(self):
        a = self.make()
        a.layers = [l for l in a.layers if l.view != "SW"]
        errors = [i for i in check_asset(self.project, a, self.cache) if i["level"] == "error"]
        self.assertEqual([i["view"] for i in errors], ["SW"])

    def test_ground_axis_and_margin_warnings_name_the_view(self):
        a = self.make()
        a.layers[4].y -= 5        # S: floating
        a.layers[6].x += 10       # W: off axis but inside the canvas
        a.tolerances["axis"] = 8  # the 26 px default suits 192 px hero cells, not this 32 px test
        report = check_asset(self.project, a, self.cache)
        found = {(i["view"], i["message"].split(":")[0]) for i in report if i["level"] == "warn"}
        self.assertIn(("S", "발 접지"), found)
        self.assertIn(("W", "발 중심축"), found)
        self.assertEqual({v for v, _ in found}, {"S", "W"})
        a.tolerances["axis"] = 12
        self.assertNotIn(("W", "발 중심축"), {(i["view"], i["message"].split(":")[0])
                                             for i in check_asset(self.project, a, self.cache)})
        a.layers[0].x = 0         # N touches the left edge
        self.assertTrue(any(i["view"] == "N" and "가장자리" in i["message"]
                            for i in check_asset(self.project, a, self.cache)))

    def test_move_layer_stays_inside_its_view(self):
        a = self.make(groups=("", "바지"))
        n_body, n_pants = a.view_layers("N")
        self.assertFalse(Project.move_layer(a, n_pants.id, +1))     # already top of N
        self.assertTrue(Project.move_layer(a, n_pants.id, -1))
        self.assertEqual([l.id for l in a.view_layers("N")], [n_pants.id, n_body.id])
        self.assertEqual([l.group for l in a.view_layers("NE")], ["", "바지"])

    def test_groups_combos_and_group_checks(self):
        a = self.make(groups=("", "바지"))
        with_pants = compose(self.project, a, self.cache, view="S")
        a.save_combo("착용")
        a.hidden_groups = ["바지"]
        a.save_combo("맨몸")
        self.assertNotEqual(compose(self.project, a, self.cache, view="S").tobytes(), with_pants.tobytes())
        self.assertTrue(a.apply_combo("착용"))
        self.assertEqual(a.hidden_groups, [])
        self.assertEqual(compose(self.project, a, self.cache, view="S").tobytes(), with_pants.tobytes())
        self.assertFalse(a.apply_combo("없는 조합"))
        # A 장비 묶음 missing in one direction, and one with a different size.
        a.layers = [l for l in a.layers if not (l.group == "바지" and l.view == "W")]
        odd = png(self.dir / "odd.png", (6, 2), (20, 20, 200, 255), (0, 0, 6, 2))
        source = self.project.import_png(odd, 6, 2, "RGBA")
        next(l for l in a.layers if l.group == "바지" and l.view == "E").source = source.id
        messages = " ".join(i["message"] for i in check_asset(self.project, a, self.cache))
        self.assertIn("W 방향에 없습니다", messages)
        self.assertIn("크기가 방향마다 다릅니다", messages)
        loaded = Project.load(self.project.save().parent)
        self.assertEqual(loaded.assets[0].combos, a.combos)


@unittest.skipIf(REPO is None, "game repository not found")
class GameSpecTests(unittest.TestCase):
    def test_presets_match_repository_records(self):
        specs = load_specs(REPO)
        self.assertEqual(specs.notes, [])
        self.assertEqual(specs.direction_order, ["N", "NE", "E", "SE", "S", "SW", "W", "NW"])
        move = specs.preset("hero-move")
        self.assertEqual((move.width, move.height, move.anchor), (192, 192, (96.0, 176.0)))
        self.assertEqual(specs.preset("hero-technique").anchor, (116.0, 176.0))
        self.assertAlmostEqual(move.display_box, 56 * 1.4)
        attack0 = specs.preset("hero-attack-0")
        self.assertEqual(attack0.anchor, (290 * 192 / 512, 470 * 192 / 512))

    def test_attack_root_parser(self):
        text = "const Vec2 roots[HERO_ATTACK_FRAMES] = {\n {1, 2}, {3.5, 4}};"
        self.assertEqual(parse_attack_roots(text), [(1, 2), (3.5, 4)])
        self.assertIsNone(parse_attack_roots("nothing"))

    def test_missing_repository_falls_back_to_free_canvas(self):
        specs = load_specs(None)
        self.assertEqual([p.key for p in specs.presets], ["free"])
        self.assertTrue(specs.notes)

    def test_default_tolerances_pass_every_runtime_idle_and_walk_cell(self):
        from paldo_studio.imaging import measure
        from paldo_studio.model import DEFAULT_TOLERANCES
        sheets = ["warrior-idle-8dir-v1.png"] + [f"warrior-walk-{d}-v1.png"
                                                 for d in ("n", "ne", "e", "se", "s", "sw", "w", "nw")]
        count = 0
        for name in sheets:
            sheet = open_png(REPO / "assets" / name).convert("RGBA")
            for i in range(8):
                m = measure(sheet.crop((i % 4 * 192, i // 4 * 192, i % 4 * 192 + 192, i // 4 * 192 + 192)))
                self.assertLessEqual(abs(m.contact_y - 176), DEFAULT_TOLERANCES["ground"], (name, i))
                self.assertLessEqual(abs(m.foot_x - 96), DEFAULT_TOLERANCES["axis"], (name, i))
                self.assertGreaterEqual(m.margin, DEFAULT_TOLERANCES["margin"], (name, i))
                count += 1
        self.assertEqual(count, 72)

    def test_runtime_idle_sheet_as_eight_direction_asset_checks_clean(self):
        with tempfile.TemporaryDirectory() as tmp:
            project = Project("p", root=Path(tmp))
            project.save()
            specs = load_specs(REPO)
            a = project.add_asset("warrior-idle", "character", 192, 192, (96, 176), "hero-move",
                                  specs.direction_order)
            sheet = REPO / "assets/warrior-idle-8dir-v1.png"
            image = open_png(sheet)
            source = project.import_png(sheet, image.width, image.height, image.mode)
            for i, v in enumerate(a.directions):
                project.add_layer(a, source, v, [i % 4 * 192, i // 4 * 192, 192, 192], "body", (96, 176), view=v)
            report = check_asset(project, a, ImageCache(), specs)
            self.assertEqual([r["level"] for r in report], ["ok"], report)

    def test_runtime_hero_cell_composes_pixel_identical(self):
        """Importing an existing runtime cell and exporting it reproduces the same pixels."""
        with tempfile.TemporaryDirectory() as tmp:
            project = Project("p", root=Path(tmp))
            project.save()
            cache = ImageCache()
            a = project.add_asset("idle-s", "character", 192, 192, (96, 176), "hero-move")
            sheet = REPO / "assets/warrior-idle-8dir-v1.png"
            before = sha(sheet)
            image = open_png(sheet)
            source = project.import_png(sheet, image.width, image.height, image.mode)
            layer = project.add_layer(a, source, "S", [0, 192, 192, 192], "body", (96, 176))
            self.assertEqual((layer.x, layer.y), (0, 0))
            report = check_asset(project, a, cache, load_specs(REPO))
            self.assertFalse(has_errors(report))
            out, _ = export_asset(project, a, cache, report)
            expected = np.asarray(image.convert("RGBA").crop((0, 192, 192, 384)))
            with Image.open(out[0]) as im:
                self.assertTrue(np.array_equal(np.asarray(im.convert("RGBA")), expected))
            self.assertEqual(sha(sheet), before)


if __name__ == "__main__":
    unittest.main()
