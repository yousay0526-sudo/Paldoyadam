"""Small dialogs: new asset, atlas cell import, check results."""

from __future__ import annotations

from pathlib import Path

from PIL import Image
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPainter, QPixmap
from PySide6.QtWidgets import (QCheckBox, QComboBox, QDialog, QDialogButtonBox, QDoubleSpinBox,
                               QFormLayout, QLabel, QLineEdit, QSpinBox, QTableWidget,
                               QTableWidgetItem, QVBoxLayout, QHeaderView)

from ..checks import LEVEL_LABEL
from ..game_specs import Specs, sheet_layout
from ..model import EIGHT_DIRECTIONS, KINDS, SLOTS
from .canvas import pil_to_qimage


class NewAssetDialog(QDialog):
    def __init__(self, specs: Specs, parent=None):
        super().__init__(parent)
        self.setWindowTitle("새 에셋")
        self.specs = specs
        form = QFormLayout()
        self.name = QLineEdit("새 에셋")
        self.kind = QComboBox()
        for key, label in KINDS.items():
            self.kind.addItem(label, key)
        self.preset = QComboBox()
        for p in specs.presets:
            self.preset.addItem(p.label, p.key)
        self.width_box, self.height_box = QSpinBox(), QSpinBox()
        for box in (self.width_box, self.height_box):
            box.setRange(1, 4096)
        self.ax, self.ay = QDoubleSpinBox(), QDoubleSpinBox()
        for box in (self.ax, self.ay):
            box.setRange(-4096, 8192)
            box.setDecimals(2)
        self.source = QLabel()
        self.source.setWordWrap(True)
        self.eight = QCheckBox("8방향 에셋 (N·NE·E·SE·S·SW·W·NW, 게임의 방향 순서)")
        form.addRow("이름", self.name)
        form.addRow("종류", self.kind)
        form.addRow("규격 프리셋", self.preset)
        form.addRow("캔버스 너비", self.width_box)
        form.addRow("캔버스 높이", self.height_box)
        form.addRow("발 기준점 X", self.ax)
        form.addRow("발 기준점 Y", self.ay)
        form.addRow("근거", self.source)
        form.addRow(self.eight)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout = QVBoxLayout(self)
        layout.addLayout(form)
        if specs.notes:
            note = QLabel("\n".join(specs.notes))
            note.setWordWrap(True)
            layout.addWidget(note)
        layout.addWidget(buttons)
        self.preset.currentIndexChanged.connect(self._apply_preset)
        self._apply_preset()

    def _apply_preset(self):
        p = self.specs.preset(self.preset.currentData())
        if p is None:
            return
        self.width_box.setValue(p.width)
        self.height_box.setValue(p.height)
        self.ax.setValue(p.anchor[0])
        self.ay.setValue(p.anchor[1])
        self.source.setText(p.source)

    def values(self) -> dict:
        return {"name": self.name.text().strip() or "새 에셋", "kind": self.kind.currentData(),
                "width": self.width_box.value(), "height": self.height_box.value(),
                "anchor": (self.ax.value(), self.ay.value()), "preset": self.preset.currentData(),
                "directions": list(self.specs.direction_order or EIGHT_DIRECTIONS) if self.eight.isChecked() else []}


