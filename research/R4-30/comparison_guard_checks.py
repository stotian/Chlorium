"""Regression cases for diagnostic exclusion and the actual 120 FPS budget."""
import importlib.util
import sys
from pathlib import Path
from unittest import TestCase, main
from unittest.mock import patch

path = Path(__file__).resolve().parents[2] / "tools/compare-auto-runs.py"
sys.path.insert(0, str(path.parent))
spec = importlib.util.spec_from_file_location("chlorium_comparison", path)
comparison = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparison)


class DiagnosticGuard(TestCase):
    def test_field_watch_cannot_be_mislabeled_as_fps_evidence(self):
        for left,right in ((True,False),(False,True),(True,True)):
            rows=[([],[],{},{"native_field_watch_diagnostic":left}),([],[],{},{"native_field_watch_diagnostic":right})]
            with patch.object(comparison,"read",side_effect=rows),patch.object(Path,"write_text")as output:
                with self.assertRaisesRegex(ValueError,"Native field watch"):
                    comparison.compare(Path("old_chlorium"),Path("new_chlorium"),Path("unused"))
                output.assert_not_called()
    def test_group_trace_cannot_be_mislabeled_as_fps_evidence(self):
        for left,right in ((True,False),(False,True),(True,True)):
            rows=[([],[],{},{"group_state_trace_diagnostic":left}),([],[],{},{"group_state_trace_diagnostic":right})]
            with patch.object(comparison,"read",side_effect=rows),patch.object(Path,"write_text")as output:
                with self.assertRaisesRegex(ValueError,"group trace"):
                    comparison.compare(Path("old_chlorium"),Path("new_chlorium"),Path("unused"))
                output.assert_not_called()
    def test_producer_capture_cannot_be_mislabeled_as_fps_evidence(self):
        for left,right in ((True,False),(False,True),(True,True)):
            rows=[([],[],{},{"native_producer_capture_diagnostic":left}),([],[],{},{"native_producer_capture_diagnostic":right})]
            with patch.object(comparison,"read",side_effect=rows),patch.object(Path,"write_text")as output:
                with self.assertRaisesRegex(ValueError,"Native producer"):
                    comparison.compare(Path("native"),Path("candidate"),Path("unused"))
                output.assert_not_called()
    def test_resident_replay_cannot_be_mislabeled_as_fps_evidence(self):
        for key in ("resident_native_replay_diagnostic", "resident_native_cache_diagnostic", "resident_native_stream_diagnostic", "resident_exact_gpu_updates_diagnostic"):
            for left,right in ((True,False),(False,True),(True,True)):
                rows=[([],[],{},{key:left}),([],[],{},{key:right})]
                with patch.object(comparison,"read",side_effect=rows),patch.object(Path,"write_text")as output:
                    with self.assertRaisesRegex(ValueError,"Resident renderer replay"):
                        comparison.compare(Path("native"),Path("candidate"),Path("unused"))
                    output.assert_not_called()
    def test_mismatched_level_identity_rejects_before_output(self):
        for left,right in ((False,True),(True,False)):
            rows=[([],[],{},{"research_source_level_id":left}),([],[],{},{"research_source_level_id":right})]
            with patch.object(comparison,"read",side_effect=rows),patch.object(Path,"write_text")as output:
                with self.assertRaisesRegex(ValueError,"source level identity"):
                    comparison.compare(Path("native"),Path("candidate"),Path("unused"))
                output.assert_not_called()

    def test_budget_uses_unadjusted_presentation_time(self):
        rows = [dict(frame_ms="7", raw_frame_ms="9", update_ms="1", visibility_ms="1")]
        self.assertEqual(comparison.summarize(rows, 120)["slow_frames"], 1)
        self.assertEqual(comparison.summarize(rows, 60)["slow_frames"], 0)

    def test_unverified_or_mismatched_120_fps_rejects_early(self):
        for left, right in ((120, 60), (120, 120)):
            with self.subTest(left=left, right=right):
                records = [([], [], {}, {"target_fps": left}), ([], [], {}, {"target_fps": right})]
                with patch.object(comparison, "read", side_effect=records), patch.object(Path, "write_text") as output:
                    with self.assertRaisesRegex(ValueError, "FPS"):
                        comparison.compare(Path("native"), Path("candidate"), Path("unused"))
                    output.assert_not_called()

    def test_visual_readbacks_cannot_count_as_fps_evidence(self):
        records = [([], [], {}, {"visual_diagnostic": True}), ([], [], {}, {})]
        with patch.object(comparison, "read", side_effect=records):
            with self.assertRaisesRegex(ValueError, "Visual readback"):
                comparison.compare(Path("native"), Path("candidate"), Path("unused"))

    def test_false_120_fps_receipt_rejects(self):
        for interval, verified, swap in ((1/60, True, 0), (1/120, False, 0), (1/120, True, 1)):
            pacing = {"event": "frame_pacing", "target_fps": 120, "interval_seconds": interval,
                      "verified": verified, "swap_interval": swap}
            records = [([pacing], [], {}, {"target_fps": 120})] * 2
            with self.subTest(interval=interval, verified=verified, swap=swap), patch.object(comparison, "read", side_effect=records):
                with self.assertRaisesRegex(ValueError, "pacing"):
                    comparison.compare(Path("native"), Path("candidate"), Path("unused"))

    def test_either_native_capture_side_rejects_before_writing(self):
        for left, right in ((True, False), (False, True), (True, True)):
            rows = [([], [], {}, {"native_pass_capture_diagnostic": left}), ([], [], {}, {"native_pass_capture_diagnostic": right})]
            with self.subTest(left=left, right=right), patch.object(comparison, "read", side_effect=rows), patch.object(Path, "write_text") as output:
                with self.assertRaisesRegex(ValueError, "Native pass capture"):
                    comparison.compare(Path("native"), Path("candidate"), Path("unused"))
                output.assert_not_called()

    def test_either_shadow_side_rejects_before_writing_a_comparison(self):
        for native, candidate in ((True, False), (False, True), (True, True)):
            with self.subTest(native=native, candidate=candidate):
                rows = [([], [], {}, {"opacity_shadow_diagnostic": native}),
                        ([], [], {}, {"opacity_shadow_diagnostic": candidate})]
                with patch.object(comparison, "read", side_effect=rows), patch.object(Path, "write_text") as output:
                    with self.assertRaisesRegex(ValueError, "Opacity shadow diagnostics"):
                        comparison.compare(Path("native"), Path("candidate"), Path("unused"))
                    output.assert_not_called()


if __name__ == "__main__":
    main()
