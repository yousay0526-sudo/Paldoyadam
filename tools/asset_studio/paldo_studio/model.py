"""Project model: plain data + JSON, no Qt and no image decoding.

A project is a folder::

    <project>/project.paldo.json   the project file (this model)
    <project>/sources/             copies of imported PNGs (never modified)
    <project>/exports/             composited PNG + metadata written by export

Imported originals stay where they were; the project only copies them, so the
user's files and the game's ``assets/`` are never touched.
"""

from __future__ import annotations

import copy
import hashlib
import json
import shutil
import uuid
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path

FORMAT = "paldo-asset-studio/1"
PROJECT_FILE = "project.paldo.json"

# Asset kinds requested for the studio (a classification for the project only).
KINDS = {
    "character": "캐릭터",
    "npc": "NPC",
    "monster": "몬스터",
    "weapon": "무기",
    "equipment": "장비",
    "item": "아이템",
}

# Suggested layer slots. These are labels for the editor only: the game itself
# currently draws the hero as ONE pre-painted cell (src/draw.c gfx_hero), so no
# slot below is a runtime contract yet.
SLOTS = {
    "body": "기본 신체",
    "head": "머리·얼굴",
    "top": "상의",
    "bottom": "하의",
    "shoes": "신발",
    "weapon": "무기",
    "accessory": "장신구",
    "reference": "참고(내보내기 제외)",
    "other": "기타",
}


def _new_id(prefix: str) -> str:
    return f"{prefix}-{uuid.uuid4().hex[:8]}"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


@dataclass
class Source:
    id: str
    file: str            # relative to the project folder, e.g. "sources/hero.png"
    original_name: str
    original_path: str   # where it was imported from (for the record only)
    sha256: str
    width: int
    height: int
    mode: str


@dataclass
class Layer:
    id: str
    name: str
    source: str                         # Source.id
    crop: list[int] | None = None       # [x, y, w, h] inside the source, or None
    x: int = 0                          # top-left of the layer image on the canvas
    y: int = 0
    pivot: list[float] | None = None    # reference point (e.g. 손잡이) in UNSCALED layer pixels
    scale: float = 1.0                  # uniform scale applied after crop (e.g. 0.25 for 4x art)
    smooth: bool = True                 # scaling filter: True = Lanczos on premultiplied alpha, False = nearest
    slot: str = "other"
    visible: bool = True
    opacity: float = 1.0                # 0..1
    locked: bool = False
    view: str = ""                      # direction code ("N".."NW") or "" for a single-view asset
    group: str = ""                     # 장비 묶음 (e.g. "무명 바지"); "" = always part of the body


@dataclass
class Asset:
    id: str
    name: str
    kind: str = "character"
    width: int = 192
    height: int = 192
    anchor: list[float] = field(default_factory=lambda: [96.0, 176.0])  # 발 기준점
    preset: str = "free"
    export_name: str = ""
    layers: list[Layer] = field(default_factory=list)  # bottom -> top, all views together
    directions: list[str] = field(default_factory=list)  # [] = one view; else e.g. N NE E SE S SW W NW
    hidden_groups: list[str] = field(default_factory=list)  # 장비 묶음 currently switched off
    combos: dict[str, list[str]] = field(default_factory=dict)  # 장비 조합 name -> visible groups
    tolerances: dict[str, float] = field(default_factory=dict)  # overrides of DEFAULT_TOLERANCES

    def layer(self, layer_id: str) -> Layer | None:
        return next((l for l in self.layers if l.id == layer_id), None)

    def views(self) -> list[str]:
        return list(self.directions) or [""]

    def view_layers(self, view: str) -> list[Layer]:
        return [l for l in self.layers if l.view == view]

    def groups(self) -> list[str]:
        seen: list[str] = []
        for l in self.layers:
            if l.group and l.group not in seen:
                seen.append(l.group)
        return seen

    def shown(self, layer: Layer) -> bool:
        """Visible in the editor and in exports (reference layers are filtered by callers)."""
        return layer.visible and layer.group not in self.hidden_groups

    def tolerance(self, key: str) -> float:
        return float(self.tolerances.get(key, DEFAULT_TOLERANCES[key]))

    def apply_combo(self, name: str) -> bool:
        if name not in self.combos:
            return False
        keep = set(self.combos[name])
        self.hidden_groups = [g for g in self.groups() if g not in keep]
        return True

    def save_combo(self, name: str) -> None:
        self.combos[name] = [g for g in self.groups() if g not in self.hidden_groups]


