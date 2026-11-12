from __future__ import annotations

import csv
import json
import math
import statistics
from collections.abc import Iterable, Mapping, Sequence
from pathlib import Path
from typing import Any

STATUSES = (
    "success",
    "sim_timeout",
    "wall_timeout",
    "runtime_error",
    "target_generation_failed",
)
SUCCESS_METRICS = (
    "convergence_time_s",
    "total_distance_m",
    "mean_distance_per_robot_m",
)
SUMMARY_FIELDS = (
    "N",
    "total",
    *(item for status in STATUSES for item in (f"{status}_count", f"{status}_rate")),
    *(
        item
        for metric in SUCCESS_METRICS
        for item in (
            f"{metric}_count",
            f"{metric}_mean",
            f"{metric}_median",
            f"{metric}_q1",
            f"{metric}_q3",
        )
    ),
)


def load_results(source: Path) -> list[dict[str, Any]]:
    """Load an aggregate CSV, a run JSON, or a directory of per-run JSON files."""
    if source.is_dir():
        run_directory = source / "runs" if (source / "runs").is_dir() else source
        rows: list[dict[str, Any]] = []
        for path in sorted(run_directory.glob("*.json")):
            try:
                rows.append(json.loads(path.read_text(encoding="utf-8")))
            except json.JSONDecodeError as error:
                raise RuntimeError(f"invalid result JSON: {path}") from error
        return rows
    if source.suffix.lower() == ".json":
        return [json.loads(source.read_text(encoding="utf-8"))]
    if source.suffix.lower() == ".csv":
        with source.open(encoding="utf-8", newline="") as input_file:
            return list(csv.DictReader(input_file))
    raise ValueError("analysis input must be a result directory, JSON, or CSV")


def _number(value: object) -> float | None:
    if value is None or value == "":
        return None
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    return number if math.isfinite(number) else None


def _quantile(sorted_values: Sequence[float], probability: float) -> float:
    if not sorted_values:
        raise ValueError("cannot take a quantile of an empty sample")
    position = probability * (len(sorted_values) - 1)
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return sorted_values[lower]
    weight = position - lower
    return sorted_values[lower] * (1.0 - weight) + sorted_values[upper] * weight


def _metric_summary(values: Sequence[float]) -> dict[str, float | int | None]:
    if not values:
        return {"count": 0, "mean": None, "median": None, "q1": None, "q3": None}
    ordered = sorted(values)
    return {
        "count": len(ordered),
        "mean": statistics.fmean(ordered),
        "median": statistics.median(ordered),
        "q1": _quantile(ordered, 0.25),
        "q3": _quantile(ordered, 0.75),
    }


def summarize_results(rows: Iterable[Mapping[str, object]]) -> list[dict[str, object]]:
    grouped: dict[int, list[Mapping[str, object]]] = {}
    for row in rows:
        try:
            n = int(row["N"])
        except (KeyError, TypeError, ValueError) as error:
            raise RuntimeError("each result must contain an integer N") from error
        status = str(row.get("status", ""))
        if status not in STATUSES:
            raise RuntimeError(f"unknown terminal status for N={n}: {status!r}")
        grouped.setdefault(n, []).append(row)

    summaries: list[dict[str, object]] = []
    for n in sorted(grouped):
        group = grouped[n]
        total = len(group)
        summary: dict[str, object] = {"N": n, "total": total}
        for status in STATUSES:
            count = sum(row.get("status") == status for row in group)
            summary[f"{status}_count"] = count
            summary[f"{status}_rate"] = count / total
        successful = [row for row in group if row.get("status") == "success"]
        for metric in SUCCESS_METRICS:
            values = [value for row in successful if (value := _number(row.get(metric))) is not None]
            for statistic_name, value in _metric_summary(values).items():
                summary[f"{metric}_{statistic_name}"] = value
        summaries.append(summary)
    return summaries


def write_summary(rows: Iterable[Mapping[str, object]], output_path: Path) -> Path:
    summaries = summarize_results(rows)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_path.with_suffix(output_path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=SUMMARY_FIELDS)
        writer.writeheader()
        writer.writerows(summaries)
    temporary.replace(output_path)
    return output_path
