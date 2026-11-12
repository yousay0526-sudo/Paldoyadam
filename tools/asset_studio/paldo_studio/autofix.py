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
    source = derived_source(project, image, base, operation, params)
    layer.source = source.id
    return source


def derived_source(project: Project, image: Image.Image, base, operation: str, params: dict):
    """Store ``image`` in ``sources/`` as made from ``base`` (a Source); the base stays as it is."""
    stem = Path(base.original_name).stem
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / f"{stem}-{operation}.png"
        image.save(path, "PNG")
        source = project.import_png(path, image.width, image.height, image.mode, derived={
            "from": base.id, "from_sha256": base.sha256, "operation": operation, "params": params})
    source.original_path = ""  # made by the studio; there is no outside file
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


# ---------------------------------------------------------------- 원화 아틀라스 자동 분할
@dataclass
class Pose:
    cell: tuple[int, int]               # (column, row) of the atlas grid
    image: Image.Image | None           # the pose alone, cropped to its bounds (None = nothing found)
    bounds: tuple[int, int, int, int] | None   # in atlas pixels
    root: tuple[float, float] | None    # foot root inside ``image``: torso-column median x, lowest row
    warning: str = ""


def find_poses(atlas: Image.Image, cols: int, rows: int, threshold: int = 16) -> list[Pose]:
    """One pose per grid cell, the way ``scripts/prepare_monster_assets.py`` picks them.

    Each cell's pose is the connected region (alpha > ``threshold``) that holds the
    opaque pixel nearest the cell centre; it may reach outside its cell (a tail,
    a weapon). A one-pixel antialias rim is kept, alpha 1..4 is dropped. The foot
    root is the median x of solid pixels in the 40..60 % height band and the lowest
    row, as in that script. Nothing is mirrored, recoloured or invented.
    """
    rgba = atlas.convert("RGBA")
    alpha = np.asarray(rgba)[..., 3]
    binary = alpha > threshold
    cw, ch = rgba.width / cols, rgba.height / rows
    poses = []
    for r in range(rows):
        for c in range(cols):
            x0, y0, x1, y1 = round(c * cw), round(r * ch), round((c + 1) * cw), round((r + 1) * ch)
            region = binary[y0:y1, x0:x1]
            if not region.any():
                poses.append(Pose((c, r), None, None, None, "칸에 그림이 없습니다"))
                continue
            ys, xs = np.nonzero(region)
            i = int(np.argmin((xs - (x1 - x0) / 2) ** 2 + (ys - (y1 - y0) / 2) ** 2))
            seed = (int(xs[i]) + x0, int(ys[i]) + y0)
            mask = Image.fromarray(binary.astype(np.uint8) * 255, "L").copy()
            ImageDraw.floodfill(mask, seed, 128)
            selected = (np.asarray(mask) == 128)
            # One-pixel antialias rim (3×3, like the script's MaxFilter(3)), not the neighbour's tail.
            grown = np.pad(selected, 1)
            grown = np.max([grown[1 + dy:grown.shape[0] - 1 + dy, 1 + dx:grown.shape[1] - 1 + dx]
                            for dy in (-1, 0, 1) for dx in (-1, 0, 1)], axis=0)
            keep = grown & (alpha > NOISE_ALPHA)
            ys, xs = np.nonzero(keep)
            bounds = (int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1)
            arr = np.asarray(rgba).copy()
            arr[~keep] = 0
            image = Image.fromarray(arr, "RGBA").crop(bounds)
            solid = np.asarray(image)[..., 3] > 128
            band = solid[int(image.height * .4):max(int(image.height * .6), int(image.height * .4) + 1)]
            bx = np.nonzero(band)[1]
            root_x = float(np.median(bx)) if bx.size else image.width / 2
            warning = ""
            if bounds[2] - bounds[0] > cw * 1.3 or bounds[3] - bounds[1] > ch * 1.3:
                warning = "자세가 이웃 칸과 붙어 있는 것 같습니다 (영역이 칸보다 30% 넘게 큼)"
            poses.append(Pose((c, r), image, bounds, (root_x, float(image.height - 1)), warning))
    return poses


def common_scale(poses: list[Pose], target_height: float | None, cell: tuple[int, int],
                 anchor: tuple[float, float], margin: float = 5) -> tuple[float, float]:
    """One scale for every pose so the motion keeps its proportions.

    ``target_height`` is the wanted height of the median pose (None = keep 1.0).
    The scale is lowered when a pose would leave the cell (``margin`` px, like the
    preparation script). Returns (scale, wanted scale) so callers can say when it
    was lowered.
    """
    found = [p for p in poses if p.image is not None]
    if not found:
        return 1.0, 1.0
    wanted = target_height / float(np.median([p.image.height for p in found])) if target_height else 1.0
    limits = [wanted]
    w, h = cell
    ax, ay = anchor
    for p in found:
        rx, ry = p.root
        limits += [(ax - margin) / max(1.0, rx), (w - ax - margin) / max(1.0, p.image.width - rx),
                   (ay - margin) / max(1.0, ry)]
    return round(min(limits), 6), round(wanted, 6)
