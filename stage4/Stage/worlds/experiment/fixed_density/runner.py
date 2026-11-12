from __future__ import annotations

import csv
import json
import os
import shlex
import shutil
import signal
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from typing import Any, AbstractSet

from .config import (
    DEFAULT_WORKERS,
    ROBOT_COUNTS,
    SIM_TIMEOUT_SECONDS,
    WALL_TIMEOUT_SECONDS,
    arena_side,
    derive_seed,
)
from .manifest import ManifestRow
from .sampling import generate_initial_poses
from .world import render_world

FINAL_STATUSES = {
    "success",
    "target_generation_failed",
    "sim_timeout",
    "wall_timeout",
    "runtime_error",
}
RESULT_FIELDS = (
    "run_id",
    "N",
    "image_id",
    "image_sha256",
    "seed",
    "arena_side",
    "status",
    "wall_time_s",
    "convergence_time_s",
    "total_distance_m",
    "mean_distance_per_robot_m",
    "target_min_distance",
    "target_mst_bottleneck",
    "final_max_target_error",
)


@dataclass(frozen=True)
class RunSpec:
    run_id: str
    n: int
    image_id: str
    image_path: Path
    image_sha256: str
    seed: int
    arena_side: float
    world_path: Path
    result_path: Path
    stage_executable: Path
    controller_seed: int | None = None

    @property
    def log_path(self) -> Path:
        return self.result_path.parent.parent / "logs" / f"{self.result_path.stem}.log"


def _base_result(spec: RunSpec, status: str) -> dict[str, Any]:
    return {
        "run_id": spec.run_id,
        "N": spec.n,
        "image_id": spec.image_id,
        "image_sha256": spec.image_sha256,
        "seed": spec.seed,
        "arena_side": spec.arena_side,
        "status": status,
        "wall_time_s": None,
        "convergence_time_s": None,
        "total_distance_m": None,
        "mean_distance_per_robot_m": None,
        "target_min_distance": None,
        "target_mst_bottleneck": None,
        "final_max_target_error": None,
    }


