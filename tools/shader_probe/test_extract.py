import unittest
from extract_gles import stage_blocks

class ExtractTests(unittest.TestCase):
    def test_nested_preprocessor_and_version_first(self):
        src = "#ifdef VERTEX\n#version 300 es\n#if FOO\nA\n#else\nB\n#endif\n#endif\n#ifdef FRAGMENT\n#version 300 es\nC\n#endif\n"
        result = stage_blocks(src)
        self.assertEqual(result["VERTEX"], "#version 300 es\n#if FOO\nA\n#else\nB\n#endif\n")
        self.assertEqual(result["FRAGMENT"], "#version 300 es\nC\n")

    def test_no_synthetic_stage_or_shader_rewriting(self):
        for src in ("binary blob", "#ifdef VERTEX\n#version 300 es\n#endif", "#ifdef VERTEX\n#version 300 es\n", "#ifdef VERTEX\nvoid main(){}\n#endif"):
            with self.assertRaises(ValueError):
                stage_blocks(src)

    def test_geometry_retained(self):
        src = "".join(f"#ifdef {stage}\n#version 310 es\nvoid main(){{}}\n#endif\n" for stage in ("VERTEX", "FRAGMENT", "GEOMETRY"))
        self.assertEqual(set(stage_blocks(src)), {"VERTEX", "FRAGMENT", "GEOMETRY"})

if __name__ == "__main__":
    unittest.main()
