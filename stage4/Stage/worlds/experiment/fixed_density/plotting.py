from __future__ import annotations

import argparse
import csv
import shutil
import statistics
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path


# Edit this block to change filenames, axis labels, or axis scaling.
@dataclass(frozen=True)
class FigureSpec:
    key: str
    filename: str
    metric: str
    y_label: str
    logarithmic_y: bool = False
    y_limits: tuple[float, float] | None = None


FIGURE_SPECS = (
    FigureSpec(
        key="convergence_time",
        filename="fixed_density_convergence_time",
        metric="convergence_time_s",
        y_label="Convergence time (s)",
        logarithmic_y=False,
        y_limits=(0.0, 3700.0),
    ),
    FigureSpec(
        key="total_distance",
        filename="fixed_density_total_distance",
        metric="total_distance_m",
        y_label="Total distance (m)",
    ),
    FigureSpec(
        key="distance_per_robot",
        filename="fixed_density_distance_per_robot",
        metric="mean_distance_per_robot_m",
        y_label="Distance per robot (m)",
    ),
)

X_LABEL = "Number of robots"
X_TICK_ROTATION_DEGREES = 0.0
FIGURE_WIDTH_MM = 89.0
FIGURE_HEIGHT_MM = 67.0
BOX_FACE_COLOR = "#C7DDF0"
LINE_COLOR = "#176B87"
MEDIAN_COLOR = "#003B5C"


def _configure_matplotlib() -> None:
    import matplotlib

    matplotlib.use("Agg")
    matplotlib.rcParams.update(
        {
            "font.family": "sans-serif",
            "font.sans-serif": ["Arial", "DejaVu Sans", "Liberation Sans"],
            "font.size": 6.5,
            "axes.labelsize": 7,
            "xtick.labelsize": 6,
            "ytick.labelsize": 6,
            "svg.fonttype": "none",
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "savefig.facecolor": "white",
        }
    )


def _draw_one_metric(
    rows: Sequence[Mapping[str, object]],
    spec: FigureSpec,
    output_dir: Path,
) -> dict[str, Path]:
    import matplotlib.pyplot as plt

    robot_counts = sorted({int(row["N"]) for row in rows})
    grouped = [
        [float(row[spec.metric]) for row in rows if int(row["N"]) == n]
        for n in robot_counts
    ]
    figure, axis = plt.subplots(
        figsize=(FIGURE_WIDTH_MM / 25.4, FIGURE_HEIGHT_MM / 25.4)
    )
    axis.boxplot(
        grouped,
        tick_labels=[str(n) for n in robot_counts],
        patch_artist=True,
        widths=0.62,
        whis=1.5,
        showfliers=True,
        boxprops={"facecolor": BOX_FACE_COLOR, "edgecolor": LINE_COLOR, "linewidth": 0.8},
        medianprops={"color": MEDIAN_COLOR, "linewidth": 1.2},
        whiskerprops={"color": LINE_COLOR, "linewidth": 0.75},
        capprops={"color": LINE_COLOR, "linewidth": 0.75},
        flierprops={
            "marker": "o",
            "markersize": 2.2,
            "markerfacecolor": "white",
            "markeredgecolor": "#555555",
            "markeredgewidth": 0.45,
            "alpha": 0.8,
        },
    )
    medians = [statistics.median(values) for values in grouped]
    axis.plot(
        range(1, len(robot_counts) + 1),
        medians,
        color=MEDIAN_COLOR,
        linewidth=0.7,
        alpha=0.75,
    )
    if spec.logarithmic_y:
        axis.set_yscale("log")
    if spec.y_limits is not None:
        axis.set_ylim(*spec.y_limits)
    axis.set_xlabel(X_LABEL)
    axis.set_ylabel(spec.y_label)
    axis.tick_params(axis="x", labelrotation=X_TICK_ROTATION_DEGREES)
    axis.grid(axis="y", color="#D9D9D9", linewidth=0.5, linestyle=(0, (2, 2)))
    axis.set_axisbelow(True)
    axis.spines["top"].set_visible(False)
    axis.spines["right"].set_visible(False)
    figure.subplots_adjust(left=0.19, right=0.98, bottom=0.20, top=0.97)

    paths = {
        f"{spec.key}_svg": output_dir / f"{spec.filename}.svg",
        f"{spec.key}_pdf": output_dir / f"{spec.filename}.pdf",
        f"{spec.key}_png": output_dir / f"{spec.filename}.png",
    }
    figure.savefig(paths[f"{spec.key}_svg"], bbox_inches="tight")
    figure.savefig(paths[f"{spec.key}_pdf"], bbox_inches="tight")
    figure.savefig(paths[f"{spec.key}_png"], dpi=600, bbox_inches="tight")
    plt.close(figure)
    return paths


def draw_metric_figures(
    rows: Sequence[Mapping[str, object]],
    *,
    output_dir: Path,
    paper_figure_dir: Path | None = None,
) -> dict[str, Path]:
    """Draw three independent figures and return their paper-facing paths."""
    _configure_matplotlib()
    output_dir.mkdir(parents=True, exist_ok=True)
    if paper_figure_dir is not None:
        paper_figure_dir.mkdir(parents=True, exist_ok=True)

    artifacts: dict[str, Path] = {}
    for spec in FIGURE_SPECS:
        generated = _draw_one_metric(rows, spec, output_dir)
        for key, source in generated.items():
            if paper_figure_dir is None:
                artifacts[key] = source
                continue
            destination = paper_figure_dir / source.name
            shutil.copyfile(source, destination)
            artifacts[key] = destination
    return artifacts


def _load_csv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as source:
        return list(csv.DictReader(source))


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Draw the three fixed-density publication figures"
    )
    parser.add_argument("--input", type=Path, required=True, help="plot source-data CSV")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--paper-figure-dir", type=Path)
    args = parser.parse_args(argv)
    artifacts = draw_metric_figures(
        _load_csv(args.input),
        output_dir=args.output_dir,
        paper_figure_dir=args.paper_figure_dir,
    )
    for key in sorted(artifacts):
        print(f"{key}: {artifacts[key]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