def _write_json_atomic(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def _log_tail(path: Path, limit: int = 4_000) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")[-limit:]
    except OSError:
        return ""


def _stage_search_path(spec: RunSpec, inherited: str | None) -> str:
    stage_source = Path(__file__).resolve().parents[3]
    install_prefix = spec.stage_executable.resolve().parent.parent
    candidates = (
        stage_source / "libstage",
        install_prefix / "share/stage",
        install_prefix / "lib/Stage-4.3",
    )
    entries = [str(path) for path in candidates if path.is_dir()]
    if inherited:
        entries.extend(path for path in inherited.split(os.pathsep) if path)
    return os.pathsep.join(dict.fromkeys(entries))


def _load_final_result(path: Path, spec: RunSpec | None = None) -> dict[str, Any] | None:
    if not path.is_file():
        return None
    try:
        result = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    if result.get("status") not in FINAL_STATUSES:
        return None
    if result.get("status") == "runtime_error" and result.get("returncode") in {
        -signal.SIGINT,
        -signal.SIGTERM,
    }:
        return None
    if spec is None:
        return result
    expected = {
        "run_id": spec.run_id,
        "N": spec.n,
        "image_id": spec.image_id,
        "image_sha256": spec.image_sha256,
        "seed": spec.seed,
    }
    if any(result.get(field) != value for field, value in expected.items()):
        return None
    try:
        recorded_side = float(result["arena_side"])
    except (KeyError, TypeError, ValueError):
        return None
    if abs(recorded_side - spec.arena_side) > 1e-9 * max(1.0, spec.arena_side):
        return None
    return result


def _retry_archive_directory(spec: RunSpec) -> Path:
    return spec.result_path.parent.parent / "retry_archive" / spec.run_id


def _archive_retry_artifacts(spec: RunSpec) -> Path:
    archive_root = _retry_archive_directory(spec)
    attempt = 1
    while (archive_root / f"attempt-{attempt:03d}").exists():
        attempt += 1
    destination = archive_root / f"attempt-{attempt:03d}"
    destination.mkdir(parents=True)
    spec.result_path.replace(destination / "result.json")
    if spec.log_path.is_file():
        spec.log_path.replace(destination / "stage.log")
    return destination


def _has_archived_retry_status(spec: RunSpec, retry_statuses: AbstractSet[str]) -> bool:
    archive_root = _retry_archive_directory(spec)
    for path in sorted(archive_root.glob("attempt-*/result.json"), reverse=True):
        result = _load_final_result(path, spec)
        if result is not None and result.get("status") in retry_statuses:
            return True
    return False


def select_retry_specs(
    specs: tuple[RunSpec, ...], retry_statuses: AbstractSet[str]
) -> tuple[RunSpec, ...]:
    """Select terminal runs to retry, including a retry interrupted after archival."""
    invalid = set(retry_statuses) - FINAL_STATUSES
    if invalid:
        raise ValueError(f"unknown retry statuses: {', '.join(sorted(invalid))}")
    selected: list[RunSpec] = []
    for spec in specs:
        current = _load_final_result(spec.result_path, spec)
        if current is not None and current.get("status") in retry_statuses:
            selected.append(spec)
        elif current is None and _has_archived_retry_status(spec, retry_statuses):
            selected.append(spec)
    return tuple(selected)


def execute_run(
    spec: RunSpec,
    *,
    wall_timeout_seconds: float | None = WALL_TIMEOUT_SECONDS,
    retry_statuses: AbstractSet[str] = frozenset(),
) -> dict[str, Any]:
    """Execute one Stage process and guarantee a terminal per-run JSON result."""
    invalid = set(retry_statuses) - FINAL_STATUSES
    if invalid:
        raise ValueError(f"unknown retry statuses: {', '.join(sorted(invalid))}")
    previous = _load_final_result(spec.result_path, spec)
    if previous is not None and previous.get("status") not in retry_statuses:
        print(f"[resume] {spec.run_id} status={previous['status']}", flush=True)
        return previous
    if wall_timeout_seconds is not None and wall_timeout_seconds <= 0.0:
        raise ValueError("wall timeout must be positive")
    if previous is not None:
        archive = _archive_retry_artifacts(spec)
        print(
            f"[retry] {spec.run_id} previous_status={previous['status']} archive={archive}",
            flush=True,
        )

    # The controller writes RESULT_PATH.tmp and atomically renames it. Create
    # the per-run directory before Stage starts so a successful controller is
    # never misclassified as runtime_error merely because runs/ was absent.
    spec.result_path.parent.mkdir(parents=True, exist_ok=True)
    spec.log_path.parent.mkdir(parents=True, exist_ok=True)

    environment = os.environ.copy()
    environment["STAGEPATH"] = _stage_search_path(spec, environment.get("STAGEPATH"))
    environment.update(
        {
            "FORMATION_RUN_ID": spec.run_id,
            "FORMATION_IMAGE_ID": spec.image_id,
            "FORMATION_IMAGE_SHA256": spec.image_sha256,
            "FORMATION_SEED": str(spec.seed),
            "FORMATION_CONTROLLER_SEED": str(
                spec.controller_seed
                if spec.controller_seed is not None
                else derive_seed(spec.seed, "controller")
            ),
            "FORMATION_EXPERIMENT_MODE": "fixed_density",
            "FORMATION_ARENA_SIDE": f"{spec.arena_side:.12g}",
            "FORMATION_RESULT_PATH": str(spec.result_path.resolve()),
            "FORMATION_SIM_TIMEOUT": f"{SIM_TIMEOUT_SECONDS:.12g}",
            "FORMATION_PROGRESS_INTERVAL": "10",
            "FORMATION_SAVE_GENERATED_IMAGE": "0",
        }
    )
    command = [
        str(spec.stage_executable),
        "-g",
        str(spec.n),
        str(spec.image_path),
        str(spec.world_path),
    ]
    started = time.monotonic()
    timeout_label = "disabled" if wall_timeout_seconds is None else f"{wall_timeout_seconds:g}s"
    print(
        f"[start] {spec.run_id} N={spec.n} wall_timeout={timeout_label} "
        f"log={spec.log_path}",
        flush=True,
    )
    try:
        with spec.log_path.open("w", encoding="utf-8") as log:
            log.write(f"run_id: {spec.run_id}\n")
            log.write(f"command: {shlex.join(command)}\n")
            log.flush()
            run_options: dict[str, Any] = {
                "check": False,
                "env": environment,
                "stdout": log,
                "stderr": subprocess.STDOUT,
                "text": True,
            }
            if wall_timeout_seconds is not None:
                run_options["timeout"] = wall_timeout_seconds
            completed = subprocess.run(command, **run_options)
    except subprocess.TimeoutExpired:
        elapsed = time.monotonic() - started
        controller_result = _load_final_result(spec.result_path, spec)
        if controller_result is not None:
            controller_result["wall_time_s"] = elapsed
            _write_json_atomic(spec.result_path, controller_result)
            print(
                f"[done] {spec.run_id} status={controller_result['status']} "
                f"wall={elapsed:.3f}s",
                flush=True,
            )
            return controller_result
        result = _base_result(spec, "wall_timeout")
        result["wall_time_s"] = elapsed
        result["error"] = f"Stage exceeded {wall_timeout_seconds:g} wall-clock seconds"
        result["log_path"] = str(spec.log_path.resolve())
        result["log_tail"] = _log_tail(spec.log_path)
        _write_json_atomic(spec.result_path, result)
        print(
            f"[done] {spec.run_id} status=wall_timeout wall={elapsed:.3f}s "
            f"log={spec.log_path}",
            flush=True,
        )
        return result
    except OSError as error:
        elapsed = time.monotonic() - started
        result = _base_result(spec, "runtime_error")
        result["wall_time_s"] = elapsed
        result["error"] = f"failed to start Stage: {error}"
        result["log_path"] = str(spec.log_path.resolve())
        result["log_tail"] = _log_tail(spec.log_path)
        _write_json_atomic(spec.result_path, result)
        print(
            f"[done] {spec.run_id} status=runtime_error wall={elapsed:.3f}s "
            f"error={error}",
            flush=True,
        )
        return result

    if completed.returncode in {-signal.SIGINT, -signal.SIGTERM}:
        print(
            f"[interrupted] {spec.run_id} returncode={completed.returncode}; "
            "no terminal result was recorded",
            flush=True,
        )
        raise KeyboardInterrupt

    elapsed = time.monotonic() - started
    controller_result = _load_final_result(spec.result_path, spec)
    if controller_result is not None:
        controller_result["wall_time_s"] = elapsed
        _write_json_atomic(spec.result_path, controller_result)
        print(
            f"[done] {spec.run_id} status={controller_result['status']} "
            f"wall={elapsed:.3f}s",
            flush=True,
        )
        return controller_result

    result = _base_result(spec, "runtime_error")
    result["wall_time_s"] = elapsed
    result["returncode"] = completed.returncode
    result["error"] = "Stage exited without writing a valid terminal result"
    result["log_path"] = str(spec.log_path.resolve())
    result["log_tail"] = _log_tail(spec.log_path)
    _write_json_atomic(spec.result_path, result)
    print(
        f"[done] {spec.run_id} status=runtime_error wall={elapsed:.3f}s "
        f"returncode={completed.returncode} log={spec.log_path}",
        flush=True,
    )
    return result


def make_run_specs(
    *,
    manifest_rows: tuple[ManifestRow, ...],
    repository_root: Path,
    output_root: Path,
    stage_executable: Path,
    robot_counts: tuple[int, ...] = ROBOT_COUNTS,
) -> tuple[RunSpec, ...]:
    """Create all worlds and immutable run specifications for an experiment."""
    experiment_dir = Path(__file__).resolve().parents[1]
    formation_world = experiment_dir / "formation_world"
    if stage_executable.parent == Path("."):
        located = shutil.which(str(stage_executable))
        if located is None:
            raise FileNotFoundError(f"Stage executable was not found on PATH: {stage_executable}")
        stage_executable = Path(located)
    elif not stage_executable.is_file():
        raise FileNotFoundError(f"Stage executable does not exist: {stage_executable}")
    specs: list[RunSpec] = []
    for n in robot_counts:
        side = arena_side(n)
        for row in manifest_rows:
            run_id = f"{row.manifest_version}-n{n:04d}-i{row.image_index:03d}-{row.image_id}"
            world_path = output_root / "worlds" / f"{run_id}.world"
            result_path = output_root / "runs" / f"{run_id}.json"
            poses = generate_initial_poses(
                n,
                side,
                seed=derive_seed(row.seed, "initial_pose"),
            )
            render_world(
                output_path=world_path,
                n=n,
                side=side,
                poses=poses,
                formation_include=formation_world / "formation.inc",
                map_include=formation_world / "map.inc",
                empty_bitmap=formation_world / "bitmaps" / "empty.png",
            )
            specs.append(
                RunSpec(
                    run_id=run_id,
                    n=n,
                    image_id=row.image_id,
                    image_path=(repository_root / row.image_path).resolve(),
                    image_sha256=row.image_sha256,
                    seed=row.seed,
                    arena_side=side,
                    world_path=world_path.resolve(),
                    result_path=result_path.resolve(),
                    stage_executable=stage_executable.resolve(),
                    controller_seed=derive_seed(row.seed, "controller"),
                )
            )
    return tuple(specs)


def aggregate_results(result_directory: Path, output_csv: Path) -> Path:
    rows: list[dict[str, Any]] = []
    for path in sorted(result_directory.glob("*.json")):
        try:
            row = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            raise RuntimeError(f"invalid result JSON: {path}") from error
        rows.append({field: row.get(field) for field in RESULT_FIELDS})
    output_csv.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_csv.with_suffix(output_csv.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="") as destination:
        writer = csv.DictWriter(destination, fieldnames=RESULT_FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    temporary.replace(output_csv)
    return output_csv


def run_batch(
    specs: tuple[RunSpec, ...],
    *,
    workers: int = DEFAULT_WORKERS,
    wall_timeout_seconds: float | None = WALL_TIMEOUT_SECONDS,
    retry_statuses: AbstractSet[str] = frozenset(),
) -> list[dict[str, Any]]:
    if workers <= 0:
        raise ValueError("workers must be positive")
    if wall_timeout_seconds is not None and wall_timeout_seconds <= 0.0:
        raise ValueError("wall timeout must be positive")
    invalid = set(retry_statuses) - FINAL_STATUSES
    if invalid:
        raise ValueError(f"unknown retry statuses: {', '.join(sorted(invalid))}")
    pending: list[RunSpec] = []
    results: list[dict[str, Any]] = []
    for spec in specs:
        previous = _load_final_result(spec.result_path, spec)
        if previous is None or previous.get("status") in retry_statuses:
            pending.append(spec)
        else:
            results.append(previous)
    with ThreadPoolExecutor(max_workers=workers) as executor:
        futures = {
            executor.submit(
                execute_run,
                spec,
                wall_timeout_seconds=wall_timeout_seconds,
                retry_statuses=retry_statuses,
            ): spec
            for spec in pending
        }
        for future in as_completed(futures):
            results.append(future.result())
    return sorted(results, key=lambda row: str(row.get("run_id", "")))
