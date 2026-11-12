from __future__ import annotations

import hashlib
import json
import math
import os
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import asdict, dataclass
from pathlib import Path

from .fixed_density.config import BASE_IMAGE_SEED, SIM_TIMEOUT_SECONDS, derive_seed
from .fixed_density.sampling import generate_initial_poses
from .fixed_density.world import render_world


DEFAULT_CASES = ("500:uts1.png", "1000:uts2.png")
TERMINAL_STATUSES = {
    "success",
    "target_generation_failed",
    "sim_timeout",
    "runtime_error",
}


@dataclass(frozen=True)
class LargeCase:
    run_id: str
    n: int
    image_id: str
    image_path: Path
    image_sha256: str
    seed: int
    controller_seed: int
    arena_side: float
    world_side: float
    world_path: Path
    result_path: Path
    log_path: Path


def default_qualitative_side(n: int) -> float:
    if n <= 0:
        raise ValueError("robot count must be positive")
    return 250.0 * math.sqrt(n / 500.0)


def parse_case(text: str, *, code_root: Path, output_root: Path, index: int) -> LargeCase:
    fields = text.rsplit(":", 2)
    if len(fields) not in (2, 3):
        raise ValueError("case must use N:IMAGE or N:IMAGE:ARENA_SIDE")
    try:
        n = int(fields[0])
    except ValueError as error:
        raise ValueError(f"invalid robot count in case: {text}") from error
    if n <= 0:
        raise ValueError("robot count must be positive")

    image_path = Path(fields[1])
    if not image_path.is_absolute():
        image_path = code_root / image_path
    image_path = image_path.resolve()
    if not image_path.is_file():
        raise ValueError(f"case image does not exist: {image_path}")

    if len(fields) == 3:
        try:
            arena_side = float(fields[2])
        except ValueError as error:
            raise ValueError(f"invalid arena side in case: {text}") from error
        if not math.isfinite(arena_side) or arena_side <= 0.0:
            raise ValueError("arena side must be positive and finite")
    else:
        arena_side = default_qualitative_side(n)

    seed = BASE_IMAGE_SEED + index
    image_id = image_path.stem
    run_id = f"qualitative-n{n}-{image_id}"
    return LargeCase(
        run_id=run_id,
        n=n,
        image_id=image_id,
        image_path=image_path,
        image_sha256=hashlib.sha256(image_path.read_bytes()).hexdigest(),
        seed=seed,
        controller_seed=derive_seed(seed, "controller"),
        arena_side=arena_side,
        world_side=1.2 * arena_side,
        world_path=output_root / "worlds" / f"{run_id}.world",
        result_path=output_root / "runs" / f"{run_id}.json",
        log_path=output_root / "logs" / f"{run_id}.log",
    )


def prepare_case(
    case: LargeCase,
    *,
    formation_include: Path,
    map_include: Path,
    empty_bitmap: Path,
    template_path: Path,
) -> Path:
    poses = generate_initial_poses(case.n, case.arena_side, case.seed)
    return render_world(
        output_path=case.world_path,
        n=case.n,
        side=case.arena_side,
        world_side=case.world_side,
        poses=poses,
        formation_include=formation_include,
        map_include=map_include,
        empty_bitmap=empty_bitmap,
        template_path=template_path,
    )


def stage_command(case: LargeCase, stage_executable: Path, *, gui: bool) -> tuple[str, ...]:
    command = [str(stage_executable)]
    if not gui:
        command.append("-g")
    command.extend((str(case.n), str(case.image_path), str(case.world_path)))
    return tuple(command)


def _prepend_path(value: str, inherited: str | None) -> str:
    entries = [value]
    if inherited:
        entries.extend(item for item in inherited.split(os.pathsep) if item)
    return os.pathsep.join(dict.fromkeys(entries))


def _run_environment(
    case: LargeCase,
    stage_executable: Path,
    *,
    sim_timeout: float,
    save_generated_image: bool,
) -> dict[str, str]:
    environment = os.environ.copy()
    stage_source = Path(__file__).resolve().parents[2]
    install_prefix = stage_executable.resolve().parent.parent
    stage_paths = (
        stage_source / "libstage",
        install_prefix / "share/stage",
        install_prefix / "lib/Stage-4.3",
    )
    environment["STAGEPATH"] = os.pathsep.join(
        [str(path) for path in stage_paths if path.is_dir()]
        + ([environment["STAGEPATH"]] if environment.get("STAGEPATH") else [])
    )
    library_path = str(install_prefix / "lib")
    environment["LD_LIBRARY_PATH"] = _prepend_path(
        library_path, environment.get("LD_LIBRARY_PATH")
    )
    environment["DYLD_LIBRARY_PATH"] = _prepend_path(
        library_path, environment.get("DYLD_LIBRARY_PATH")
    )
    environment.update(
        {
            "FORMATION_RUN_ID": case.run_id,
            "FORMATION_IMAGE_ID": case.image_id,
            "FORMATION_IMAGE_SHA256": case.image_sha256,
            "FORMATION_SEED": str(case.seed),
            "FORMATION_CONTROLLER_SEED": str(case.controller_seed),
            "FORMATION_EXPERIMENT_MODE": "qualitative",
            "FORMATION_ARENA_SIDE": f"{case.arena_side:.12g}",
            "FORMATION_RESULT_PATH": str(case.result_path.resolve()),
            "FORMATION_SIM_TIMEOUT": f"{sim_timeout:.12g}",
            "FORMATION_PROGRESS_INTERVAL": "10",
            "FORMATION_SAVE_GENERATED_IMAGE": "1" if save_generated_image else "0",
        }
    )
    return environment


