"""Pixel work with Pillow and NumPy: loading, inspection, compositing, export."""

from __future__ import annotations

import json
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
from PIL import Image

from . import __version__
from .model import Asset, Layer, Project, sha256_file


@dataclass
class ImageInfo:
    width: int
    height: int
    mode: str
    has_alpha: bool
    alpha_min: int
    alpha_max: int
    transparent_ratio: float     # alpha == 0
    partial_ratio: float         # 0 < alpha < 255
    bbox: tuple[int, int, int, int] | None   # visible pixels, right/bottom exclusive

    def summary(self) -> list[str]:
        lines = [f"크기 {self.width}×{self.height}, 모드 {self.mode}"]
        if not self.has_alpha:
            lines.append("알파 채널 없음 (투명 배경 아님)")
        else:
            lines.append(f"알파 {self.alpha_min}~{self.alpha_max}, 완전 투명 {self.transparent_ratio:.1%}, "
                         f"반투명 {self.partial_ratio:.1%}")
        lines.append(f"보이는 영역 {self.bbox}" if self.bbox else "보이는 픽셀 없음")
        return lines


def open_png(path: Path) -> Image.Image:
    with Image.open(path) as im:
        if im.format != "PNG":
            raise ValueError(f"PNG가 아닙니다: {im.format}")
        im.load()
        return im.copy()


def inspect(image: Image.Image) -> ImageInfo:
    has_alpha = image.mode in ("RGBA", "LA", "PA") or (image.mode == "P" and "transparency" in image.info)
    rgba = image.convert("RGBA")
    alpha = np.asarray(rgba.getchannel("A"))
    total = alpha.size or 1
    bbox = rgba.getchannel("A").getbbox()
    return ImageInfo(image.width, image.height, image.mode, has_alpha,
                     int(alpha.min()) if alpha.size else 0, int(alpha.max()) if alpha.size else 0,
                     float((alpha == 0).sum()) / total,
                     float(((alpha > 0) & (alpha < 255)).sum()) / total, bbox)


class ImageCache:
    """Decoded source images keyed by project-relative path."""

    def __init__(self):
        self._images: dict[str, Image.Image] = {}
        self._layers: dict[tuple, Image.Image] = {}

    def get(self, project: Project, source_id: str) -> Image.Image | None:
        source = project.source(source_id)
        if source is None:
            return None
        if source.file not in self._images:
            path = project.source_path(source)
            if not path.is_file():
                return None
            self._images[source.file] = open_png(path).convert("RGBA")
        return self._images[source.file]

    def layer_image(self, project: Project, layer: Layer) -> Image.Image | None:
        """The layer's pixels after crop and scale (the original stays untouched)."""
        image = self.get(project, layer.source)
        if image is None:
            return None
        key = (layer.source, tuple(layer.crop) if layer.crop else None, float(layer.scale), bool(layer.smooth))
        if key in self._layers:
            return self._layers[key]
        if layer.crop:
            x, y, w, h = layer.crop
            image = image.crop((x, y, x + w, y + h))
        if layer.scale != 1:
            image = scale_rgba(image, layer.scale, layer.smooth)
        if len(self._layers) > 256:
            self._layers.clear()
        self._layers[key] = image
        return image

    def clear(self):
        self._images.clear()
        self._layers.clear()


def scale_rgba(image: Image.Image, scale: float, smooth: bool = True) -> Image.Image:
    """Resize without dark fringes: filter in premultiplied alpha (RGBa)."""
    size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
    if not smooth:
        return image.resize(size, Image.Resampling.NEAREST)
    return image.convert("RGBa").resize(size, Image.Resampling.LANCZOS).convert("RGBA")


def _with_opacity(image: Image.Image, opacity: float) -> Image.Image:
    if opacity >= 1:
        return image
    arr = np.asarray(image).copy()
    arr[..., 3] = np.round(arr[..., 3].astype(np.float32) * max(0.0, opacity)).astype(np.uint8)
    return Image.fromarray(arr, "RGBA")


