"""GUI smoke test with the real widgets on Qt's offscreen platform.

Skipped when PySide6 is not installed.
"""

from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

try:
    from PySide6.QtCore import QPoint, Qt
    from PySide6.QtTest import QTest
    from PySide6.QtWidgets import QApplication
except ImportError:  # pragma: no cover
    QApplication = None

from paldo_studio.game_specs import find_repo, load_specs  # noqa: E402
from paldo_studio.model import Project  # noqa: E402

REPO = find_repo(HERE)


@unittest.skipIf(QApplication is None, "PySide6 not installed")
@unittest.skipIf(REPO is None, "game repository not found")
class GuiSmokeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from paldo_studio.ui.main_window import MainWindow
        cls.app = QApplication.instance() or QApplication([])
        cls.MainWindow = MainWindow

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.window = self.MainWindow(load_specs(REPO))
        self.window.resize(1280, 800)
        self.window.show()
        QTest.qWaitForWindowExposed(self.window)

    def tearDown(self):
        self.window.dirty = False
        self.window.close()
        self.tmp.cleanup()

    def test_full_mvp_flow(self):
        w = self.window
        w.new_project(self.dir / "proj", "시험 프로젝트")
        w.new_asset("hero idle s", "character", 192, 192, (96, 176), "hero-move")
        sheet = REPO / "assets/warrior-idle-8dir-v1.png"
        layer = w.import_png(sheet, crop=[0, 192, 192, 192], align_to_anchor=(96, 176), name="정지 S", slot="body")
        self.assertEqual((layer.x, layer.y), (0, 0))
        self.assertEqual(w.layer_list.count(), 1)
        self.assertEqual(w.asset_tree.topLevelItem(0).text(0), "캐릭터 (1)")

        # Drag the layer 10 canvas px to the right on the real canvas widget.
        canvas = w.canvas
        canvas.set_zoom(2.0)
        start = QPoint(round(canvas.pan.x() + 96 * canvas.zoom), round(canvas.pan.y() + 120 * canvas.zoom))
        QTest.mousePress(canvas, Qt.LeftButton, Qt.NoModifier, start)
        QTest.mouseMove(canvas, start + QPoint(10, 0))
        QTest.mouseMove(canvas, start + QPoint(20, 0))
        QTest.mouseRelease(canvas, Qt.LeftButton, Qt.NoModifier, start + QPoint(20, 0))
        self.assertEqual(w.layer.x, 10)
        self.assertEqual(w.l_x.value(), 10)
        self.assertTrue(w.dirty)

        # Keyboard nudge, then one undo per gesture.
        canvas.setFocus()
        QTest.keyClick(canvas, Qt.Key_Down)
        QTest.keyClick(canvas, Qt.Key_Down)
        self.assertEqual(w.layer.y, 2)
        w.undo()
        self.assertEqual((w.layer.x, w.layer.y), (10, 0))
        w.undo()
        self.assertEqual((w.layer.x, w.layer.y), (0, 0))
        w.redo()
        self.assertEqual(w.layer.x, 10)

        # Property panel edits.
        w.l_x.setValue(0)
        w.l_opacity.setValue(50)
        self.assertEqual(w.layer.opacity, 0.5)
        w.l_opacity.setValue(100)
        w.layer_list.item(0).setCheckState(Qt.Unchecked)
        self.assertFalse(w.layer.visible)
        w.layer_list.item(0).setCheckState(Qt.Checked)

        # Pivot + scale keeps the pivot where it is on the canvas.
        w.l_pivot_on.setChecked(True)
        w.l_px.setValue(96)
        w.l_py.setValue(176)
        w.l_scale.setValue(0.5)
        self.assertEqual((w.layer.x + 96 * 0.5, w.layer.y + 176 * 0.5), (96, 176))
        w.l_scale.setValue(1.0)
        self.assertEqual((w.layer.x, w.layer.y), (0, 0))

        # Anchor tool sets the asset anchor by clicking.
        w.set_mode("anchor")
        QTest.mouseClick(canvas, Qt.LeftButton, Qt.NoModifier,
                         QPoint(round(canvas.pan.x() + 90 * canvas.zoom), round(canvas.pan.y() + 170 * canvas.zoom)))
        self.assertEqual(w.asset.anchor, [90.0, 170.0])  # undo rebuilt the objects: look up by id
        w.undo()
        self.assertEqual(w.asset.anchor, [96.0, 176.0])
        w.set_mode("move")

        # Second layer and ordering.
        walk = w.import_png(REPO / "assets/warrior-walk-s-v1.png", crop=[0, 0, 192, 192],
                            align_to_anchor=(96, 176), name="걷기 S 0")
        w.l_slot.setCurrentIndex(w.l_slot.findData("reference"))
        self.assertEqual(w.layer.slot, "reference")
        w.move_layer(-1)
        self.assertEqual(w.asset.layers[0].id, walk.id)

        # Save, reopen, export.
        self.assertTrue(w.save_project())
        self.assertFalse(w.dirty)
        w.open_project(self.dir / "proj")
        self.assertEqual(len(w.project.assets[0].layers), 2)
        report = w.run_check()
        self.assertEqual([i["level"] for i in report], ["ok"])
        pngs, meta = w.export_current(report)
        self.assertTrue(pngs[0].is_file() and meta.is_file())
        self.assertEqual(pngs[0].parent, self.dir / "proj" / "exports")

        # A small window still shows a usable canvas.
        w.resize(900, 600)
        QApplication.processEvents()
        self.assertGreaterEqual(canvas.width(), 320)
        self.assertGreaterEqual(canvas.height(), 240)

    def test_eight_direction_flow(self):
        w = self.window
        w.new_project(self.dir / "p8", "8방향")
        order = w.specs.direction_order
        w.new_asset("warrior idle", "character", 192, 192, (96, 176), "hero-move", order)
        self.assertTrue(w.act_import_sheet.isEnabled())
        self.assertEqual(w.view, "N")
        crops = {v: [i % 4 * 192, i // 4 * 192, 192, 192] for i, v in enumerate(order)}
        made = w.import_sheet(REPO / "assets/warrior-idle-8dir-v1.png", crops, "정지", slot="body")
        self.assertEqual(len(made), 8)
        self.assertTrue(all((l.x, l.y) == (0, 0) for l in made.values()))
        # An equipment set: the walk sheet frame 0 of each direction stands in for gear here.
        for v in order:
            w.set_view(v)
            w.import_png(REPO / f"assets/warrior-walk-{v.lower()}-v1.png", crop=[0, 0, 192, 192],
                         align_to_anchor=(96, 176), name="장비 시험", group="시험 장비")
        self.assertEqual(w.asset.groups(), ["시험 장비"])

        # Direction buttons switch the edited view and keep the same 장비 묶음 selected.
        w.set_view("S")
        QTest.mouseClick(w.dir_buttons["E"], Qt.LeftButton)
        self.assertEqual(w.view, "E")
        self.assertEqual((w.layer.view, w.layer.group), ("E", "시험 장비"))
        self.assertEqual(w.layer_list.count(), 2)
        self.assertTrue(w.dir_buttons["E"].isChecked())
        self.assertIsNotNone(w.overview.cells["E"].pixmap())
        self.assertIn("방향 간 차이", w.overview.summary.text())

        # Toggling the 장비 묶음 hides it in every direction; combos remember it.
        w.save_combo("시험 착용")
        w.set_group_visible("시험 장비", False)
        self.assertEqual(w.asset.hidden_groups, ["시험 장비"])
        w.save_combo("기본")
        w.apply_combo("시험 착용")
        self.assertEqual(w.asset.hidden_groups, [])
        w.apply_combo("기본")
        self.assertEqual(w.asset.hidden_groups, ["시험 장비"])
        w.undo()
        self.assertEqual(w.asset.hidden_groups, [])

        # Onion skin of another direction.
        w.c_onion.setCurrentIndex(w.c_onion.findData("W"))
        self.assertIsNotNone(w.canvas._onion)

        # Lift one direction: the check names it and jumps there.
        w.set_view("SW")
        body = next(l for l in w.asset.view_layers("SW") if not l.group)
        w.select_layer(body.id)
        w.set_group_visible("시험 장비", False)
        w.l_y.setValue(-6)
        report = w.run_check()
        lifted = [i for i in report if i["view"] == "SW" and i["message"].startswith("발 접지")]
        self.assertEqual(len(lifted), 1, report)
        w.set_view("N")
        w.goto_check_item(lifted[0])
        self.assertEqual(w.view, "SW")
        w.l_y.setValue(0)
        report = w.run_check()
        self.assertEqual([i["level"] for i in report], ["ok"], report)
        pngs, _ = w.export_current()
        self.assertEqual(len(pngs), 8)

    def test_timeline_animation_flow(self):
        import numpy as np
        from PIL import Image
        w = self.window
        w.new_project(self.dir / "anim", "애니메이션")
        w.new_asset("warrior-walk-e-v1", "character", 192, 192, (96, 176), "hero-move")
        crops = {("", f): [f % 4 * 192, f // 4 * 192, 192, 192] for f in range(8)}
        made = w.import_sheet(REPO / "assets/warrior-walk-e-v1.png", crops, "걷기", slot="body")
        self.assertEqual((len(made), w.asset.frames), (8, 8))
        self.assertEqual(w.frame_strip.count(), 8)
        self.assertEqual(w.frame_label.text(), "프레임 1/8")

        # Clicking a thumbnail selects that frame; layers and canvas follow.
        rect = w.frame_strip.visualItemRect(w.frame_strip.item(3))
        QTest.mouseClick(w.frame_strip.viewport(), Qt.LeftButton, Qt.NoModifier, rect.center())
        self.assertEqual((w.frame, w.canvas.frame, w.layer.frame), (3, 3, 3))
        self.assertEqual(w.layer_list.count(), 1)
        w.step_frame(+1)
        self.assertEqual(w.frame, 4)
        w.set_frame(7)
        w.step_frame(+1)
        self.assertEqual(w.frame, 0)  # wraps

        # Playback: the timer advances frames; FPS only sets the editor interval.
        w.t_fps.setValue(12)
        self.assertEqual(w.asset.fps, 12.0)
        w.toggle_play()
        self.assertTrue(w.timer.isActive())
        self.assertEqual(w.timer.interval(), round(1000 / 12))
        w._tick()
        w._tick()
        self.assertEqual(w.frame, 2)
        w.toggle_play()
        self.assertFalse(w.timer.isActive())
        self.assertEqual(w.t_buttons["play"].text(), "▶ 재생")

        # Onion skin of the previous and next frame.
        w.t_onion.setChecked(True)
        self.assertEqual(len(w.canvas._onion_frames), 2)
        w.t_onion.setChecked(False)

        # Frame operations, each one undo step.
        w.add_frame(copy=True)
        self.assertEqual((w.asset.frames, w.frame), (9, 3))
        self.assertEqual(w.asset.cell_layers("", 3)[0].crop, w.asset.cell_layers("", 2)[0].crop)
        w.undo()
        self.assertEqual(w.asset.frames, 8)
        w.add_frame(copy=False)
        self.assertEqual(w.layer_list.count(), 0)
        self.assertEqual(w.run_check()[0]["level"], "error")
        w.delete_frame()
        self.assertEqual(w.asset.frames, 8)
        w.set_frame(1)
        w.move_frame(+1)
        self.assertEqual(w.frame, 2)
        self.assertEqual(w.asset.cell_layers("", 2)[0].crop[0], 192)
        w.undo()
        self.assertEqual(w.asset.cell_layers("", 1)[0].crop[0], 192)

        # One frame gets its own foot root; the anchor fields then edit that frame only.
        w.set_frame(5)
        w.t_anchor.setChecked(True)
        self.assertIn("5", w.asset.frame_anchors)
        w.f_ay.setValue(168)
        self.assertEqual((w.asset.anchor_for(5)[1], w.asset.anchor[1]), (168, 176))
        report = w.run_check()
        lifted = [i for i in report if i["message"].startswith("발 접지")]
        self.assertEqual([(i["frame"], i["level"]) for i in lifted], [(5, "warn")], report)
        w.set_frame(0)
        w.goto_check_item(lifted[0])
        self.assertEqual(w.frame, 5)
        w.t_anchor.setChecked(False)
        self.assertEqual(w.asset.frame_anchors, {})

        # Sheet preview and export in the runtime 4×2 layout reproduce the walk sheet.
        w.f_layout.setCurrentIndex(w.f_layout.findData("per_view"))
        w.f_columns.setValue(4)
        sheets = w.sheet_preview()
        self.assertEqual([(n, im.size) for n, im in sheets], [("warrior-walk-e-v1.png", (768, 384))])
        report = w.run_check()
        self.assertEqual([i["level"] for i in report], ["ok"], report)
        pngs, _ = w.export_current(report)
        with Image.open(REPO / "assets/warrior-walk-e-v1.png") as want, Image.open(pngs[0]) as got:
            self.assertTrue(np.array_equal(np.asarray(got.convert("RGBA")), np.asarray(want.convert("RGBA"))))

        # Save and reopen keeps frames and layout.
        self.assertTrue(w.save_project())
        w.open_project(self.dir / "anim")
        self.assertEqual((w.asset.frames, w.asset.sheet_layout, w.asset.fps), (8, "per_view", 12.0))

    def test_sheet_import_dialog_modes(self):
        from paldo_studio.imaging import open_png
        from paldo_studio.ui.dialogs import SheetImportDialog
        order = self.window.specs.direction_order
        path = REPO / "assets/monsters/boar-walk-8dir-v1.png"
        d = SheetImportDialog(path, open_png(path), self.window.specs, order, [], view="E", frame=2)
        self.assertEqual(d.mode.currentData(), "rows_all")
        crops = d.crops()
        self.assertEqual(len(crops), 32)
        self.assertEqual(crops[("SE", 1)], [192, 3 * 192, 192, 192])
        d.mode.setCurrentIndex(d.mode.findData("rows"))
        d.frame.setValue(3)
        self.assertEqual(sorted({f for _, f in d.crops()}), [2])  # into the current frame
        d.mode.setCurrentIndex(d.mode.findData("frames"))
        self.assertEqual(list(d.crops())[:2], [("E", 0), ("E", 1)])
        path = REPO / "assets/warrior-attack-v1.png"
        d = SheetImportDialog(path, open_png(path), self.window.specs, [], [])
        self.assertEqual((d.mode.count(), d.mode.currentData(), len(d.crops())), (1, "frames", 6))

    def test_projects_cannot_live_in_runtime_assets(self):
        with self.assertRaises(ValueError):
            self.window.new_project(REPO / "assets" / "studio-test", "x")
        self.assertFalse((REPO / "assets" / "studio-test").exists())

    def test_existing_project_is_not_overwritten(self):
        Project("a", root=self.dir / "p").save()
        with self.assertRaises(FileExistsError):
            self.window.new_project(self.dir / "p", "b")


if __name__ == "__main__":
    unittest.main()
