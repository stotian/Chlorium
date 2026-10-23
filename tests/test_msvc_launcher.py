import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("launcher", Path(__file__).parents[1] / "tools/msvc_launcher.py")
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class IncludeTrackingTests(unittest.TestCase):
    def test_utf8_nonbreaking_space(self):
        source = "Remarque\u00a0: inclusion du fichier\u00a0:  C:\\src\\Renderer.hpp\r\n".encode()
        self.assertEqual(launcher.normalize(source), b"Note: including file: C:\\src\\Renderer.hpp\r\n")

    def test_cp1252_output(self):
        source = "Remarque\u00a0: inclusion du fichier\u00a0: C:\\src\\Buffer.hpp\n".encode("cp1252")
        self.assertEqual(launcher.normalize(source), b"Note: including file: C:\\src\\Buffer.hpp\n")

    def test_diagnostics_and_english_are_unchanged(self):
        for line in (b"source.cpp(20): error C2446: message\r\n", b"Note: including file: C:\\header.hpp\n", b"Remarque: unrelated warning\n"):
            self.assertEqual(launcher.normalize(line), line)

    def test_nested_include_indentation(self):
        self.assertEqual(launcher.normalize(b"Remarque : inclusion du fichier :       C:\\nested.hpp\n"), b"Note: including file: C:\\nested.hpp\n")


if __name__ == "__main__":
    unittest.main()