def compose(project: Project, asset: Asset, cache: ImageCache,
            include_reference: bool = False, view: str | None = None, frame: int = 0) -> Image.Image:
    """Draw visible layers bottom -> top onto a transparent canvas ("over" blend).

    Layers may hang outside the canvas; only the overlap is drawn. ``view`` picks
    one direction of an 8-direction asset (default: the first view). Layers of a
    switched-off 장비 묶음 are skipped.
    """
    if view is None:
        view = asset.views()[0]
    canvas = Image.new("RGBA", (asset.width, asset.height), (0, 0, 0, 0))
    for layer in asset.layers:
        if layer.view != view or layer.frame != frame or not asset.shown(layer) or \
                (layer.slot == "reference" and not include_reference):
            continue
        image = cache.layer_image(project, layer)
        if image is None:
            continue
        image = _with_opacity(image, layer.opacity)
        left, top = max(0, layer.x), max(0, layer.y)
        right = min(asset.width, layer.x + image.width)
        bottom = min(asset.height, layer.y + image.height)
        if right <= left or bottom <= top:
            continue
        piece = image.crop((left - layer.x, top - layer.y, right - layer.x, bottom - layer.y))
        canvas.alpha_composite(piece, (left, top))
    return canvas


def layer_bounds(project: Project, layer: Layer, cache: ImageCache) -> tuple[int, int, int, int] | None:
    image = cache.layer_image(project, layer)
    if image is None:
        return None
    return layer.x, layer.y, layer.x + image.width, layer.y + image.height


@dataclass
class Measure:
    """Where the visible art of one composed view sits, for the alignment checks."""
    bbox: tuple[int, int, int, int] | None
    contact_y: int | None      # lowest visible row (the sole touching the ground)
    foot_x: float | None       # alpha-weighted centre of the lowest 12 % (at least 4 px) of the art
    margin: int | None         # smallest distance from the art to the canvas edge
    centre: tuple[float, float] | None = None  # alpha-weighted centre of all visible art


# Alpha 1..4 specks are treated as noise, as in scripts/prepare_direction_assets.py
# ("alpha_noise_cutoff": 4 in art/hero/directions/preparation.json).
NOISE_ALPHA = 4


def measure(image: Image.Image) -> Measure:
    alpha = np.asarray(image.getchannel("A")).astype(np.float32)
    alpha[alpha <= NOISE_ALPHA] = 0
    bbox = Image.fromarray((alpha > 0).astype(np.uint8) * 255).getbbox()
    if bbox is None:
        return Measure(None, None, None, None)
    x0, y0, x1, y1 = bbox
    band = max(4, round((y1 - y0) * 0.12))
    rows = alpha[max(y0, y1 - band):y1]
    weights = rows.sum(axis=0)
    foot_x = float((weights * np.arange(alpha.shape[1])).sum() / weights.sum()) + 0.5
    margin = min(x0, y0, image.width - x1, image.height - y1)
    ys, xs = np.indices(alpha.shape)
    total = alpha.sum()
    centre = (float((alpha * xs).sum() / total) + 0.5, float((alpha * ys).sum() / total) + 0.5)
    return Measure(bbox, y1 - 1, foot_x, margin, centre)


def view_suffix(view: str) -> str:
    return f"-{view.lower()}" if view else ""


# Export layouts. Every sheet layout reproduces one that the game already loads:
SHEET_LAYOUTS = {
    "cells": "셀마다 PNG 한 장",
    "per_view": "방향마다 시트 한 장, 프레임을 열 수만큼 가로로 (warrior-walk-*-v1, attack·무공 3×2)",
    "views_in_cells": "시트 한 장, 방향을 열 수만큼 가로로 (warrior-idle-8dir-v1 4×2, 프레임 1개일 때)",
    "rows_by_view": "시트 한 장, 행 = 방향·열 = 프레임 (monsters/*-walk-8dir-v1)",
}


def layout_problem(asset: Asset) -> str | None:
    if asset.sheet_layout not in SHEET_LAYOUTS:
        return f"알 수 없는 내보내기 형식 {asset.sheet_layout}"
    if asset.sheet_columns < 1:
        return "시트 열 수는 1 이상이어야 합니다"
    if asset.sheet_layout == "views_in_cells" and asset.frames != 1:
        return "방향을 칸으로 늘어놓는 시트는 프레임이 1개일 때만 쓸 수 있습니다"
    if asset.sheet_layout == "rows_by_view" and not asset.directions:
        return "행 = 방향 시트는 8방향 에셋에서만 쓸 수 있습니다"
    return None


