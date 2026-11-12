"""Small dialogs: new asset, cell/sheet import, check results, sheet preview, templates, artwork import."""

from __future__ import annotations

from pathlib import Path

from PIL import Image
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPainter, QPixmap
from PySide6.QtWidgets import (QCheckBox, QComboBox, QDialog, QDialogButtonBox, QDoubleSpinBox,
                               QFormLayout, QHBoxLayout, QHeaderView, QLabel, QLineEdit, QListWidget,
                               QListWidgetItem, QScrollArea, QSpinBox, QTableWidget, QTableWidgetItem,
                               QVBoxLayout, QWidget)

from ..autofix import background_colour, clean_alpha_noise, remove_background
from ..checks import LEVEL_LABEL
from ..game_specs import Specs, sheet_layout
from ..model import EIGHT_DIRECTIONS, KINDS, SLOTS
from ..templates import Template
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
        table = QTableWidget(len(report), 5)
        table.setHorizontalHeaderLabels(["결과", "방향", "프레임", "레이어", "내용"])
        table.horizontalHeader().setSectionResizeMode(4, QHeaderView.Stretch)
        table.verticalHeader().setVisible(False)
        for row, item in enumerate(report):
            level = QTableWidgetItem(LEVEL_LABEL[item["level"]])
            level.setForeground(self.COLORS[item["level"]])
            table.setItem(row, 0, level)
            table.setItem(row, 1, QTableWidgetItem(item.get("view") or "-"))
            frame = item.get("frame")
            table.setItem(row, 2, QTableWidgetItem("-" if frame is None else str(frame + 1)))
            table.setItem(row, 3, QTableWidgetItem(item["layer"] or "(에셋 전체)"))
            message = QTableWidgetItem(item["message"])
            message.setToolTip(item["message"])
            table.setItem(row, 4, message)
        table.setWordWrap(True)
        table.resizeRowsToContents()
        table.setEditTriggers(QTableWidget.NoEditTriggers)
        table.cellDoubleClicked.connect(self._picked)
        self.table = table
        layout = QVBoxLayout(self)
        layout.addWidget(table)
        layout.addWidget(QLabel("행을 두 번 누르면 해당 방향·프레임·레이어로 이동합니다."))
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
    """Import many cells of one sheet at once: into directions, frames, or both."""

    MODES = {"cells": "셀 순서 = 방향 순서 (정지 4×2 시트 등) — 현재 프레임에",
             "rows": "행 = 방향, 열 하나만 (몬스터 시트의 한 프레임) — 현재 프레임에",
             "frames": "셀 순서 = 프레임 순서 (걷기 4×2, 공격·무공 3×2) — 현재 방향에",
             "rows_all": "행 = 방향, 열 = 프레임 (몬스터 걷기·공격 시트 전체)"}

    def __init__(self, path: Path, image: Image.Image, specs: Specs, directions: list[str],
                 groups: list[str], parent=None, view: str = "", frame: int = 0):
        super().__init__(parent)
        self.setWindowTitle(f"시트 한 번에 가져오기 — {path.name}")
        self.image, self.directions = image, directions
        self.view, self.current_frame = view, frame
        layout_guess = None
        if specs.repo is not None:
            try:
                layout_guess = sheet_layout(specs, path.resolve().relative_to(specs.repo).as_posix())
            except ValueError:
                layout_guess = None
        form = QFormLayout()
        self.mode = QComboBox()
        for key, label in self.MODES.items():
            if directions or key == "frames":
                self.mode.addItem(label, key)
        self.cols, self.rows, self.frame, self.count = QSpinBox(), QSpinBox(), QSpinBox(), QSpinBox()
        for box in (self.cols, self.rows):
            box.setRange(1, 64)
        cols, rows = (layout_guess[0], layout_guess[1]) if layout_guess else CellImportDialog._guess(image)
        self.cols.setValue(cols)
        self.rows.setValue(rows)
        if directions and rows == len(directions) and cols > 1:
            self.mode.setCurrentIndex(self.mode.findData("rows_all"))
        elif not directions or "walk" in path.stem or "attack" in path.stem or (cols, rows) == (3, 2):
            self.mode.setCurrentIndex(self.mode.findData("frames"))
        self.frame.setRange(0, 63)
        self.count.setRange(1, 64 * 64)
        self.count.setValue(cols * rows)
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
        form.addRow("가져올 열 (0부터)", self.frame)
        form.addRow("프레임 수", self.count)
        form.addRow("레이어 이름", self.name)
        form.addRow("장비 묶음 (비우면 몸)", self.group)
        form.addRow("부위", self.slot)
        layout = QVBoxLayout(self)
        if directions:
            layout.addWidget(QLabel(f"방향 순서: {' '.join(directions)}  (현재 방향 {view}, 프레임 {frame + 1})"))
        layout.addLayout(form)
        layout.addWidget(self.info)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        self.ok_button = buttons.button(QDialogButtonBox.Ok)
        for w in (self.cols, self.rows, self.frame, self.count):
            w.valueChanged.connect(self._update)
        self.cols.valueChanged.connect(lambda: self.count.setValue(self.cols.value() * self.rows.value()))
        self.rows.valueChanged.connect(lambda: self.count.setValue(self.cols.value() * self.rows.value()))
        self.mode.currentIndexChanged.connect(self._update)
        self._update()

    def crops(self) -> dict[tuple[str, int], list[int]] | None:
        """(direction, frame) -> crop rectangle, or None when the layout does not fit."""
        cols, rows = self.cols.value(), self.rows.value()
        w, h = self.image.width // cols, self.image.height // rows
        mode = self.mode.currentData()

        def rect(index):
            return [index % cols * w, index // cols * h, w, h]

        if mode == "frames":
            if self.count.value() > cols * rows:
                return None
            return {(self.view, f): rect(f) for f in range(self.count.value())}
        if rows < len(self.directions) and mode != "cells":
            return None
        out = {}
        for i, d in enumerate(self.directions):
            if mode == "cells":
                if i >= cols * rows:
                    return None
                out[(d, self.current_frame)] = rect(i)
            elif mode == "rows":
                if self.frame.value() >= cols:
                    return None
                out[(d, self.current_frame)] = rect(i * cols + self.frame.value())
            else:
                for f in range(cols):
                    out[(d, f)] = rect(i * cols + f)
        return out

    def _update(self):
        mode = self.mode.currentData()
        self.frame.setEnabled(mode == "rows")
        self.count.setEnabled(mode == "frames")
        crops = self.crops()
        self.ok_button.setEnabled(crops is not None)
        if crops is None:
            self.info.setText("이 배치로는 셀을 다 채울 수 없습니다 (시트의 행·열 수를 확인하세요).")
            return
        w, h = next(iter(crops.values()))[2:]
        frames = len({f for _, f in crops})
        views = len({v for v, _ in crops})
        self.info.setText(f"{w}×{h} 셀 {len(crops)}개로 레이어를 만듭니다 (방향 {views}개 × 프레임 {frames}개). "
                          "모자란 프레임은 새로 늘립니다. 셀이 캔버스와 같은 크기면 셀 좌표 그대로 놓입니다.")

    def values(self) -> dict:
        return {"crops": self.crops(), "name": self.name.text().strip() or "시트",
                "group": self.group.currentText().strip(), "slot": self.slot.currentData()}


class SheetPreviewDialog(QDialog):
    """Shows the exact images that 내보내기 would write, on a checker background."""

    def __init__(self, sheets: list[tuple[str, Image.Image]], layout_label: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle("시트 미리보기")
        self.resize(900, 640)
        body = QVBoxLayout()
        holder = QWidget()
        holder.setLayout(body)
        self.labels = []
        for name, image in sheets:
            body.addWidget(QLabel(f"{name}  ({image.width}×{image.height})"))
            label = QLabel()
            label.setPixmap(_checker_pixmap(image))
            body.addWidget(label)
            self.labels.append(label)
        body.addStretch(1)
        scroll = QScrollArea()
        scroll.setWidget(holder)
        scroll.setWidgetResizable(True)
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(f"형식: {layout_label}"))
        layout.addWidget(scroll)
        buttons = QDialogButtonBox(QDialogButtonBox.Close)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)


