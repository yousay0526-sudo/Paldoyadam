"""8방향 한눈에 보기: all directions in the game's compass layout with guides."""

from __future__ import annotations

from PySide6.QtCore import QPointF, Qt, Signal
from PySide6.QtGui import QColor, QPainter, QPen, QPixmap
from PySide6.QtWidgets import QGridLayout, QLabel, QVBoxLayout, QWidget

from ..imaging import ImageCache, compose, measure
from ..model import Asset, Project
from .canvas import pil_to_qimage

# Compass placement used by docs/HERO_DIRECTIONS.md (centre left empty).
COMPASS = {"NW": (0, 0), "N": (0, 1), "NE": (0, 2), "W": (1, 0), "E": (1, 2),
           "SW": (2, 0), "S": (2, 1), "SE": (2, 2)}


class _Cell(QLabel):
    clicked = Signal(str)

    def __init__(self, view: str):
        super().__init__()
        self.view = view
        self.setAlignment(Qt.AlignCenter)
        self.setCursor(Qt.PointingHandCursor)

    def mousePressEvent(self, _event):
        self.clicked.emit(self.view)


class Overview(QWidget):
    viewClicked = Signal(str)

    def __init__(self, cache: ImageCache, parent=None):
        super().__init__(parent)
        self.cache = cache
        self.cells: dict[str, _Cell] = {}
        grid = QGridLayout()
        grid.setSpacing(4)
        for view, (r, c) in COMPASS.items():
            cell = _Cell(view)
            cell.clicked.connect(self.viewClicked)
            self.cells[view] = cell
            grid.addWidget(cell, r, c)
        self.center = QLabel()
        self.center.setAlignment(Qt.AlignCenter)
        self.center.setWordWrap(True)
        grid.addWidget(self.center, 1, 1)
        self.summary = QLabel()
        self.summary.setWordWrap(True)
        self.summary.setTextInteractionFlags(Qt.TextSelectableByMouse)
        layout = QVBoxLayout(self)
        layout.addLayout(grid)
        layout.addWidget(self.summary)
        layout.addStretch(1)

    def refresh(self, project: Project | None, asset: Asset | None, current: str, size: int = 92):
        if asset is None or not asset.directions:
            for cell in self.cells.values():
                cell.clear()
                cell.setToolTip("")
            self.center.setText("8방향 에셋을 고르면\n여기에 방향별로\n나란히 표시됩니다.")
            self.summary.clear()
            return
        self.center.setText("방향을 누르면\n그 방향을 편집합니다.\n\n초록: 발 기준선\n하늘: 중심축\n분홍: 측정 바닥·발중심")
        rows = []
        bottoms, feet = [], []
        scale = size / max(asset.width, asset.height)
        for view, cell in self.cells.items():
            if view not in asset.directions:
                cell.clear()
                continue
            image = compose(project, asset, self.cache, view=view)
            m = measure(image)
            pixmap = QPixmap(round(asset.width * scale), round(asset.height * scale))
            pixmap.fill(QColor(150, 150, 150) if view != current else QColor(170, 160, 120))
            p = QPainter(pixmap)
            p.drawImage(pixmap.rect(), pil_to_qimage(image))
            ax, ay = asset.anchor[0] * scale, asset.anchor[1] * scale
            p.setPen(QPen(QColor(80, 220, 255), 1))
            p.drawLine(QPointF(ax, 0), QPointF(ax, pixmap.height()))
            p.setPen(QPen(QColor(60, 200, 80), 1))
            p.drawLine(QPointF(0, ay), QPointF(pixmap.width(), ay))
            if m.bbox is not None:
                p.setPen(QPen(QColor(255, 60, 190), 1))
                cy = m.contact_y * scale
                p.drawLine(QPointF(0, cy), QPointF(pixmap.width(), cy))
                fx = m.foot_x * scale
                p.drawLine(QPointF(fx, cy - 8), QPointF(fx, cy + 3))
                bottoms.append(m.contact_y)
                feet.append(m.foot_x)
            p.setPen(QColor(20, 20, 20))
            p.drawText(QPointF(4, 14), view + ("  ◀ 편집 중" if view == current else ""))
            p.end()
            cell.setPixmap(pixmap)
            if m.bbox is None:
                cell.setToolTip(f"{view}: 그림 없음")
                rows.append(f"{view}: 그림 없음")
            else:
                text = f"{view}: 바닥 y={m.contact_y}, 발중심 x={m.foot_x:.1f}, 가장자리 여백 {m.margin}px"
                cell.setToolTip(text)
                rows.append(text)
        if bottoms:
            rows.append(f"방향 간 차이: 바닥 {min(bottoms)}~{max(bottoms)} ({max(bottoms) - min(bottoms)}px), "
                        f"발중심 {min(feet):.1f}~{max(feet):.1f} ({max(feet) - min(feet):.1f}px)")
        self.summary.setText("\n".join(rows))
