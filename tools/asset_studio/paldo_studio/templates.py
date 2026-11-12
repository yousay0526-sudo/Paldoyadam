"""Asset templates: the structure of an asset (canvas, foot root, directions,
frames, sheet layout, tolerances) without its art.

Built-in templates are read from files that already exist in the game
repository, the same way ``game_specs`` reads presets:

* hero idle / walk / basic attack / technique  -> ``game_specs`` presets and the
  runtime sheets in ``assets/``
* monsters -> ``art/monsters/runtime.json`` (cell, foot root, direction order,
  installed sheets). Frame counts come from the installed sheet sizes.

NPCs have no built-in template: ``assets/npcs.png`` is one strip whose four
cells have different widths (``src/draw.c``), so there is no fixed NPC cell to
copy. That decision is left to the user (or a user template).

A template can name runtime sheets as *reference*: making an asset from it then
adds those cells as reference layers (shown translucent, never exported), to
trace or compare against. Nothing in the repository is modified.
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from pathlib import Path

from .game_specs import Specs
from .model import KINDS, Asset, Project

TEMPLATE_FORMAT = "paldo-asset-template/1"
TEMPLATE_SUFFIX = ".paldo-template.json"


@dataclass
class Template:
    key: str
    label: str
    kind: str
    width: int
    height: int
    anchor: list[float]
    preset: str = "free"
    directions: list[str] = field(default_factory=list)
    frames: int = 1
    frame_anchors: dict[str, list[float]] = field(default_factory=dict)
    sheet_layout: str = "cells"
    sheet_columns: int = 4
    tolerances: dict[str, float] = field(default_factory=dict)
    fps: float = 8.0
    export_name: str = ""
    source: str = ""            # where the numbers came from
    notes: str = ""
    # [{"path": repo-relative PNG, "view", "frame", "crop": [x, y, w, h]}]
    reference: list[dict] = field(default_factory=list)
    builtin: bool = False

    def to_dict(self) -> dict:
        data = asdict(self)
        data.pop("builtin")
        return {"format": TEMPLATE_FORMAT, **data}

    @classmethod
    def from_dict(cls, data: dict) -> "Template":
        if data.get("format") != TEMPLATE_FORMAT:
            raise ValueError(f"지원하지 않는 템플릿 형식: {data.get('format')!r}")
        data = {k: v for k, v in data.items() if k != "format"}
        template = cls(**data)
        if template.kind not in KINDS:
            raise ValueError(f"알 수 없는 종류: {template.kind}")
        if template.width <= 0 or template.height <= 0 or template.frames < 1:
            raise ValueError("템플릿의 캔버스 크기와 프레임 수가 올바르지 않습니다")
        return template

    @classmethod
    def from_asset(cls, asset: Asset, label: str, key: str = "") -> "Template":
        """Structure of ``asset`` only: no layers, no art, no references."""
        return cls(key or label, label, asset.kind, asset.width, asset.height, list(asset.anchor), asset.preset,
                   list(asset.directions), asset.frames, {k: list(v) for k, v in asset.frame_anchors.items()},
                   asset.sheet_layout, asset.sheet_columns, dict(asset.tolerances), asset.fps,
                   asset.export_name, f"에셋 '{asset.name}'에서 저장")

    def summary(self) -> str:
        parts = [f"{self.width}×{self.height}", f"발 {self.anchor[0]:g},{self.anchor[1]:g}"]
        parts.append(f"{len(self.directions)}방향" if self.directions else "방향 없음")
        parts.append(f"{self.frames}프레임")
        if self.frame_anchors:
            parts.append(f"프레임별 발 기준점 {len(self.frame_anchors)}개")
        parts.append(f"내보내기 {self.sheet_layout}·{self.sheet_columns}열")
        return ", ".join(parts)


def save_template(template: Template, path: Path) -> Path:
    path = Path(path)
    if not path.name.endswith(TEMPLATE_SUFFIX):
        path = path.with_name(path.name + TEMPLATE_SUFFIX)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(template.to_dict(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return path


def load_template(path: Path) -> Template:
    return Template.from_dict(json.loads(Path(path).read_text(encoding="utf-8")))


def project_templates(project: Project | None) -> list[tuple[Path, Template]]:
    """User templates kept in ``<project>/templates/``. Broken files are skipped."""
    if project is None or project.root is None:
        return []
    out = []
    for path in sorted((project.root / "templates").glob("*" + TEMPLATE_SUFFIX)):
        try:
            out.append((path, load_template(path)))
        except (OSError, ValueError, TypeError, KeyError):
            continue
    return out


def _png_size(path: Path) -> tuple[int, int] | None:
    """Width and height from the PNG header (no decoding)."""
    try:
        with open(path, "rb") as f:
            head = f.read(24)
    except OSError:
        return None
    if len(head) < 24 or head[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return int.from_bytes(head[16:20], "big"), int.from_bytes(head[20:24], "big")


def builtin_templates(specs: Specs) -> tuple[list[Template], list[str]]:
    """Templates read from the repository, and notes on anything that could not be read."""
    out: list[Template] = []
    notes: list[str] = []
    repo = specs.repo
    if repo is None:
        return out, ["게임 저장소가 없어 기본 템플릿이 없습니다."]
    order = specs.direction_order

    def cells(path: str, cols: int, rows: int, keys: list[tuple[str, int]]) -> list[dict]:
        size = _png_size(repo / path)
        if size is None:
            notes.append(f"{path}를 읽지 못해 참고 그림 없이 만듭니다.")
            return []
        w, h = size[0] // cols, size[1] // rows
        return [{"path": path, "view": v, "frame": f, "crop": [i % cols * w, i // cols * h, w, h]}
                for i, (v, f) in enumerate(keys)]

    move = specs.preset("hero-move")
    if move and order:
        out.append(Template(
            "hero-idle-8dir", "주인공 정지 8방향 (warrior-idle-8dir-v1 배치)", "character", move.width, move.height,
            list(move.anchor), move.key, list(order), 1, {}, "views_in_cells", 4, export_name="warrior-idle-8dir",
            source=move.source, builtin=True,
            reference=cells("assets/warrior-idle-8dir-v1.png", 4, 2, [(v, 0) for v in order])))
        ref = []
        for v in order:
            ref += cells(f"assets/warrior-walk-{v.lower()}-v1.png", 4, 2, [(v, f) for f in range(8)])
        out.append(Template(
            "hero-walk-8dir", "주인공 걷기 8방향×8프레임 (warrior-walk-*-v1, 방향마다 4×2)", "character",
            move.width, move.height, list(move.anchor), move.key, list(order), 8, {}, "per_view", 4,
            export_name="warrior-walk", source=move.source, builtin=True, reference=ref,
            notes="내보내면 <파일명>-n.png … -nw.png. 실행 파일 이름(warrior-walk-n-v1.png)으로는 사람이 바꿔 넣는다."))
    attack = [specs.preset(f"hero-attack-{i}") for i in range(6)]
    if all(attack):
        out.append(Template(
            "hero-attack", "주인공 기본 공격 6프레임 (warrior-attack-v1, 3×2)", "character", 192, 192,
            list(attack[0].anchor), attack[0].key, [], 6, {str(i): list(p.anchor) for i, p in enumerate(attack)},
            "per_view", 3, export_name="warrior-attack", source=attack[0].source, builtin=True,
            reference=cells("assets/warrior-attack-v1.png", 3, 2, [("", f) for f in range(6)]),
            notes="프레임별 발 기준점은 src/draw.c roots(192 셀 환산). 게임의 fit 배율·공격 속도는 템플릿에 없다."))
    technique = specs.preset("hero-technique")
    if technique:
        out.append(Template(
            "hero-technique", "주인공 무공 6프레임 (warrior-cleave·sweep·thrust·retreat-v1, 3×2)", "character",
            technique.width, technique.height, list(technique.anchor), technique.key, [], 6, {}, "per_view", 3,
            export_name="warrior-technique", source=technique.source, builtin=True,
            reference=cells("assets/warrior-cleave-v1.png", 3, 2, [("", f) for f in range(6)]),
            notes="참고 그림은 cleave(베기). 다른 무공은 시트 가져오기로 바꿔 넣는다."))

    runtime = repo / "art/monsters/runtime.json"
    try:
        data = json.loads(runtime.read_text(encoding="utf-8"))
        cell, root = int(data["cell"]), data["foot_root"]
        m_order = [d.upper() for d in data["direction_order"]]
        for entry in data["installed"]:
            path, name = entry["path"], entry["name"]
            size = _png_size(repo / path)
            if size is None:
                notes.append(f"{path}를 읽지 못했습니다.")
                continue
            cols, rows = size[0] // cell, size[1] // cell
            species, action = (name.split("-", 1) + [""])[:2]
            if cols * rows == len(m_order) and rows < len(m_order):  # idle: directions in cells (4×2)
                frames, layout, columns = 1, "views_in_cells", cols
                ref = cells(path, cols, rows, [(v, 0) for v in m_order])
                shape = f"8방향 {cols}×{rows}"
            elif rows == len(m_order):  # motion: one row per direction
                frames, layout, columns = cols, "rows_by_view", cols
                ref = cells(path, cols, rows, [(v, f) for v in m_order for f in range(cols)])
                shape = f"8방향×{cols}프레임, 행 = 방향"
            else:
                notes.append(f"{path}의 배치({size[0]}×{size[1]})를 알 수 없어 건너뜁니다.")
                continue
            out.append(Template(
                f"monster-{name}", f"몬스터 {action or name} ({species} 기준, {shape})", "monster", cell, cell,
                [float(root[0]), float(root[1])], "free", list(m_order), frames, {}, layout, columns,
                export_name=f"{species}-{action}-8dir" if action else name, source="art/monsters/runtime.json",
                builtin=True, reference=ref,
                notes="게임은 몬스터를 셀째 그리며, 화면 크기는 src/draw.c gfx_enemy(멧돼지 60·도축자 110 × "
                      "ACTOR_SCALE × monster_draw_scale)가 정한다. 템플릿은 그 값을 바꾸지 않는다."))
    except (OSError, KeyError, ValueError, TypeError) as exc:
        notes.append(f"몬스터 규격(art/monsters/runtime.json)을 읽지 못했습니다: {exc}")
    notes.append("NPC 기본 템플릿은 없습니다: assets/npcs.png는 셀 폭이 서로 다른 한 장짜리 띠라 "
                 "고정 셀 규격이 아직 정해지지 않았습니다. NPC는 내 템플릿으로 만들어 쓰세요.")
    return out, notes


def asset_from_template(project: Project, template: Template, name: str, repo: Path | None = None,
                        with_reference: bool = False) -> Asset:
    """New asset with the template's structure; optionally the runtime cells as reference layers."""
    from .imaging import open_png  # Pillow only when references are imported

    asset = project.add_asset(name, template.kind, template.width, template.height, tuple(template.anchor),
                              template.preset, template.directions)
    asset.frames = template.frames
    asset.frame_anchors = {k: list(v) for k, v in template.frame_anchors.items()}
    asset.sheet_layout, asset.sheet_columns = template.sheet_layout, template.sheet_columns
    asset.tolerances = dict(template.tolerances)
    asset.fps = template.fps
    if template.export_name:
        asset.export_name = template.export_name
    if with_reference and repo is not None:
        sources = {}
        for ref in template.reference:
            path = Path(repo) / ref["path"]
            if ref["path"] not in sources:
                image = open_png(path)
                sources[ref["path"]] = project.import_png(path, image.width, image.height, image.mode)
            crop = ref["crop"]
            # Whole runtime cells: their foot is already at this frame's root, so they stay at 0,0.
            align = tuple(asset.anchor_for(ref["frame"])) if (crop[2], crop[3]) == (asset.width, asset.height) \
                else None
            label = f"참고 {Path(ref['path']).stem}" + (f" {ref['view']}" if ref["view"] else "")
            project.add_layer(asset, sources[ref["path"]], label, crop, "reference", align,
                              view=ref["view"], frame=ref["frame"])
    return asset
