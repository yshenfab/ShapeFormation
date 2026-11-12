from __future__ import annotations

import math
import random
from collections.abc import Sequence

from bridson import poisson_disc_samples

from .config import BOUNDARY_MARGIN, MINIMUM_CENTER_DISTANCE, derive_seed

Point = tuple[float, float]
SAMPLING_CLEARANCE = 1e-6


def _minimum_distance(points: Sequence[Point]) -> float:
    if len(points) < 2:
        return math.inf
    return min(
        math.hypot(ax - bx, ay - by)
        for index, (ax, ay) in enumerate(points)
        for bx, by in points[index + 1 :]
    )


def validate_initial_poses(
    poses: Sequence[Point],
    side: float,
    *,
    boundary_margin: float = BOUNDARY_MARGIN,
    minimum_distance: float = MINIMUM_CENTER_DISTANCE,
) -> None:
    if side <= 2.0 * boundary_margin:
        raise ValueError("arena is too small for the requested boundary margin")
    low = -side / 2.0 + boundary_margin
    high = side / 2.0 - boundary_margin
    if any(not (low <= x <= high and low <= y <= high) for x, y in poses):
        raise ValueError("generated pose violates arena boundary margin")
    actual_minimum = _minimum_distance(poses)
    if actual_minimum + 1e-9 < minimum_distance:
        raise ValueError(
            f"generated poses are only {actual_minimum:.12g} Stage units apart; "
            f"required at least {minimum_distance:.12g}"
        )


def generate_initial_poses(
    n: int,
    side: float,
    seed: int,
    *,
    boundary_margin: float = BOUNDARY_MARGIN,
    minimum_distance: float = MINIMUM_CENTER_DISTANCE,
    attempts_per_active_point: int = 30,
) -> tuple[Point, ...]:
    """Generate deterministic Bridson Poisson-disk poses centered on the arena."""
    if n <= 0:
        raise ValueError("robot count must be positive")
    interior_side = side - 2.0 * boundary_margin
    if interior_side <= 0.0:
        raise ValueError("arena is too small for the requested boundary margin")

    sampler = random.Random(derive_seed(seed, "bridson"))
    candidates = poisson_disc_samples(
        width=interior_side,
        height=interior_side,
        # A tiny clearance keeps decimal serialization from reducing a pair below
        # the protocol's exact 4-unit minimum.
        r=minimum_distance + SAMPLING_CLEARANCE,
        k=attempts_per_active_point,
        random=sampler.random,
    )
    if len(candidates) < n:
        raise RuntimeError(
            f"Bridson sampling produced {len(candidates)} candidates for {n} robots; "
            "increase arena area or reduce minimum distance"
        )

    selector = random.Random(derive_seed(seed, "candidate_selection"))
    selected = list(candidates)
    selector.shuffle(selected)
    offset = interior_side / 2.0
    poses = tuple((x - offset, y - offset) for x, y in selected[:n])
    validate_initial_poses(
        poses,
        side,
        boundary_margin=boundary_margin,
        minimum_distance=minimum_distance,
    )
    return poses
