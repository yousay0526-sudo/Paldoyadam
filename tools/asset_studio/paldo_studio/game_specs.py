"""Canvas presets read from the game repository, never invented here.

Every value below is taken from files that already exist in the repository:

* ``art/hero/directions/preparation.json``  -> movement cell, foot root, direction order
* ``art/hero/techniques/preparation.json``  -> technique cell and foot root
* ``src/draw.c`` (``gfx_hero``)              -> basic attack roots (512-unit space)

If a file is missing or its format changed, the matching preset is simply not
offered and the reason is kept in ``Specs.notes`` so the GUI can show it.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path


@dataclass(frozen=True)
class Preset:
    key: str
    label: str
    width: int
    height: int
    anchor: tuple[float, float]
    source: str  # where the numbers came from
    display_box: float = 0.0  # on-screen size of the whole cell in world units (before zoom)

    def to_dict(self) -> dict:
        return {"key": self.key, "label": self.label, "width": self.width,
                "height": self.height, "anchor": list(self.anchor), "source": self.source}


@dataclass
class Specs:
    repo: Path | None
    actor_scale: float = 1.4
    presets: list[Preset] = field(default_factory=list)
    direction_order: list[str] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)

    def preset(self, key: str) -> Preset | None:
        return next((p for p in self.presets if p.key == key), None)


FREE_PRESET = Preset("free", "자유 크기 (게임 규격 아님)", 256, 256, (128, 240), "사용자 지정")


def find_repo(start: Path) -> Path | None:
    """Walk up from ``start`` to the repository root (has src/draw.c and assets/)."""
    start = start.resolve()
    for candidate in [start, *start.parents]:
        if (candidate / "src" / "draw.c").is_file() and (candidate / "assets").is_dir():
            return candidate
    return None


def _read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def parse_attack_roots(draw_c: str) -> list[tuple[float, float]] | None:
    """Return the six ``roots[HERO_ATTACK_FRAMES]`` pairs from ``gfx_hero``."""
    match = re.search(r"roots\s*\[\s*HERO_ATTACK_FRAMES\s*\]\s*=\s*\{(.*?)\};", draw_c, re.S)
    if not match:
        return None
    pairs = re.findall(r"\{\s*([0-9.]+)\s*,\s*([0-9.]+)\s*\}", match.group(1))
    return [(float(x), float(y)) for x, y in pairs] or None


def parse_float_list(draw_c: str, name: str) -> list[float] | None:
    match = re.search(name + r"\s*\[\s*HERO_ATTACK_FRAMES\s*\]\s*=\s*\{(.*?)\};", draw_c, re.S)
    if not match:
        return None
    return [float(v.rstrip("f")) for v in re.findall(r"[0-9.]+f?", match.group(1))] or None


def load_specs(repo: Path | None) -> Specs:
    specs = Specs(repo=repo)
    if repo is not None:
        try:
            m = re.search(r"#define\s+ACTOR_SCALE\s+([0-9.]+)f?", (repo / "src/draw.h").read_text(encoding="utf-8"))
            if m:
                specs.actor_scale = float(m.group(1))
            else:
                specs.notes.append("ACTOR_SCALE를 찾지 못해 1.4로 표시합니다.")
        except OSError as exc:
            specs.notes.append(f"draw.h를 읽지 못했습니다: {exc}")
    # Cell box sizes used by gfx_hero (src/draw.c): idle/walk 56, technique 80,
    # attack 64 × per-frame fit; all multiplied by ACTOR_SCALE.
    scale = specs.actor_scale
    if repo is None:
        specs.notes.append("게임 저장소를 찾지 못해 게임 규격 프리셋을 불러오지 않았습니다.")
        specs.presets.append(FREE_PRESET)
        return specs

    directions = repo / "art/hero/directions/preparation.json"
    try:
        d = _read_json(directions)
        cell, root = int(d["cell"]), d["root"]
        specs.direction_order = [s.upper() for s in d["direction_order"]]
        specs.presets.append(Preset(
            "hero-move", f"주인공 정지·걷기 셀 ({cell}×{cell}, 발 {root[0]},{root[1]})",
            cell, cell, (float(root[0]), float(root[1])),
            "art/hero/directions/preparation.json", 56 * scale))
    except (OSError, KeyError, ValueError, TypeError) as exc:
        specs.notes.append(f"정지·걷기 규격을 읽지 못했습니다: {exc}")

    techniques = repo / "art/hero/techniques/preparation.json"
    try:
        t = _read_json(techniques)
        cell, root = int(t["cell"]), t["root"]
        specs.presets.append(Preset(
            "hero-technique", f"주인공 무공 셀 ({cell}×{cell}, 발 {root[0]},{root[1]})",
            cell, cell, (float(root[0]), float(root[1])),
            "art/hero/techniques/preparation.json", 80 * scale))
    except (OSError, KeyError, ValueError, TypeError) as exc:
        specs.notes.append(f"무공 규격을 읽지 못했습니다: {exc}")

    try:
        draw_c = (repo / "src/draw.c").read_text(encoding="utf-8")
        roots = parse_attack_roots(draw_c)
        if roots is None:
            raise ValueError("gfx_hero의 roots 표를 찾지 못함")
        fit = parse_float_list(draw_c, r"fit") or [1.0] * len(roots)
        for i, (x, y) in enumerate(roots):
            # The code keeps these in a 512-unit cell; runtime cells are 192 px.
            u, v = round(x * 192 / 512, 2), round(y * 192 / 512, 2)
            specs.presets.append(Preset(
                f"hero-attack-{i}", f"주인공 기본 공격 셀 {i} (192×192, 발 {u},{v})",
                192, 192, (u, v), "src/draw.c gfx_hero roots × 192/512",
                64 * scale * (fit[i] if i < len(fit) else 1.0)))
    except (OSError, ValueError) as exc:
        specs.notes.append(f"기본 공격 기준점을 읽지 못했습니다: {exc}")

    specs.presets.append(FREE_PRESET)
    return specs


# Known runtime sheets: (relative path, columns, rows, cell labels or None).
def known_sheets(specs: Specs) -> list[tuple[str, int, int, list[str] | None]]:
    order = specs.direction_order or ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
    sheets: list[tuple[str, int, int, list[str] | None]] = [
        ("assets/warrior-idle-8dir-v1.png", 4, 2, [f"정지 {d}" for d in order])]
    for d in order:
        sheets.append((f"assets/warrior-walk-{d.lower()}-v1.png", 4, 2,
                       [f"걷기 {d} 프레임 {i}" for i in range(8)]))
    sheets.append(("assets/warrior-attack-v1.png", 3, 2, [f"기본 공격 {i}" for i in range(6)]))
    for t in ("cleave", "sweep", "thrust", "retreat"):
        sheets.append((f"assets/warrior-{t}-v1.png", 3, 2, [f"{t} {i}" for i in range(6)]))
    for m in ("boar", "butcher"):
        sheets.append((f"assets/monsters/{m}-idle-8dir-v1.png", 4, 2, [f"{m} 정지 {d}" for d in order]))
        # Motion sheets: one row per direction, four frames per row (gfx_enemy).
        for motion in ("walk", "attack" if m == "boar" else "skill"):
            sheets.append((f"assets/monsters/{m}-{motion}-8dir-v1.png", 4, 8,
                           [f"{m} {motion} {d} 프레임 {f}" for d in order for f in range(4)]))
    return sheets


def sheet_layout(specs: Specs, repo_relative: str) -> tuple[int, int, list[str] | None] | None:
    for path, cols, rows, labels in known_sheets(specs):
        if path == repo_relative:
            return cols, rows, labels
    return None
