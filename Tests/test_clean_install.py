"""Run against a built app via DUALDECK_TEST_EXE, outside any project tree."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


@unittest.skipUnless(os.environ.get("DUALDECK_TEST_EXE"), "set DUALDECK_TEST_EXE to a built candidate")
class CleanInstallTests(unittest.TestCase):
    def test_packaged_app_does_not_inherit_data_from_a_parent_checkout(self):
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary)
            (parent / "Source").mkdir()
            (parent / "CMakeLists.txt").write_text("project marker")
            (parent / "data").mkdir()
            library = parent / "data/library.json"
            library.write_bytes(b"private unrelated broken library")
            portable = parent / "portable"
            portable.mkdir()
            (portable / "BUILD-INFO.json").write_text('{}')
            binary = portable / "DualDeck.exe"
            shutil.copyfile(os.environ["DUALDECK_TEST_EXE"], binary)
            report = portable / "reports/performance.txt"
            process = subprocess.run([str(binary), "--performance-test", str(report)], timeout=60)
            self.assertEqual(process.returncode, 0, report.read_text(encoding="utf-8-sig"))
            self.assertEqual(library.read_bytes(), b"private unrelated broken library")
            self.assertFalse((portable / "data/library.json").exists())

    def test_performance_probe_works_without_personal_library(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            binary = root / "DualDeck.exe"
            shutil.copyfile(os.environ["DUALDECK_TEST_EXE"], binary)
            report = root / "reports/performance.txt"
            process = subprocess.run([str(binary), "--performance-test", str(report)], timeout=60)
            text = report.read_text(encoding="utf-8-sig")
            self.assertEqual(process.returncode, 0, text)
            self.assertIn("Failures: 0", text)
            self.assertIn("50,000 pads", text)
            self.assertFalse((root / "data/library.json").exists())


if __name__ == "__main__":
    unittest.main()