# Check thresholds, chosen so that all 72 current runtime idle/walk hero cells pass
# (measured with imaging.measure on main 0805c14): lowest visible row 175..176 for
# foot root y=176, foot-band centre 72.3..120.4 for root x=96 (largest offset 24.4),
# smallest edge margin 13. scripts/prepare_direction_assets.py requires >= 4 px margin.
DEFAULT_TOLERANCES = {"ground": 2.0, "axis": 26.0, "margin": 4.0}
TOLERANCE_LABELS = {"ground": "발 접지 허용 (px)", "axis": "발 중심축 허용 (px)", "margin": "셀 가장자리 여백 (px)"}

# Game direction order (art/hero/directions/preparation.json, src/draw.h HeroDirection).
EIGHT_DIRECTIONS = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]


def pivot_on_canvas(layer: Layer) -> tuple[float, float] | None:
    if layer.pivot is None:
        return None
    return layer.x + layer.pivot[0] * layer.scale, layer.y + layer.pivot[1] * layer.scale


@dataclass
class Project:
    name: str
    root: Path | None = None
    sources: list[Source] = field(default_factory=list)
    assets: list[Asset] = field(default_factory=list)
    created: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat(timespec="seconds"))

    # ---- lookup -------------------------------------------------------
    def source(self, source_id: str) -> Source | None:
        return next((s for s in self.sources if s.id == source_id), None)

    def asset(self, asset_id: str) -> Asset | None:
        return next((a for a in self.assets if a.id == asset_id), None)

    def source_path(self, source: Source) -> Path:
        if self.root is None:
            raise ValueError("project has no folder yet")
        return self.root / source.file

    @property
    def exports_dir(self) -> Path:
        if self.root is None:
            raise ValueError("project has no folder yet")
        return self.root / "exports"

    # ---- editing ------------------------------------------------------
    def add_asset(self, name: str, kind: str, width: int, height: int,
                  anchor: tuple[float, float], preset: str = "free",
                  directions: list[str] | None = None) -> Asset:
        if kind not in KINDS:
            raise ValueError(f"unknown kind {kind}")
        if width <= 0 or height <= 0:
            raise ValueError("canvas size must be positive")
        asset = Asset(_new_id("asset"), name, kind, int(width), int(height),
                      [float(anchor[0]), float(anchor[1])], preset, directions=list(directions or []))
        # Runtime files in assets/ use lower-case ASCII names (warrior-walk-n-v1.png).
        slug = "".join(c if c.isascii() and c.isalnum() else "-" for c in name.lower())
        slug = "-".join(p for p in slug.split("-") if p)
        asset.export_name = slug if len(slug) >= 3 else f"{kind}-{asset.id[-8:]}"
        self.assets.append(asset)
        return asset

    def import_png(self, path: Path, width: int, height: int, mode: str) -> Source:
        """Copy ``path`` into ``sources/``. Re-importing identical bytes reuses the copy."""
        if self.root is None:
            raise ValueError("save the project before importing images")
        path = Path(path)
        digest = sha256_file(path)
        for existing in self.sources:
            if existing.sha256 == digest:
                return existing
        folder = self.root / "sources"
        folder.mkdir(parents=True, exist_ok=True)
        target = folder / f"{digest[:12]}-{_safe_name(path.stem)}.png"
        if not target.exists():
            shutil.copyfile(path, target)
        source = Source(_new_id("src"), target.relative_to(self.root).as_posix(), path.name,
                        str(path), digest, width, height, mode)
        self.sources.append(source)
        return source

    def add_layer(self, asset: Asset, source: Source, name: str,
                  crop: list[int] | None = None, slot: str = "other",
                  align_to_anchor: tuple[float, float] | None = None,
                  view: str = "", group: str = "") -> Layer:
        if view not in asset.views():
            raise ValueError(f"에셋에 없는 방향입니다: {view!r}")
        layer = Layer(_new_id("layer"), name, source.id, list(crop) if crop else None, slot=slot,
                      view=view, group=group)
        w, h = (crop[2], crop[3]) if crop else (source.width, source.height)
        if align_to_anchor is not None:
            # Same cell geometry: put the layer's own foot point on the asset anchor.
            layer.pivot = [float(align_to_anchor[0]), float(align_to_anchor[1])]
            layer.x = round(asset.anchor[0] - align_to_anchor[0])
            layer.y = round(asset.anchor[1] - align_to_anchor[1])
        else:
            # Centre horizontally, bottom on the ground line.
            layer.x = round(asset.anchor[0] - w / 2)
            layer.y = round(asset.anchor[1] - h)
        asset.layers.append(layer)
        return layer

    def snap_pivot_to_anchor(self, asset: Asset, layer: Layer) -> bool:
        if layer.pivot is None:
            return False
        layer.x = round(asset.anchor[0] - layer.pivot[0] * layer.scale)
        layer.y = round(asset.anchor[1] - layer.pivot[1] * layer.scale)
        return True

    @staticmethod
    def move_layer(asset: Asset, layer_id: str, delta: int) -> bool:
        """Swap with the next layer of the SAME view in the delta direction (+1 = front)."""
        idx = next((i for i, l in enumerate(asset.layers) if l.id == layer_id), None)
        if idx is None or delta == 0:
            return False
        view = asset.layers[idx].view
        step = 1 if delta > 0 else -1
        j = idx + step
        while 0 <= j < len(asset.layers) and asset.layers[j].view != view:
            j += step
        if not 0 <= j < len(asset.layers):
            return False
        asset.layers[idx], asset.layers[j] = asset.layers[j], asset.layers[idx]
        return True

    # ---- (de)serialisation -------------------------------------------
    def to_dict(self) -> dict:
        return {
            "format": FORMAT,
            "name": self.name,
            "created": self.created,
            "sources": [asdict(s) for s in self.sources],
            "assets": [asdict(a) for a in self.assets],
        }

    @classmethod
    def from_dict(cls, data: dict, root: Path | None = None) -> "Project":
        if data.get("format") != FORMAT:
            raise ValueError(f"지원하지 않는 프로젝트 형식: {data.get('format')!r}")
        project = cls(name=data["name"], root=root, created=data.get("created", ""))
        project.sources = [Source(**s) for s in data.get("sources", [])]
        for a in data.get("assets", []):
            a = dict(a)
            layers = [Layer(**l) for l in a.pop("layers", [])]
            project.assets.append(Asset(**a, layers=layers))
        return project

    def save(self, root: Path | None = None) -> Path:
        if root is not None:
            root = Path(root)
            if self.root is not None and root.resolve() != self.root.resolve():
                # "Save as": bring the source copies along so the project stays whole.
                for s in self.sources:
                    dest = root / s.file
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    if not dest.exists():
                        shutil.copyfile(self.root / s.file, dest)
            self.root = root
        if self.root is None:
            raise ValueError("no project folder")
        self.root.mkdir(parents=True, exist_ok=True)
        path = self.root / PROJECT_FILE
        tmp = path.with_suffix(".tmp")
        tmp.write_text(json.dumps(self.to_dict(), ensure_ascii=False, indent=2), encoding="utf-8")
        tmp.replace(path)
        return path

    @classmethod
    def load(cls, path: Path) -> "Project":
        path = Path(path)
        if path.is_dir():
            path = path / PROJECT_FILE
        data = json.loads(path.read_text(encoding="utf-8"))
        return cls.from_dict(data, root=path.parent)


