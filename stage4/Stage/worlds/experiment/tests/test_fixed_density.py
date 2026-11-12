from __future__ import annotations

import csv
import hashlib
import json
import math
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest

EXPERIMENT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(EXPERIMENT_DIR))

from fixed_density.config import (
    ARENA_AREA_PER_ROBOT,
    ROBOT_COUNTS,
    STAGE_UNIT_METERS,
    arena_side,
    derive_seed,
)
from fixed_density.analysis import summarize_results, write_summary
from fixed_density.cli import _default_output_root
from fixed_density.manifest import command_qualifier, freeze_manifest, load_manifest
from fixed_density.publication import (
    PublicationReportError,
    build_publication_summary,
    generate_publication_report,
    render_statistics_table,
)
from fixed_density.runner import RunSpec, aggregate_results, execute_run, select_retry_specs
from fixed_density.sampling import generate_initial_poses
from fixed_density.world import render_world


class ConfigTests(unittest.TestCase):
    def test_default_output_is_under_formation_results(self) -> None:
        code_root = EXPERIMENT_DIR.parents[3]
        self.assertEqual(
            _default_output_root(),
            code_root / "stage4/Stage/formation/results/fixed_density_v1",
        )

    def test_arena_formula_and_unit_conversion(self) -> None:
        self.assertEqual(ARENA_AREA_PER_ROBOT, 50.0)
        self.assertEqual(STAGE_UNIT_METERS, 0.1)
        self.assertEqual(ROBOT_COUNTS, tuple(range(50, 501, 50)))
        self.assertAlmostEqual(arena_side(50), 50.0)
        self.assertAlmostEqual(arena_side(500), math.sqrt(25_000.0))
        self.assertAlmostEqual(arena_side(500) * STAGE_UNIT_METERS, 15.8113883008)
        with self.assertRaises(ValueError):
            arena_side(0)

    def test_seed_derivation_is_stable_and_domain_separated(self) -> None:
        base = 20_270_809
        self.assertEqual(derive_seed(base, "initial_pose"), derive_seed(base, "initial_pose"))
        self.assertNotEqual(derive_seed(base, "initial_pose"), derive_seed(base, "controller"))


