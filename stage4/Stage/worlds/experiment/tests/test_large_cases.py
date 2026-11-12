from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from stage4.Stage.worlds.experiment.large_cases import (
    default_qualitative_side,
    parse_case,
    prepare_case,
    run_case,
    stage_command,
)


EXPERIMENT_DIR = Path(__file__).resolve().parents[1]


class LargeCaseTests(unittest.TestCase):
    def test_default_side_preserves_existing_500_robot_density(self) -> None:
        self.assertAlmostEqual(default_qualitative_side(500), 250.0)
        self.assertAlmostEqual(default_qualitative_side(1000), 250.0 * 2.0**0.5)

    def test_case_parser_accepts_default_and_explicit_side(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image = root / "uts.png"
            image.write_bytes(b"image")
            default = parse_case("500:uts.png", code_root=root, output_root=root / "out", index=0)
            explicit = parse_case(
                "1000:uts.png:400", code_root=root, output_root=root / "out", index=1
            )
            self.assertEqual(default.n, 500)
            self.assertAlmostEqual(default.arena_side, 250.0)
            self.assertAlmostEqual(default.world_side, 300.0)
            self.assertEqual(explicit.n, 1000)
            self.assertAlmostEqual(explicit.arena_side, 400.0)
            self.assertAlmostEqual(explicit.world_side, 480.0)
            self.assertNotEqual(default.seed, explicit.seed)

    def test_prepare_case_generates_body_only_world(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image = root / "uts.png"
            image.write_bytes(b"image")
            case = parse_case("10:uts.png:50", code_root=root, output_root=root / "out", index=0)
            prepare_case(
                case,
                formation_include=EXPERIMENT_DIR / "formation_world" / "formation.inc",
                map_include=EXPERIMENT_DIR / "formation_world" / "map.inc",
                empty_bitmap=EXPERIMENT_DIR / "formation_world" / "bitmaps" / "empty.png",
                template_path=EXPERIMENT_DIR / "formation_qualitative.world.template",
            )
            text = case.world_path.read_text(encoding="utf-8")
            self.assertEqual(text.count("flocker(pose"), 10)
            self.assertIn("define flocker pioneer2dx_no_sonar", text)
            self.assertIn("size [60.000000 60.000000 0.800]", text)

    def test_headless_stage_command_disables_gui(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image = root / "uts.png"
            image.write_bytes(b"image")
            case = parse_case("500:uts.png", code_root=root, output_root=root / "out", index=0)
            command = stage_command(case, root / "stage", gui=False)
            self.assertEqual(command[1], "-g")
            self.assertEqual(command[2], "500")
            self.assertNotIn("-g", stage_command(case, root / "stage", gui=True))

    def test_invalid_case_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(ValueError):
                parse_case("500", code_root=root, output_root=root / "out", index=0)
            with self.assertRaises(ValueError):
                parse_case("0:uts.png", code_root=root, output_root=root / "out", index=0)

    def test_forced_failed_run_does_not_reuse_stale_success(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image = root / "uts.png"
            image.write_bytes(b"image")
            case = parse_case("10:uts.png:50", code_root=root, output_root=root / "out", index=0)
            case.world_path.parent.mkdir(parents=True)
            case.world_path.write_text("world", encoding="utf-8")
            case.result_path.parent.mkdir(parents=True)
            case.result_path.write_text(
                json.dumps(
                    {
                        "run_id": case.run_id,
                        "N": case.n,
                        "image_id": case.image_id,
                        "image_sha256": case.image_sha256,
                        "seed": case.seed,
                        "arena_side": case.arena_side,
                        "status": "success",
                    }
                ),
                encoding="utf-8",
            )
            stage = root / "stage"
            stage.write_text("#!/bin/sh\nexit 1\n", encoding="utf-8")
            stage.chmod(0o755)

            result = run_case(
                case,
                stage,
                gui=False,
                sim_timeout=1.0,
                save_generated_image=False,
                force=True,
            )
            self.assertEqual(result["status"], "runtime_error")
            archived = list((case.result_path.parent.parent / "retry_archive").rglob("result.json"))
            self.assertEqual(len(archived), 1)

    def test_changed_case_does_not_resume_stale_result(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image = root / "uts.png"
            image.write_bytes(b"new-image")
            case = parse_case("10:uts.png:50", code_root=root, output_root=root / "out", index=0)
            case.world_path.parent.mkdir(parents=True)
            case.world_path.write_text("world", encoding="utf-8")
            case.result_path.parent.mkdir(parents=True)
            case.result_path.write_text(
                json.dumps(
                    {
                        "run_id": case.run_id,
                        "N": case.n,
                        "image_id": case.image_id,
                        "image_sha256": "stale-hash",
                        "seed": case.seed,
                        "arena_side": 40.0,
                        "status": "success",
                    }
                ),
                encoding="utf-8",
            )
            stage = root / "stage"
            stage.write_text("#!/bin/sh\nexit 1\n", encoding="utf-8")
            stage.chmod(0o755)

            result = run_case(
                case,
                stage,
                gui=False,
                sim_timeout=1.0,
                save_generated_image=False,
                force=False,
            )
            self.assertEqual(result["status"], "runtime_error")


if __name__ == "__main__":
    unittest.main()
