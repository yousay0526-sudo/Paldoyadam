#!/usr/bin/env python3
"""Capture review screenshots of the studio on Qt's offscreen platform.

    python tools/asset_studio/scripts/capture_screens.py --out <folder>
        [--body-sheet body-nolegs.png --legs legs-768.png]

Without options it uses runtime images from the repository only (read only).
``--body-sheet`` (768×384, idle 8-direction layout without legs) and ``--legs``
(one 768×768 cell, 4× the runtime cell, foot at 384,704) show the
"hero + 무명 바지·짚신" layering example with art from the unmerged leg study.
The project is written to a temporary folder; nothing in the repository changes.
"""

from __future__ import annotations

import argparse
import os
import sys
import tempfile
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

from PySide6.QtWidgets import QApplication  # noqa: E402

from paldo_studio.game_specs import find_repo, load_specs  # noqa: E402
from paldo_studio.imaging import open_png  # noqa: E402
from paldo_studio.ui.dialogs import CellImportDialog, CheckDialog  # noqa: E402
from paldo_studio.ui.main_window import MainWindow  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--body-sheet", type=Path)
    parser.add_argument("--legs", type=Path)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    app = QApplication([])
    repo = find_repo(HERE)
    specs = load_specs(repo)
    window = MainWindow(specs)
    window.resize(1440, 900)
    window.show()
    with tempfile.TemporaryDirectory() as tmp:
        window.new_project(Path(tmp) / "demo", "팔도 시연")
        idle = repo / "assets/warrior-idle-8dir-v1.png"
        e_cell = [2 * 192, 0, 192, 192]   # direction order N NE E SE / S SW W NW
        if args.body_sheet and args.legs:
            window.new_asset("주인공 정지 E · 무명 바지·짚신", "character", 192, 192, (96, 176), "hero-move")
            window.import_png(idle, crop=e_cell, align_to_anchor=(96, 176), name="기존 주인공 정지 E (비교)",
                              slot="reference")
            window.layer.visible = False
            window.import_png(args.body_sheet, crop=e_cell, align_to_anchor=(96, 176),
                              name="몸 (다리 제거)", slot="body")
            legs = window.import_png(args.legs, name="무명 바지·짚신 (4배 원화)", slot="bottom")
            window.layer.pivot = [384.0, 704.0]
            window.l_scale.setValue(0.25)
            window.project.snap_pivot_to_anchor(window.asset, window.layer)
            window.select_layer(legs.id)
            window.asset.export_name = "warrior-idle-e-cotton-straw-demo"
        else:
            window.new_asset("주인공 정지 E", "character", 192, 192, (96, 176), "hero-move")
            window.import_png(idle, crop=e_cell, align_to_anchor=(96, 176), name="정지 E", slot="body")
            window.import_png(repo / "assets/warrior-walk-e-v1.png", crop=[0, 0, 192, 192],
                              align_to_anchor=(96, 176), name="걷기 E 0 (비교)", slot="reference")
        window.refresh_all()
        window.canvas.fit()
        app.processEvents()
        window.grab().save(str(args.out / "studio-main.png"))

        image = open_png(repo / "assets/warrior-walk-se-v1.png")
        dialog = CellImportDialog(repo / "assets/warrior-walk-se-v1.png", image, specs, (192, 192), (96, 176))
        dialog.cell.setCurrentIndex(3)
        dialog.show()
        app.processEvents()
        dialog.grab().save(str(args.out / "studio-cell-import.png"))
        dialog.close()

        check = CheckDialog(window.run_check(), "내보내기 전 검사", True)
        check.show()
        app.processEvents()
        check.grab().save(str(args.out / "studio-check.png"))
        check.close()

        # Same check with a Korean output name (differs from the assets/ naming rule).
        window.asset.export_name = "주인공-시안"
        check = CheckDialog(window.run_check(), "내보내기 전 검사 (경고 예시)", True)
        check.show()
        app.processEvents()
        check.grab().save(str(args.out / "studio-check-warnings.png"))
        check.close()

        # ---- phase 2: one 8-direction asset -----------------------------
        order = specs.direction_order
        crops = {v: [i % 4 * 192, i // 4 * 192, 192, 192] for i, v in enumerate(order)}
        window.new_asset("주인공 정지 8방향", "character", 192, 192, (96, 176), "hero-move", order)
        window.asset.export_name = "warrior-idle-8dir-demo"
        if args.body_sheet and args.legs:
            window.import_sheet(idle, crops, "기존 주인공 (비교)", slot="reference")
            for layer in window.asset.layers:
                layer.visible = False
            window.import_sheet(args.body_sheet, crops, "몸 (다리 제거)", slot="body")
            window.set_view("E")
            window.import_png(args.legs, name="무명 바지·짚신 (4배 원화)", slot="bottom", group="무명 바지·짚신")
            window.layer.pivot = [384.0, 704.0]
            window.l_scale.setValue(0.25)
            window.project.snap_pivot_to_anchor(window.asset, window.layer)
        else:
            window.import_sheet(idle, crops, "정지", slot="body")
            window.set_view("E")
        window.c_onion.setCurrentIndex(window.c_onion.findData("SE"))
        window.refresh_all()
        window.canvas.fit()
        window.overview_dock.raise_()
        app.processEvents()
        window.grab().save(str(args.out / "studio2-eight-directions.png"))

        report = window.run_check()
        check = CheckDialog(report, "내보내기 전 검사 (8방향)", True)
        check.resize(820, 360)
        check.show()
        app.processEvents()
        check.grab().save(str(args.out / "studio2-check.png"))
        check.close()
        window.dirty = False
        window.close()
    print(f"saved to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