def _completed_result(case: LargeCase) -> dict[str, object] | None:
    if not case.result_path.is_file():
        return None
    try:
        result = json.loads(case.result_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    if result.get("run_id") != case.run_id or result.get("status") not in TERMINAL_STATUSES:
        return None
    expected = {
        "N": case.n,
        "image_id": case.image_id,
        "image_sha256": case.image_sha256,
        "seed": case.seed,
    }
    if any(result.get(field) != value for field, value in expected.items()):
        return None
    try:
        recorded_side = float(result["arena_side"])
    except (KeyError, TypeError, ValueError):
        return None
    if abs(recorded_side - case.arena_side) > 1e-9 * max(1.0, case.arena_side):
        return None
    return result


def _archive_previous_attempt(case: LargeCase) -> Path:
    archive_root = case.result_path.parent.parent / "retry_archive" / case.run_id
    attempt = 1
    while (archive_root / f"attempt-{attempt:03d}").exists():
        attempt += 1
    destination = archive_root / f"attempt-{attempt:03d}"
    destination.mkdir(parents=True)
    case.result_path.replace(destination / "result.json")
    if case.log_path.is_file():
        case.log_path.replace(destination / "stage.log")
    return destination


def run_case(
    case: LargeCase,
    stage_executable: Path,
    *,
    gui: bool,
    sim_timeout: float,
    save_generated_image: bool,
    force: bool,
) -> dict[str, object]:
    previous = _completed_result(case)
    if previous is not None and not force:
        print(f"[resume] {case.run_id} status={previous['status']}", flush=True)
        return previous
    if previous is not None:
        archive = _archive_previous_attempt(case)
        print(f"[rerun] {case.run_id} archive={archive}", flush=True)

    case.result_path.parent.mkdir(parents=True, exist_ok=True)
    case.log_path.parent.mkdir(parents=True, exist_ok=True)
    command = stage_command(case, stage_executable, gui=gui)
    environment = _run_environment(
        case,
        stage_executable,
        sim_timeout=sim_timeout,
        save_generated_image=save_generated_image,
    )
    print(f"[start] {case.run_id} N={case.n} gui={int(gui)} log={case.log_path}", flush=True)
    started = time.monotonic()
    if gui:
        completed = subprocess.run(command, env=environment, check=False)
    else:
        with case.log_path.open("w", encoding="utf-8") as log:
            completed = subprocess.run(
                command,
                env=environment,
                stdout=log,
                stderr=subprocess.STDOUT,
                check=False,
            )
    wall_time = time.monotonic() - started
    result = _completed_result(case)
    if result is None:
        result = {
            "run_id": case.run_id,
            "N": case.n,
            "image_id": case.image_id,
            "image_sha256": case.image_sha256,
            "seed": case.seed,
            "arena_side": case.arena_side,
            "status": "runtime_error",
            "returncode": completed.returncode,
            "wall_time_s": wall_time,
        }
        temporary = case.result_path.with_suffix(".json.tmp")
        temporary.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        temporary.replace(case.result_path)
    print(
        f"[done] {case.run_id} status={result['status']} wall_time_s={wall_time:.1f}",
        flush=True,
    )
    return result


def write_case_manifest(cases: tuple[LargeCase, ...], output_root: Path) -> Path:
    output_root.mkdir(parents=True, exist_ok=True)
    path = output_root / "cases.json"
    payload = []
    for case in cases:
        row = asdict(case)
        row.update({key: str(value) for key, value in row.items() if isinstance(value, Path)})
        payload.append(row)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return path


def run_cases(
    cases: tuple[LargeCase, ...],
    stage_executable: Path,
    *,
    workers: int,
    gui: bool,
    sim_timeout: float = SIM_TIMEOUT_SECONDS,
    save_generated_image: bool = False,
    force: bool = False,
) -> tuple[dict[str, object], ...]:
    if workers <= 0:
        raise ValueError("workers must be positive")
    if gui and workers != 1:
        raise ValueError("GUI cases must run with exactly one worker")
    if not math.isfinite(sim_timeout) or sim_timeout <= 0.0:
        raise ValueError("simulation timeout must be positive and finite")
    if workers == 1:
        return tuple(
            run_case(
                case,
                stage_executable,
                gui=gui,
                sim_timeout=sim_timeout,
                save_generated_image=save_generated_image,
                force=force,
            )
            for case in cases
        )

    indexed_results: list[dict[str, object] | None] = [None] * len(cases)
    with ThreadPoolExecutor(max_workers=workers) as executor:
        futures = {
            executor.submit(
                run_case,
                case,
                stage_executable,
                gui=False,
                sim_timeout=sim_timeout,
                save_generated_image=save_generated_image,
                force=force,
            ): index
            for index, case in enumerate(cases)
        }
        for future in as_completed(futures):
            indexed_results[futures[future]] = future.result()
    return tuple(result for result in indexed_results if result is not None)