class AnalysisTests(unittest.TestCase):
    def test_summary_counts_timeouts_and_uses_only_success_metrics(self) -> None:
        rows = [
            {
                "N": 50,
                "status": "success",
                "convergence_time_s": 10.0,
                "total_distance_m": 20.0,
                "mean_distance_per_robot_m": 0.4,
            },
            {
                "N": 50,
                "status": "success",
                "convergence_time_s": 30.0,
                "total_distance_m": 40.0,
                "mean_distance_per_robot_m": 0.8,
            },
            {
                "N": 50,
                "status": "sim_timeout",
                "convergence_time_s": 7200.0,
                "total_distance_m": 999.0,
                "mean_distance_per_robot_m": 999.0,
            },
            {"N": 100, "status": "wall_timeout"},
        ]
        summary = summarize_results(rows)
        n50 = summary[0]
        self.assertEqual(n50["total"], 3)
        self.assertEqual(n50["success_count"], 2)
        self.assertAlmostEqual(n50["success_rate"], 2 / 3)
        self.assertEqual(n50["sim_timeout_count"], 1)
        self.assertEqual(n50["convergence_time_s_count"], 2)
        self.assertEqual(n50["convergence_time_s_mean"], 20.0)
        self.assertEqual(n50["convergence_time_s_median"], 20.0)
        self.assertEqual(n50["convergence_time_s_q1"], 15.0)
        self.assertEqual(n50["convergence_time_s_q3"], 25.0)
        n100 = summary[1]
        self.assertEqual(n100["wall_timeout_rate"], 1.0)
        self.assertEqual(n100["convergence_time_s_count"], 0)
        self.assertIsNone(n100["convergence_time_s_mean"])

    def test_summary_csv_keeps_empty_timeout_metrics_empty(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "summary.csv"
            write_summary([{"N": 500, "status": "wall_timeout"}], output)
            with output.open(encoding="utf-8", newline="") as source:
                row = next(csv.DictReader(source))
            self.assertEqual(row["wall_timeout_count"], "1")
            self.assertEqual(row["convergence_time_s_count"], "0")
            self.assertEqual(row["convergence_time_s_mean"], "")


class PublicationReportTests(unittest.TestCase):
    @staticmethod
    def _successful_rows() -> list[dict[str, object]]:
        rows: list[dict[str, object]] = []
        for n, times, totals in (
            (50, (10.0, 30.0), (20.0, 40.0)),
            (100, (20.0, 60.0), (60.0, 100.0)),
        ):
            for index, (time, total) in enumerate(zip(times, totals, strict=True)):
                rows.append(
                    {
                        "run_id": f"n{n}-i{index}",
                        "N": n,
                        "image_id": f"image-{index}",
                        "image_sha256": f"{index + 1:064x}",
                        "seed": 20_270_809 + index,
                        "status": "success",
                        "convergence_time_s": time,
                        "total_distance_m": total,
                        "mean_distance_per_robot_m": total / n,
                        "target_min_distance": 3.0,
                        "target_mst_bottleneck": 10.0,
                        "final_max_target_error": 0.5,
                    }
                )
        return rows

    def test_publication_summary_requires_a_complete_successful_design(self) -> None:
        rows = self._successful_rows()
        summaries = build_publication_summary(
            rows,
            expected_robot_counts=(50, 100),
            expected_per_size=2,
        )
        self.assertEqual([row["N"] for row in summaries], [50, 100])
        self.assertEqual(summaries[0]["success_count"], 2)
        self.assertEqual(summaries[0]["convergence_time_s_median"], 20.0)
        self.assertEqual(summaries[0]["convergence_time_s_q1"], 15.0)
        self.assertEqual(summaries[0]["convergence_time_s_q3"], 25.0)

        incomplete = rows[:-1]
        with self.assertRaisesRegex(PublicationReportError, "expected 2 runs"):
            build_publication_summary(
                incomplete,
                expected_robot_counts=(50, 100),
                expected_per_size=2,
            )

        failed = [dict(row) for row in rows]
        failed[0]["status"] = "sim_timeout"
        with self.assertRaisesRegex(PublicationReportError, "not successful"):
            build_publication_summary(
                failed,
                expected_robot_counts=(50, 100),
                expected_per_size=2,
            )

    def test_publication_summary_checks_metric_invariants(self) -> None:
        rows = self._successful_rows()
        rows[0]["mean_distance_per_robot_m"] = 999.0
        with self.assertRaisesRegex(PublicationReportError, "distance arithmetic"):
            build_publication_summary(
                rows,
                expected_robot_counts=(50, 100),
                expected_per_size=2,
            )

    def test_statistics_table_reports_median_iqr_and_success_counts(self) -> None:
        summaries = build_publication_summary(
            self._successful_rows(),
            expected_robot_counts=(50, 100),
            expected_per_size=2,
        )
        table = render_statistics_table(summaries)
        self.assertIn(r"\label{tab:fixed-density-results}", table)
        self.assertIn(r"50 & 2/2 & 20.0 [15.0, 25.0]", table)
        self.assertIn(r"100 & 2/2 & 40.0 [30.0, 50.0]", table)

    def test_report_writes_source_data_vector_figure_and_latex(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            artifacts = generate_publication_report(
                self._successful_rows(),
                output_dir=root / "publication",
                figure_dir=root / "fig",
                expected_robot_counts=(50, 100),
                expected_per_size=2,
            )
            for key in (
                "summary_csv",
                "statistics_table_tex",
                "results_text_tex",
                "convergence_time_svg",
                "convergence_time_pdf",
                "convergence_time_png",
                "total_distance_svg",
                "total_distance_pdf",
                "total_distance_png",
                "distance_per_robot_svg",
                "distance_per_robot_pdf",
                "distance_per_robot_png",
                "figure_qa",
            ):
                self.assertTrue(artifacts[key].is_file(), key)
            for key in (
                "convergence_time_svg",
                "total_distance_svg",
                "distance_per_robot_svg",
            ):
                self.assertIn("<text", artifacts[key].read_text(encoding="utf-8"))
            self.assertFalse((root / "fig" / "fixed_density_metrics.pdf").exists())
            self.assertIn("4/4", artifacts["results_text_tex"].read_text(encoding="utf-8"))


class SamplingTests(unittest.TestCase):
    def test_poisson_sampling_is_reproducible_and_safe(self) -> None:
        n = 100
        side = arena_side(n)
        first = generate_initial_poses(n, side, seed=1234)
        second = generate_initial_poses(n, side, seed=1234)
        different = generate_initial_poses(n, side, seed=1235)

        self.assertEqual(first, second)
        self.assertNotEqual(first, different)
        self.assertEqual(len(first), n)
        half = side / 2.0
        for x, y in first:
            self.assertGreaterEqual(x, -half + 1.0)
            self.assertLessEqual(x, half - 1.0)
            self.assertGreaterEqual(y, -half + 1.0)
            self.assertLessEqual(y, half - 1.0)
        minimum = min(
            math.hypot(ax - bx, ay - by)
            for index, (ax, ay) in enumerate(first)
            for bx, by in first[index + 1 :]
        )
        self.assertGreaterEqual(minimum, 4.0)

    def test_all_planned_sizes_have_enough_samples(self) -> None:
        for n in ROBOT_COUNTS:
            poses = generate_initial_poses(n, arena_side(n), seed=derive_seed(99, "initial_pose"))
            self.assertEqual(len(poses), n)


class ManifestTests(unittest.TestCase):
    def test_command_qualifier_consumes_json_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "qualifier"
            executable.write_text(
                "#!/bin/sh\n"
                "if [ \"$1\" = 50 ]; then\n"
                "  printf '{\"success\":true,\"target_min_distance\":1.0,\"mst_bottleneck\":2.0}\\n'\n"
                "else\n"
                "  printf '{\"success\":false,\"error\":\"failed\"}\\n'\n"
                "fi\n",
                encoding="utf-8",
            )
            executable.chmod(executable.stat().st_mode | stat.S_IXUSR)
            image = root / "image.png"
            image.write_bytes(b"png")
            qualifier = command_qualifier(executable)
            self.assertTrue(qualifier(image, 50, 50.0))
            self.assertFalse(qualifier(image, 100, arena_side(100)))

    def test_manifest_is_lexical_relative_hashed_and_seeded(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image_dir = root / "formation" / "img"
            image_dir.mkdir(parents=True)
            for name, data in (("z.png", b"z"), ("a.png", b"a"), ("m.png", b"m")):
                (image_dir / name).write_bytes(data)
            output = root / "manifest.csv"

            def qualifies(path: Path, n: int, side: float) -> bool:
                self.assertAlmostEqual(side, arena_side(n))
                return path.name != "m.png"

            freeze_manifest(
                image_dir=image_dir,
                output_path=output,
                repository_root=root,
                qualifier=qualifies,
                expected_count=2,
                version="test-v1",
            )
            rows = load_manifest(output, repository_root=root)

            self.assertEqual([row.image_id for row in rows], ["a", "z"])
            self.assertEqual([row.image_index for row in rows], [0, 1])
            self.assertEqual([row.seed for row in rows], [20_270_809, 20_270_810])
            self.assertEqual(rows[0].image_path, Path("formation/img/a.png"))
            self.assertEqual(rows[0].image_sha256, hashlib.sha256(b"a").hexdigest())

    def test_manifest_count_mismatch_stops_freeze(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image_dir = root / "img"
            image_dir.mkdir()
            (image_dir / "only.png").write_bytes(b"x")
            with self.assertRaisesRegex(RuntimeError, "expected exactly 2"):
                freeze_manifest(
                    image_dir=image_dir,
                    output_path=root / "manifest.csv",
                    repository_root=root,
                    qualifier=lambda _path, _n, _side: True,
                    expected_count=2,
                )

    def test_manifest_checks_every_image_at_every_size_without_short_circuiting(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image_dir = root / "img"
            image_dir.mkdir()
            (image_dir / "a.png").write_bytes(b"a")
            (image_dir / "b.png").write_bytes(b"b")

            calls: list[tuple[str, int]] = []

            def qualifier(path: Path, n: int, _side: float) -> bool:
                calls.append((path.name, n))
                return path.name == "a.png"

            freeze_manifest(
                image_dir=image_dir,
                output_path=root / "manifest.csv",
                repository_root=root,
                qualifier=qualifier,
                expected_count=1,
            )
            self.assertEqual(len(calls), 2 * len(ROBOT_COUNTS))

    def test_checked_in_manifest_contains_exactly_75_valid_hashes(self) -> None:
        code_root = EXPERIMENT_DIR.parents[3]
        path = EXPERIMENT_DIR / "manifests" / "formation_scaling_v1.csv"
        rows = load_manifest(path, repository_root=code_root)
        self.assertEqual(len(rows), 75)
        self.assertEqual([row.image_index for row in rows], list(range(75)))
        self.assertEqual([row.seed for row in rows], [20_270_809 + i for i in range(75)])
        for row in rows:
            image = code_root / row.image_path
            self.assertTrue(image.is_file(), image)
            self.assertEqual(hashlib.sha256(image.read_bytes()).hexdigest(), row.image_sha256)


class WorldTests(unittest.TestCase):
    def test_world_contains_batch_settings_and_poses(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "worlds" / "run.world"
            rendered = render_world(
                output_path=output,
                n=2,
                side=10.0,
                poses=((1.25, -2.5), (-3.0, 4.0)),
                formation_include=EXPERIMENT_DIR / "formation_world" / "formation.inc",
                map_include=EXPERIMENT_DIR / "formation_world" / "map.inc",
                empty_bitmap=EXPERIMENT_DIR / "formation_world" / "bitmaps" / "empty.png",
            )
            text = rendered.read_text(encoding="utf-8")
            self.assertIn("threads 1", text)
            self.assertIn("paused 0", text)
            self.assertIn("define flocker pioneer2dx_no_sonar", text)
            self.assertNotIn("define flocker pioneer2dx_front_sonar", text)
            self.assertIn("size [10.000000 10.000000 0.800]", text)
            self.assertEqual(text.count("flocker(pose"), 2)
            self.assertIn("flocker(pose [1.250000000 -2.500000000 0.0 0.0])", text)

    def test_qualitative_world_template_is_paused_and_separate(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "worlds" / "uts.world"
            rendered = render_world(
                output_path=output,
                n=2,
                side=250.0,
                world_side=300.0,
                poses=((10.0, -20.0), (-30.0, 40.0)),
                formation_include=EXPERIMENT_DIR / "formation_world" / "formation.inc",
                map_include=EXPERIMENT_DIR / "formation_world" / "map.inc",
                empty_bitmap=EXPERIMENT_DIR / "formation_world" / "bitmaps" / "empty.png",
                template_path=EXPERIMENT_DIR / "formation_qualitative.world.template",
            )
            text = rendered.read_text(encoding="utf-8")
            self.assertIn("qualitative formation world", text)
            self.assertIn("paused 1", text)
            self.assertIn("threads 1", text)
            self.assertIn("define flocker pioneer2dx_no_sonar", text)
            self.assertNotIn("define flocker pioneer2dx_front_sonar", text)
            self.assertIn("size [300.000000 300.000000 0.800]", text)
            self.assertEqual(text.count("flocker(pose"), 2)


class RunnerTests(unittest.TestCase):
    def _script(self, directory: Path, body: str) -> Path:
        path = directory / "fake-stage"
        path.write_text("#!/bin/sh\n" + body, encoding="utf-8")
        path.chmod(path.stat().st_mode | stat.S_IXUSR)
        return path

    def _spec(self, directory: Path, stage: Path) -> RunSpec:
        image = directory / "image.png"
        image.write_bytes(b"png")
        world = directory / "run.world"
        world.write_text("world", encoding="utf-8")
        return RunSpec(
            run_id="test-run",
            n=50,
            image_id="image",
            image_path=image,
            image_sha256=hashlib.sha256(b"png").hexdigest(),
            seed=20_270_809,
            arena_side=50.0,
            world_path=world,
            result_path=directory / "runs" / "run.json",
            stage_executable=stage,
        )

    def test_runner_invokes_normalized_stage_cli_and_preserves_controller_result(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            args_file = directory / "args.txt"
            stagepath_file = directory / "stagepath.txt"
            mode_file = directory / "mode.txt"
            controller_result = {
                "status": "success",
                "run_id": "test-run",
                "N": 50,
                "image_id": "image",
                "image_sha256": hashlib.sha256(b"png").hexdigest(),
                "seed": 20_270_809,
                "arena_side": 50.0,
            }
            stage = self._script(
                directory,
                f'printf "%s\\n" "$@" > "{args_file}"\n'
                f'printf "%s" "$STAGEPATH" > "{stagepath_file}"\n'
                f'printf "%s" "$FORMATION_EXPERIMENT_MODE" > "{mode_file}"\n'
                f"printf '%s' '{json.dumps(controller_result)}' > \"$FORMATION_RESULT_PATH\"\n",
            )
            spec = self._spec(directory, stage)
            result = execute_run(spec, wall_timeout_seconds=2.0)
            self.assertEqual(result["status"], "success")
            self.assertGreaterEqual(result["wall_time_s"], 0.0)
            on_disk = json.loads(spec.result_path.read_text(encoding="utf-8"))
            self.assertEqual(on_disk["wall_time_s"], result["wall_time_s"])
            log_path = directory / "logs" / "run.log"
            self.assertTrue(log_path.is_file())
            self.assertIn("fake-stage -g 50", log_path.read_text(encoding="utf-8"))
            self.assertEqual(mode_file.read_text(encoding="utf-8"), "fixed_density")
            self.assertEqual(
                args_file.read_text(encoding="utf-8").splitlines(),
                ["-g", "50", str(spec.image_path), str(spec.world_path)],
            )
            self.assertEqual(os.environ.get("FORMATION_RESULT_PATH"), None)
            stage_paths = stagepath_file.read_text(encoding="utf-8").split(os.pathsep)
            self.assertIn(str(EXPERIMENT_DIR.parents[1] / "libstage"), stage_paths)

            aggregate = directory / "aggregate.csv"
            aggregate_results(spec.result_path.parent, aggregate)
            with aggregate.open(encoding="utf-8", newline="") as source:
                row = next(csv.DictReader(source))
            self.assertEqual(float(row["wall_time_s"]), result["wall_time_s"])

    def test_runner_writes_wall_timeout_result(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            stage = self._script(directory, "sleep 1\n")
            spec = self._spec(directory, stage)
            result = execute_run(spec, wall_timeout_seconds=0.05)
            self.assertEqual(result["status"], "wall_timeout")
            on_disk = json.loads(spec.result_path.read_text(encoding="utf-8"))
            self.assertEqual(on_disk["status"], "wall_timeout")
            self.assertEqual(on_disk["N"], 50)
            self.assertIn("final_max_target_error", on_disk)
            self.assertNotIn("max_target_error", on_disk)

    def test_runner_retries_wall_timeout_without_outer_timeout_and_archives_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            timeout_file = directory / "timeout-value.txt"
            controller_result = {
                "status": "success",
                "run_id": "test-run",
                "N": 50,
                "image_id": "image",
                "image_sha256": hashlib.sha256(b"png").hexdigest(),
                "seed": 20_270_809,
                "arena_side": 50.0,
            }
            stage = self._script(
                directory,
                f'printf "%s" "$FORMATION_SIM_TIMEOUT" > "{timeout_file}"\n'
                f"printf '%s' '{json.dumps(controller_result)}' > \"$FORMATION_RESULT_PATH\"\n",
            )
            spec = self._spec(directory, stage)
            spec.result_path.parent.mkdir(parents=True)
            previous = dict(controller_result)
            previous.update({"status": "wall_timeout", "wall_time_s": 1200.0})
            spec.result_path.write_text(json.dumps(previous), encoding="utf-8")
            spec.log_path.parent.mkdir(parents=True)
            spec.log_path.write_text("original timeout log\n", encoding="utf-8")

            result = execute_run(
                spec,
                wall_timeout_seconds=None,
                retry_statuses=frozenset({"wall_timeout"}),
            )

            self.assertEqual(result["status"], "success")
            self.assertEqual(timeout_file.read_text(encoding="utf-8"), "7200")
            archive = directory / "retry_archive" / "test-run" / "attempt-001"
            archived_result = json.loads((archive / "result.json").read_text(encoding="utf-8"))
            self.assertEqual(archived_result["status"], "wall_timeout")
            self.assertEqual(
                (archive / "stage.log").read_text(encoding="utf-8"),
                "original timeout log\n",
            )

    def test_retry_selection_includes_only_requested_status_and_interrupted_retry(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            stage = self._script(directory, "exit 0\n")
            cases = [directory / name for name in ("timeout", "success", "missing")]
            for case in cases:
                case.mkdir()
            timeout_spec, success_spec, missing_spec = (
                self._spec(case, stage) for case in cases
            )
            for spec, status in ((timeout_spec, "wall_timeout"), (success_spec, "success")):
                spec.result_path.parent.mkdir(parents=True)
                payload = {
                    "status": status,
                    "run_id": spec.run_id,
                    "N": spec.n,
                    "image_id": spec.image_id,
                    "image_sha256": spec.image_sha256,
                    "seed": spec.seed,
                    "arena_side": spec.arena_side,
                }
                spec.result_path.write_text(json.dumps(payload), encoding="utf-8")
            history = (
                missing_spec.result_path.parent.parent
                / "retry_archive"
                / missing_spec.run_id
                / "attempt-001"
            )
            history.mkdir(parents=True)
            archived = {
                "status": "wall_timeout",
                "run_id": missing_spec.run_id,
                "N": missing_spec.n,
                "image_id": missing_spec.image_id,
                "image_sha256": missing_spec.image_sha256,
                "seed": missing_spec.seed,
                "arena_side": missing_spec.arena_side,
            }
            (history / "result.json").write_text(json.dumps(archived), encoding="utf-8")

            selected = select_retry_specs(
                (timeout_spec, success_spec, missing_spec),
                frozenset({"wall_timeout"}),
            )

            self.assertEqual(selected, (timeout_spec, missing_spec))

    def test_runner_does_not_resume_a_result_from_another_spec(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            stage = self._script(
                directory,
                'printf \'{"status":"success","run_id":"test-run","N":50,'
                '"image_id":"image","image_sha256":"'
                + hashlib.sha256(b"png").hexdigest()
                + '","seed":20270809,"arena_side":50.0}\' > "$FORMATION_RESULT_PATH"\n',
            )
            spec = self._spec(directory, stage)
            spec.result_path.parent.mkdir(parents=True)
            spec.result_path.write_text(
                json.dumps({"status": "success", "run_id": "stale-run"}),
                encoding="utf-8",
            )

            result = execute_run(spec, wall_timeout_seconds=2.0)

            self.assertEqual(result["run_id"], "test-run")
            self.assertEqual(result["N"], 50)

    def test_runner_retries_a_previous_user_interruption(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            controller_result = {
                "status": "success",
                "run_id": "test-run",
                "N": 50,
                "image_id": "image",
                "image_sha256": hashlib.sha256(b"png").hexdigest(),
                "seed": 20_270_809,
                "arena_side": 50.0,
            }
            stage = self._script(
                directory,
                f"printf '%s' '{json.dumps(controller_result)}' > \"$FORMATION_RESULT_PATH\"\n",
            )
            spec = self._spec(directory, stage)
            spec.result_path.parent.mkdir(parents=True)
            interrupted = dict(controller_result)
            interrupted.update({"status": "runtime_error", "returncode": -2})
            spec.result_path.write_text(json.dumps(interrupted), encoding="utf-8")

            result = execute_run(spec, wall_timeout_seconds=2.0)

            self.assertEqual(result["status"], "success")

    def test_timeout_does_not_accept_a_stale_controller_result(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            stage = self._script(
                directory,
                "printf '{\"status\":\"success\",\"run_id\":\"stale-run\"}' "
                '> "$FORMATION_RESULT_PATH"\n'
                "sleep 1\n",
            )
            spec = self._spec(directory, stage)

            result = execute_run(spec, wall_timeout_seconds=0.05)

            self.assertEqual(result["status"], "wall_timeout")
            self.assertEqual(result["run_id"], "test-run")
            self.assertEqual(result["image_sha256"], spec.image_sha256)

    def test_runner_writes_runtime_error_when_stage_cannot_start(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            not_executable = directory / "stage"
            not_executable.write_text("not executable", encoding="utf-8")
            spec = self._spec(directory, not_executable)
            result = execute_run(spec, wall_timeout_seconds=1.0)
            self.assertEqual(result["status"], "runtime_error")
            self.assertIn("failed to start Stage", result["error"])


class AutomationScriptTests(unittest.TestCase):
    def test_ubuntu_rebuild_builds_every_install_target(self) -> None:
        script = EXPERIMENT_DIR.parents[3] / "stage4/rebuild-formation-ubuntu.sh"
        contents = script.read_text(encoding="utf-8")
        build_and_test = contents[
            contents.index("cmake --build"):contents.index("cmake --install")
        ]

        self.assertNotIn("--target", build_and_test)
        self.assertIn(
            'cmake --build "$BUILD_DIR" -j"$BUILD_JOBS"', build_and_test
        )
        self.assertIn(
            'ctest --test-dir "$BUILD_DIR" --output-on-failure', build_and_test
        )

    def test_run_command_uses_fixed_results_directory_and_generates_summary(self) -> None:
        script = EXPERIMENT_DIR.parents[2] / "run-formation-experiments.sh"
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            log = directory / "uv-args.txt"
            fake_uv = directory / "uv"
            fake_uv.write_text(
                "#!/bin/sh\n"
                "printf '%s\\n' \"$*\" >> \"$FORMATION_TEST_LOG\"\n",
                encoding="utf-8",
            )
            fake_uv.chmod(fake_uv.stat().st_mode | stat.S_IXUSR)
            fake_stage = directory / "stage"
            fake_stage.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            fake_stage.chmod(fake_stage.stat().st_mode | stat.S_IXUSR)
            results = directory / "results"
            environment = os.environ.copy()
            environment.update(
                {
                    "FORMATION_TEST_LOG": str(log),
                    "FORMATION_UV_BIN": str(fake_uv),
                    "FORMATION_STAGE_BIN": str(fake_stage),
                    "FORMATION_RESULT_ROOT": str(results),
                    "FORMATION_WORKERS": "3",
                    "FORMATION_WALL_TIMEOUT": "42",
                    "FORMATION_UV_CACHE_DIR": str(directory / "uv-cache"),
                }
            )

            completed = subprocess.run(
                [str(script), "run", "--only-n", "50"],
                check=False,
                capture_output=True,
                text=True,
                env=environment,
            )

            self.assertEqual(completed.returncode, 0, completed.stderr)
            commands = log.read_text(encoding="utf-8").splitlines()
            runner = EXPERIMENT_DIR / "run_fixed_density.py"
            self.assertEqual(
                commands,
                [
                    f"run python {runner} run --stage {fake_stage} "
                    f"--output-root {results} --workers 3 --wall-timeout 42 --only-n 50",
                    f"run python {runner} analyze --input {results} "
                    f"--output {results / 'summary_by_n.csv'}",
                ],
            )

    def test_retry_wall_timeouts_uses_one_worker_and_disables_outer_timeout(self) -> None:
        script = EXPERIMENT_DIR.parents[2] / "run-formation-experiments.sh"
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            log = directory / "uv-args.txt"
            fake_uv = directory / "uv"
            fake_uv.write_text(
                "#!/bin/sh\n"
                "printf '%s\\n' \"$*\" >> \"$FORMATION_TEST_LOG\"\n",
                encoding="utf-8",
            )
            fake_uv.chmod(fake_uv.stat().st_mode | stat.S_IXUSR)
            fake_stage = directory / "stage"
            fake_stage.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            fake_stage.chmod(fake_stage.stat().st_mode | stat.S_IXUSR)
            results = directory / "results"
            (results / "runs").mkdir(parents=True)
            environment = os.environ.copy()
            environment.update(
                {
                    "FORMATION_TEST_LOG": str(log),
                    "FORMATION_UV_BIN": str(fake_uv),
                    "FORMATION_STAGE_BIN": str(fake_stage),
                    "FORMATION_RESULT_ROOT": str(results),
                    "FORMATION_UV_CACHE_DIR": str(directory / "uv-cache"),
                }
            )

            completed = subprocess.run(
                [str(script), "retry-wall-timeouts"],
                check=False,
                capture_output=True,
                text=True,
                env=environment,
            )

            self.assertEqual(completed.returncode, 0, completed.stderr)
            commands = log.read_text(encoding="utf-8").splitlines()
            runner = EXPERIMENT_DIR / "run_fixed_density.py"
            self.assertEqual(
                commands,
                [
                    f"run python {runner} run --stage {fake_stage} "
                    f"--output-root {results} --workers 1 --no-wall-timeout "
                    "--retry-status wall_timeout",
                    f"run python {runner} analyze --input {results} "
                    f"--output {results / 'summary_by_n.csv'}",
                ],
            )

    def test_report_command_generates_publication_artifacts(self) -> None:
        script = EXPERIMENT_DIR.parents[2] / "run-formation-experiments.sh"
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            log = directory / "uv-args.txt"
            fake_uv = directory / "uv"
            fake_uv.write_text(
                "#!/bin/sh\n"
                "printf '%s\\n' \"$*\" >> \"$FORMATION_TEST_LOG\"\n",
                encoding="utf-8",
            )
            fake_uv.chmod(fake_uv.stat().st_mode | stat.S_IXUSR)
            results = directory / "results"
            environment = os.environ.copy()
            environment.update(
                {
                    "FORMATION_TEST_LOG": str(log),
                    "FORMATION_UV_BIN": str(fake_uv),
                    "FORMATION_RESULT_ROOT": str(results),
                    "FORMATION_UV_CACHE_DIR": str(directory / "uv-cache"),
                }
            )

            completed = subprocess.run(
                [str(script), "report"],
                check=False,
                capture_output=True,
                text=True,
                env=environment,
            )

            self.assertEqual(completed.returncode, 0, completed.stderr)
            runner = EXPERIMENT_DIR / "run_fixed_density.py"
            self.assertEqual(
                log.read_text(encoding="utf-8").strip(),
                f"run python {runner} report --input {results} "
                f"--output-dir {results / 'publication'}",
            )


if __name__ == "__main__":
    unittest.main()
