"""New loaded/loaded comparison guards; the shared legacy guards have separate tests."""
import copy
import importlib.util
import sys
from pathlib import Path
from unittest import TestCase, main
from unittest.mock import patch

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / "tools"))
spec = importlib.util.spec_from_file_location("renderer_comparison", root / "tools/compare-renderers.py")
comparison = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparison)


class ComparisonGuards(TestCase):
    def fixture(self):
        row = dict(raw_frame_ms=8, frame_ms=8, update_ms=2, visibility_ms=1)
        coverage = dict(observed_frames=16, excluded_frames=0, renderer_inactive_frames=0, gpu_pending=0)
        runs = []
        for i, variant in enumerate(("a", "b", "b", "a")):
            provenance = dict(renderer_variant=variant, production_sha256=variant * 64, started_utc=f"2026-11-09T09:0{i}:00Z",
                              native=False, low_detail_requested=False, disable_shake_requested=True, level=129598490,
                              target_from_percent=74, target_to_percent=75, coverage=coverage.copy())
            runs.append(([dict(event="comparison_variant", production_enabled=True)], [row] * 16, {74: [row] * 16}, provenance))
        return copy.deepcopy(runs)

    def run_comparison(self, runs):
        def validate(a, ar, ap, b, br, bp):
            return 120, [ap["coverage"], bp["coverage"]]
        with patch.object(comparison.pair, "read", side_effect=runs) as read, patch.object(comparison.pair, "validate_conditions", side_effect=validate):
            result = comparison.compare([Path(f"run-{i}") for i in range(4)])
            self.assertTrue(all(call.args[1] is True for call in read.call_args_list))
            return result

    def test_balanced_loaded_loaded_runs_remain_non_release_evidence(self):
        result = self.run_comparison(self.fixture())
        self.assertFalse(result["normal_activation_proven"])
        self.assertEqual(result["sequence"], ["a", "b", "b", "a"])

    def test_rejects_changed_packages(self):
        runs = self.fixture(); runs[2][3]["production_sha256"] = "c" * 64
        with self.assertRaisesRegex(ValueError, "changed"):
            self.run_comparison(runs)

    def test_rejects_incomplete_or_disturbed_coverage(self):
        for key, value in (("excluded_frames", 2), ("observed_frames", 0), ("renderer_inactive_frames", 1), ("gpu_pending", 1)):
            with self.subTest(key=key):
                runs = self.fixture(); runs[2][3]["coverage"][key] = value
                with self.assertRaisesRegex(ValueError, "coverage"):
                    self.run_comparison(runs)

    def test_rejects_partial_bins_and_missing_activation(self):
        runs = self.fixture(); runs[2][2].clear()
        with self.assertRaisesRegex(ValueError, "bins"):
            self.run_comparison(runs)
        runs = self.fixture(); runs[2][0][0]["production_enabled"] = False
        with self.assertRaisesRegex(ValueError, "activation"):
            self.run_comparison(runs)

    def test_rejects_deaths_and_duplicate_runs(self):
        runs = self.fixture(); runs[2][0].append(dict(event="death"))
        with self.assertRaisesRegex(ValueError, "Deaths"):
            self.run_comparison(runs)
        with self.assertRaisesRegex(ValueError, "distinct"):
            comparison.compare([Path("same")] * 4)


if __name__ == "__main__":
    main()
