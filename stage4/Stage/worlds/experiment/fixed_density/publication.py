from __future__ import annotations

import csv
import json
import math
from collections import Counter
from collections.abc import Iterable, Mapping, Sequence
from pathlib import Path

from .analysis import SUCCESS_METRICS, summarize_results
from .config import ROBOT_COUNTS, SIM_TIMEOUT_SECONDS, arena_side
from .plotting import draw_metric_figures

EXPECTED_IMAGES_PER_SIZE = 75
TARGET_DISTANCE_LOWER_BOUND = 2.625
TARGET_MST_UPPER_BOUND = 11.4
TARGET_ERROR_TOLERANCE = 0.5


class PublicationReportError(RuntimeError):
    """Raised when the result set is not safe to use for publication."""


def _finite_number(row: Mapping[str, object], field: str) -> float:
    value = row.get(field)
    try:
        number = float(value)
    except (TypeError, ValueError) as error:
        raise PublicationReportError(
            f"run {row.get('run_id', '<unknown>')} has no numeric {field}"
        ) from error
    if not math.isfinite(number):
        raise PublicationReportError(
            f"run {row.get('run_id', '<unknown>')} has non-finite {field}"
        )
    return number


def _validate_successful_run(row: Mapping[str, object], n: int) -> None:
    run_id = row.get("run_id", "<unknown>")
    total_distance = _finite_number(row, "total_distance_m")
    per_robot_distance = _finite_number(row, "mean_distance_per_robot_m")
    convergence_time = _finite_number(row, "convergence_time_s")
    target_minimum = _finite_number(row, "target_min_distance")
    mst_bottleneck = _finite_number(row, "target_mst_bottleneck")
    target_error = _finite_number(row, "final_max_target_error")

    if convergence_time < 0.0 or convergence_time > SIM_TIMEOUT_SECONDS:
        raise PublicationReportError(f"run {run_id} has invalid convergence time")
    if total_distance < 0.0 or per_robot_distance < 0.0:
        raise PublicationReportError(f"run {run_id} has a negative distance")
    if not math.isclose(total_distance, per_robot_distance * n, rel_tol=1e-9, abs_tol=1e-8):
        raise PublicationReportError(f"run {run_id} fails distance arithmetic")
    if target_minimum + 1e-9 < TARGET_DISTANCE_LOWER_BOUND:
        raise PublicationReportError(f"run {run_id} violates target minimum distance")
    if mst_bottleneck - 1e-9 > TARGET_MST_UPPER_BOUND:
        raise PublicationReportError(f"run {run_id} violates target connectivity")
    if target_error - 1e-9 > TARGET_ERROR_TOLERANCE:
        raise PublicationReportError(f"run {run_id} violates the arrival tolerance")


def build_publication_summary(
    rows: Iterable[Mapping[str, object]],
    *,
    expected_robot_counts: Sequence[int] = ROBOT_COUNTS,
    expected_per_size: int = EXPECTED_IMAGES_PER_SIZE,
) -> list[dict[str, object]]:
    """Validate the balanced design and return descriptive statistics by swarm size."""
    materialized = [dict(row) for row in rows]
    grouped: dict[int, list[dict[str, object]]] = {}
    for row in materialized:
        try:
            n = int(row["N"])
        except (KeyError, TypeError, ValueError) as error:
            raise PublicationReportError("each run must contain an integer N") from error
        grouped.setdefault(n, []).append(row)

    expected = tuple(expected_robot_counts)
    if tuple(sorted(grouped)) != tuple(sorted(expected)):
        raise PublicationReportError(
            f"robot counts are {tuple(sorted(grouped))}; expected {tuple(sorted(expected))}"
        )
    reference_images: set[str] | None = None
    reference_signatures: dict[str, tuple[str, int]] | None = None
    seen_run_ids: set[str] = set()
    for n in expected:
        group = grouped[n]
        if len(group) != expected_per_size:
            raise PublicationReportError(
                f"N={n} has {len(group)} runs; expected {expected_per_size} runs"
            )
        failed = [row for row in group if row.get("status") != "success"]
        if failed:
            statuses = Counter(str(row.get("status")) for row in failed)
            raise PublicationReportError(f"N={n} contains runs that are not successful: {statuses}")
        image_ids = {str(row.get("image_id", "")) for row in group}
        if "" in image_ids or len(image_ids) != expected_per_size:
            raise PublicationReportError(f"N={n} does not contain unique image instances")
        if reference_images is None:
            reference_images = image_ids
        elif image_ids != reference_images:
            raise PublicationReportError(f"N={n} does not reuse the same image set")
        signatures: dict[str, tuple[str, int]] = {}
        for row in group:
            run_id = str(row.get("run_id", ""))
            if not run_id or run_id in seen_run_ids:
                raise PublicationReportError(f"duplicate or missing run_id at N={n}")
            seen_run_ids.add(run_id)
            if "arena_side" in row and not math.isclose(
                _finite_number(row, "arena_side"),
                arena_side(n),
                rel_tol=1e-9,
                abs_tol=1e-8,
            ):
                raise PublicationReportError(f"run {run_id} has the wrong arena side")
            image_id = str(row["image_id"])
            image_hash = str(row.get("image_sha256", ""))
            try:
                seed = int(row["seed"])
            except (KeyError, TypeError, ValueError) as error:
                raise PublicationReportError(f"run {run_id} has no valid seed") from error
            if len(image_hash) != 64 or any(
                character not in "0123456789abcdef" for character in image_hash.lower()
            ):
                raise PublicationReportError(f"run {run_id} has no valid image hash")
            signatures[image_id] = (image_hash.lower(), seed)
            _validate_successful_run(row, n)
        if reference_signatures is None:
            reference_signatures = signatures
        elif signatures != reference_signatures:
            raise PublicationReportError(f"N={n} does not reuse the same image hashes and seeds")

    summary = summarize_results(materialized)
    for output, n in zip(summary, sorted(expected), strict=True):
        group = grouped[n]
        output["convergence_time_s_max"] = max(
            _finite_number(row, "convergence_time_s") for row in group
        )
        output["target_min_distance_min"] = min(
            _finite_number(row, "target_min_distance") for row in group
        )
        output["target_mst_bottleneck_max"] = max(
            _finite_number(row, "target_mst_bottleneck") for row in group
        )
        output["final_max_target_error_max"] = max(
            _finite_number(row, "final_max_target_error") for row in group
        )
    return summary


