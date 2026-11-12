"""Main window: left (assets, layers), centre (canvas), right (properties), bottom (view bar, timeline)."""

from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import QSize, Qt, QTimer
from PySide6.QtGui import QAction, QActionGroup, QColor, QIcon, QKeySequence, QPainter, QPixmap
from PySide6.QtWidgets import (QAbstractItemView, QCheckBox, QComboBox, QDockWidget, QDoubleSpinBox,
                               QFileDialog, QFormLayout, QGroupBox, QHBoxLayout, QInputDialog, QLayout,
                               QLabel, QLineEdit, QListWidget, QListWidgetItem, QMainWindow,
                               QMessageBox, QPushButton, QScrollArea, QSlider, QSpinBox,
                               QToolBar, QTreeWidget, QTreeWidgetItem, QVBoxLayout, QWidget)

from .. import __version__
from .. import autofix
from ..checks import check_asset, has_errors
from ..game_specs import Specs
from ..imaging import SHEET_LAYOUTS, ImageCache, build_sheets, compose, export_asset, inspect, layout_problem, open_png
from ..model import EIGHT_DIRECTIONS, KINDS, PROJECT_FILE, SLOTS, TOLERANCE_LABELS, History, Project
from .canvas import BACKGROUNDS, Canvas, pil_to_qimage, preview_pixmap
from ..templates import (TEMPLATE_SUFFIX, Template, asset_from_template, builtin_templates, load_template,
                         project_templates, save_template)
from .dialogs import (ArtworkImportDialog, CellImportDialog, CheckDialog, NewAssetDialog, SheetImportDialog,
                      SheetPreviewDialog, TemplateDialog)