def _checker_pixmap(image: Image.Image, tile: int = 16) -> QPixmap:
    pixmap = QPixmap(image.width, image.height)
    painter = QPainter(pixmap)
    for y in range(0, image.height, tile):
        for x in range(0, image.width, tile):
            painter.fillRect(x, y, tile, tile, QColor(204, 204, 204) if (x // tile + y // tile) % 2 == 0
                             else QColor(153, 153, 153))
    painter.drawImage(0, 0, pil_to_qimage(image))
    painter.end()
    return pixmap


class TemplateDialog(QDialog):
    """Pick a template (built-in from the repository, or a user template) for a new asset."""

    def __init__(self, entries: list[tuple[str, Template]], notes: list[str], parent=None):
        super().__init__(parent)
        self.setWindowTitle("템플릿에서 새 에셋")
        self.resize(760, 520)
        self.entries = entries   # (group label, Template)
        self.list = QListWidget()
        for group, template in entries:
            item = QListWidgetItem(f"[{group}] {template.label}")
            item.setToolTip(template.summary())
            self.list.addItem(item)
        self.detail = QLabel()
        self.detail.setWordWrap(True)
        self.detail.setTextInteractionFlags(Qt.TextSelectableByMouse)
        self.name = QLineEdit()
        self.reference = QCheckBox("기존 실행 그림을 참고 레이어로 넣기 (내보내기 제외, 원본은 복사본만 사용)")
        form = QFormLayout()
        form.addRow("에셋 이름", self.name)
        form.addRow(self.reference)
        layout = QVBoxLayout(self)
        layout.addWidget(self.list, 2)
        layout.addWidget(self.detail, 1)
        layout.addLayout(form)
        if notes:
            note = QLabel("\n".join(notes))
            note.setWordWrap(True)
            note.setStyleSheet("color: #8a6d1d")
            layout.addWidget(note)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        self.ok_button = buttons.button(QDialogButtonBox.Ok)
        self.list.currentRowChanged.connect(self._picked)
        if entries:
            self.list.setCurrentRow(0)
        else:
            self.ok_button.setEnabled(False)

    def template(self):
        row = self.list.currentRow()
        return self.entries[row][1] if 0 <= row < len(self.entries) else None

    def _picked(self, row: int):
        t = self.template()
        if t is None:
            return
        lines = [f"<b>{t.label}</b>", t.summary(), f"근거: {t.source or '-'}"]
        if t.notes:
            lines.append(t.notes)
        lines.append(f"참고 그림 칸: {len(t.reference)}개" if t.reference else "참고 그림 없음")
        self.detail.setText("<br>".join(lines))
        self.reference.setEnabled(bool(t.reference))
        self.reference.setChecked(bool(t.reference))
        self.name.setText(t.label.split(" (")[0])

    def values(self) -> dict:
        return {"template": self.template(), "name": self.name.text().strip() or "새 에셋",
                "with_reference": self.reference.isChecked() and self.reference.isEnabled()}


class ArtworkImportDialog(QDialog):
    """GPT 원화 가져오기: local, step-by-step clean-up of one downloaded artwork before it becomes a layer."""

    def __init__(self, path: Path, image: Image.Image, has_reference: bool, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"원화 가져오기 — {path.name}")
        self.image = image.convert("RGBA")
        self.remove_bg = QCheckBox("배경 제거 (네 모서리 색과 이어진 부분만)")
        self.tolerance = QSpinBox()
        self.tolerance.setRange(1, 128)
        self.tolerance.setValue(24)
        self.feather = QCheckBox("경계 1px 부드럽게")
        self.feather.setChecked(True)
        self.noise = QCheckBox("알파 1~4 잡점 지우기")
        self.noise.setChecked(True)
        self.height_mode = QComboBox()
        self.height_mode.addItem("키 그대로", "")
        if has_reference:
            self.height_mode.addItem("참고 레이어(기존 그림)와 같은 키", "reference")
        self.height_mode.addItem("키를 숫자로 (px)", "number")
        self.height_px = QSpinBox()
        self.height_px.setRange(4, 4096)
        self.height_px.setValue(136)
        self.snap = QCheckBox("발 맞추기 (그림 바닥·발 중심을 발 기준점에)")
        self.snap.setChecked(True)
        self.slot = QComboBox()
        for key, label in SLOTS.items():
            self.slot.addItem(label, key)
        self.before, self.after = QLabel(), QLabel()
        self.info = QLabel()
        self.info.setWordWrap(True)
        colour = background_colour(self.image)
        self.remove_bg.setChecked(colour is not None)
        form = QFormLayout()
        form.addRow(self.remove_bg)
        form.addRow("배경 색 허용 차이", self.tolerance)
        form.addRow(self.feather)
        form.addRow(self.noise)
        form.addRow("키", self.height_mode)
        form.addRow("목표 키 (px)", self.height_px)
        form.addRow(self.snap)
        form.addRow("부위", self.slot)
        pics = QHBoxLayout()
        pics.addWidget(self.before)
        pics.addWidget(self.after)
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(f"원본 {image.width}×{image.height}, {image.mode}. 원본 파일은 바꾸지 않고, "
                                "보정한 그림은 프로젝트 sources/에 새 사본으로 저장합니다. 네트워크는 쓰지 않습니다."))
        layout.addLayout(pics)
        layout.addWidget(self.info)
        layout.addLayout(form)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        for w in (self.remove_bg, self.feather, self.noise):
            w.toggled.connect(self._update)
        self.tolerance.valueChanged.connect(self._update)
        self.height_mode.currentIndexChanged.connect(self._update)
        self._update()

    def processed(self) -> tuple[Image.Image, list[str]]:
        image, notes = self.image, []
        if self.remove_bg.isChecked():
            result = remove_background(image, self.tolerance.value(), self.feather.isChecked())
            image = result.image
            notes.append(f"배경 {result.colour} 제거 {result.removed}px" if result.colour else result.reason)
        if self.noise.isChecked():
            image, changed = clean_alpha_noise(image)
            notes.append(f"잡점 {changed}px")
        return image, notes

    def _update(self):
        self.height_px.setEnabled(self.height_mode.currentData() == "number")
        image, notes = self.processed()
        side = 220
        for label, im in ((self.before, self.image), (self.after, image)):
            k = side / max(im.width, im.height)
            small = im.resize((max(1, round(im.width * k)), max(1, round(im.height * k))))
            label.setPixmap(_checker_pixmap(small, 10))
        self.info.setText("왼쪽 원본 · 오른쪽 보정 결과.  " + ", ".join(notes))

    def values(self) -> dict:
        mode = self.height_mode.currentData()
        return {"remove_bg": self.remove_bg.isChecked(), "tolerance": self.tolerance.value(),
                "feather": self.feather.isChecked(), "clean_noise": self.noise.isChecked(),
                "height": float(self.height_px.value()) if mode == "number" else (mode or None),
                "snap": self.snap.isChecked(), "slot": self.slot.currentData()}
