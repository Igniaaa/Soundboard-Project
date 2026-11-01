"""Integration checks for distributable artifacts, using only the stdlib."""
import hashlib
import importlib.util
import json
import pathlib
import tempfile
import unittest
import zipfile
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[1]


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location("release", ROOT / "scripts/release.py")
        self.assertTrue(spec.origin and pathlib.Path(spec.origin).is_file(),
                        "release packaging implementation is missing")
        self.release = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.release)
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = pathlib.Path(self.tmp.name) / "project"
        self.root.mkdir()
        for name in self.release.ROOT_FILES:
            (self.root / name).write_text("fixture\n", encoding="utf-8")
        (self.root / "VERSION").write_text("1.0.0-rc1\n")
        for name in self.release.SOURCE_DIRS:
            folder = self.root / name
            folder.mkdir(parents=True, exist_ok=True)
            (folder / "example.txt").write_text("source")

    def prepare_juce(self):
        archive = self.root / "deps/JUCE-9.0.4.zip"
        directory = self.root / "deps/JUCE-9.0.4"
        directory.mkdir(parents=True)
        (directory / "module.h").write_bytes(b"original dependency")
        with zipfile.ZipFile(archive, "w") as package:
            package.writestr("JUCE-9.0.4/module.h", b"original dependency")
        return directory, hashlib.sha256(archive.read_bytes()).hexdigest()

    def test_modified_missing_or_extra_juce_source_is_rejected(self):
        directory, expected = self.prepare_juce()
        self.assertEqual(self.release.verify_juce(self.root, directory, expected), expected)
        (directory / "module.h").write_bytes(b"local modification")
        with self.assertRaisesRegex(ValueError, "JUCE"):
            self.release.verify_juce(self.root, directory, expected)
        (directory / "module.h").write_bytes(b"original dependency")
        (directory / "extra.h").write_text("extra source")
        with self.assertRaisesRegex(ValueError, "JUCE"):
            self.release.verify_juce(self.root, directory, expected)
        (directory / "extra.h").unlink()
        (directory / "module.h").unlink()
        with self.assertRaisesRegex(ValueError, "JUCE"):
            self.release.verify_juce(self.root, directory, expected)

    def test_source_edit_during_packaging_cannot_produce_an_artifact(self):
        binary = self.root / "DualDeck.exe"
        binary.write_bytes(b"MZfixture")
        _, expected = self.prepare_juce()
        self.release.stamp(self.root, binary, self.release.fingerprint(self.root), juce_hash=expected)
        archive = self.release.source_archive

        def edit_after_archiving(root, output):
            result = archive(root, output)
            (self.root / "Source/example.txt").write_text("concurrent edit")
            return result

        output = pathlib.Path(self.tmp.name) / "raced"
        with mock.patch.object(self.release, "source_archive", side_effect=edit_after_archiving):
            with self.assertRaisesRegex(ValueError, "source"):
                self.release.bundle(self.root, binary, output, juce_hash=expected)
        self.assertFalse(output.exists())

    def test_source_excludes_private_and_generated_files(self):
        for name in ("data", "sounds", "deps", "tools", "keyboard-runtime", "test-output", ".git"):
            folder = self.root / name
            folder.mkdir(exist_ok=True)
            (folder / "PRIVATE").write_text("must never ship")
        (self.root / "Source/__pycache__").mkdir()
        (self.root / "Source/__pycache__/cached.pyc").write_bytes(b"cache")
        output = pathlib.Path(self.tmp.name) / "source.zip"
        self.release.source_archive(self.root, output)
        with zipfile.ZipFile(output) as package:
            names = package.namelist()
            self.assertIn("Source/example.txt", names)
            self.assertIn(".github/example.txt", names)
            self.assertFalse(any("PRIVATE" in n or "__pycache__" in n for n in names))
            self.assertNotIn("scripts/portable-msvc.py", names)

    def test_missing_required_file_and_existing_output_are_rejected(self):
        output = pathlib.Path(self.tmp.name) / "source.zip"
        (self.root / "LICENSE").unlink()
        with self.assertRaisesRegex(ValueError, "LICENSE"):
            self.release.source_archive(self.root, output)
        self.assertFalse(output.exists())
        (self.root / "LICENSE").write_text("license")
        output.write_bytes(b"preserve")
        with self.assertRaises(FileExistsError):
            self.release.source_archive(self.root, output)
        self.assertEqual(output.read_bytes(), b"preserve")

    def test_link_cannot_import_a_private_file(self):
        private = pathlib.Path(self.tmp.name) / "private.txt"
        private.write_text("private")
        try:
            (self.root / "Source/linked.txt").symlink_to(private)
        except OSError:
            self.skipTest("Windows symbolic-link privilege unavailable")
        with self.assertRaisesRegex(ValueError, "link"):
            self.release.source_archive(self.root, pathlib.Path(self.tmp.name) / "source.zip")

    def test_versions_reject_paths_and_stable_is_not_a_candidate(self):
        for bad in ("../private", "1.0.0/rc1", "1.0.0 rc1", "1.0.0"):
            (self.root / "VERSION").write_text(bad)
            with self.assertRaises(ValueError):
                self.release.read_version(self.root)

    def test_binary_bundle_contains_corresponding_source_and_no_driver(self):
        binary = self.root / "DualDeck.exe"
        binary.write_bytes(b"MZ" + b"test fixture")
        juce = self.root / "deps/JUCE-9.0.4.zip"
        juce.parent.mkdir()
        juce.write_bytes(b"dependency fixture")
        output = pathlib.Path(self.tmp.name) / "artifacts"
        expected = hashlib.sha256(juce.read_bytes()).hexdigest()
        self.release.stamp(self.root, binary, self.release.fingerprint(self.root), juce_hash=expected)
        self.release.bundle(self.root, binary, output, juce_hash=expected)
        with zipfile.ZipFile(output / "DualDeck-1.0.0-rc1-windows-x64-preview.zip") as package:
            names = package.namelist()
            self.assertIn("DualDeck.exe", names)
            self.assertIn("source/DualDeck-1.0.0-rc1-source.zip", names)
            self.assertIn("source/JUCE-9.0.4.zip", names)
            self.assertFalse(any(n.endswith((".dll", ".sys")) for n in names))
            manifest = json.loads(package.read("BUILD-INFO.json"))
            self.assertEqual(manifest["status"], "preview-not-for-stable-release")
            self.assertEqual(manifest["binary_sha256"], hashlib.sha256(binary.read_bytes()).hexdigest())
        checksums = (output / "SHA256SUMS.txt").read_text()
        for archive in output.glob("*.zip"):
            self.assertIn(hashlib.sha256(archive.read_bytes()).hexdigest(), checksums)

    def test_modified_sources_or_binary_cannot_be_paired_with_an_old_build(self):
        binary = self.root / "DualDeck.exe"
        binary.write_bytes(b"MZfixture")
        juce = self.root / "deps/JUCE-9.0.4.zip"
        juce.parent.mkdir()
        juce.write_bytes(b"fixture")
        expected = hashlib.sha256(juce.read_bytes()).hexdigest()
        self.release.stamp(self.root, binary, self.release.fingerprint(self.root), juce_hash=expected)
        (self.root / "Source/example.txt").write_text("changed after compilation")
        with self.assertRaisesRegex(ValueError, "source"):
            self.release.bundle(self.root, binary, pathlib.Path(self.tmp.name) / "bad-source", juce_hash=expected)
        self.release.stamp(self.root, binary, self.release.fingerprint(self.root), juce_hash=expected)
        binary.write_bytes(b"MZchanged")
        with self.assertRaisesRegex(ValueError, "binary"):
            self.release.bundle(self.root, binary, pathlib.Path(self.tmp.name) / "bad-binary", juce_hash=expected)

    def test_source_changes_during_build_are_rejected(self):
        before = self.release.fingerprint(self.root)
        (self.root / "Source/example.txt").write_text("changed during build")
        binary = self.root / "DualDeck.exe"
        binary.write_bytes(b"MZfixture")
        with self.assertRaisesRegex(ValueError, "source"):
            self.release.stamp(self.root, binary, before)

    def test_wrong_dependency_is_rejected_before_any_artifact_is_written(self):
        binary = self.root / "DualDeck.exe"
        binary.write_bytes(b"MZfixture")
        juce = self.root / "deps/JUCE-9.0.4.zip"
        juce.parent.mkdir()
        juce.write_bytes(b"tampered")
        output = pathlib.Path(self.tmp.name) / "artifacts"
        with self.assertRaisesRegex(ValueError, "JUCE"):
            self.release.bundle(self.root, binary, output)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
