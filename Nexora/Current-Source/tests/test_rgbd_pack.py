import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location("pack",Path(__file__).parents[1]/"scripts/pack_rgbd.py")
pack=importlib.util.module_from_spec(spec);spec.loader.exec_module(pack)
class TestPair(unittest.TestCase):
    def setUp(self):
        self.rgb=dict(width=3840,height=1920,fps=60.,frames=600,duration=10.)
        self.depth=dict(width=960,height=480,fps=60.,frames=600,duration=10.)
    def test_different_depth_resolution_is_valid(self): pack.validate_pair(self.rgb,self.depth)
    def test_fps_mismatch(self):
        self.depth['fps']=30
        with self.assertRaisesRegex(ValueError,'FPS'): pack.validate_pair(self.rgb,self.depth)
    def test_frames_mismatch(self):
        self.depth['frames']=599
        with self.assertRaisesRegex(ValueError,'frame count'): pack.validate_pair(self.rgb,self.depth)
    def test_bad_projection(self):
        self.depth['width']=480
        with self.assertRaisesRegex(ValueError,'2:1'): pack.validate_pair(self.rgb,self.depth)
if __name__=='__main__':unittest.main()