def build_sheets(project: Project, asset: Asset, cache: ImageCache) -> list[tuple[str, Image.Image, dict]]:
    """(file suffix, image, what it holds) per output file for ``asset.sheet_layout``."""
    problem = layout_problem(asset)
    if problem:
        raise ValueError(problem)
    w, h, cols = asset.width, asset.height, asset.sheet_columns
    views, frames = asset.views(), asset.frames

    def grid(cells: list[Image.Image], columns: int) -> Image.Image:
        rows = -(-len(cells) // columns)
        sheet = Image.new("RGBA", (w * min(columns, len(cells)), h * rows), (0, 0, 0, 0))
        for i, cell in enumerate(cells):
            sheet.paste(cell, (i % columns * w, i // columns * h))
        return sheet

    if asset.sheet_layout == "cells":
        out = []
        for v in views:
            for f in range(frames):
                suffix = view_suffix(v) + (f"-f{f}" if frames > 1 else "")
                out.append((suffix, compose(project, asset, cache, view=v, frame=f), {"view": v, "frame": f}))
        return out
    if asset.sheet_layout == "per_view":
        return [(view_suffix(v), grid([compose(project, asset, cache, view=v, frame=f) for f in range(frames)], cols),
                 {"view": v, "frames": frames, "columns": cols}) for v in views]
    if asset.sheet_layout == "views_in_cells":
        return [("", grid([compose(project, asset, cache, view=v) for v in views], cols),
                 {"views": views, "columns": cols})]
    cells = [compose(project, asset, cache, view=v, frame=f) for v in views for f in range(frames)]
    return [("", grid(cells, frames), {"rows": views, "frames": frames})]


def export_asset(project: Project, asset: Asset, cache: ImageCache, report: list[dict],
                 overwrite: bool = False) -> tuple[list[Path], Path]:
    """Write the PNGs of ``asset.sheet_layout`` and one ``exports/<name>.json``.

    Sources are never targets. Returns (PNG paths in output order, metadata path).
    """
    name = asset.export_name or asset.name
    if not name or any(c in name for c in "/\\:*?\"<>|") or name.startswith("."):
        raise ValueError(f"내보내기 파일명이 올바르지 않습니다: {name!r}")
    folder = project.exports_dir
    folder.mkdir(parents=True, exist_ok=True)
    sheets = build_sheets(project, asset, cache)
    pngs = [folder / f"{name}{suffix}.png" for suffix, _, _ in sheets]
    meta = folder / f"{name}.json"
    protected = {project.source_path(s).resolve() for s in project.sources}
    for target in (*pngs, meta):
        if target.resolve() in protected:
            raise ValueError("원본 이미지를 덮어쓸 수 없습니다")
        if target.exists() and not overwrite:
            raise FileExistsError(str(target))
    outputs = []
    for (_, image, holds), png in zip(sheets, pngs):
        image.save(png, "PNG")
        outputs.append({**holds, "file": png.name, "size": list(image.size), "sha256": sha256_file(png)})
    used = {l.source for l in asset.layers}
    metadata = {
        "tool": f"Paldo Asset Studio {__version__}",
        "exported": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "asset": {"name": asset.name, "kind": asset.kind, "preset": asset.preset,
                  "size": [asset.width, asset.height], "anchor": asset.anchor,
                  "directions": asset.directions, "hidden_groups": asset.hidden_groups,
                  "frames": asset.frames, "frame_anchors": asset.frame_anchors,
                  "layout": asset.sheet_layout, "columns": asset.sheet_columns,
                  "preview_fps": asset.fps},
        "outputs": outputs,
        "layers": [{"name": l.name, "view": l.view, "frame": l.frame, "group": l.group, "slot": l.slot,
                    "visible": l.visible, "opacity": l.opacity,
                    "position": [l.x, l.y], "pivot": l.pivot, "crop": l.crop,
                    "scale": l.scale, "smooth": l.smooth,
                    "source": l.source} for l in asset.layers],
        "sources": [{"id": s.id, "file": s.file, "original_name": s.original_name,
                     "sha256": s.sha256} for s in project.sources if s.id in used],
        "checks": report,
        "note": "실행용 사본입니다. 게임 assets/ 폴더에 직접 쓰지 않았습니다.",
    }
    meta.write_text(json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8")
    return pngs, meta
