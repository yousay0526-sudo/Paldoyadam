"""Read-only example for the checked-in hero atlases. Run with Python and Pillow."""

import hashlib
import json
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
DIRECTIONS = ("n", "ne", "e", "se", "s", "sw", "w", "nw")
TECHNIQUES = ("cleave", "sweep", "thrust", "retreat")
CELL = 192


def require(condition, message):
    if not condition:
        raise ValueError(message)


def check_sheet(name, columns, record=None):
    path = ROOT / "assets" / name
    with Image.open(path) as sheet:
        require(sheet.format == "PNG" and sheet.mode == "RGBA", f"{name}: expected RGBA PNG")
        require(sheet.size == (columns * CELL, 2 * CELL), f"{name}: wrong grid {sheet.size}")
        if record is not None:
            require(len(record["frames"]) == columns * 2, f"{name}: metadata frame count")

        alpha = sheet.getchannel("A")
        for index in range(columns * 2):
            x, y = index % columns * CELL, index // columns * CELL
            cell = alpha.crop((x, y, x + CELL, y + CELL))
            label = f"{name}, cell {index}"
            low, high = cell.getextrema()
            require(low == 0 and high > 0, f"{label}: missing transparency or visible art")
            if record is not None:
                bounds = cell.getbbox()
                require(list(bounds) == record["frames"][index]["runtime_bounds"], f"{label}: bounds")
                left, top, right, bottom = bounds
                require(min(left, top, CELL - right, CELL - bottom) >= 4, f"{label}: margin")
        if record is not None:
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            require(digest == record["runtime_sha256"], f"{name}: differs from recorded PNG")
    return columns * 2


def main():
    art = ROOT / "art" / "hero"
    directions = json.loads((art / "directions/preparation.json").read_text(encoding="utf-8"))
    techniques = json.loads((art / "techniques/preparation.json").read_text(encoding="utf-8"))
    require(tuple(directions["direction_order"]) == DIRECTIONS, "direction order")
    require((directions["cell"], directions["root"]) == (CELL, [96, 176]), "movement root")
    require((techniques["cell"], techniques["root"]) == (CELL, [116, 176]), "technique root")

    movement = directions["atlases"]
    count = check_sheet("warrior-idle-8dir-v1.png", 4, movement["idle"])
    for direction in DIRECTIONS:
        count += check_sheet(f"warrior-walk-{direction}-v1.png", 4, movement[direction])
    count += check_sheet("warrior-attack-v1.png", 3)
    for technique in TECHNIQUES:
        count += check_sheet(f"warrior-{technique}-v1.png", 3, techniques["techniques"][technique])
    print(f"PASS: {count} runtime hero cells")


if __name__ == "__main__":
    main()
