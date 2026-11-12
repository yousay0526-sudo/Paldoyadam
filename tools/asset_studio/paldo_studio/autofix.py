"""자동 보정: small, explicit corrections. No Qt.

Two kinds, both undoable from the editor:

* Image corrections (배경 제거, 잡점 제거) never change an existing file. They
  write a NEW image into the project's ``sources/`` (the original copy stays) and
  point the layer at it; the new source records what it was made from.
* Placement corrections (발 맞추기, 키 맞추기) only change layer position and
  scale, measured with the same ``imaging.measure`` the checks use.

Nothing here changes game values (cell size, foot roots, draw scales) and
nothing calls the network.
"""

from __future__ import annotations

import tempfile
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from .imaging import NOISE_ALPHA, ImageCache, compose, measure
from .model import Asset, Layer, Project


# ---------------------------------------------------------------- image corrections
@dataclass
class BackgroundResult:
    image: Image.Image
    colour: tuple[int, int, int] | None   # the background colour that was removed (None = nothing to do)
    removed: int                          # pixels made transparent
    reason: str = ""


def background_colour(image: Image.Image) -> tuple[int, int, int] | None:
    """The colour shared by the four corners, or None when they are transparent or disagree."""
    rgba = np.asarray(image.convert("RGBA")).astype(int)
    corners = [rgba[0, 0], rgba[0, -1], rgba[-1, 0], rgba[-1, -1]]
    if any(c[3] < 250 for c in corners):
        return None
    base = np.median(np.array([c[:3] for c in corners]), axis=0)
    if any(np.abs(c[:3] - base).max() > 24 for c in corners):
        return None
    return tuple(int(v) for v in base)


def remove_background(image: Image.Image, tolerance: int = 24, feather: bool = True) -> BackgroundResult:
    """Make the background that touches the image border transparent.

    The background colour is taken from the four corners. Only pixels connected
    to the border and within ``tolerance`` (largest channel difference) are
    removed, so the same colour inside the figure stays. With ``feather`` the
    one-pixel rim next to the removed area gets partial alpha by its colour
    distance, which softens the cut-out edge a little.
    """
    rgba = image.convert("RGBA")
    colour = background_colour(rgba)
    if colour is None:
        return BackgroundResult(rgba.copy(), None, 0, "네 모서리가 투명하거나 색이 서로 달라 배경색을 정하지 못했습니다")
    arr = np.asarray(rgba).astype(int)
    diff = np.abs(arr[..., :3] - np.array(colour)).max(axis=2)
    near = diff <= tolerance
    # Flood from the border through "near background" pixels: pad by one so a single fill reaches every edge.
    # copy(): an image made straight from a NumPy buffer can ignore in-place drawing
    mask = Image.fromarray(np.pad(near, 1, constant_values=True).astype(np.uint8) * 255, "L").copy()
    ImageDraw.floodfill(mask, (0, 0), 128)
    background = (np.asarray(mask)[1:-1, 1:-1] == 128)
    out = arr.copy()
    out[background, 3] = 0
    if feather:
        # Rim: not background, but next to it. Alpha grows with distance from the background colour.
        grown = background.copy()
        grown[1:, :] |= background[:-1, :]
        grown[:-1, :] |= background[1:, :]
        grown[:, 1:] |= background[:, :-1]
        grown[:, :-1] |= background[:, 1:]
        rim = grown & ~background
        k = np.clip((diff[rim] - tolerance) / max(tolerance, 1), 0, 1)
        out[rim, 3] = np.minimum(out[rim, 3], (k * 255).round().astype(int))
    out[out[..., 3] == 0, :3] = 0
    return BackgroundResult(Image.fromarray(out.astype(np.uint8), "RGBA"), colour, int(background.sum()))


def clean_alpha_noise(image: Image.Image, cutoff: int = NOISE_ALPHA) -> tuple[Image.Image, int]:
    """Alpha 1..cutoff becomes 0 (the same cutoff the checks ignore). Returns (image, pixels changed)."""
    arr = np.asarray(image.convert("RGBA")).copy()
    noise = (arr[..., 3] > 0) & (arr[..., 3] <= cutoff)
    arr[noise] = 0
    return Image.fromarray(arr, "RGBA"), int(noise.sum())


def add_derived_source(project: Project, image: Image.Image, layer: Layer, operation: str, params: dict):
    """Store ``image`` as a new source made from ``layer``'s source and point the layer at it."""
    base = project.source(layer.source)
    if base is None:
        raise ValueError("레이어의 원본이 없습니다")
    stem = Path(base.original_name).stem
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / f"{stem}-{operation}.png"
        image.save(path, "PNG")
        source = project.import_png(path, image.width, image.height, image.mode, derived={
            "from": base.id, "from_sha256": base.sha256, "operation": operation, "params": params})
    source.original_path = ""  # made by the studio; there is no outside file
    layer.source = source.id
    return source


