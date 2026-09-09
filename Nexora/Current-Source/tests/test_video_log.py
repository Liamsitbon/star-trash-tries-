import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location(
    "video_log", Path(__file__).resolve().parents[1] / "scripts/check_video_log.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class VideoLogTests(unittest.TestCase):
    def test_decoder_callback_is_not_pixel_evidence(self):
        report = MODULE.summarize("Nexora decoded frame 39 revealed dome 'world'")
        self.assertEqual(report["probe_status"], "no_pixel_evidence")
        self.assertFalse(report["runtime_fix_proven"])

    def test_white_samples_and_valid_bindings(self):
        report = MODULE.summarize(
            "Nexora video diagnostic binding dome='world' target=-1 playerTarget=-1 "
            "materialTexture=-1 material=3 rendererMaterial=3\n"
            "Nexora video diagnostic pixels dome='world' samples=320 "
            "minRGB=(255,255,255) maxRGB=(255,255,255)"
        )["reports"][0]
        self.assertTrue(report["bindings_consistent"])
        self.assertIn("near_white_samples", report["observation"])

    def test_absent_bindings_are_not_consistent(self):
        report = MODULE.summarize(
            "Nexora video diagnostic pixels dome='world' samples=320 "
            "minRGB=(0,16,32) maxRGB=(140,150,160)"
        )["reports"][0]
        self.assertFalse(report["bindings_consistent"])
        self.assertIn("varied_samples", report["observation"])

    def test_other_dome_binding_is_not_reused(self):
        report = MODULE.summarize(
            "Nexora video diagnostic binding dome='other' target=1 playerTarget=1 "
            "materialTexture=1 material=3 rendererMaterial=3\n"
            "Nexora video diagnostic pixels dome='world' samples=320 "
            "minRGB=(0,0,0) maxRGB=(0,0,0)"
        )["reports"][0]
        self.assertFalse(report["bindings_consistent"])
        self.assertIn("near_black_samples", report["observation"])


if __name__ == "__main__":
    unittest.main()
