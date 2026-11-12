from __future__ import annotations

import argparse
from pathlib import Path

from .analysis import load_results, write_summary
from .config import DEFAULT_WORKERS, MANIFEST_VERSION, ROBOT_COUNTS, WALL_TIMEOUT_SECONDS
from .manifest import command_qualifier, freeze_manifest, load_manifest
from .publication import generate_publication_report
from .runner import (
    FINAL_STATUSES,
    aggregate_results,
    make_run_specs,
    run_batch,
    select_retry_specs,
)


def _code_root() -> Path:
    return Path(__file__).resolve().parents[5]


def _default_manifest() -> Path:
    return Path(__file__).resolve().parents[1] / "manifests" / "formation_scaling_v1.csv"


def _default_output_root() -> Path:
    return _code_root() / "stage4/Stage/formation/results/fixed_density_v1"


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Fixed-density Stage experiment tooling")
    subparsers = parser.add_subparsers(dest="command", required=True)

    qualify = subparsers.add_parser("qualify", help="re-run target qualification and freeze 75 images")
    qualify.add_argument("--qualifier", type=Path, required=True)
    qualify.add_argument("--manifest", type=Path, default=_default_manifest())
    qualify.add_argument("--image-dir", type=Path, default=_code_root() / "stage4/Stage/formation/img")
    qualify.add_argument("--expected-count", type=int, default=75)
    qualify.add_argument("--workers", type=int, default=DEFAULT_WORKERS)

    batch = subparsers.add_parser("run", help="generate worlds and run/resume the batch")
    batch.add_argument("--stage", type=Path, required=True)
    batch.add_argument("--manifest", type=Path, default=_default_manifest())
    batch.add_argument(
        "--output-root",
        type=Path,
        default=_default_output_root(),
    )
    batch.add_argument("--workers", type=int, default=DEFAULT_WORKERS)
    batch.add_argument("--wall-timeout", type=float, default=WALL_TIMEOUT_SECONDS)
    batch.add_argument(
        "--no-wall-timeout",
        action="store_true",
        help="disable the outer wall-clock timeout; the controller simulation timeout remains active",
    )
    batch.add_argument(
        "--retry-status",
        action="append",
        choices=sorted(FINAL_STATUSES),
        help="run only existing results with this terminal status; repeat for multiple statuses",
    )
    batch.add_argument("--only-n", type=int, nargs="+", choices=ROBOT_COUNTS)
    batch.add_argument(
        "--image-id",
        action="append",
        help="run only this manifest image id; repeat to select multiple images",
    )
    batch.add_argument("--prepare-only", action="store_true")

    analyze = subparsers.add_parser(
        "analyze",
        help="summarize rates and successful-run distributions by N",
    )
    analyze.add_argument(
        "--input",
        type=Path,
        default=_default_output_root(),
        help="experiment root, runs directory, aggregate CSV, or one JSON file",
    )
    analyze.add_argument(
        "--output",
        type=Path,
        default=_default_output_root() / "summary_by_n.csv",
    )

    report = subparsers.add_parser(
        "report",
        help="validate the complete design and generate publication statistics and figures",
    )
    report.add_argument(
        "--input",
        type=Path,
        default=_default_output_root(),
        help="experiment root, runs directory, aggregate CSV, or one JSON file",
    )
    report.add_argument(
        "--output-dir",
        type=Path,
        default=_default_output_root() / "publication",
    )
    report.add_argument(
        "--figure-dir",
        type=Path,
        default=_code_root().parent / "fig",
        help="paper figure directory receiving PDF, SVG, and PNG copies",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    code_root = _code_root()
    if args.command == "analyze":
        output = write_summary(load_results(args.input), args.output)
        print(f"summary: {output}")
        return 0
    if args.command == "report":
        retry_archive = args.input / "retry_archive" if args.input.is_dir() else None
        artifacts = generate_publication_report(
            load_results(args.input),
            output_dir=args.output_dir,
            figure_dir=args.figure_dir,
            retry_archive=retry_archive,
        )
        print(f"publication summary: {artifacts['summary_csv']}")
        print(f"LaTeX table: {artifacts['statistics_table_tex']}")
        print(f"convergence-time figure: {artifacts['convergence_time_pdf']}")
        print(f"total-distance figure: {artifacts['total_distance_pdf']}")
        print(f"per-robot-distance figure: {artifacts['distance_per_robot_pdf']}")
        return 0
    if args.command == "qualify":
        output = freeze_manifest(
            image_dir=args.image_dir,
            output_path=args.manifest,
            repository_root=code_root,
            qualifier=command_qualifier(args.qualifier),
            expected_count=args.expected_count,
            version=MANIFEST_VERSION,
            workers=args.workers,
        )
        print(f"qualified manifest: {output}")
        return 0

    rows = load_manifest(args.manifest, repository_root=code_root)
    if len(rows) != 75:
        raise SystemExit(f"current manifest has {len(rows)} images; expected exactly 75")
    if args.image_id:
        selected_ids = set(args.image_id)
        rows = tuple(row for row in rows if row.image_id in selected_ids)
        missing_ids = selected_ids - {row.image_id for row in rows}
        if missing_ids:
            raise SystemExit(f"image ids are not in the manifest: {', '.join(sorted(missing_ids))}")
    robot_counts = tuple(args.only_n) if args.only_n else ROBOT_COUNTS
    specs = make_run_specs(
        manifest_rows=rows,
        repository_root=code_root,
        output_root=args.output_root,
        stage_executable=args.stage,
        robot_counts=robot_counts,
    )
    retry_statuses = frozenset(args.retry_status or ())
    if retry_statuses:
        specs = select_retry_specs(specs, retry_statuses)
        print(
            f"selected {len(specs)} run(s) with status "
            f"{','.join(sorted(retry_statuses))} in {args.output_root}"
        )
    else:
        print(f"prepared {len(specs)} runs in {args.output_root}")
    if not args.prepare_only:
        wall_timeout = None if args.no_wall_timeout else args.wall_timeout
        results = run_batch(
            specs,
            workers=args.workers,
            wall_timeout_seconds=wall_timeout,
            retry_statuses=retry_statuses,
        )
        print(f"completed or resumed {len(results)} runs")
    aggregate = aggregate_results(args.output_root / "runs", args.output_root / "aggregate.csv")
    print(f"aggregate: {aggregate}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