# ---------------------------------------------------------------- placement corrections
def _exported_layers(asset: Asset, view: str, frame: int) -> list[Layer]:
    return [l for l in asset.cell_layers(view, frame) if asset.shown(l) and l.slot != "reference"]


def _composite(project: Project, asset: Asset, cache: ImageCache, layers: list[Layer]) -> Image.Image:
    canvas = Image.new("RGBA", (asset.width, asset.height), (0, 0, 0, 0))
    for layer in layers:
        image = cache.layer_image(project, layer)
        if image is not None:
            sheet = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
            sheet.paste(image, (layer.x, layer.y))  # plain copy, clipped at the canvas edge
            canvas = Image.alpha_composite(canvas, sheet)
    return canvas


def snap_cell_to_foot(project: Project, asset: Asset, cache: ImageCache, view: str, frame: int,
                      ground: bool = True, axis: bool = True,
                      layers: list[Layer] | None = None) -> tuple[int, int] | None:
    """Move the exported layers of one cell so the measured ground / foot centre meets the foot root.

    With ``layers`` only those layers are measured and moved (e.g. one new artwork
    on top of an existing body). Returns the (dx, dy) applied, or None when there
    is no visible art.
    """
    if layers is None:
        layers = _exported_layers(asset, view, frame)
        image = compose(project, asset, cache, view=view, frame=frame) if layers else None
    else:
        image = _composite(project, asset, cache, layers) if layers else None
    if image is None:
        return None
    m = measure(image)
    if m.bbox is None:
        return None
    ax, ay = asset.anchor_for(frame)
    dx = round(ax - m.foot_x) if axis else 0
    dy = round(ay - m.contact_y) if ground else 0
    for layer in layers:
        layer.x += dx
        layer.y += dy
    return dx, dy


def snap_all_to_foot(project: Project, asset: Asset, cache: ImageCache, ground: bool = True,
                     axis: bool = True) -> dict[tuple[str, int], tuple[int, int]]:
    """``snap_cell_to_foot`` for every direction and frame; only cells that moved are returned."""
    moved = {}
    for view in asset.views():
        for frame in range(asset.frames):
            delta = snap_cell_to_foot(project, asset, cache, view, frame, ground, axis)
            if delta and delta != (0, 0):
                moved[(view, frame)] = delta
    return moved


def visible_height(image: Image.Image) -> int | None:
    """Height of the art, ignoring alpha 1..4 like the checks (``imaging.measure``)."""
    m = measure(image)
    return None if m.bbox is None else m.bbox[3] - m.bbox[1]


def reference_height(project: Project, asset: Asset, cache: ImageCache, view: str, frame: int) -> int | None:
    """Visible height of the reference layers of one cell (e.g. the runtime cell from a template)."""
    refs = [l for l in asset.cell_layers(view, frame) if l.slot == "reference" and l.visible]
    return visible_height(_composite(project, asset, cache, refs)) if refs else None


def match_height(project: Project, layer: Layer, cache: ImageCache, target: float) -> float | None:
    """Scale ``layer`` so its art is ``target`` px tall, keeping its bottom-centre in place.

    The measured height is a step function of the scale (resampling softens the
    edges), so after the proportional estimate a few nearby scales are tried and
    the one whose measured height is closest to ``target`` wins (it can stay 1 px
    off when no nearby scale lands exactly on it). Returns the new
    scale, or None when the layer has no visible art.
    """
    image = cache.layer_image(project, layer)
    m = measure(image) if image is not None and target > 0 else None
    if m is None or m.bbox is None:
        return None
    x0, y0, x1, y1 = m.bbox
    foot = (layer.x + (x0 + x1) / 2, layer.y + y1)               # canvas point to keep
    in_source = ((x0 + x1) / 2 / layer.scale, y1 / layer.scale)  # same point before scaling
    estimate = layer.scale * target / (y1 - y0)
    best = None
    for k in (0, 1, -1, 2, -2, 3, -3, 4, -4, 6, -6, 8, -8):
        candidate = round(estimate * (1 + k * 0.004), 4)
        layer.scale = candidate
        trial = cache.layer_image(project, layer)
        mm = measure(trial)
        if mm.bbox is None:
            continue
        error = abs(mm.bbox[3] - mm.bbox[1] - target)
        if best is None or error < best[0]:
            best = (error, candidate, mm.bbox)
        if error < 0.5:
            break
    _, layer.scale, (bx0, _by0, bx1, by1) = best
    # keep the bottom-centre: use the measured bottom of the chosen scale
    layer.x = round(foot[0] - (bx0 + bx1) / 2)
    layer.y = round(foot[1] - by1)
    return layer.scale
