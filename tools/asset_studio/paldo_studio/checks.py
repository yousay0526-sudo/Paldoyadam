"""Pre-export checks for the MVP (a small subset of the full automatic check).

Levels: ``ok`` (정상), ``warn`` (경고), ``error`` (오류). Errors block export.
These checks are technical only; they say nothing about artistic quality.
"""

from __future__ import annotations

import re

from .game_specs import Specs
from .imaging import ImageCache, compose, inspect
from .model import Asset, Project

LEVEL_LABEL = {"ok": "정상", "warn": "경고", "error": "오류"}


def _item(level: str, message: str, layer: str | None = None) -> dict:
    return {"level": level, "message": message, "layer": layer}


def check_asset(project: Project, asset: Asset, cache: ImageCache, specs: Specs | None = None) -> list[dict]:
    out: list[dict] = []
    exported = [l for l in asset.layers if l.visible and l.slot != "reference"]
    if not exported:
        out.append(_item("error", "내보낼 보이는 레이어가 없습니다"))

    for layer in asset.layers:
        source = project.source(layer.source)
        if source is None or not project.source_path(source).is_file():
            out.append(_item("error", "원본 파일이 없습니다", layer.name))
            continue
        image = cache.get(project, layer.source)
        if layer.crop:
            x, y, w, h = layer.crop
            if x < 0 or y < 0 or x + w > image.width or y + h > image.height or w <= 0 or h <= 0:
                out.append(_item("error", f"잘라낸 영역 {layer.crop}이 원본 {image.width}×{image.height} 밖입니다",
                                 layer.name))
                continue
        if source.mode not in ("RGBA", "LA", "PA", "P"):
            out.append(_item("warn", f"원본에 알파 채널이 없습니다 (모드 {source.mode})", layer.name))
        if not layer.visible or layer.slot == "reference":
            continue
        info = inspect(cache.layer_image(project, layer))
        if info.bbox is None:
            out.append(_item("warn", "보이는 픽셀이 없는 레이어입니다", layer.name))
            continue
        bx0, by0, bx1, by1 = info.bbox
        vis = (layer.x + bx0, layer.y + by0, layer.x + bx1, layer.y + by1)
        if vis[0] < 0 or vis[1] < 0 or vis[2] > asset.width or vis[3] > asset.height:
            out.append(_item("warn", f"보이는 그림이 캔버스 밖으로 나갑니다 (캔버스 좌표 {vis}) — 잘려서 저장됩니다",
                             layer.name))
        if layer.pivot is None and layer.slot in ("weapon", "head"):
            out.append(_item("warn", "무기·머리 레이어에 기준점(손잡이·머리)이 없습니다", layer.name))

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

    if not any(i["level"] == "error" for i in out) and exported:
        result = inspect(compose(project, asset, cache))
        if result.bbox is None:
            out.append(_item("error", "합성 결과가 완전히 투명합니다"))
        elif result.transparent_ratio == 0:
            out.append(_item("warn", "합성 결과에 투명 픽셀이 없습니다 (배경이 남았을 수 있음)"))
        else:
            bx0, by0, bx1, by1 = result.bbox
            if by1 < asset.anchor[1] - 8:
                out.append(_item("warn", f"그림 아래쪽 끝(y={by1})이 발 기준선(y={asset.anchor[1]:g})보다 "
                                         "8px 이상 위에 있습니다"))

    if not out:
        out.append(_item("ok", "검사 항목 모두 정상"))
    return out


def has_errors(report: list[dict]) -> bool:
    return any(i["level"] == "error" for i in report)
