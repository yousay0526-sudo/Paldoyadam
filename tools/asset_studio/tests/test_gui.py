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
        png, meta = w.export_current(report)
        self.assertTrue(png.is_file() and meta.is_file())
        self.assertEqual(png.parent, self.dir / "proj" / "exports")

        # A small window still shows a usable canvas.
        w.resize(900, 600)
        QApplication.processEvents()
        self.assertGreaterEqual(canvas.width(), 320)
        self.assertGreaterEqual(canvas.height(), 240)

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