from .overview import Overview


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
        self.view = ""
        self.frame = 0
        self.dirty = False
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._tick)
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
        self._build_timeline()
        self._build_overview()
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
        self.act_from_template = self._action(a, "템플릿에서 새 에셋…", QKeySequence("Ctrl+T"),
                                              self.ask_new_from_template)
        self.act_save_template = self._action(a, "현재 에셋을 템플릿으로 저장…", QKeySequence("Ctrl+Shift+T"),
                                              self.ask_save_template)
        self.act_template_file = self._action(a, "템플릿 파일 가져오기…", QKeySequence(), self.ask_import_template_file)
        a.addSeparator()
        self.act_import = self._action(a, "PNG 가져오기…", QKeySequence("Ctrl+I"), self.ask_import_png)
        self.act_import_cell = self._action(a, "시트에서 셀 가져오기…", QKeySequence("Ctrl+Shift+I"),
                                            lambda: self.ask_import_png(cell=True))
        self.act_import_sheet = self._action(a, "시트 여러 셀 한 번에 가져오기 (방향·프레임)…",
                                             QKeySequence("Ctrl+Alt+I"), self.ask_import_sheet)

        c = menu.addMenu("보정(&C)")
        self.act_artwork = self._action(c, "GPT 원화 가져오기 (현재 칸)…", QKeySequence("Ctrl+G"),
                                        self.ask_import_artwork)
        c.addSeparator()
        self.act_bg = self._action(c, "선택 레이어 배경 제거…", QKeySequence("Ctrl+Shift+B"), self.ask_remove_background)
        self.act_noise = self._action(c, "선택 레이어 잡점(알파 1~4) 지우기", QKeySequence(), self.clean_noise)
        c.addSeparator()
        self.act_snap = self._action(c, "발 맞추기: 현재 칸", QKeySequence("Ctrl+Shift+F"),
                                     lambda: self.snap_foot(False))
        self.act_snap_all = self._action(c, "발 맞추기: 모든 방향·프레임", QKeySequence(),
                                         lambda: self.snap_foot(True))
        self.act_height = self._action(c, "키 맞추기: 선택 레이어를 참고 레이어 키로", QKeySequence(),
                                       self.match_reference_height)
        self.fix_actions = (self.act_artwork, self.act_bg, self.act_noise, self.act_snap, self.act_snap_all,
                            self.act_height)

        x = menu.addMenu("내보내기(&X)")
        self.act_check = self._action(x, "검사", QKeySequence("F7"), self.show_check)
        self.act_export = self._action(x, "게임용 PNG 내보내기…", QKeySequence("Ctrl+E"), self.ask_export)
        self.act_sheet_preview = self._action(x, "시트 미리보기…", QKeySequence("Ctrl+Shift+E"),
                                              self.show_sheet_preview)
        x.addAction("내보내기 폴더 위치 보기", self._show_exports_path)

        h = menu.addMenu("도움말(&H)")
        h.addAction("정보", lambda: QMessageBox.information(
            self, "Paldo Asset Studio",
            f"팔도 에셋 스튜디오 {__version__} (4차)\n\n"
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
        bar.addAction(self.act_import_sheet)
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

        v.addWidget(QLabel("장비 묶음 (체크 = 착용, 모든 방향에 적용)"))
        self.group_list = QListWidget()
        self.group_list.itemChanged.connect(self._group_item_changed)
        v.addWidget(self.group_list, 1)
        row = QHBoxLayout()
        self.combo_box = QComboBox()
        self.combo_box.setToolTip("저장한 장비 조합")
        self.combo_box.activated.connect(lambda _i: self.apply_combo(self.combo_box.currentText()))
        b_save = QPushButton("조합 저장")
        b_save.clicked.connect(self.ask_save_combo)
        b_drop = QPushButton("조합 삭제")
        b_drop.clicked.connect(self.delete_combo)
        row.addWidget(self.combo_box, 1)
        row.addWidget(b_save)
        row.addWidget(b_drop)
        v.addLayout(row)
        box.setMinimumWidth(260)
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
        self.f_dirs = QLabel()
        form.addRow("방향", self.f_dirs)
        self.f_frames = QLabel()
        form.addRow("프레임", self.f_frames)
        self.f_layout = QComboBox()
        for key, label in SHEET_LAYOUTS.items():
            self.f_layout.addItem(label.split(" (")[0], key)
            self.f_layout.setItemData(self.f_layout.count() - 1, label, Qt.ToolTipRole)
        self.f_columns = self._spin(1, 64)
        form.addRow("내보내기 형식", self.f_layout)
        form.addRow("시트 열 수", self.f_columns)
        self.f_tol = {}
        for key, label in TOLERANCE_LABELS.items():
            box = self._spin(0, 512, 1)
            box.valueChanged.connect(lambda val, k=key: self._asset_tolerance(k, val))
            self.f_tol[key] = box
            form.addRow(label, box)
        v.addWidget(g)
        self.asset_group = g

        g = QGroupBox("레이어 속성")
        form = QFormLayout(g)
        self.l_name = QLineEdit()
        self.l_slot = QComboBox()
        for key, label in SLOTS.items():
            self.l_slot.addItem(label, key)
        self.l_group = QComboBox()
        self.l_group.setEditable(True)
        self.l_group.setToolTip("같은 이름의 레이어는 모든 방향에서 함께 켜고 끕니다 (비우면 몸)")
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
        form.addRow("장비 묶음", self.l_group)
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
        self.f_layout.currentIndexChanged.connect(lambda: self._asset_field("sheet_layout", self.f_layout.currentData()))
        self.f_columns.valueChanged.connect(lambda v: self._asset_field("sheet_columns", int(v)))
        self.l_name.editingFinished.connect(lambda: self._layer_field("name", self.l_name.text().strip()))
        self.l_slot.currentIndexChanged.connect(lambda: self._layer_field("slot", self.l_slot.currentData()))
        self.l_group.lineEdit().editingFinished.connect(self._layer_group_edited)
        self.l_group.activated.connect(lambda _i: self._layer_group_edited())
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
        outer = QVBoxLayout(box)
        outer.setContentsMargins(4, 2, 4, 2)
        h = QHBoxLayout()
        outer.addLayout(h)
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
        h.addStretch(1)
        h = QHBoxLayout()
        outer.addLayout(h)
        h.addWidget(QLabel("방향"))
        self.dir_buttons = {}
        for d in EIGHT_DIRECTIONS:
            b = QPushButton(d)
            b.setCheckable(True)
            b.setMaximumWidth(40)
            b.clicked.connect(lambda _=False, v=d: self.set_view(v))
            self.dir_buttons[d] = b
            h.addWidget(b)
        h.addWidget(QLabel("겹쳐 보기"))
        self.c_onion = QComboBox()
        self.c_onion.addItem("없음", "")
        for d in EIGHT_DIRECTIONS:
            self.c_onion.addItem(d, d)
        self.c_onion.currentIndexChanged.connect(self._onion_changed)
        self.c_onion_alpha = QSlider(Qt.Horizontal)
        self.c_onion_alpha.setRange(5, 90)
        self.c_onion_alpha.setValue(35)
        self.c_onion_alpha.setMaximumWidth(90)
        self.c_onion_alpha.valueChanged.connect(self._onion_changed)
        self.c_measure = QCheckBox("측정선")
        self.c_measure.setChecked(True)
        self.c_measure.toggled.connect(lambda v: (setattr(self.canvas, "show_measure", v), self.canvas.update()))
        h.addWidget(self.c_onion)
        h.addWidget(self.c_onion_alpha)
        h.addWidget(self.c_measure)
        h.addStretch(1)
        self.coord_label = QLabel("좌표 -")
        self.coord_label.setMinimumWidth(170)
        h.addWidget(self.coord_label)
        self._dock("보기", box, Qt.BottomDockWidgetArea, "bottom")

    def _build_timeline(self):
        box = QWidget()
        outer = QVBoxLayout(box)
        outer.setContentsMargins(4, 2, 4, 2)
        h = QHBoxLayout()
        outer.addLayout(h)
        self.frame_label = QLabel("프레임 -")
        self.frame_label.setMinimumWidth(90)
        h.addWidget(self.frame_label)
        self.t_buttons = {}
        for key, text, tip, slot in (
                ("first", "⏮", "첫 프레임", lambda: self.set_frame(0)),
                ("prev", "◀", "이전 프레임 (,)", lambda: self.step_frame(-1)),
                ("play", "▶ 재생", "재생 / 일시정지 (Space는 캔버스 밖에서)", self.toggle_play),
                ("next", "▶|", "다음 프레임 (.)", lambda: self.step_frame(+1)),
                ("add", "+ 빈 프레임", "현재 프레임 뒤에 빈 프레임을 넣습니다 (모든 방향)", lambda: self.add_frame(False)),
                ("dup", "프레임 복제", "현재 프레임을 뒤에 복제합니다 (모든 방향)", lambda: self.add_frame(True)),
                ("del", "프레임 삭제", "현재 프레임을 지웁니다 (모든 방향)", self.delete_frame),
                ("left", "◀ 앞으로", "프레임 순서를 앞으로", lambda: self.move_frame(-1)),
                ("right", "뒤로 ▶", "프레임 순서를 뒤로", lambda: self.move_frame(+1))):
            b = QPushButton(text)
            b.setToolTip(tip)
            b.clicked.connect(slot)
            h.addWidget(b)
            self.t_buttons[key] = b
        self.t_buttons["play"].setCheckable(True)
        h.addWidget(QLabel("FPS(미리보기)"))
        self.t_fps = self._spin(1, 60, 1)
        self.t_fps.setToolTip("편집기 재생 속도만 바꿉니다. 게임의 공격 속도·프레임 시간은 바꾸지 않습니다.")
        self.t_fps.valueChanged.connect(self.set_fps)
        h.addWidget(self.t_fps)
        h.addStretch(1)
        h = QHBoxLayout()
        outer.addLayout(h)
        self.t_onion = QCheckBox("앞뒤 프레임 겹쳐 보기")
        self.t_onion.toggled.connect(self._onion_frames_changed)
        self.t_anchor = QCheckBox("이 프레임만 발 기준점 따로")
        self.t_anchor.setToolTip("기본 공격처럼 프레임마다 발 위치가 다른 시트에 씁니다")
        self.t_anchor.toggled.connect(self.set_frame_anchor_override)
        self.t_attack = QPushButton("기본 공격 발 기준점 채우기 (draw.c)")
        self.t_attack.setToolTip("src/draw.c의 기본 공격 프레임별 발 위치(192 셀 환산)를 프레임 1~6에 넣습니다")
        self.t_attack.clicked.connect(self.fill_attack_anchors)
        self.t_preview = QPushButton("시트 미리보기")
        self.t_preview.clicked.connect(self.show_sheet_preview)
        for w in (self.t_onion, self.t_anchor, self.t_attack, self.t_preview):
            h.addWidget(w)
        h.addStretch(1)
        self.frame_strip = QListWidget()
        self.frame_strip.setViewMode(QListWidget.IconMode)
        self.frame_strip.setFlow(QListWidget.LeftToRight)
        self.frame_strip.setWrapping(False)
        self.frame_strip.setMovement(QListWidget.Static)
        self.frame_strip.setIconSize(QSize(56, 56))
        self.frame_strip.setFixedHeight(96)
        self.frame_strip.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.frame_strip.itemSelectionChanged.connect(self._strip_selected)
        outer.addWidget(self.frame_strip)
        for key, seq in (("prev", ","), ("next", ".")):
            act = QAction(self)
            act.setShortcut(QKeySequence(seq))
            act.triggered.connect(self.t_buttons[key].click)
            self.addAction(act)
        self._dock("타임라인", box, Qt.BottomDockWidgetArea, "timeline")

    def _build_overview(self):
        self.overview = Overview(self.cache)
        self.overview.viewClicked.connect(self.set_view)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(self.overview)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        dock = self._dock("8방향 한눈에 보기", scroll, Qt.RightDockWidgetArea, "overview")
        self.tabifyDockWidget(self.findChild(QDockWidget, "right"), dock)
        self.findChild(QDockWidget, "right").raise_()
        self.overview_dock = dock

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
        self._fill_groups()
        self._fill_view_bar()
        self._fill_timeline()
        self._show()
        has_project, has_asset = self.project is not None, self.asset is not None
        for act in (self.act_save, self.act_save_as, self.act_new_asset, self.act_from_template,
                    self.act_template_file):
            act.setEnabled(has_project)
        for act in self.fix_actions:
            act.setEnabled(has_asset)
        self.act_save_template.setEnabled(has_asset)
        for act in (self.act_import, self.act_import_cell, self.act_check, self.act_export,
                    self.act_import_sheet, self.act_sheet_preview):
            act.setEnabled(has_asset)
        self.act_undo.setEnabled(bool(self.history and self.history.can_undo))
        self.act_redo.setEnabled(bool(self.history and self.history.can_redo))
        self._update_title()

    def _show(self):
        """Redraw the canvas and the 8-direction overview for the current view and frame."""
        self.canvas.set_asset(self.project, self.asset, self.layer_id, self.view, self.frame)
        self.overview.refresh(self.project, self.asset, self.view, self.frame)

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
                kind = "8방향" if a.directions else f"레이어 {len(a.layers)}"
                if a.frames > 1:
                    kind += f", {a.frames}프레임"
                child = QTreeWidgetItem([f"{a.name}  ({a.width}×{a.height}, {kind})"])
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
            for layer in reversed(a.cell_layers(self.view, self.frame)):
                item = QListWidgetItem(f"{layer.name}  [{SLOTS.get(layer.slot, layer.slot)}]"
                                       + (f"  〈{layer.group}〉" if layer.group else "")
                                       + ("  🔒" if layer.locked else ""))
                item.setData(Qt.UserRole, layer.id)
                item.setFlags(item.flags() | Qt.ItemIsUserCheckable)
                item.setCheckState(Qt.Checked if layer.visible else Qt.Unchecked)
                if self.project.source(layer.source) is None or \
                        not self.project.source_path(self.project.source(layer.source)).is_file():
                    item.setForeground(QColor(220, 70, 70))
                elif layer.group in a.hidden_groups:
                    item.setForeground(QColor(140, 140, 140))
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
            anchor = a.anchor_for(self.frame)
            self.f_ax.setValue(anchor[0])
            self.f_ay.setValue(anchor[1])
            self.f_export.setText(a.export_name)
            self.f_dirs.setText(" ".join(a.directions) if a.directions else "단일 (방향 없음)")
            for key, box in self.f_tol.items():
                box.setValue(a.tolerance(key))
            own = sorted(int(k) + 1 for k in a.frame_anchors)
            self.f_frames.setText(f"{a.frames}개" + (f" (발 기준점 따로: {', '.join(map(str, own))})" if own else ""))
            self.f_layout.setCurrentIndex(max(0, self.f_layout.findData(a.sheet_layout)))
            self.f_columns.setValue(a.sheet_columns)
        if l:
            self.l_name.setText(l.name)
            self.l_slot.setCurrentIndex(max(0, self.l_slot.findData(l.slot)))
            self.l_group.clear()
            self.l_group.addItems([""] + a.groups())
            self.l_group.setCurrentText(l.group)
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
            result = compose(self.project, a, self.cache, view=self.view, frame=self.frame)
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
        self.view = a.views()[0] if a else ""
        self.frame = 0
        self._stop_play()
        self._pick_top_layer()
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
        self.canvas.set_asset(self.project, self.asset, self.layer_id, self.view, self.frame)

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
            self._fill_groups()
        self._fill_properties()
        self._show()

    def _changed_from_canvas(self):
        self._mark()
        self._fill_properties()
        self._show()

    def _asset_field(self, field: str, value):
        a = self.asset
        if self._syncing or a is None or getattr(a, field) == value or value in ("", None):
            return
        self._checkpoint(f"{a.id}-{field}")
        setattr(a, field, value)
        self._after_edit(assets=field in ("name", "kind", "width", "height"))
        if field in ("sheet_layout", "sheet_columns"):
            problem = layout_problem(a)
            if problem:
                self.statusBar().showMessage(f"내보내기 형식: {problem}", 6000)

    def _asset_anchor(self, index: int, value: float):
        a = self.asset
        if self._syncing or a is None or a.anchor_for(self.frame)[index] == value:
            return
        self._checkpoint(f"{a.id}-anchor{index}")
        a.anchor_for(self.frame)[index] = float(value)  # the frame's own foot root when it has one
        self._after_edit()

    def _layer_field(self, field: str, value, allow_empty: bool = False):
        l = self.layer
        if self._syncing or l is None or getattr(l, field) == value or \
                value is None or (value == "" and not allow_empty):
            return
        self._checkpoint(f"{l.id}-{field}")
        setattr(l, field, value)
        self._after_edit(layers=field in ("name", "slot", "visible", "locked", "group"))

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
            same = a.cell_layers(self.view, self.frame)
            index = same.index(l)
            a.layers.remove(l)
            same.remove(l)
            self.layer_id = same[min(index, len(same) - 1)].id if same else ""
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
        a = self.asset
        if a is None or self.view not in a.views():
            self.view = a.views()[0] if a else ""
        self.frame = min(self.frame, a.frames - 1) if a else 0
        if self.layer is None or (self.layer.view, self.layer.frame) != (self.view, self.frame):
            self._pick_top_layer()
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
        anchor = a.anchor_for(self.frame)
        dx, dy = x - anchor[0], y - anchor[1]
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
        self.view = a.views()[0] if a else ""
        self.frame = 0
        self._stop_play()
        self._pick_top_layer()
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
        self.timer.stop()
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

    def new_asset(self, name, kind, width, height, anchor, preset="free", directions=None):
        self._checkpoint()
        a = self.project.add_asset(name, kind, width, height, anchor, preset, directions)
        self.asset_id, self.layer_id, self.view, self.frame = a.id, "", a.views()[0], 0
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

    def import_png(self, path: Path, crop=None, align_to_anchor=None, name=None, slot="other", group=""):
        image = open_png(Path(path))
        self._checkpoint()
        source = self.project.import_png(Path(path), image.width, image.height, image.mode)
        layer = self.project.add_layer(self.asset, source, name or Path(path).stem, crop, slot, align_to_anchor,
                                       view=self.view, group=group, frame=self.frame)
        self.layer_id = layer.id
        self._mark()
        self.refresh_all()
        if image.mode != "RGBA":
            self.statusBar().showMessage(f"주의: 원본 모드가 {image.mode}입니다 (RGBA 아님)", 6000)
        return layer

    # ================================================================ directions & 장비 묶음
    def _pick_top_layer(self):
        a = self.asset
        same = a.cell_layers(self.view, self.frame) if a else []
        self.layer_id = same[-1].id if same else ""

    def _follow_layer(self, old):
        """After a direction or frame change keep the same 장비 묶음 / layer name selected."""
        a = self.asset
        match = None
        if old is not None and a is not None:
            def base(layer):  # "정지 SW" -> "정지" for layers made by the sheet import
                suffix = f" {layer.view}"
                return layer.name[:-len(suffix)] if layer.view and layer.name.endswith(suffix) else layer.name
            same = a.cell_layers(self.view, self.frame)
            match = next((l for l in same if l.group == old.group and base(l) == base(old)), None) or \
                next((l for l in same if old.group and l.group == old.group), None) or \
                next((l for l in same if base(l) == base(old)), None)
        if match is not None:
            self.layer_id = match.id
        else:
            self._pick_top_layer()

    def set_view(self, view: str):
        a = self.asset
        if a is None or view not in a.views():
            self._fill_view_bar()
            return
        if view != self.view:
            old = self.layer
            self.view = view
            self._follow_layer(old)
        self.refresh_all()

    def _fill_view_bar(self):
        a = self.asset
        for d, b in self.dir_buttons.items():
            b.setEnabled(bool(a and d in a.directions))
            b.setChecked(bool(a and d == self.view))

    def _onion_changed(self):
        self.canvas.onion_view = self.c_onion.currentData() or None
        self.canvas.onion_opacity = self.c_onion_alpha.value() / 100
        self.canvas.onion_frames = self.t_onion.isChecked()
        self.canvas.set_asset(self.project, self.asset, self.layer_id, self.view, self.frame)

    def _fill_groups(self):
        self._syncing = True
        self.group_list.clear()
        a = self.asset
        if a:
            for g in a.groups():
                count = len({l.view for l in a.layers if l.group == g})
                item = QListWidgetItem(f"{g}  ({count}/{len(a.views())} 방향)" if a.directions else g)
                item.setData(Qt.UserRole, g)
                item.setFlags(item.flags() | Qt.ItemIsUserCheckable)
                item.setCheckState(Qt.Unchecked if g in a.hidden_groups else Qt.Checked)
                self.group_list.addItem(item)
        current = self.combo_box.currentText()
        self.combo_box.clear()
        if a:
            self.combo_box.addItems(list(a.combos))
            if current in a.combos:
                self.combo_box.setCurrentText(current)
        self._syncing = False

    def _group_item_changed(self, item):
        a = self.asset
        if self._syncing or a is None:
            return
        g, on = item.data(Qt.UserRole), item.checkState() == Qt.Checked
        if on == (g not in a.hidden_groups):
            return
        self._checkpoint()
        if on:
            a.hidden_groups.remove(g)
        else:
            a.hidden_groups.append(g)
        self._after_edit(layers=True)

    def set_group_visible(self, group: str, visible: bool):
        for i in range(self.group_list.count()):
            item = self.group_list.item(i)
            if item.data(Qt.UserRole) == group:
                item.setCheckState(Qt.Checked if visible else Qt.Unchecked)

    def ask_save_combo(self):
        a = self.asset
        if a is None:
            return
        name, ok = QInputDialog.getText(self, "장비 조합 저장", "조합 이름 (현재 착용 상태를 저장)",
                                        text=self.combo_box.currentText())
        if ok and name.strip():
            self.save_combo(name.strip())

    def save_combo(self, name: str):
        self._checkpoint()
        self.asset.save_combo(name)
        self._after_edit(layers=True)
        self.combo_box.setCurrentText(name)

    def apply_combo(self, name: str):
        a = self.asset
        if a is None or name not in a.combos:
            return
        self._checkpoint()
        a.apply_combo(name)
        self._after_edit(layers=True)

    def delete_combo(self):
        a = self.asset
        name = self.combo_box.currentText()
        if a is None or name not in a.combos:
            return
        self._checkpoint()
        del a.combos[name]
        self._after_edit(layers=True)

    def _layer_group_edited(self):
        self._layer_field("group", self.l_group.currentText().strip(), allow_empty=True)

    def _asset_tolerance(self, key: str, value: float):
        a = self.asset
        if self._syncing or a is None or a.tolerance(key) == value:
            return
        self._checkpoint(f"{a.id}-tol-{key}")
        a.tolerances[key] = float(value)
        self._after_edit()

    def ask_import_sheet(self):
        a = self.asset
        if not a:
            return
        start = str(self.specs.repo / "assets") if self.specs.repo else ""
        path, _ = QFileDialog.getOpenFileName(self, "시트 한 번에 가져오기", start, "PNG 이미지 (*.png)")
        if not path:
            return
        try:
            image = open_png(Path(path))
        except (OSError, ValueError) as exc:
            QMessageBox.critical(self, "가져오기 실패", str(exc))
            return
        dialog = SheetImportDialog(Path(path), image, self.specs, a.directions, a.groups(), self,
                                   view=self.view, frame=self.frame)
        if dialog.exec():
            v = dialog.values()
            self.import_sheet(Path(path), v["crops"], v["name"], v["group"], v["slot"])

    def import_sheet(self, path: Path, crops: dict, name: str, group: str = "", slot: str = "other"):
        """One layer per cell from one sheet; one undo step.

        ``crops`` maps a direction (into the current frame) or a (direction, frame)
        pair to a crop rectangle. Missing frames are added. Returns the made layers
        under the same keys.
        """
        a = self.asset
        image = open_png(Path(path))
        self._checkpoint()
        source = self.project.import_png(Path(path), image.width, image.height, image.mode)
        made = {}
        for key, crop in crops.items():
            view, frame = key if isinstance(key, tuple) else (key, self.frame)
            align = tuple(a.anchor_for(frame)) if (crop[2], crop[3]) == (a.width, a.height) else None
            made[key] = self.project.add_layer(a, source, f"{name} {view}" if view else name, crop, slot, align,
                                               view=view, group=group, frame=frame)
        here = next((l for l in made.values() if (l.view, l.frame) == (self.view, self.frame)), None)
        self.layer_id = here.id if here else self.layer_id
        self._mark()
        self.refresh_all()
        return made

    def goto_check_item(self, item: dict):
        a = self.asset
        if a is None:
            return
        if item.get("frame") is not None and item["frame"] != self.frame:
            self.set_frame(item["frame"])
        if item.get("view") in a.views() and item.get("view") != self.view:
            self.set_view(item["view"])
        if item.get("layer_id") and a.layer(item["layer_id"]):
            self.select_layer(item["layer_id"])

    # ================================================================ frames & playback
    def _fill_timeline(self):
        a = self.asset
        self._syncing = True
        self.frame_strip.clear()
        has = a is not None
        for key, b in self.t_buttons.items():
            b.setEnabled(has and (key not in ("play", "first", "prev", "next", "del", "left", "right") or a.frames > 1))
        if has:
            self.t_buttons["left"].setEnabled(self.frame > 0)
            self.t_buttons["right"].setEnabled(self.frame < a.frames - 1)
            self.frame_label.setText(f"프레임 {self.frame + 1}/{a.frames}")
            size = 56
            scale = size / max(a.width, a.height)
            for f in range(a.frames):
                image = compose(self.project, a, self.cache, view=self.view, frame=f)
                pixmap = QPixmap(max(1, round(a.width * scale)), max(1, round(a.height * scale)))
                pixmap.fill(QColor(150, 150, 150))
                painter = QPainter(pixmap)
                painter.setRenderHint(QPainter.SmoothPixmapTransform, True)
                painter.drawImage(pixmap.rect(), pil_to_qimage(image))
                painter.end()
                label = str(f + 1) + ("·발" if str(f) in a.frame_anchors else "")
                item = QListWidgetItem(QIcon(pixmap), label)
                item.setData(Qt.UserRole, f)
                if not a.cell_layers(self.view, f):
                    item.setForeground(QColor(220, 70, 70))
                    item.setToolTip("이 방향의 이 프레임에 레이어가 없습니다")
                self.frame_strip.addItem(item)
                if f == self.frame:
                    item.setSelected(True)
            self.t_fps.setValue(a.fps)
            self.t_anchor.setChecked(str(self.frame) in a.frame_anchors)
            self.t_attack.setEnabled(a.frames > 1 and a.preset.startswith("hero-attack")
                                     and self.specs.preset("hero-attack-0") is not None)
        else:
            self.frame_label.setText("프레임 -")
        self.t_anchor.setEnabled(has)
        self.t_onion.setEnabled(has)
        self.t_preview.setEnabled(has)
        self._syncing = False

    def _strip_selected(self):
        if self._syncing:
            return
        items = self.frame_strip.selectedItems()
        if items:
            self.set_frame(items[0].data(Qt.UserRole))

    def set_frame(self, frame: int):
        a = self.asset
        if a is None or not 0 <= frame < a.frames:
            return
        if frame != self.frame:
            old = self.layer
            self.frame = frame
            self._follow_layer(old)
        if self.timer.isActive():
            # Light refresh while playing: no thumbnail rebuild.
            self._syncing = True
            for i in range(self.frame_strip.count()):
                self.frame_strip.item(i).setSelected(i == frame)
            self._syncing = False
            self.frame_label.setText(f"프레임 {frame + 1}/{a.frames}")
            self._fill_layers()
            self._fill_properties()
            self._show()
            return
        self.refresh_all()

    def step_frame(self, delta: int):
        a = self.asset
        if a is not None and a.frames > 1:
            self.set_frame((self.frame + delta) % a.frames)

    def add_frame(self, copy: bool):
        a = self.asset
        if a is None:
            return
        self._checkpoint()
        a.insert_frame(self.frame + 1, self.frame if copy else None)
        old = self.layer
        self.frame += 1
        self._follow_layer(old)
        self._mark()
        self.refresh_all()

    def delete_frame(self):
        a = self.asset
        if a is None or a.frames <= 1:
            return
        self._checkpoint()
        a.delete_frame(self.frame)
        self.frame = min(self.frame, a.frames - 1)
        self._pick_top_layer()
        self._mark()
        self.refresh_all()

    def move_frame(self, delta: int):
        a = self.asset
        if a is None:
            return
        self._checkpoint()
        if a.move_frame(self.frame, delta):
            self.frame += delta
            self._mark()
            self.refresh_all()

    def set_fps(self, value: float):
        a = self.asset
        if self._syncing or a is None or a.fps == value or value <= 0:
            return
        self._checkpoint(f"{a.id}-fps")
        a.fps = float(value)
        self._mark()
        if self.timer.isActive():
            self.timer.setInterval(round(1000 / a.fps))

    def toggle_play(self):
        if self.timer.isActive():
            self._stop_play()
        else:
            a = self.asset
            if a is None or a.frames < 2:
                self._stop_play()
                return
            self.timer.start(round(1000 / a.fps))
            self.t_buttons["play"].setChecked(True)
            self.t_buttons["play"].setText("⏸ 일시정지")

    def _stop_play(self):
        self.timer.stop()
        if hasattr(self, "t_buttons"):
            self.t_buttons["play"].setChecked(False)
            self.t_buttons["play"].setText("▶ 재생")
            if self.asset is not None:
                self._fill_timeline()

    def _tick(self):
        self.step_frame(+1)

    def _onion_frames_changed(self, on: bool):
        self.canvas.onion_frames = bool(on)
        self.canvas.onion_opacity = self.c_onion_alpha.value() / 100
        self._show()

    def set_frame_anchor_override(self, on: bool):
        a = self.asset
        if self._syncing or a is None or (str(self.frame) in a.frame_anchors) == on:
            return
        self._checkpoint()
        if on:
            a.frame_anchors[str(self.frame)] = list(a.anchor)
        else:
            del a.frame_anchors[str(self.frame)]
        self._after_edit()
        self._fill_timeline()

    def fill_attack_anchors(self):
        """Per-frame foot roots of the basic attack, as src/draw.c places them (192 cell space)."""
        a = self.asset
        if a is None:
            return
        roots = [self.specs.preset(f"hero-attack-{i}") for i in range(a.frames)]
        roots = [(i, p) for i, p in enumerate(roots) if p is not None]
        if not roots:
            return
        self._checkpoint()
        for i, preset in roots:
            a.frame_anchors[str(i)] = [float(v) for v in preset.anchor]
        self._after_edit()
        self._fill_timeline()
        self.statusBar().showMessage(f"프레임 {len(roots)}개의 발 기준점을 draw.c 값으로 맞췄습니다", 5000)

    def sheet_preview(self):
        a = self.asset
        return [(f"{a.export_name or a.name}{suffix}.png", image)
                for suffix, image, _ in build_sheets(self.project, a, self.cache)]

    def show_sheet_preview(self):
        a = self.asset
        if a is None:
            return
        try:
            sheets = self.sheet_preview()
        except ValueError as exc:
            QMessageBox.warning(self, "시트 미리보기", str(exc))
            return
        SheetPreviewDialog(sheets, SHEET_LAYOUTS[a.sheet_layout], self).exec()

    # ================================================================ templates
    def template_entries(self) -> tuple[list[tuple[str, Template]], list[str]]:
        builtins, notes = builtin_templates(self.specs)
        entries = [("기본", t) for t in builtins]
        entries += [("내 템플릿", t) for _, t in project_templates(self.project)]
        return entries, notes

    def ask_new_from_template(self):
        if not self.project:
            return
        entries, notes = self.template_entries()
        dialog = TemplateDialog(entries, notes, self)
        if dialog.exec():
            v = dialog.values()
            if v["template"] is not None:
                self.new_asset_from_template(v["template"], v["name"], v["with_reference"])

    def new_asset_from_template(self, template: Template, name: str, with_reference: bool = False):
        self._checkpoint()
        a = asset_from_template(self.project, template, name, self.specs.repo, with_reference)
        self.asset_id, self.view, self.frame = a.id, a.views()[0], 0
        self._pick_top_layer()
        self._mark()
        self.refresh_all()
        self.canvas.fit()
        return a

    def ask_save_template(self):
        a = self.asset
        if a is None:
            return
        label, ok = QInputDialog.getText(self, "템플릿으로 저장",
                                         "템플릿 이름 (그림은 빼고 크기·발 기준점·방향·프레임·내보내기 형식만 저장)",
                                         text=a.name)
        if ok and label.strip():
            path = self.save_as_template(label.strip())
            self.statusBar().showMessage(f"템플릿 저장: {path}", 6000)

    def save_as_template(self, label: str) -> Path:
        slug = "".join(c if c.isascii() and c.isalnum() else "-" for c in label.lower()).strip("-") or \
            f"template-{self.asset.id[-8:]}"
        folder = self.project.root / "templates"
        path = folder / (slug + TEMPLATE_SUFFIX)
        n = 2
        while path.exists():
            path = folder / f"{slug}-{n}{TEMPLATE_SUFFIX}"
            n += 1
        return save_template(Template.from_asset(self.asset, label), path)

    def ask_import_template_file(self):
        if not self.project:
            return
        path, _ = QFileDialog.getOpenFileName(self, "템플릿 파일 가져오기", "", f"팔도 템플릿 (*{TEMPLATE_SUFFIX})")
        if path:
            try:
                saved = self.import_template_file(Path(path))
            except (OSError, ValueError, TypeError) as exc:
                QMessageBox.critical(self, "템플릿 가져오기 실패", str(exc))
                return
            self.statusBar().showMessage(f"템플릿을 프로젝트에 넣었습니다: {saved.name}", 6000)

    def import_template_file(self, path: Path) -> Path:
        """Copy a template file from elsewhere into this project's templates/ (validated first)."""
        template = load_template(path)
        return save_template(template, self.project.root / "templates" / Path(path).name)

    # ================================================================ 자동 보정
    def _cell_has_reference(self) -> bool:
        a = self.asset
        return bool(a and any(l.slot == "reference" for l in a.cell_layers(self.view, self.frame)))

    def ask_import_artwork(self):
        if not self.asset:
            return
        path, _ = QFileDialog.getOpenFileName(self, "GPT 원화 가져오기", "", "PNG 이미지 (*.png)")
        if not path:
            return
        try:
            image = open_png(Path(path))
        except (OSError, ValueError) as exc:
            QMessageBox.critical(self, "가져오기 실패", str(exc))
            return
        dialog = ArtworkImportDialog(Path(path), image, self._cell_has_reference(), self)
        if dialog.exec():
            layer = self.import_artwork(Path(path), **dialog.values())
            self.statusBar().showMessage(f"원화 레이어 '{layer.name}' 배율 {layer.scale:g}", 6000)

    def import_artwork(self, path: Path, remove_bg=True, tolerance=24, feather=True, clean_noise=True,
                       height=None, snap=True, slot="body"):
        """GPT 원화 → (배경 제거 · 잡점 제거 → 새 사본) → 키 맞추기 → 발 맞추기, as ONE undo step."""
        a = self.asset
        original = open_png(Path(path))
        self._checkpoint()
        source = self.project.import_png(Path(path), original.width, original.height, original.mode)
        layer = self.project.add_layer(a, source, Path(path).stem, None, slot, None, view=self.view,
                                       frame=self.frame)
        image, params = original.convert("RGBA"), {}
        if remove_bg:
            result = autofix.remove_background(image, tolerance, feather)
            if result.colour is not None:
                image = result.image
                params["background"] = {"colour": list(result.colour), "tolerance": tolerance, "feather": feather,
                                        "removed": result.removed}
        if clean_noise:
            image, changed = autofix.clean_alpha_noise(image)
            params["noise"] = {"cutoff": autofix.NOISE_ALPHA, "changed": changed}
        if params.get("background") or params.get("noise", {}).get("changed"):
            autofix.add_derived_source(self.project, image, layer, "cleaned", params)
        target = None
        if height == "reference":
            target = autofix.reference_height(self.project, a, self.cache, self.view, self.frame)
        elif isinstance(height, (int, float)):
            target = float(height)
        if target:
            autofix.match_height(self.project, layer, self.cache, target)
        if snap:
            autofix.snap_cell_to_foot(self.project, a, self.cache, self.view, self.frame, layers=[layer])
        self.layer_id = layer.id
        self._mark()
        self.refresh_all()
        return layer

    def ask_remove_background(self):
        l = self.layer
        if l is None:
            return
        tolerance, ok = QInputDialog.getInt(self, "배경 제거", "배경 색 허용 차이 (1~128)", 24, 1, 128)
        if ok:
            result = self.remove_background(tolerance)
            if result.colour is None:
                QMessageBox.information(self, "배경 제거", result.reason)

    def remove_background(self, tolerance: int = 24, feather: bool = True):
        l = self.layer
        result = autofix.remove_background(self.cache.get(self.project, l.source), tolerance, feather)
        if result.colour is None:
            return result
        self._checkpoint()
        autofix.add_derived_source(self.project, result.image, l, "background",
                                   {"colour": list(result.colour), "tolerance": tolerance, "feather": feather})
        self._after_edit(layers=True)
        return result

    def clean_noise(self) -> int:
        l = self.layer
        if l is None:
            return 0
        image, changed = autofix.clean_alpha_noise(self.cache.get(self.project, l.source))
        if changed:
            self._checkpoint()
            autofix.add_derived_source(self.project, image, l, "noise", {"cutoff": autofix.NOISE_ALPHA})
            self._after_edit(layers=True)
        self.statusBar().showMessage(f"잡점 {changed}px", 4000)
        return changed

    def snap_foot(self, every_cell: bool = False):
        a = self.asset
        if a is None:
            return None
        self._checkpoint()
        if every_cell:
            moved = autofix.snap_all_to_foot(self.project, a, self.cache)
            text = f"발 맞추기: {len(moved)}칸 이동"
        else:
            moved = autofix.snap_cell_to_foot(self.project, a, self.cache, self.view, self.frame)
            text = f"발 맞추기: {moved}" if moved else "발 맞추기: 그림 없음"
        self._after_edit()
        self._fill_timeline()
        self.statusBar().showMessage(text, 5000)
        return moved

    def match_reference_height(self):
        a, l = self.asset, self.layer
        if a is None or l is None or l.slot == "reference":
            return None
        target = autofix.reference_height(self.project, a, self.cache, self.view, self.frame)
        if not target:
            self.statusBar().showMessage("이 칸에 보이는 참고 레이어가 없습니다", 5000)
            return None
        self._checkpoint()
        scale = autofix.match_height(self.project, l, self.cache, target)
        self._after_edit()
        return scale

    # ================================================================ check & export
    def run_check(self) -> list[dict]:
        return check_asset(self.project, self.asset, self.cache, self.specs)

    def show_check(self):
        if self.asset:
            dialog = CheckDialog(self.run_check(), f"검사 — {self.asset.name}", False, self)
            dialog.on_pick = self.goto_check_item
            dialog.exec()

    def ask_export(self):
        if not self.asset:
            return
        if self.dirty and not self.save_project():
            return
        report = self.run_check()
        dialog = CheckDialog(report, f"내보내기 전 검사 — {self.asset.name}", True, self)
        dialog.on_pick = self.goto_check_item
        if not dialog.exec():
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
        QMessageBox.information(self, "내보내기 완료", "\n".join(str(p) for p in [*png, meta]))

    def export_current(self, report=None, overwrite=False):
        report = report if report is not None else self.run_check()
        if has_errors(report):
            raise ValueError("검사 오류가 있어 내보낼 수 없습니다")
        return export_asset(self.project, self.asset, self.cache, report, overwrite)

    def _show_exports_path(self):
        if self.project:
            QMessageBox.information(self, "내보내기 폴더", str(self.project.exports_dir))
