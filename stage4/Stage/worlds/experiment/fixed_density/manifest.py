from __future__ import annotations

import csv
import hashlib
import json
import subprocess
from collections.abc import Callable
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

from .config import BASE_IMAGE_SEED, MANIFEST_VERSION, ROBOT_COUNTS, arena_side

Qualifier = Callable[[Path, int, float], bool]
FIELDNAMES = (
    "manifest_version",
    "image_index",
    "image_id",
    "image_path",
    "image_sha256",
    "seed",
)


@dataclass(frozen=True)
class ManifestRow:
    manifest_version: str
    image_index: int
    image_id: str
    image_path: Path
    image_sha256: str
    seed: int


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def command_qualifier(executable: Path) -> Qualifier:
    executable = executable.resolve()

    def qualify(image_path: Path, n: int, side: float) -> bool:
        completed = subprocess.run(
            [str(executable), str(n), str(image_path.resolve()), f"{side:.12g}"],
            check=False,
            capture_output=True,
            text=True,
        )
        if completed.returncode != 0:
            raise RuntimeError(
                f"qualification command failed for {image_path.name}, N={n}: "
                f"{completed.stderr.strip() or completed.stdout.strip()}"
            )
        try:
            report = json.loads(completed.stdout.strip().splitlines()[-1])
        except (IndexError, json.JSONDecodeError) as error:
            raise RuntimeError(
                f"qualification command returned invalid JSON for {image_path.name}, N={n}"
            ) from error
        if "success" not in report:
            raise RuntimeError("qualification JSON is missing the 'success' field")
        return bool(report["success"])

    return qualify


def freeze_manifest(
    *,
    image_dir: Path,
    output_path: Path,
    repository_root: Path,
    qualifier: Qualifier,
    expected_count: int = 75,
    version: str = MANIFEST_VERSION,
    workers: int = 4,
) -> Path:
    """Qualify all PNGs for every N and atomically freeze the accepted set."""
    image_dir = image_dir.resolve()
    repository_root = repository_root.resolve()
    images = sorted(image_dir.glob("*.png"), key=lambda path: path.name)
    if workers <= 0:
        raise ValueError("qualification workers must be positive")
    tasks = [(image, n) for image in images for n in ROBOT_COUNTS]
    with ThreadPoolExecutor(max_workers=workers) as executor:
        outcomes = executor.map(lambda task: qualifier(task[0], task[1], arena_side(task[1])), tasks)
        qualification = dict(zip(tasks, outcomes, strict=True))
    accepted = [
        image for image in images if all(qualification[(image, n)] for n in ROBOT_COUNTS)
    ]
    if len(accepted) != expected_count:
        raise RuntimeError(
            f"qualification accepted {len(accepted)} images; expected exactly {expected_count}; "
            "manifest was not changed"
        )

    rows: list[dict[str, object]] = []
    for image_index, image in enumerate(accepted):
        try:
            relative = image.relative_to(repository_root)
        except ValueError as error:
            raise ValueError(f"image is outside repository root: {image}") from error
        rows.append(
            {
                "manifest_version": version,
                "image_index": image_index,
                "image_id": image.stem,
                "image_path": relative.as_posix(),
                "image_sha256": sha256_file(image),
                "seed": BASE_IMAGE_SEED + image_index,
            }
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_path.with_suffix(output_path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="") as destination:
        writer = csv.DictWriter(destination, fieldnames=FIELDNAMES)
        writer.writeheader()
        writer.writerows(rows)
    temporary.replace(output_path)
    return output_path


def load_manifest(path: Path, *, repository_root: Path) -> tuple[ManifestRow, ...]:
    repository_root = repository_root.resolve()
    with path.open(encoding="utf-8", newline="") as source:
        raw_rows = list(csv.DictReader(source))
    if not raw_rows:
        raise RuntimeError(f"manifest is empty: {path}")
    if tuple(raw_rows[0].keys()) != FIELDNAMES:
        raise RuntimeError(f"manifest columns must be exactly {', '.join(FIELDNAMES)}")

    rows: list[ManifestRow] = []
    version = raw_rows[0]["manifest_version"]
    for expected_index, raw in enumerate(raw_rows):
        row = ManifestRow(
            manifest_version=raw["manifest_version"],
            image_index=int(raw["image_index"]),
            image_id=raw["image_id"],
            image_path=Path(raw["image_path"]),
            image_sha256=raw["image_sha256"],
            seed=int(raw["seed"]),
        )
        if row.manifest_version != version:
            raise RuntimeError("manifest mixes multiple versions")
        if row.image_index != expected_index:
            raise RuntimeError("manifest image_index must be contiguous from zero")
        if row.seed != BASE_IMAGE_SEED + expected_index:
            raise RuntimeError("manifest seed does not match 20270809 + image_index")
        image = repository_root / row.image_path
        if not image.is_file():
            raise RuntimeError(f"manifest image does not exist: {image}")
        if sha256_file(image) != row.image_sha256:
            raise RuntimeError(f"manifest hash mismatch: {image}")
        rows.append(row)
    if [row.image_id for row in rows] != sorted(row.image_id for row in rows):
        raise RuntimeError("manifest images are not in lexical order")
    return tuple(rows)
