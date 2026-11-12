from __future__ import annotations

import argparse
import sys
from pathlib import Path

CODE_ROOT = Path(__file__).resolve().parents[4]
if str(CODE_ROOT) not in sys.path:
    sys.path.insert(0, str(CODE_ROOT))

from stage4.Stage.worlds.experiment.large_cases import (
    DEFAULT_CASES,
    parse_case,
    prepare_case,
    run_cases,
    write_case_manifest,
)


def _code_root() -> Path:
    return CODE_ROOT


def _parser() -> argparse.ArgumentParser:
    code_root = _code_root()
    parser = argparse.ArgumentParser(description="Prepare and run qualitative large-swarm cases")
    parser.add_argument("action", choices=("prepare", "run", "gui"))
    parser.add_argument(
        "--case",
        action="append",
        dest="cases",
        help="N:IMAGE or N:IMAGE:ARENA_SIDE; repeat for multiple cases",
    )
    parser.add_argument(
        "--output-root",
        type=Path,
        default=code_root / "stage4/Stage/formation/results/large_cases_v1",
    )
    parser.add_argument(
        "--stage",
        type=Path,
        default=code_root / "stage4/stage-install/bin/stage",
    )
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--sim-timeout", type=float, default=7200.0)
    parser.add_argument("--save-generated-image", action="store_true")
    parser.add_argument("--force", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    code_root = _code_root()
    output_root = args.output_root.resolve()
    if args.action == "gui" and (args.cases is None or len(args.cases) != 1):
        raise SystemExit("gui requires exactly one --case / N:IMAGE[:ARENA_SIDE] argument")
    case_texts = tuple(args.cases or DEFAULT_CASES)
    cases = tuple(
        parse_case(text, code_root=code_root, output_root=output_root, index=index)
        for index, text in enumerate(case_texts)
    )

    experiment_dir = Path(__file__).resolve().parent
    for case in cases:
        prepare_case(
            case,
            formation_include=experiment_dir / "formation_world/formation.inc",
            map_include=experiment_dir / "formation_world/map.inc",
            empty_bitmap=experiment_dir / "formation_world/bitmaps/empty.png",
            template_path=experiment_dir / "formation_qualitative.world.template",
        )
        print(
            f"[prepared] {case.run_id} side={case.arena_side:.3f} "
            f"world_side={case.world_side:.3f} world={case.world_path}"
        )
    manifest = write_case_manifest(cases, output_root)
    print(f"case manifest: {manifest}")

    if args.action == "prepare":
        return 0
    stage_executable = args.stage.resolve()
    if not stage_executable.is_file():
        raise SystemExit(f"Stage executable does not exist: {stage_executable}")
    gui = args.action == "gui"
    run_cases(
        cases,
        stage_executable,
        workers=1 if gui else args.workers,
        gui=gui,
        sim_timeout=args.sim_timeout,
        save_generated_image=args.save_generated_image,
        force=args.force or gui,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
