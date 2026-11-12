#!/usr/bin/env python3
"""Start Paldo Asset Studio.

    python tools/asset_studio/run_studio.py [project.paldo.json | project folder]
"""

from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))


def main(argv: list[str]) -> int:
    try:
        from PySide6.QtWidgets import QApplication, QMessageBox
    except ImportError:
        print("PySide6가 필요합니다: python -m pip install -r tools/asset_studio/requirements.txt",
              file=sys.stderr)
        return 1
    from paldo_studio.game_specs import find_repo, load_specs
    from paldo_studio.ui.main_window import MainWindow

    app = QApplication(argv)
    app.setApplicationName("Paldo Asset Studio")
    window = MainWindow(load_specs(find_repo(HERE)))
    window.show()
    if len(argv) > 1:
        try:
            window.open_project(Path(argv[1]))
        except (OSError, ValueError, KeyError, TypeError) as exc:
            QMessageBox.critical(window, "열기 실패", str(exc))
    return app.exec()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