class CellImportDialog(QDialog):
    """Pick one cell of a sprite sheet. The whole sheet is still copied as the source."""

    def __init__(self, path: Path, image: Image.Image, specs: Specs, canvas_size: tuple[int, int],
                 canvas_anchor: tuple[float, float], parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"셀 가져오기 — {path.name}")
        self.image = image
        layout_guess = None
        if specs.repo is not None:
            try:
                layout_guess = sheet_layout(specs, path.resolve().relative_to(specs.repo).as_posix())
            except ValueError:
                layout_guess = None
        self.labels = layout_guess[2] if layout_guess else None
        cols, rows = (layout_guess[0], layout_guess[1]) if layout_guess else self._guess(image)
        form = QFormLayout()
        self.cols, self.rows = QSpinBox(), QSpinBox()
        self.cols.setRange(1, 64)
        self.rows.setRange(1, 64)
        self.cols.setValue(cols)
        self.rows.setValue(rows)
        self.cell = QComboBox()
        self.align = QCheckBox("셀이 캔버스와 같은 크기면 셀 좌표 그대로 배치 (발 기준점 일치)")
        self.align.setChecked(True)
        self.canvas_size, self.canvas_anchor = canvas_size, canvas_anchor
        self.info = QLabel()
        self.preview = QLabel()
        self.preview.setMinimumSize(200, 200)
        self.preview.setAlignment(Qt.AlignCenter)
        form.addRow("열 수", self.cols)
        form.addRow("행 수", self.rows)
        form.addRow("셀", self.cell)
        form.addRow(self.align)
        form.addRow("셀 정보", self.info)
        layout = QVBoxLayout(self)
        known = "저장소의 알려진 실행 시트 배치를 적용했습니다." if layout_guess else \
            "알려진 시트가 아니어서 192px 격자로 추정했습니다. 열·행을 확인하세요."
        layout.addWidget(QLabel(known))
        layout.addLayout(form)
        layout.addWidget(self.preview)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        self.cols.valueChanged.connect(self._refill)
        self.rows.valueChanged.connect(self._refill)
        self.cell.currentIndexChanged.connect(self._show)
        self._refill()

    @staticmethod
    def _guess(image: Image.Image) -> tuple[int, int]:
        if image.width % 192 == 0 and image.height % 192 == 0:
            return image.width // 192, image.height // 192
        return 1, 1

    def _refill(self):
        self.cell.blockSignals(True)
        self.cell.clear()
        count = self.cols.value() * self.rows.value()
        for i in range(count):
            label = self.labels[i] if self.labels and len(self.labels) == count else f"셀 {i}"
            self.cell.addItem(f"{i}: {label}", i)
        self.cell.blockSignals(False)
        self._show()

    def crop(self) -> list[int]:
        w, h = self.image.width // self.cols.value(), self.image.height // self.rows.value()
        i = self.cell.currentData() or 0
        return [i % self.cols.value() * w, i // self.cols.value() * h, w, h]

    def label(self) -> str:
        return self.cell.currentText().split(": ", 1)[-1]

    def align_anchor(self) -> tuple[float, float] | None:
        _, _, w, h = self.crop()
        if self.align.isChecked() and (w, h) == tuple(self.canvas_size):
            return tuple(self.canvas_anchor)
        return None

    def _show(self):
        x, y, w, h = self.crop()
        exact = self.image.width % self.cols.value() == 0 and self.image.height % self.rows.value() == 0
        self.info.setText(f"영역 ({x},{y}) {w}×{h}" + ("" if exact else " — 시트 크기가 격자로 나누어떨어지지 않음"))
        cell = self.image.crop((x, y, x + w, y + h)).convert("RGBA")
        scale = max(1, 192 // max(w, h, 1))
        cell = cell.resize((w * scale, h * scale), Image.Resampling.NEAREST)
        pixmap = QPixmap(cell.width, cell.height)
        pixmap.fill(QColor(150, 150, 150))
        p = QPainter(pixmap)
        p.drawImage(0, 0, pil_to_qimage(cell))
        p.end()
        self.preview.setPixmap(pixmap.scaled(240, 240, Qt.KeepAspectRatio, Qt.FastTransformation))


class CheckDialog(QDialog):
    COLORS = {"ok": QColor(60, 160, 80), "warn": QColor(200, 150, 30), "error": QColor(200, 60, 60)}

    def __init__(self, report: list[dict], title: str, ask_export: bool, parent=None):
        super().__init__(parent)
        self.setWindowTitle(title)
        self.resize(720, 320)
        self.report = report
        self.on_pick = None   # callable(item) — jump to the direction/layer of a row
        table = QTableWidget(len(report), 4)
        table.setHorizontalHeaderLabels(["결과", "방향", "레이어", "내용"])
        table.horizontalHeader().setSectionResizeMode(3, QHeaderView.Stretch)
        table.verticalHeader().setVisible(False)
        for row, item in enumerate(report):
            level = QTableWidgetItem(LEVEL_LABEL[item["level"]])
            level.setForeground(self.COLORS[item["level"]])
            table.setItem(row, 0, level)
            table.setItem(row, 1, QTableWidgetItem(item.get("view") or "-"))
            table.setItem(row, 2, QTableWidgetItem(item["layer"] or "(에셋 전체)"))
            message = QTableWidgetItem(item["message"])
            message.setToolTip(item["message"])
            table.setItem(row, 3, message)
        table.setWordWrap(True)
        table.resizeRowsToContents()
        table.setEditTriggers(QTableWidget.NoEditTriggers)
        table.cellDoubleClicked.connect(self._picked)
        self.table = table
        layout = QVBoxLayout(self)
        layout.addWidget(table)
        layout.addWidget(QLabel("행을 두 번 누르면 해당 방향·레이어로 이동합니다."))
        layout.addWidget(QLabel("자동 검사는 기술 항목만 확인합니다. 그림의 완성도는 직접 판단해야 합니다."))
        errors = any(i["level"] == "error" for i in report)
        if ask_export and not errors:
            buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
            buttons.button(QDialogButtonBox.Ok).setText("내보내기")
        else:
            buttons = QDialogButtonBox(QDialogButtonBox.Close)
            if ask_export:
                layout.addWidget(QLabel("오류가 있어 내보낼 수 없습니다."))
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def _picked(self, row: int, _column: int):
        if self.on_pick is not None and 0 <= row < len(self.report):
            self.on_pick(self.report[row])


class SheetImportDialog(QDialog):
    """Import one sheet into all eight direction views of an asset at once."""

    MODES = {"cells": "셀 순서 = 방향 순서 (정지 4×2 시트 등)",
             "rows": "행 = 방향, 열 = 프레임 (몬스터 걷기·공격 시트)"}

    def __init__(self, path: Path, image: Image.Image, specs: Specs, directions: list[str],
                 groups: list[str], parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"8방향 시트 가져오기 — {path.name}")
        self.image, self.directions = image, directions
        layout_guess = None
        if specs.repo is not None:
            try:
                layout_guess = sheet_layout(specs, path.resolve().relative_to(specs.repo).as_posix())
            except ValueError:
                layout_guess = None
        form = QFormLayout()
        self.mode = QComboBox()
        for key, label in self.MODES.items():
            self.mode.addItem(label, key)
        self.cols, self.rows, self.frame = QSpinBox(), QSpinBox(), QSpinBox()
        for box in (self.cols, self.rows):
            box.setRange(1, 64)
        cols, rows = (layout_guess[0], layout_guess[1]) if layout_guess else CellImportDialog._guess(image)
        self.cols.setValue(cols)
        self.rows.setValue(rows)
        if rows == len(directions) and cols > 1:
            self.mode.setCurrentIndex(1)
        self.frame.setRange(0, 63)
        self.name = QLineEdit(path.stem)
        self.group = QComboBox()
        self.group.setEditable(True)
        self.group.addItem("")
        for g in groups:
            self.group.addItem(g)
        self.slot = QComboBox()
        for key, label in SLOTS.items():
            self.slot.addItem(label, key)
        self.info = QLabel()
        self.info.setWordWrap(True)
        form.addRow("배치", self.mode)
        form.addRow("열 수", self.cols)
        form.addRow("행 수", self.rows)
        form.addRow("프레임(열)", self.frame)
        form.addRow("레이어 이름", self.name)
        form.addRow("장비 묶음 (비우면 몸)", self.group)
        form.addRow("부위", self.slot)
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(f"방향 순서: {' '.join(directions)}"))
        layout.addLayout(form)
        layout.addWidget(self.info)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        self.ok_button = buttons.button(QDialogButtonBox.Ok)
        for w in (self.cols, self.rows, self.frame):
            w.valueChanged.connect(self._update)
        self.mode.currentIndexChanged.connect(self._update)
        self._update()

    def crops(self) -> dict[str, list[int]] | None:
        cols, rows = self.cols.value(), self.rows.value()
        w, h = self.image.width // cols, self.image.height // rows
        out = {}
        for i, d in enumerate(self.directions):
            if self.mode.currentData() == "cells":
                index = i
            else:
                index = i * cols + self.frame.value()
            if self.mode.currentData() == "rows" and (self.frame.value() >= cols or i >= rows):
                return None
            if index >= cols * rows:
                return None
            out[d] = [index % cols * w, index // cols * h, w, h]
        return out

    def _update(self):
        self.frame.setEnabled(self.mode.currentData() == "rows")
        crops = self.crops()
        self.ok_button.setEnabled(crops is not None)
        if crops is None:
            self.info.setText("이 배치로는 8방향을 모두 채울 수 없습니다.")
        else:
            w, h = next(iter(crops.values()))[2:]
            self.info.setText(f"방향마다 {w}×{h} 셀 하나씩 레이어를 만듭니다. 셀이 캔버스와 같은 크기면 셀 좌표 그대로 놓입니다.")

    def values(self) -> dict:
        return {"crops": self.crops(), "name": self.name.text().strip() or "시트",
                "group": self.group.currentText().strip(), "slot": self.slot.currentData()}