def _interval(summary: Mapping[str, object], metric: str, decimals: int) -> str:
    median = float(summary[f"{metric}_median"])
    q1 = float(summary[f"{metric}_q1"])
    q3 = float(summary[f"{metric}_q3"])
    return f"{median:.{decimals}f} [{q1:.{decimals}f}, {q3:.{decimals}f}]"


def render_statistics_table(summaries: Sequence[Mapping[str, object]]) -> str:
    sample_count = int(summaries[0]["total"])
    lines = [
        r"\begin{table*}[t]",
        r"\centering",
        "\\caption{Fixed-density formation results. Values are median "
        "[first quartile, third quartile] over the "
        f"{sample_count} image instances at each swarm size.}}",
        r"\label{tab:fixed-density-results}",
        r"\small",
        r"\begin{tabular}{rcrrr}",
        r"\hline",
        r"Robots & Success & Convergence time (s) & Total distance (m) & Distance per robot (m) \\",
        r"\hline",
    ]
    for row in summaries:
        lines.append(
            f"{int(row['N'])} & {int(row['success_count'])}/{int(row['total'])} & "
            f"{_interval(row, 'convergence_time_s', 1)} & "
            f"{_interval(row, 'total_distance_m', 1)} & "
            f"{_interval(row, 'mean_distance_per_robot_m', 3)} \\\\"
        )
    lines.extend([r"\hline", r"\end{tabular}", r"\end{table*}", ""])
    return "\n".join(lines)


def _retry_counts(retry_archive: Path | None) -> Counter[int]:
    counts: Counter[int] = Counter()
    if retry_archive is None or not retry_archive.is_dir():
        return counts
    for path in retry_archive.rglob("result.json"):
        try:
            row = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise PublicationReportError(f"cannot read retry archive result: {path}") from error
        if row.get("status") == "wall_timeout":
            counts[int(row["N"])] += 1
    return counts


