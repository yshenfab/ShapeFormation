from __future__ import annotations

import os
from collections.abc import Sequence
from pathlib import Path

from .sampling import Point

TEMPLATE_PATH = Path(__file__).resolve().parents[1] / "formation_batch.world.template"


def _relative_reference(target: Path, output_directory: Path) -> str:
    return Path(os.path.relpath(target.resolve(), output_directory.resolve())).as_posix()


def render_world(
    *,
    output_path: Path,
    n: int,
    side: float,
    world_side: float | None = None,
    poses: Sequence[Point],
    formation_include: Path,
    map_include: Path,
    empty_bitmap: Path,
    template_path: Path = TEMPLATE_PATH,
) -> Path:
    if n <= 0 or len(poses) != n:
        raise ValueError(f"expected {n} poses, got {len(poses)}")
    if side <= 0.0:
        raise ValueError("arena side must be positive")
    rendered_world_side = side if world_side is None else world_side
    if rendered_world_side < side:
        raise ValueError("physical world side must not be smaller than the formation arena")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    template = template_path.read_text(encoding="utf-8")
    pose_lines = "\n".join(
        f"flocker(pose [{x:.9f} {y:.9f} 0.0 0.0])" for x, y in poses
    )
    replacements = {
        "@FORMATION_INCLUDE@": _relative_reference(formation_include, output_path.parent),
        "@MAP_INCLUDE@": _relative_reference(map_include, output_path.parent),
        "@EMPTY_BITMAP@": _relative_reference(empty_bitmap, output_path.parent),
        "@ARENA_SIDE@": f"{rendered_world_side:.6f}",
        "@WINDOW_SCALE@": f"{max(1.0, 900.0 / rendered_world_side):.6f}",
        "@ROBOT_POSES@": pose_lines,
    }
    rendered = template
    for marker, value in replacements.items():
        rendered = rendered.replace(marker, value)
    if "@" in rendered:
        raise RuntimeError("world template contains an unresolved placeholder")
    output_path.write_text(rendered, encoding="utf-8")
    return output_path
