"""Central work area: transparent checkerboard, zoom/pan, guides, layer dragging."""

from __future__ import annotations

from PIL import Image
from PySide6.QtCore import QPointF, QRectF, Qt, Signal
from PySide6.QtGui import QColor, QImage, QPainter, QPen, QPixmap
from PySide6.QtWidgets import QWidget

from ..imaging import ImageCache, compose, measure
from ..model import Asset, Project, pivot_on_canvas

BACKGROUNDS = {
    "checker": "투명 격자",
    "dark": "어두운 배경",
    "light": "밝은 배경",
    "ground": "마을 땅색",
}


def pil_to_qimage(image: Image.Image) -> QImage:
    rgba = image.convert("RGBA")
    data = rgba.tobytes("raw", "RGBA")
    return QImage(data, rgba.width, rgba.height, rgba.width * 4, QImage.Format_RGBA8888).copy()


class Canvas(QWidget):
    layerSelected = Signal(str)            # layer id ("" = none)
    aboutToChange = Signal(str)            # merge key for History.checkpoint
    changed = Signal()
    cursorMoved = Signal(float, float, bool)   # canvas x, y, inside canvas
    zoomChanged = Signal(float)

    MODES = ("move", "anchor", "pivot")

    def __init__(self, cache: ImageCache, parent=None):
        super().__init__(parent)
        self.cache = cache
        self.project: Project | None = None
        self.asset: Asset | None = None
        self.selected: str = ""
        self.zoom = 3.0
        self.pan = QPointF(40, 40)
        self.mode = "move"
        self.background = "checker"
        self.ground_image: QImage | None = None
        self.show_grid = True
        self.show_guides = True
        self.view = ""
        self.frame = 0
        self.onion_frames = False              # previous / next frame of this direction, translucent
        self._onion_frames: list[tuple[QImage, QColor]] = []
        self.onion_view: str | None = None   # another direction drawn on top, translucent
        self.onion_opacity = 0.35
        self._onion: QImage | None = None
        self.show_measure = True               # measured ground contact / foot centre of this view
        self.measured = None
        self._qimages: dict[tuple, QImage] = {}
        self._drag: tuple | None = None
        self._pan_from: tuple | None = None
        self._drag_serial = 0
        self.setMinimumSize(320, 240)
        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.StrongFocus)

    # ---- data -----------------------------------------------------------
    def set_asset(self, project: Project | None, asset: Asset | None, selected: str = "", view: str = "",
                  frame: int = 0):
        self.project, self.asset, self.selected, self.view, self.frame = project, asset, selected, view, frame
        self._onion = None
        self._onion_frames = []
        if asset is not None and self.onion_view and self.onion_view != view and \
                self.onion_view in asset.views():
            self._onion = pil_to_qimage(compose(project, asset, self.cache, view=self.onion_view, frame=frame))
        if asset is not None and self.onion_frames and asset.frames > 1:
            # previous frame tinted red, next frame tinted green, like common animation tools
            for other, tint in ((frame - 1, QColor(255, 90, 90)), (frame + 1, QColor(90, 220, 120))):
                if 0 <= other < asset.frames:
                    image = compose(project, asset, self.cache, view=view, frame=other)
                    self._onion_frames.append((_tinted(pil_to_qimage(image), tint), tint))
        self.measured = measure(compose(project, asset, self.cache, view=view, frame=frame)) \
            if asset is not None else None
        self.update()

    def _layers(self):
        return [l for l in self.asset.cell_layers(self.view, self.frame) if self.asset.shown(l)]

    def _anchor(self):
        return self.asset.anchor_for(self.frame)

    def invalidate_images(self):
        self._qimages.clear()
        self.update()

    def _layer_qimage(self, layer) -> QImage | None:
        key = (layer.source, tuple(layer.crop) if layer.crop else None, layer.scale, layer.smooth)
        if key not in self._qimages:
            image = self.cache.layer_image(self.project, layer)
            if image is None:
                return None
            self._qimages[key] = pil_to_qimage(image)
        return self._qimages[key]

    # ---- view -----------------------------------------------------------
    def set_zoom(self, zoom: float, around: QPointF | None = None):
        zoom = max(0.25, min(32.0, zoom))
        if around is None:
            around = QPointF(self.width() / 2, self.height() / 2)
        cx, cy = self.to_canvas(around)
        self.zoom = zoom
        self.pan = QPointF(around.x() - cx * zoom, around.y() - cy * zoom)
        self.zoomChanged.emit(self.zoom)
        self.update()

    def fit(self):
        if not self.asset:
            return
        margin = 40
        zoom = min((self.width() - margin * 2) / self.asset.width,
                   (self.height() - margin * 2) / self.asset.height)
        self.zoom = max(0.25, min(32.0, zoom))
        self.pan = QPointF((self.width() - self.asset.width * self.zoom) / 2,
                           (self.height() - self.asset.height * self.zoom) / 2)
        self.zoomChanged.emit(self.zoom)
        self.update()

    def to_canvas(self, p: QPointF) -> tuple[float, float]:
        return (p.x() - self.pan.x()) / self.zoom, (p.y() - self.pan.y()) / self.zoom

    # ---- painting -------------------------------------------------------
    def paintEvent(self, _event):
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor(46, 46, 52))
        if not self.asset or not self.project:
            painter.setPen(QColor(190, 190, 190))
            painter.drawText(self.rect(), Qt.AlignCenter,
                             "파일 ▸ 새 프로젝트 또는 열기로 시작하세요.\n그다음 에셋을 만들고 PNG를 가져옵니다.")
            return
        a = self.asset
        painter.translate(self.pan)
        painter.scale(self.zoom, self.zoom)
        area = QRectF(0, 0, a.width, a.height)
        self._paint_background(painter, area)
        painter.setRenderHint(QPainter.SmoothPixmapTransform, False)
        for image, _ in self._onion_frames:
            painter.setOpacity(self.onion_opacity)
            painter.drawImage(QPointF(0, 0), image)
        for layer in self._layers():
            image = self._layer_qimage(layer)
            if image is None:
                continue
            painter.setOpacity(layer.opacity * (0.5 if layer.slot == "reference" else 1))
            painter.drawImage(QPointF(layer.x, layer.y), image)
        if self._onion is not None:
            painter.setOpacity(self.onion_opacity)
            painter.drawImage(QPointF(0, 0), self._onion)
        painter.setOpacity(1)
        painter.resetTransform()
        self._paint_overlay(painter)

    def _paint_background(self, painter: QPainter, area: QRectF):
        if self.background == "dark":
            painter.fillRect(area, QColor(24, 26, 30))
        elif self.background == "light":
            painter.fillRect(area, QColor(236, 232, 222))
        elif self.background == "ground" and self.ground_image is not None:
            painter.drawImage(area, self.ground_image, QRectF(0, 0, area.width(), area.height()))
        elif self.background == "ground":
            painter.fillRect(area, QColor(122, 101, 72))
        else:
            tile = 8
            light, dark = QColor(204, 204, 204), QColor(153, 153, 153)
            for ty in range(0, int(area.height()), tile):
                for tx in range(0, int(area.width()), tile):
                    color = light if (tx // tile + ty // tile) % 2 == 0 else dark
                    painter.fillRect(QRectF(tx, ty, min(tile, area.width() - tx),
                                            min(tile, area.height() - ty)), color)

    def _sx(self, x: float) -> float:
        return self.pan.x() + x * self.zoom

    def _sy(self, y: float) -> float:
        return self.pan.y() + y * self.zoom

    def _paint_overlay(self, painter: QPainter):
        a = self.asset
        left, top, right, bottom = self._sx(0), self._sy(0), self._sx(a.width), self._sy(a.height)
        if self.show_grid and self.zoom >= 8:
            painter.setPen(QPen(QColor(0, 0, 0, 40), 1))
            for x in range(a.width + 1):
                painter.drawLine(QPointF(self._sx(x), top), QPointF(self._sx(x), bottom))
            for y in range(a.height + 1):
                painter.drawLine(QPointF(left, self._sy(y)), QPointF(right, self._sy(y)))
        painter.setPen(QPen(QColor(255, 255, 255, 160), 1))
        painter.drawRect(QRectF(left, top, right - left, bottom - top))
        if self.show_guides:
            anchor = self._anchor()
            ax, ay = self._sx(anchor[0]), self._sy(anchor[1])
            painter.setPen(QPen(QColor(80, 220, 255, 200), 1, Qt.DashLine))   # 중심축
            painter.drawLine(QPointF(ax, top), QPointF(ax, bottom))
            painter.setPen(QPen(QColor(110, 240, 120, 220), 1, Qt.DashLine))  # 발 접지선
            painter.drawLine(QPointF(left, ay), QPointF(right, ay))
            painter.setPen(QPen(QColor(110, 240, 120), 2))
            painter.drawEllipse(QPointF(ax, ay), 5, 5)
            painter.drawText(QPointF(ax + 8, ay - 6),
                             "발 기준점 (이 프레임 전용)" if str(self.frame) in a.frame_anchors else "발 기준점")
        m = self.measured
        if self.show_measure and m is not None and m.bbox is not None:
            pen = QPen(QColor(255, 80, 200, 230), 1, Qt.DotLine)
            painter.setPen(pen)
            cy = self._sy(m.contact_y)
            painter.drawLine(QPointF(left, cy), QPointF(right, cy))
            fx = self._sx(m.foot_x)
            painter.drawLine(QPointF(fx, cy - 18), QPointF(fx, cy + 6))
            painter.drawText(QPointF(left + 6, cy + 16), f"측정 바닥 y={m.contact_y}  발중심 x={m.foot_x:.1f}")
        title = []
        if self.view:
            title.append(f"방향 {self.view}")
        if a.frames > 1:
            title.append(f"프레임 {self.frame + 1}/{a.frames}")
        if self._onion is not None:
            title.append(f"(겹쳐 보기: {self.onion_view})")
        if self._onion_frames:
            title.append("(앞 프레임 빨강 · 뒤 프레임 초록)")
        if title:
            painter.setPen(QColor(255, 255, 255))
            painter.drawText(QPointF(left + 6, top - 6), "  ".join(title))
        layer = a.layer(self.selected) if self.selected else None
        if layer is not None:
            image = self._layer_qimage(layer)
            if image is not None:
                painter.setPen(QPen(QColor(255, 210, 60), 1, Qt.DashLine))
                painter.drawRect(QRectF(self._sx(layer.x), self._sy(layer.y),
                                        image.width() * self.zoom, image.height() * self.zoom))
            if layer.pivot is not None:
                cx, cy = pivot_on_canvas(layer)
                px, py = self._sx(cx), self._sy(cy)
                painter.setPen(QPen(QColor(255, 140, 40), 2))
                painter.drawLine(QPointF(px - 7, py), QPointF(px + 7, py))
                painter.drawLine(QPointF(px, py - 7), QPointF(px, py + 7))
                painter.drawText(QPointF(px + 8, py + 18), "레이어 기준점")

    # ---- interaction ----------------------------------------------------
    def _hit(self, cx: float, cy: float):
        """Top-most visible, unlocked layer with a visible pixel under (cx, cy)."""
        for layer in reversed(self._layers()):
            if layer.locked:
                continue
            image = self.cache.layer_image(self.project, layer)
            if image is None:
                continue
            lx, ly = int(cx - layer.x), int(cy - layer.y)
            if 0 <= lx < image.width and 0 <= ly < image.height and image.getpixel((lx, ly))[3] > 0:
                return layer
        return None

    def mousePressEvent(self, event):
        if not self.asset:
            return
        self.setFocus()
        pos = event.position()
        if event.button() == Qt.MiddleButton or (event.button() == Qt.LeftButton and
                                                  event.modifiers() & Qt.AltModifier):
            self._pan_from = (pos, QPointF(self.pan))
            return
        if event.button() != Qt.LeftButton:
            return
        cx, cy = self.to_canvas(pos)
        if self.mode == "anchor":
            self.aboutToChange.emit("")
            point = [float(round(cx)), float(round(cy))]
            if str(self.frame) in self.asset.frame_anchors:
                self.asset.frame_anchors[str(self.frame)] = point
            else:
                self.asset.anchor = point
            self.changed.emit()
            return
        layer = self.asset.layer(self.selected) if self.selected else None
        if self.mode == "pivot":
            if layer is not None:
                self.aboutToChange.emit("")
                layer.pivot = [round((cx - layer.x) / layer.scale, 1), round((cy - layer.y) / layer.scale, 1)]
                self.changed.emit()
            return
        hit = self._hit(cx, cy)
        if hit is not None and hit.id != self.selected:
            self.selected = hit.id
            self.layerSelected.emit(hit.id)
            layer = hit
        if layer is not None and not layer.locked:
            self._drag_serial += 1
            self._drag = (layer.id, cx, cy, layer.x, layer.y, False)
        self.update()

    def mouseMoveEvent(self, event):
        pos = event.position()
        if self._pan_from is not None:
            start, pan = self._pan_from
            self.pan = pan + (pos - start)
            self.update()
            return
        if not self.asset:
            return
        cx, cy = self.to_canvas(pos)
        inside = 0 <= cx < self.asset.width and 0 <= cy < self.asset.height
        self.cursorMoved.emit(cx, cy, inside)
        if self._drag is not None:
            layer_id, sx, sy, ox, oy, started = self._drag
            layer = self.asset.layer(layer_id)
            nx, ny = ox + round(cx - sx), oy + round(cy - sy)
            if layer is not None and (nx, ny) != (layer.x, layer.y):
                if not started:
                    self.aboutToChange.emit(f"drag-{self._drag_serial}")
                    self._drag = (layer_id, sx, sy, ox, oy, True)
                layer.x, layer.y = nx, ny
                self.changed.emit()

    def mouseReleaseEvent(self, _event):
        self._drag = None
        self._pan_from = None

    def wheelEvent(self, event):
        steps = event.angleDelta().y() / 120
        if steps:
            self.set_zoom(self.zoom * (1.25 ** steps), event.position())

    def keyPressEvent(self, event):
        layer = self.asset.layer(self.selected) if self.asset and self.selected else None
        moves = {Qt.Key_Left: (-1, 0), Qt.Key_Right: (1, 0), Qt.Key_Up: (0, -1), Qt.Key_Down: (0, 1)}
        if layer is not None and not layer.locked and event.key() in moves:
            step = 10 if event.modifiers() & Qt.ShiftModifier else 1
            dx, dy = moves[event.key()]
            self.aboutToChange.emit(f"nudge-{layer.id}")
            layer.x += dx * step
            layer.y += dy * step
            self.changed.emit()
            return
        super().keyPressEvent(event)


def _tinted(image: QImage, color: QColor) -> QImage:
    """Silhouette of ``image`` in ``color`` (keeps the alpha)."""
    out = QImage(image.size(), QImage.Format_ARGB32_Premultiplied)
    out.fill(Qt.transparent)
    painter = QPainter(out)
    painter.drawImage(0, 0, image)
    painter.setCompositionMode(QPainter.CompositionMode_SourceIn)
    painter.fillRect(out.rect(), color)
    painter.end()
    return out


def preview_pixmap(image: Image.Image, logical: float, background: QColor | QImage | None,
                   magnify: int = 2) -> QPixmap:
    """Game-size readability preview: scale the cell to its on-screen logical size."""
    size = max(1, round(image.width * logical)), max(1, round(image.height * logical))
    small = image.resize(size, Image.Resampling.NEAREST)
    big = small.resize((size[0] * magnify, size[1] * magnify), Image.Resampling.NEAREST)
    pixmap = QPixmap(big.width, big.height)
    painter = QPainter(pixmap)
    if isinstance(background, QImage):
        painter.drawImage(pixmap.rect(), background)
    else:
        painter.fillRect(pixmap.rect(), background or QColor(122, 101, 72))
    painter.drawImage(0, 0, pil_to_qimage(big))
    painter.end()
    return pixmap