def _safe_name(text: str) -> str:
    keep = "".join(c if c.isalnum() or c in "-_" else "-" for c in text.strip())
    keep = "-".join(p for p in keep.split("-") if p)
    return keep[:60] or "asset"


class History:
    """Snapshot undo/redo over ``Project.to_dict()``.

    Call ``checkpoint()`` *before* a change. Snapshots are small (no pixels).
    """

    def __init__(self, project: Project, limit: int = 200):
        self.project = project
        self.limit = limit
        self._undo: list[dict] = []
        self._redo: list[dict] = []
        self._last_key: str | None = None

    def checkpoint(self, merge_key: str | None = None) -> None:
        """Snapshot before a change. Repeated edits with the same ``merge_key``
        (e.g. spinning one number box) collapse into a single undo step."""
        if merge_key is not None and merge_key == self._last_key and self._undo:
            self._redo.clear()
            return
        self._last_key = merge_key
        self._undo.append(copy.deepcopy(self.project.to_dict()))
        if len(self._undo) > self.limit:
            self._undo.pop(0)
        self._redo.clear()

    def _restore(self, data: dict) -> None:
        restored = Project.from_dict(data, root=self.project.root)
        self.project.sources = restored.sources
        self.project.assets = restored.assets
        self.project.name = restored.name

    def undo(self) -> bool:
        self._last_key = None
        if not self._undo:
            return False
        self._redo.append(copy.deepcopy(self.project.to_dict()))
        self._restore(self._undo.pop())
        return True

    def redo(self) -> bool:
        self._last_key = None
        if not self._redo:
            return False
        self._undo.append(copy.deepcopy(self.project.to_dict()))
        self._restore(self._redo.pop())
        return True

    @property
    def can_undo(self) -> bool:
        return bool(self._undo)

    @property
    def can_redo(self) -> bool:
        return bool(self._redo)
