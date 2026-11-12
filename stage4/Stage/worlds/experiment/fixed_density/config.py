from __future__ import annotations

import hashlib
import math

ROBOT_COUNTS = tuple(range(50, 501, 50))
ARENA_AREA_PER_ROBOT = 50.0
STAGE_UNIT_METERS = 0.1
BOUNDARY_MARGIN = 1.0
MINIMUM_CENTER_DISTANCE = 4.0
BASE_IMAGE_SEED = 20_270_809
SIM_TIMEOUT_SECONDS = 7_200.0
WALL_TIMEOUT_SECONDS = 20.0 * 60.0
DEFAULT_WORKERS = 4
MANIFEST_VERSION = "formation-scaling-v1"


def arena_side(n: int) -> float:
    """Return the square arena side in Stage units for ``n`` robots."""
    if n <= 0:
        raise ValueError("robot count must be positive")
    return math.sqrt(ARENA_AREA_PER_ROBOT * n)


def derive_seed(base_seed: int, domain: str) -> int:
    """Derive a stable 32-bit seed without sharing RNG state across domains."""
    if base_seed < 0:
        raise ValueError("seed must be non-negative")
    if not domain:
        raise ValueError("seed domain must not be empty")
    payload = f"{base_seed}:{domain}".encode("utf-8")
    return int.from_bytes(hashlib.sha256(payload).digest()[:4], "big")
