"""Main window: left (assets, layers), centre (canvas), right (properties), bottom (view bar)."""

from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QAction, QActionGroup, QColor, QKeySequence
from PySide6.QtWidgets import (QAbstractItemView, QCheckBox, QComboBox, QDockWidget, QDoubleSpinBox,
                               QFileDialog, QFormLayout, QGroupBox, QHBoxLayout, QInputDialog, QLayout,
                               QLabel, QLineEdit, QListWidget, QListWidgetItem, QMainWindow,
                               QMessageBox, QPushButton, QScrollArea, QSlider, QSpinBox,
                               QToolBar, QTreeWidget, QTreeWidgetItem, QVBoxLayout, QWidget)

from .. import __version__
from ..checks import check_asset, has_errors
from ..game_specs import Specs
from ..imaging import ImageCache, compose, export_asset, inspect, open_png
from ..model import KINDS, PROJECT_FILE, SLOTS, History, Project
from .canvas import BACKGROUNDS, Canvas, pil_to_qimage, preview_pixmap
from .dialogs import CellImportDialog, CheckDialog, NewAssetDialog


def _short(text: str, limit: int = 34) -> str:
    return text if len(text) <= limit else text[:limit - 12] + "…" + text[-11:]


class MainWindow(QMainWindow):
    def __init__(self, specs: Specs):
        super().__init__()
        self.specs = specs
        self.project: Project | None = None
        self.history: History | None = None
        self.asset_id = ""
        self.layer_id = ""
        self.dirty = False
        self.cache = ImageCache()
        self._syncing = False
        self.resize(1440, 900)

        self.canvas = Canvas(self.cache)
        self.canvas.ground_image = self._ground_sample()
        self.setCentralWidget(self.canvas)
        self.canvas.layerSelected.connect(self.select_layer)
        self.canvas.aboutToChange.connect(self._checkpoint)
        self.canvas.changed.connect(self._changed_from_canvas)
        self.canvas.cursorMoved.connect(self._cursor)
        self.canvas.zoomChanged.connect(lambda z: self.zoom_label.setText(f"{z * 100:.0f}%"))

        self._build_actions()
        self._build_left()
        self._build_right()
        self._build_bottom()
        self.refresh_all()

    # ================================================================ build
    def _build_actions(self):
        menu = self.menuBar()
        f = menu.addMenu("파일(&F)")
        self.act_new = self._action(f, "새 프로젝트…", QKeySequence.New, self.ask_new_project)
        self.act_open = self._action(f, "프로젝트 열기…", QKeySequence.Open, self.ask_open_project)
        self.act_save = self._action(f, "저장", QKeySequence.Save, self.save_project)
        self.act_save_as = self._action(f, "다른 이름으로 저장…", QKeySequence("Ctrl+Shift+S"), self.ask_save_as)
        f.addSeparator()
        self._action(f, "종료", QKeySequence.Quit, self.close)

        e = menu.addMenu("편집(&E)")
        self.act_undo = self._action(e, "실행 취소", QKeySequence.Undo, self.undo)
        self.act_redo = self._action(e, "다시 실행", QKeySequence("Ctrl+Y"), self.redo)
        self.act_redo.setShortcuts([QKeySequence("Ctrl+Y"), QKeySequence("Ctrl+Shift+Z")])

        a = menu.addMenu("에셋(&A)")
        self.act_new_asset = self._action(a, "새 에셋…", QKeySequence("Ctrl+Shift+N"), self.ask_new_asset)
        self.act_import = self._action(a, "PNG 가져오기…", QKeySequence("Ctrl+I"), self.ask_import_png)
        self.act_import_cell = self._action(a, "시트에서 셀 가져오기…", QKeySequence("Ctrl+Shift+I"),
                                            lambda: self.ask_import_png(cell=True))

        x = menu.addMenu("내보내기(&X)")
        self.act_check = self._action(x, "검사", QKeySequence("F7"), self.show_check)
        self.act_export = self._action(x, "게임용 PNG 내보내기…", QKeySequence("Ctrl+E"), self.ask_export)
        x.addAction("내보내기 폴더 위치 보기", self._show_exports_path)

        h = menu.addMenu("도움말(&H)")
        h.addAction("정보", lambda: QMessageBox.information(
            self, "Paldo Asset Studio",
            f"팔도 에셋 스튜디오 {__version__} (1차 MVP)\n\n"
            "원본 PNG는 프로젝트 sources/에 복사해 보관하고 수정하지 않습니다.\n"
            "내보내기는 프로젝트 exports/에만 씁니다. 게임 assets/는 바꾸지 않습니다.\n\n"
            + ("게임 저장소: " + str(self.specs.repo) if self.specs.repo else "게임 저장소를 찾지 못했습니다.")))

        bar = QToolBar("도구")
        bar.setObjectName("tools")
        self.addToolBar(bar)
        for act in (self.act_new, self.act_open, self.act_save):
            bar.addAction(act)
        bar.addSeparator()
        bar.addAction(self.act_undo)
        bar.addAction(self.act_redo)
        bar.addSeparator()
        group = QActionGroup(self)
        self.mode_actions = {}
        for key, label, tip in (("move", "레이어 이동", "드래그하거나 화살표 키(Shift=10px)로 이동"),
                                ("anchor", "발 기준점 찍기", "캔버스를 눌러 에셋의 발 기준점을 정함"),
                                ("pivot", "레이어 기준점 찍기", "선택 레이어의 기준점(예: 손잡이)을 정함")):
            act = QAction(label, self, checkable=True)
            act.setToolTip(tip)
            act.triggered.connect(lambda _=False, k=key: self.set_mode(k))
            group.addAction(act)
            bar.addAction(act)
            self.mode_actions[key] = act
        self.mode_actions["move"].setChecked(True)
        bar.addSeparator()
        bar.addAction(self.act_import)
        bar.addAction(self.act_import_cell)
        bar.addAction(self.act_check)
        bar.addAction(self.act_export)

    def _action(self, menu, text, shortcut, slot) -> QAction:
        act = QAction(text, self)
        act.setShortcut(shortcut)
        act.triggered.connect(slot)
        menu.addAction(act)
        return act

    def _dock(self, title: str, widget: QWidget, area, name: str) -> QDockWidget:
        dock = QDockWidget(title, self)
        dock.setObjectName(name)
        dock.setWidget(widget)
        dock.setFeatures(QDockWidget.DockWidgetMovable | QDockWidget.DockWidgetFloatable)
        self.addDockWidget(area, dock)
        return dock

    def _build_left(self):
        box = QWidget()
        v = QVBoxLayout(box)
        v.addWidget(QLabel("에셋 (종류별)"))
        self.asset_tree = QTreeWidget()
        self.asset_tree.setHeaderHidden(True)
        self.asset_tree.itemSelectionChanged.connect(self._asset_tree_selected)
        v.addWidget(self.asset_tree, 2)
        row = QHBoxLayout()
        b_new = QPushButton("새 에셋")
        b_new.clicked.connect(self.ask_new_asset)
        b_del = QPushButton("에셋 삭제")
        b_del.clicked.connect(self.delete_asset)
        row.addWidget(b_new)
        row.addWidget(b_del)
        v.addLayout(row)

        v.addWidget(QLabel("레이어 (위가 앞, 체크 = 표시)"))
        self.layer_list = QListWidget()
        self.layer_list.setSelectionMode(QAbstractItemView.SingleSelection)
        self.layer_list.itemSelectionChanged.connect(self._layer_list_selected)
        self.layer_list.itemChanged.connect(self._layer_item_changed)
        v.addWidget(self.layer_list, 3)
        row = QHBoxLayout()
        for text, slot in (("▲ 앞으로", lambda: self.move_layer(+1)), ("▼ 뒤로", lambda: self.move_layer(-1)),
                           ("복제", self.duplicate_layer), ("삭제", self.delete_layer)):
            b = QPushButton(text)
            b.clicked.connect(slot)
            row.addWidget(b)
        v.addLayout(row)
        box.setMinimumWidth(240)
        self._dock("프로젝트", box, Qt.LeftDockWidgetArea, "left")

    def _spin(self, lo, hi, decimals=None):
        box = QDoubleSpinBox() if decimals is not None else QSpinBox()
        box.setRange(lo, hi)
        if decimals is not None:
            box.setDecimals(decimals)
        box.setKeyboardTracking(False)
        return box

    def _build_right(self):
        inner = QWidget()
        v = QVBoxLayout(inner)
        # Never squeeze the form rows: let the scroll area scroll instead.
        v.setSizeConstraint(QLayout.SetMinimumSize)

        g = QGroupBox("에셋 속성")
        form = QFormLayout(g)
        self.f_name = QLineEdit()
        self.f_kind = QComboBox()
        for key, label in KINDS.items():
            self.f_kind.addItem(label, key)
        self.f_preset = QLabel()
        self.f_preset.setWordWrap(True)
        self.f_w, self.f_h = self._spin(1, 4096), self._spin(1, 4096)
        self.f_ax, self.f_ay = self._spin(-4096, 8192, 2), self._spin(-4096, 8192, 2)
        self.f_export = QLineEdit()
        form.addRow("이름", self.f_name)
        form.addRow("종류", self.f_kind)
        form.addRow("규격", self.f_preset)
        form.addRow("캔버스 너비", self.f_w)
        form.addRow("캔버스 높이", self.f_h)
        form.addRow("발 기준점 X", self.f_ax)
        form.addRow("발 기준점 Y", self.f_ay)
        form.addRow("내보낼 파일명", self.f_export)
        v.addWidget(g)
        self.asset_group = g

        g = QGroupBox("레이어 속성")
        form = QFormLayout(g)
        self.l_name = QLineEdit()
        self.l_slot = QComboBox()
        for key, label in SLOTS.items():
            self.l_slot.addItem(label, key)
        self.l_x, self.l_y = self._spin(-8192, 8192), self._spin(-8192, 8192)
        self.l_scale = self._spin(0.01, 16, 3)
        self.l_scale.setSingleStep(0.05)
        self.l_smooth = QCheckBox("부드럽게 축소 (끄면 최근접)")
        self.l_pivot_on = QCheckBox("기준점 사용 (원본 픽셀 좌표)")
        self.l_px, self.l_py = self._spin(-8192, 8192, 1), self._spin(-8192, 8192, 1)
        self.l_snap = QPushButton("레이어 기준점을 발 기준점에 맞추기")
        self.l_snap.clicked.connect(self.snap_pivot)
        self.l_opacity = QSlider(Qt.Horizontal)
        self.l_opacity.setRange(0, 100)
        self.l_visible = QCheckBox("표시")
        self.l_locked = QCheckBox("잠금")
        form.addRow("이름", self.l_name)
        form.addRow("부위(제안 분류)", self.l_slot)
        form.addRow("위치 X", self.l_x)
        form.addRow("위치 Y", self.l_y)
        form.addRow("배율", self.l_scale)
        form.addRow(self.l_smooth)
        form.addRow(self.l_pivot_on)
        form.addRow("기준점 X", self.l_px)
        form.addRow("기준점 Y", self.l_py)
        form.addRow(self.l_snap)
        form.addRow("불투명도", self.l_opacity)
        row = QHBoxLayout()
        row.addWidget(self.l_visible)
        row.addWidget(self.l_locked)
        form.addRow(row)
        v.addWidget(g)
        self.layer_group = g

        g = QGroupBox("이미지 정보")
        gv = QVBoxLayout(g)
        self.info_label = QLabel()
        self.info_label.setWordWrap(True)
        self.info_label.setTextInteractionFlags(Qt.TextSelectableByMouse)
        gv.addWidget(self.info_label)
        v.addWidget(g)

        g = QGroupBox("게임 크기 미리보기 (가독성)")
        gv = QVBoxLayout(g)
        self.preview_label = QLabel()
        self.preview_label.setAlignment(Qt.AlignCenter)
        self.preview_note = QLabel()
        self.preview_note.setWordWrap(True)
        gv.addWidget(self.preview_label)
        gv.addWidget(self.preview_note)
        v.addWidget(g)
        v.addStretch(1)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(inner)
        scroll.setMinimumWidth(330)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self._dock("속성", scroll, Qt.RightDockWidgetArea, "right")

        # Wiring: every field edit becomes one merged undo step per field.
        self.f_name.editingFinished.connect(lambda: self._asset_field("name", self.f_name.text().strip()))
        self.f_kind.currentIndexChanged.connect(lambda: self._asset_field("kind", self.f_kind.currentData()))
        self.f_w.valueChanged.connect(lambda v: self._asset_field("width", int(v)))
        self.f_h.valueChanged.connect(lambda v: self._asset_field("height", int(v)))
        self.f_ax.valueChanged.connect(lambda v: self._asset_anchor(0, v))
        self.f_ay.valueChanged.connect(lambda v: self._asset_anchor(1, v))
        self.f_export.editingFinished.connect(lambda: self._asset_field("export_name", self.f_export.text().strip()))
        self.l_name.editingFinished.connect(lambda: self._layer_field("name", self.l_name.text().strip()))
        self.l_slot.currentIndexChanged.connect(lambda: self._layer_field("slot", self.l_slot.currentData()))
        self.l_x.valueChanged.connect(lambda v: self._layer_field("x", int(v)))
        self.l_y.valueChanged.connect(lambda v: self._layer_field("y", int(v)))
        self.l_scale.valueChanged.connect(self._layer_scale)
        self.l_smooth.toggled.connect(lambda v: self._layer_field("smooth", bool(v)))
        self.l_pivot_on.toggled.connect(self._pivot_toggled)
        self.l_px.valueChanged.connect(lambda v: self._layer_pivot(0, v))
        self.l_py.valueChanged.connect(lambda v: self._layer_pivot(1, v))
        self.l_opacity.valueChanged.connect(lambda v: self._layer_field("opacity", v / 100))
        self.l_visible.toggled.connect(lambda v: self._layer_field("visible", bool(v)))
        self.l_locked.toggled.connect(lambda v: self._layer_field("locked", bool(v)))

    def _build_bottom(self):
        box = QWidget()
        h = QHBoxLayout(box)
        for text, slot in (("−", lambda: self.canvas.set_zoom(self.canvas.zoom / 1.25)),
                           ("+", lambda: self.canvas.set_zoom(self.canvas.zoom * 1.25)),
                           ("1:1", lambda: self.canvas.set_zoom(1)),
                           ("맞춤", self.canvas.fit)):
            b = QPushButton(text)
            b.setMaximumWidth(56)
            b.clicked.connect(slot)
            h.addWidget(b)
        self.zoom_label = QLabel("300%")
        h.addWidget(self.zoom_label)
        self.c_grid = QCheckBox("픽셀 격자")
        self.c_grid.setChecked(True)
        self.c_grid.toggled.connect(lambda v: (setattr(self.canvas, "show_grid", v), self.canvas.update()))
        self.c_guides = QCheckBox("중심축·발 접지선")
        self.c_guides.setChecked(True)
        self.c_guides.toggled.connect(lambda v: (setattr(self.canvas, "show_guides", v), self.canvas.update()))
        self.c_bg = QComboBox()
        for key, label in BACKGROUNDS.items():
            self.c_bg.addItem(label, key)
        self.c_bg.currentIndexChanged.connect(self._background_changed)
        h.addWidget(self.c_grid)
        h.addWidget(self.c_guides)
        h.addWidget(QLabel("배경"))
        h.addWidget(self.c_bg)
        h.addSpacing(16)
        self.coord_label = QLabel("좌표 -")
        self.coord_label.setMinimumWidth(170)
        h.addWidget(self.coord_label)
        h.addStretch(1)
        later = QLabel("타임라인·방향 전환·프레임 선택: 2·3차 단계 (승인 후)")
        later.setEnabled(False)
        h.addWidget(later)
        self._dock("보기", box, Qt.BottomDockWidgetArea, "bottom")

    def _ground_sample(self):
        """A patch of the village painting for the readability background (read only)."""
        if self.specs.repo is None:
            return None
        path = self.specs.repo / "assets" / "village.jpg"
        if not path.is_file():
            return None
        try:
            from PIL import Image
            with Image.open(path) as im:
                w, h = im.size
                patch = im.convert("RGBA").crop((w // 2 - 256, h // 2 - 256, w // 2 + 256, h // 2 + 256))
            return pil_to_qimage(patch)
        except OSError:
            return None

    # ================================================================ state
    @property
    def asset(self):
        return self.project.asset(self.asset_id) if self.project and self.asset_id else None

    @property
    def layer(self):
        a = self.asset
        return a.layer(self.layer_id) if a and self.layer_id else None

    def _checkpoint(self, key: str = ""):
        if self.history:
            self.history.checkpoint(key or None)

    def _mark(self):
        self.dirty = True
        self._update_title()
        self.act_undo.setEnabled(bool(self.history and self.history.can_undo))
        self.act_redo.setEnabled(bool(self.history and self.history.can_redo))

    def _update_title(self):
        name = self.project.name if self.project else "프로젝트 없음"
        self.setWindowTitle(f"{'*' if self.dirty else ''}{name} — 팔도 에셋 스튜디오")

    def refresh_all(self):
        self._fill_assets()
        self._fill_layers()
        self._fill_properties()
        self.canvas.set_asset(self.project, self.asset, self.layer_id)
        has_project, has_asset = self.project is not None, self.asset is not None
        for act in (self.act_save, self.act_save_as, self.act_new_asset):
            act.setEnabled(has_project)
        for act in (self.act_import, self.act_import_cell, self.act_check, self.act_export):
            act.setEnabled(has_asset)
        self.act_undo.setEnabled(bool(self.history and self.history.can_undo))
        self.act_redo.setEnabled(bool(self.history and self.history.can_redo))
        self._update_title()

    def _fill_assets(self):
        self._syncing = True
        self.asset_tree.clear()
        if self.project:
            groups = {}
            for key, label in KINDS.items():
                item = QTreeWidgetItem([label])
                item.setFlags(Qt.ItemIsEnabled)
                self.asset_tree.addTopLevelItem(item)
                groups[key] = item
            for a in self.project.assets:
                child = QTreeWidgetItem([f"{a.name}  ({a.width}×{a.height}, 레이어 {len(a.layers)})"])
                child.setData(0, Qt.UserRole, a.id)
                groups[a.kind].addChild(child)
                if a.id == self.asset_id:
                    child.setSelected(True)
            for key, item in groups.items():
                item.setText(0, f"{KINDS[key]} ({item.childCount()})")
            self.asset_tree.expandAll()
        self._syncing = False

    def _fill_layers(self):
        self._syncing = True
        self.layer_list.clear()
        a = self.asset
        if a:
            for layer in reversed(a.layers):
                item = QListWidgetItem(f"{layer.name}  [{SLOTS.get(layer.slot, layer.slot)}]"
                                       + ("  🔒" if layer.locked else ""))
                item.setData(Qt.UserRole, layer.id)
                item.setFlags(item.flags() | Qt.ItemIsUserCheckable)
                item.setCheckState(Qt.Checked if layer.visible else Qt.Unchecked)
                if self.project.source(layer.source) is None or \
                        not self.project.source_path(self.project.source(layer.source)).is_file():
                    item.setForeground(QColor(220, 70, 70))
                self.layer_list.addItem(item)
                if layer.id == self.layer_id:
                    item.setSelected(True)
        self._syncing = False

    def _fill_properties(self):
        self._syncing = True
        a, l = self.asset, self.layer
        self.asset_group.setEnabled(a is not None)
        self.layer_group.setEnabled(l is not None)
        if a:
            self.f_name.setText(a.name)
            self.f_kind.setCurrentIndex(self.f_kind.findData(a.kind))
            preset = self.specs.preset(a.preset)
            self.f_preset.setText(preset.label if preset else a.preset)
            self.f_w.setValue(a.width)
            self.f_h.setValue(a.height)
            self.f_ax.setValue(a.anchor[0])
            self.f_ay.setValue(a.anchor[1])
            self.f_export.setText(a.export_name)
        if l:
            self.l_name.setText(l.name)
            self.l_slot.setCurrentIndex(max(0, self.l_slot.findData(l.slot)))
            self.l_x.setValue(l.x)
            self.l_y.setValue(l.y)
            self.l_scale.setValue(l.scale)
            self.l_smooth.setChecked(l.smooth)
            self.l_pivot_on.setChecked(l.pivot is not None)
            self.l_px.setEnabled(l.pivot is not None)
            self.l_py.setEnabled(l.pivot is not None)
            self.l_snap.setEnabled(l.pivot is not None)
            self.l_px.setValue(l.pivot[0] if l.pivot else 0)
            self.l_py.setValue(l.pivot[1] if l.pivot else 0)
            self.l_opacity.setValue(round(l.opacity * 100))
            self.l_visible.setChecked(l.visible)
            self.l_locked.setChecked(l.locked)
        self._syncing = False
        self._fill_info()

    def _fill_info(self):
        a, l = self.asset, self.layer
        lines: list[str] = []
        if l is not None:
            image = self.cache.layer_image(self.project, l)
            if image is None:
                lines.append("선택 레이어: 원본 파일을 찾을 수 없음")
            else:
                info = inspect(image)
                lines.append("<b>선택 레이어</b>")
                s = self.project.source(l.source)
                if s:
                    lines.append(f"원본 {_short(s.original_name)} ({s.width}×{s.height}, {s.mode})")
                    lines.append(f"보관 사본 sources/{_short(Path(s.file).name)}, SHA-256 {s.sha256[:12]}…")
                if l.crop:
                    lines.append(f"셀 영역 {l.crop}")
                lines += info.summary()
        if a is not None:
            result = compose(self.project, a, self.cache)
            info = inspect(result)
            lines.append("<b>합성 결과</b>")
            lines += info.summary()
            preset = self.specs.preset(a.preset)
            box = preset.display_box if preset and preset.display_box else 56 * self.specs.actor_scale
            scale = box / 192 if a.width == 192 else box / max(a.width, 1)
            bg = self.canvas.ground_image if self.c_bg.currentData() in ("ground", "checker") else \
                QColor(24, 26, 30) if self.c_bg.currentData() == "dark" else QColor(236, 232, 222)
            self.preview_label.setPixmap(preview_pixmap(result, scale, bg, magnify=2))
            self.preview_note.setText(
                f"셀 전체를 화면 {box:.1f} 논리 단위로 줄인 뒤 2배 확대(최근접). 실제 화면은 "
                "창 높이에 따른 월드 줌이 더 곱해집니다.")
        else:
            self.preview_label.clear()
            self.preview_note.clear()
        self.info_label.setText("<br>".join(lines))

    # ================================================================ selection
    def _asset_tree_selected(self):
        if self._syncing:
            return
        items = self.asset_tree.selectedItems()
        if items and items[0].data(0, Qt.UserRole):
            self.select_asset(items[0].data(0, Qt.UserRole))

    def select_asset(self, asset_id: str):
        self.asset_id = asset_id
        a = self.asset
        self.layer_id = a.layers[-1].id if a and a.layers else ""
        self.refresh_all()
        self.canvas.fit()

    def _layer_list_selected(self):
        if self._syncing:
            return
        items = self.layer_list.selectedItems()
        if items:
            self.select_layer(items[0].data(Qt.UserRole))

    def select_layer(self, layer_id: str):
        self.layer_id = layer_id
        self._fill_layers()
        self._fill_properties()
        self.canvas.set_asset(self.project, self.asset, self.layer_id)

    def _layer_item_changed(self, item):
        if self._syncing:
            return
        layer = self.asset.layer(item.data(Qt.UserRole))
        visible = item.checkState() == Qt.Checked
        if layer and layer.visible != visible:
            self._checkpoint()
            layer.visible = visible
            self._after_edit()

    # ================================================================ editing
    def _after_edit(self, layers=False, assets=False):
        self._mark()
        if assets:
            self._fill_assets()
        if layers:
            self._fill_layers()
        self._fill_properties()
        self.canvas.set_asset(self.project, self.asset, self.layer_id)

    def _changed_from_canvas(self):
        self._mark()
        self._fill_properties()
        self.canvas.update()

    def _asset_field(self, field: str, value):
        a = self.asset
        if self._syncing or a is None or getattr(a, field) == value or value in ("", None):
            return
        self._checkpoint(f"{a.id}-{field}")
        setattr(a, field, value)
        self._after_edit(assets=field in ("name", "kind", "width", "height"))

    def _asset_anchor(self, index: int, value: float):
        a = self.asset
        if self._syncing or a is None or a.anchor[index] == value:
            return
        self._checkpoint(f"{a.id}-anchor{index}")
        a.anchor[index] = float(value)
        self._after_edit()

    def _layer_field(self, field: str, value):
        l = self.layer
        if self._syncing or l is None or getattr(l, field) == value or value in ("", None):
            return
        self._checkpoint(f"{l.id}-{field}")
        setattr(l, field, value)
        self._after_edit(layers=field in ("name", "slot", "visible", "locked"))

    def _layer_scale(self, value: float):
        """Scale around the pivot (or the bottom-centre) so the layer stays put."""
        a, l = self.asset, self.layer
        if self._syncing or l is None or value <= 0 or l.scale == value:
            return
        self._checkpoint(f"{l.id}-scale")
        image = self.cache.layer_image(self.project, l)
        if l.pivot is not None:
            fx, fy = l.pivot
        elif image is not None:
            fx, fy = image.width / l.scale / 2, image.height / l.scale
        else:
            fx = fy = 0
        cx, cy = l.x + fx * l.scale, l.y + fy * l.scale
        l.scale = round(float(value), 4)
        l.x, l.y = round(cx - fx * l.scale), round(cy - fy * l.scale)
        self._after_edit()

    def _pivot_toggled(self, on: bool):
        l = self.layer
        if self._syncing or l is None or (l.pivot is not None) == on:
            return
        self._checkpoint()
        if on:
            image = self.cache.layer_image(self.project, l)
            # Default: the layer's own bottom-centre (a reasonable foot/grip start).
            l.pivot = [round(image.width / l.scale / 2, 1), round(image.height / l.scale, 1)] if image else [0.0, 0.0]
        else:
            l.pivot = None
        self._after_edit()

    def _layer_pivot(self, index: int, value: float):
        l = self.layer
        if self._syncing or l is None or l.pivot is None or l.pivot[index] == value:
            return
        self._checkpoint(f"{l.id}-pivot{index}")
        l.pivot[index] = float(value)
        self._after_edit()

    def snap_pivot(self):
        a, l = self.asset, self.layer
        if a and l and l.pivot is not None:
            self._checkpoint()
            self.project.snap_pivot_to_anchor(a, l)
            self._after_edit()

    def move_layer(self, delta: int):
        a, l = self.asset, self.layer
        if a and l:
            self._checkpoint()
            if Project.move_layer(a, l.id, delta):
                self._after_edit(layers=True)

    def duplicate_layer(self):
        a, l = self.asset, self.layer
        if not (a and l):
            return
        import copy
        import uuid
        self._checkpoint()
        twin = copy.deepcopy(l)
        twin.id = f"layer-{uuid.uuid4().hex[:8]}"
        twin.name = f"{l.name} 사본"
        a.layers.insert(a.layers.index(l) + 1, twin)
        self.layer_id = twin.id
        self._after_edit(layers=True, assets=True)

    def delete_layer(self):
        a, l = self.asset, self.layer
        if a and l:
            self._checkpoint()
            index = a.layers.index(l)
            a.layers.remove(l)
            self.layer_id = a.layers[min(index, len(a.layers) - 1)].id if a.layers else ""
            self._after_edit(layers=True, assets=True)

    def delete_asset(self):
        a = self.asset
        if not a:
            return
        if QMessageBox.question(self, "에셋 삭제", f"'{a.name}' 에셋을 삭제할까요? (원본 PNG는 남습니다. "
                                "실행 취소로 되돌릴 수 있습니다)") != QMessageBox.Yes:
            return
        self._checkpoint()
        self.project.assets.remove(a)
        self.asset_id = self.project.assets[0].id if self.project.assets else ""
        self.layer_id = ""
        self._mark()
        self.refresh_all()

    def set_mode(self, mode: str):
        self.canvas.mode = mode
        self.mode_actions[mode].setChecked(True)
        self.statusBar().showMessage({"move": "레이어 이동: 드래그 또는 화살표 키",
                                      "anchor": "캔버스를 눌러 발 기준점을 정합니다",
                                      "pivot": "선택 레이어 위를 눌러 레이어 기준점을 정합니다"}[mode])

    def undo(self):
        if self.history and self.history.undo():
            self._after_undo()

    def redo(self):
        if self.history and self.history.redo():
            self._after_undo()

    def _after_undo(self):
        if self.asset is None:
            self.asset_id = self.project.assets[0].id if self.project.assets else ""
        if self.layer is None:
            a = self.asset
            self.layer_id = a.layers[-1].id if a and a.layers else ""
        self._mark()
        self.refresh_all()

    def _background_changed(self):
        self.canvas.background = self.c_bg.currentData()
        self.canvas.update()
        self._fill_info()

    def _cursor(self, x: float, y: float, inside: bool):
        a = self.asset
        if not a:
            return
        dx, dy = x - a.anchor[0], y - a.anchor[1]
        self.coord_label.setText(f"좌표 ({int(x)}, {int(y)})  발 기준 ({dx:+.0f}, {dy:+.0f})"
                                 + ("" if inside else "  캔버스 밖"))

    # ================================================================ project files
    def _confirm_discard(self) -> bool:
        if not self.dirty:
            return True
        answer = QMessageBox.question(self, "저장하지 않은 변경", "변경 내용을 저장할까요?",
                                      QMessageBox.Save | QMessageBox.Discard | QMessageBox.Cancel)
        if answer == QMessageBox.Save:
            return self.save_project()
        return answer == QMessageBox.Discard

    def ask_new_project(self):
        if not self._confirm_discard():
            return
        folder = QFileDialog.getExistingDirectory(self, "새 프로젝트를 만들 빈 폴더 선택")
        if not folder:
            return
        name, ok = QInputDialog.getText(self, "새 프로젝트", "프로젝트 이름", text=Path(folder).name)
        if ok:
            self.new_project(Path(folder), name.strip() or Path(folder).name)

    def new_project(self, folder: Path, name: str):
        folder = Path(folder)
        if (folder / PROJECT_FILE).exists():
            raise FileExistsError(f"이미 프로젝트가 있습니다: {folder}")
        self._guard_repo_folder(folder)
        self.project = Project(name=name, root=folder)
        self.project.save()
        self._loaded()

    def _guard_repo_folder(self, folder: Path):
        """Keep studio projects out of the game's runtime folder."""
        repo = self.specs.repo
        if repo is not None:
            try:
                rel = folder.resolve().relative_to((repo / "assets").resolve())
            except ValueError:
                return
            raise ValueError(f"게임 실행 폴더 assets/{rel} 안에는 프로젝트를 만들 수 없습니다")

    def ask_open_project(self):
        if not self._confirm_discard():
            return
        path, _ = QFileDialog.getOpenFileName(self, "프로젝트 열기", "", f"팔도 프로젝트 ({PROJECT_FILE})")
        if path:
            try:
                self.open_project(Path(path))
            except (OSError, ValueError, KeyError, TypeError) as exc:
                QMessageBox.critical(self, "열기 실패", str(exc))

    def open_project(self, path: Path):
        self.project = Project.load(path)
        self._loaded()

    def _loaded(self):
        self.cache.clear()
        self.canvas.invalidate_images()
        self.history = History(self.project)
        self.asset_id = self.project.assets[0].id if self.project.assets else ""
        a = self.asset
        self.layer_id = a.layers[-1].id if a and a.layers else ""
        self.dirty = False
        self.refresh_all()
        self.canvas.fit()

    def save_project(self) -> bool:
        if not self.project:
            return False
        try:
            self.project.save()
        except OSError as exc:
            QMessageBox.critical(self, "저장 실패", str(exc))
            return False
        self.dirty = False
        self._update_title()
        self.statusBar().showMessage(f"저장: {self.project.root / PROJECT_FILE}", 4000)
        return True

    def ask_save_as(self):
        if not self.project:
            return
        folder = QFileDialog.getExistingDirectory(self, "새 프로젝트 폴더 선택")
        if folder:
            if (Path(folder) / PROJECT_FILE).exists() and QMessageBox.question(
                    self, "덮어쓰기", "그 폴더에 이미 프로젝트가 있습니다. 덮어쓸까요?") != QMessageBox.Yes:
                return
            self._guard_repo_folder(Path(folder))
            self.project.save(Path(folder))
            self.dirty = False
            self.refresh_all()

    def closeEvent(self, event):
        if self._confirm_discard():
            event.accept()
        else:
            event.ignore()

    # ================================================================ assets & import
    def ask_new_asset(self):
        if not self.project:
            return
        dialog = NewAssetDialog(self.specs, self)
        if dialog.exec():
            self.new_asset(**dialog.values())

    def new_asset(self, name, kind, width, height, anchor, preset="free"):
        self._checkpoint()
        a = self.project.add_asset(name, kind, width, height, anchor, preset)
        self.asset_id, self.layer_id = a.id, ""
        self._mark()
        self.refresh_all()
        self.canvas.fit()
        return a

    def ask_import_png(self, cell: bool = False):
        if not self.asset:
            return
        start = str(self.specs.repo / "assets") if self.specs.repo else ""
        path, _ = QFileDialog.getOpenFileName(self, "PNG 가져오기", start, "PNG 이미지 (*.png)")
        if not path:
            return
        try:
            image = open_png(Path(path))
        except (OSError, ValueError) as exc:
            QMessageBox.critical(self, "가져오기 실패", str(exc))
            return
        crop, align, label = None, None, Path(path).stem
        if cell:
            dialog = CellImportDialog(Path(path), image, self.specs, (self.asset.width, self.asset.height),
                                      tuple(self.asset.anchor), self)
            if not dialog.exec():
                return
            crop, align, label = dialog.crop(), dialog.align_anchor(), dialog.label()
        self.import_png(Path(path), crop=crop, align_to_anchor=align, name=label)

    def import_png(self, path: Path, crop=None, align_to_anchor=None, name=None, slot="other"):
        image = open_png(Path(path))
        self._checkpoint()
        source = self.project.import_png(Path(path), image.width, image.height, image.mode)
        layer = self.project.add_layer(self.asset, source, name or Path(path).stem, crop, slot, align_to_anchor)
        self.layer_id = layer.id
        self._mark()
        self.refresh_all()
        if image.mode != "RGBA":
            self.statusBar().showMessage(f"주의: 원본 모드가 {image.mode}입니다 (RGBA 아님)", 6000)
        return layer

    # ================================================================ check & export
    def run_check(self) -> list[dict]:
        return check_asset(self.project, self.asset, self.cache, self.specs)

    def show_check(self):
        if self.asset:
            CheckDialog(self.run_check(), f"검사 — {self.asset.name}", False, self).exec()

    def ask_export(self):
        if not self.asset:
            return
        if self.dirty and not self.save_project():
            return
        report = self.run_check()
        if not CheckDialog(report, f"내보내기 전 검사 — {self.asset.name}", True, self).exec():
            return
        if has_errors(report):
            return
        try:
            png, meta = self.export_current(report, overwrite=False)
        except FileExistsError:
            if QMessageBox.question(self, "덮어쓰기", "같은 이름의 내보낸 파일이 있습니다. 덮어쓸까요?") != QMessageBox.Yes:
                return
            png, meta = self.export_current(report, overwrite=True)
        except ValueError as exc:
            QMessageBox.critical(self, "내보내기 실패", str(exc))
            return
        QMessageBox.information(self, "내보내기 완료", f"{png}\n{meta}")

    def export_current(self, report=None, overwrite=False):
        report = report if report is not None else self.run_check()
        if has_errors(report):
            raise ValueError("검사 오류가 있어 내보낼 수 없습니다")
        return export_asset(self.project, self.asset, self.cache, report, overwrite)

    def _show_exports_path(self):
        if self.project:
            QMessageBox.information(self, "내보내기 폴더", str(self.project.exports_dir))