def _render_results_text(
    summaries: Sequence[Mapping[str, object]], retry_counts: Mapping[int, int]
) -> str:
    total = sum(int(row["total"]) for row in summaries)
    success = sum(int(row["success_count"]) for row in summaries)
    first, last = summaries[0], summaries[-1]
    time_ratio = float(last["convergence_time_s_median"]) / float(
        first["convergence_time_s_median"]
    )
    total_ratio = float(last["total_distance_m_median"]) / float(
        first["total_distance_m_median"]
    )
    per_robot_ratio = float(last["mean_distance_per_robot_m_median"]) / float(
        first["mean_distance_per_robot_m_median"]
    )
    maximum_time = max(float(row["convergence_time_s_max"]) for row in summaries)
    retry_total = sum(retry_counts.values())
    retry_detail = ", ".join(
        f"$n={n}$: {retry_counts[n]}" for n in sorted(retry_counts)
    )
    retry_sentence = ""
    if retry_total:
        retry_sentence = (
            f" The initial 1200-s wall-clock watchdog censored {retry_total} runs "
            f"({retry_detail}). All were selected mechanically and rerun with the same "
            "controller, worlds, images, and seeds after removing only the outer wall-clock cap; "
            "the 7200-s simulated-time limit was unchanged."
        )
    medians = [float(row["convergence_time_s_median"]) for row in summaries]
    monotonic_sentence = ""
    if any(current < previous for previous, current in zip(medians, medians[1:])):
        monotonic_sentence = (
            " The convergence-time medians are not strictly monotonic across adjacent sizes, "
            "so these results support an overall scaling trend rather than a deterministic "
            "per-size ordering."
        )
    return (
        f"All {success}/{total} runs converged before the simulated-time limit; the longest "
        f"successful run required {maximum_time:.0f} simulated seconds.{retry_sentence} "
        f"From $n={int(first['N'])}$ to $n={int(last['N'])}$, the median convergence time "
        f"increased from {float(first['convergence_time_s_median']):.1f} to "
        f"{float(last['convergence_time_s_median']):.1f} s ({time_ratio:.1f}$\\times$), "
        f"the median total distance increased from {float(first['total_distance_m_median']):.1f} "
        f"to {float(last['total_distance_m_median']):.1f} m ({total_ratio:.1f}$\\times$), "
        f"and the median distance per robot increased from "
        f"{float(first['mean_distance_per_robot_m_median']):.3f} to "
        f"{float(last['mean_distance_per_robot_m_median']):.3f} m "
        f"({per_robot_ratio:.1f}$\\times$).{monotonic_sentence}\n"
    )


def _write_summary_csv(summaries: Sequence[Mapping[str, object]], output: Path) -> None:
    fields = list(summaries[0])
    with output.open("w", encoding="utf-8", newline="") as destination:
        writer = csv.DictWriter(destination, fieldnames=fields)
        writer.writeheader()
        writer.writerows(summaries)


def _write_source_data(rows: Sequence[Mapping[str, object]], output: Path) -> None:
    fields = ("run_id", "N", "image_id", *SUCCESS_METRICS)
    with output.open("w", encoding="utf-8", newline="") as destination:
        writer = csv.DictWriter(destination, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def generate_publication_report(
    rows: Iterable[Mapping[str, object]],
    *,
    output_dir: Path,
    figure_dir: Path,
    expected_robot_counts: Sequence[int] = ROBOT_COUNTS,
    expected_per_size: int = EXPECTED_IMAGES_PER_SIZE,
    retry_archive: Path | None = None,
) -> dict[str, Path]:
    materialized = [dict(row) for row in rows]
    summaries = build_publication_summary(
        materialized,
        expected_robot_counts=expected_robot_counts,
        expected_per_size=expected_per_size,
    )
    output_dir.mkdir(parents=True, exist_ok=True)
    figure_dir.mkdir(parents=True, exist_ok=True)

    artifacts = {
        "summary_csv": output_dir / "publication_summary.csv",
        "source_data_csv": output_dir / "fixed_density_metrics_source_data.csv",
        "statistics_table_tex": output_dir / "statistics_table.tex",
        "results_text_tex": output_dir / "results_text.tex",
        "figure_qa": output_dir / "figure_qa.md",
    }
    _write_summary_csv(summaries, artifacts["summary_csv"])
    _write_source_data(materialized, artifacts["source_data_csv"])
    artifacts["statistics_table_tex"].write_text(
        render_statistics_table(summaries), encoding="utf-8"
    )
    retry_counts = _retry_counts(retry_archive)
    artifacts["results_text_tex"].write_text(
        _render_results_text(summaries, retry_counts), encoding="utf-8"
    )
    artifacts.update(
        draw_metric_figures(
            materialized,
            output_dir=output_dir,
            paper_figure_dir=figure_dir,
        )
    )

    retry_total = sum(retry_counts.values())
    artifacts["figure_qa"].write_text(
        "# Fixed-density figure QA\n\n"
        "- Core conclusion: all balanced fixed-density runs converged, while time and travel "
        "generally increase with swarm size.\n"
        "- Archetype: three independent quantitative distribution figures.\n"
        "- Backend: Python/matplotlib only.\n"
        "- Final size: 89 mm wide per figure; SVG keeps editable text; PDF uses "
        "TrueType fonts; PNG is 600 dpi.\n"
        f"- Sample definition: {expected_per_size} distinct image instances at each of "
        f"{len(expected_robot_counts)} swarm sizes.\n"
        "- Center and spread: median, quartiles, 1.5-IQR whiskers; all outliers are retained.\n"
        "- Inferential test: none; the figure reports descriptive repeated-instance "
        "distributions.\n"
        f"- Operational censoring provenance: {retry_total} archived initial "
        "wall-timeout attempts.\n"
        "- Limitations: simulation only; no collision-clearance or communication-load metric; "
        "the motion layer has no formal safety guarantee.\n",
        encoding="utf-8",
    )
    return artifacts
