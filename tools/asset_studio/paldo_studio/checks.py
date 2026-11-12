"""Technical checks before export (정상 / 경고 / 오류). Errors block export.

Phase 2 adds per-direction checks: missing directions, ground contact and foot
axis against the asset's foot root, cell edge margin, 장비 묶음 missing in some
directions and size mismatch inside one 장비 묶음. Thresholds come from
``Asset.tolerance`` (defaults measured on the current runtime hero sheets).
These checks are technical only; they say nothing about artistic quality.
"""

from __future__ import annotations

import re

from .game_specs import Specs
from .imaging import ImageCache, compose, inspect, measure
from .model import Asset, Project

LEVEL_LABEL = {"ok": "정상", "warn": "경고", "error": "오류"}
# Kinds that stand on the ground; icons and loose gear pieces skip ground/axis checks.
STANDING_KINDS = {"character", "npc", "monster"}


def _item(level: str, message: str, layer: str | None = None, view: str = "", layer_id: str = "") -> dict:
    return {"level": level, "message": message, "layer": layer, "view": view, "layer_id": layer_id}


def _exported(asset: Asset, view: str):
    return [l for l in asset.view_layers(view) if asset.shown(l) and l.slot != "reference"]


def check_asset(project: Project, asset: Asset, cache: ImageCache, specs: Specs | None = None) -> list[dict]:
    out: list[dict] = []
    views = asset.views()
    directional = bool(asset.directions)

    # ---- per layer ----------------------------------------------------
    broken: set[str] = set()
    for layer in asset.layers:
        v = layer.view
        source = project.source(layer.source)
        if source is None or not project.source_path(source).is_file():
            out.append(_item("error", "원본 파일이 없습니다", layer.name, v, layer.id))
            broken.add(v)
            continue
        image = cache.get(project, layer.source)
        if layer.crop:
            x, y, w, h = layer.crop
            if x < 0 or y < 0 or x + w > image.width or y + h > image.height or w <= 0 or h <= 0:
                out.append(_item("error", f"잘라낸 영역 {layer.crop}이 원본 {image.width}×{image.height} 밖입니다",
                                 layer.name, v, layer.id))
                broken.add(v)
                continue
        if source.mode not in ("RGBA", "LA", "PA", "P"):
            out.append(_item("warn", f"원본에 알파 채널이 없습니다 (모드 {source.mode})", layer.name, v, layer.id))
        if not asset.shown(layer) or layer.slot == "reference":
            continue
        info = inspect(cache.layer_image(project, layer))
        if info.bbox is None:
            out.append(_item("warn", "보이는 픽셀이 없는 레이어입니다", layer.name, v, layer.id))
            continue
        bx0, by0, bx1, by1 = info.bbox
        vis = (layer.x + bx0, layer.y + by0, layer.x + bx1, layer.y + by1)
        if vis[0] < 0 or vis[1] < 0 or vis[2] > asset.width or vis[3] > asset.height:
            out.append(_item("warn", f"보이는 그림이 캔버스 밖으로 나갑니다 (캔버스 좌표 {vis}) — 잘려서 저장됩니다",
                             layer.name, v, layer.id))
        if layer.pivot is None and layer.slot in ("weapon", "head"):
            out.append(_item("warn", "무기·머리 레이어에 기준점(손잡이·머리)이 없습니다", layer.name, v, layer.id))

    # ---- 장비 묶음 across directions ------------------------------------
    if directional:
        for group in asset.groups():
            members = [l for l in asset.layers if l.group == group]
            have = {l.view for l in members}
            missing = [v for v in views if v not in have]
            if missing:
                out.append(_item("warn", f"장비 묶음 '{group}'이(가) {', '.join(missing)} 방향에 없습니다"))
            sizes = {}
            for l in members:
                image = cache.layer_image(project, l)
                if image is not None:
                    sizes.setdefault(image.size, []).append(l.view)
            if len(sizes) > 1:
                text = "; ".join(f"{w}×{h}: {', '.join(vs)}" for (w, h), vs in sizes.items())
                out.append(_item("warn", f"장비 묶음 '{group}'의 레이어 크기가 방향마다 다릅니다 ({text})"))

    # ---- file name and preset -----------------------------------------
    name = asset.export_name or asset.name
    if not re.fullmatch(r"[a-z0-9][a-z0-9-]*", name or ""):
        out.append(_item("warn", f"내보낼 파일명 '{name}'이 기존 실행 파일 규칙(영문 소문자·숫자·하이픈, "
                                 "예: warrior-walk-n-v1)과 다릅니다"))
    if specs is not None and asset.preset != "free":
        preset = specs.preset(asset.preset)
        if preset is None:
            out.append(_item("warn", f"프리셋 {asset.preset}을 현재 저장소에서 찾지 못했습니다"))
        elif (asset.width, asset.height) != (preset.width, preset.height) or \
                [float(v) for v in asset.anchor] != [float(v) for v in preset.anchor]:
            out.append(_item("warn", f"캔버스·발 기준점이 프리셋({preset.width}×{preset.height}, "
                                     f"발 {preset.anchor})과 다릅니다 — 게임 규격 변경은 별도 승인 대상"))

    # ---- per composed view --------------------------------------------
    ground, axis, margin = asset.tolerance("ground"), asset.tolerance("axis"), asset.tolerance("margin")
    ax, ay = asset.anchor
    for view in views:
        if not _exported(asset, view):
            out.append(_item("error", "방향 이미지가 없습니다 (내보낼 보이는 레이어 없음)" if directional
                             else "내보낼 보이는 레이어가 없습니다", None, view))
            continue
        if view in broken:
            continue
        image = compose(project, asset, cache, view=view)
        info = inspect(image)
        if info.bbox is None:
            out.append(_item("error", "합성 결과가 완전히 투명합니다", None, view))
            continue
        if info.transparent_ratio == 0:
            out.append(_item("warn", "합성 결과에 투명 픽셀이 없습니다 (배경이 남았을 수 있음)", None, view))
            continue
        m = measure(image)
        if m.margin < margin:
            out.append(_item("warn", f"그림이 셀 가장자리에서 {m.margin}px (기준 {margin:g}px 이상) — 잘림·번짐 위험",
                             None, view))
        if asset.kind in STANDING_KINDS:
            if abs(m.contact_y - ay) > ground:
                where = "위에 떠 있습니다" if m.contact_y < ay else "아래로 내려갔습니다"
                out.append(_item("warn", f"발 접지: 그림 바닥 y={m.contact_y}가 발 기준선 y={ay:g}보다 "
                                         f"{abs(m.contact_y - ay):g}px {where} (허용 ±{ground:g})", None, view))
            if abs(m.foot_x - ax) > axis:
                out.append(_item("warn", f"발 중심축: 발 부분 중심 x={m.foot_x:.1f}가 기준 x={ax:g}에서 "
                                         f"{m.foot_x - ax:+.1f}px (허용 ±{axis:g})", None, view))

    if not out:
        out.append(_item("ok", "검사 항목 모두 정상"))
    return out


def has_errors(report: list[dict]) -> bool:
    return any(i["level"] == "error" for i in report)
