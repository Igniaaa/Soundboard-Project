"""Build local preview archives. Never uploads, installs, or captures input."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import tempfile
import zipfile

ROOT_FILES = (
    "CMakeLists.txt", "Build.ps1", "Setup.ps1", "Test.ps1", "VERSION", "LICENSE",
    "COPYRIGHT.md", "README.md", "GETTING-STARTED.md", "BUILDING.md",
    "CONTRIBUTING.md", "THIRD-PARTY-NOTICES.md", "RELEASE-CHECKLIST.md",
    "CHANGELOG.md", "RELEASE-PLAN.md", "VERIFICA-RELEASE.md", "JUCE-LICENSE.md", "LEGGIMI.md", "GITHUB.md",
    ".gitignore", ".gitattributes", "Setup-KeyboardBridge.ps1",
    "Inspect-KeyboardDriver.ps1", "Build-UiDebug.ps1", "DOCUMENTAZIONE-CODICE.md",
    "EDITOR-UI.md", "TASTIERA-BLOCCO.md",
    "Changelogs/VERIFICHE-0.2.md", "Changelogs/VERIFICHE-0.4.md",
    "Changelogs/VERIFICHE-0.5.md", "Changelogs/VERIFICHE-0.6.md", "Changelogs/VERIFICHE-0.7.md",
    "Changelogs/VERIFICHE-AUDIO-REVIEW.md", "Changelogs/VERIFICHE-ORDINAMENTO.md",
    "Changelogs/VERIFICHE-RIVERBERO.md", "Changelogs/VERIFICHE-SCRUB.md",
    "Changelogs/VERIFICHE-PRIMO-AVVIO.md", "Changelogs/VERIFICHE-STREAMING.md",
    "Changelogs/VERIFICHE-TASTIERA-COMPLETA.md",
)
SOURCE_DIRS = ("Source", "Tests", "scripts", "Assets", ".github")
JUCE_HASH = "16ad4cfca70b73df6352e4e840ba5dc2e755d36b8ab2dd976ef3ecedcd18f6a9"
EXTENSIONS = {".cpp", ".h", ".py", ".ps1", ".md", ".txt", ".yml", ".yaml", ".ico", ".png"}


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_version(root):
    version = (Path(root) / "VERSION").read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+-rc[1-9]\d*", version):
        raise ValueError("VERSION must be an explicit candidate, e.g. 1.0.0-rc1")
    return version


def check_local(path, root):
    path, root = Path(path), Path(root).resolve()
    # Reparse points include Windows directory junctions, not just symlinks.
    for part in (path, *path.parents):
        if part.is_symlink() or (part.exists() and
                getattr(part.lstat(), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT):
            raise ValueError(f"Source link/reparse point is not distributable: {path}")
        if part == root:
            break
    if not path.resolve().is_relative_to(root):
        raise ValueError(f"Source escapes project: {path}")


def source_files(root):
    root = Path(root).resolve()
    read_version(root)
    result = []
    for name in ROOT_FILES:
        path = root / name
        check_local(path, root)
        if not path.is_file():
            raise ValueError(f"Required release source missing: {name}")
        result.append(path)
    for name in SOURCE_DIRS:
        folder = root / name
        check_local(folder, root)
        if not folder.is_dir():
            raise ValueError(f"Required source directory missing: {name}")
        for directory, folders, files in os.walk(folder, followlinks=False):
            for child in folders:
                check_local(Path(directory) / child, root)
            folders[:] = [f for f in folders if f not in {"__pycache__", ".git"}]
            for child in files:
                path = Path(directory) / child
                check_local(path, root)
                if child == "portable-msvc.py" or child.startswith("."):
                    continue
                if path.suffix.lower() in EXTENSIONS or child.endswith(".rc.in"):
                    result.append(path)
    return sorted(result, key=lambda p: p.relative_to(root).as_posix())


def source_archive(root, output):
    root, output = Path(root).resolve(), Path(output)
    if output.exists():
        raise FileExistsError(output)
    files = source_files(root)
    output.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive creation protects existing artifacts even across concurrent runs.
    with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED) as package:
        for path in files:
            package.write(path, path.relative_to(root).as_posix())
    return output


def fingerprint(root):
    root = Path(root).resolve()
    digest = hashlib.sha256()
    for path in source_files(root):
        digest.update(path.relative_to(root).as_posix().encode("utf-8") + b"\0")
        digest.update(bytes.fromhex(sha256(path)))
    return digest.hexdigest()


def verify_juce(root, directory, expected_hash=JUCE_HASH):
    archive, directory = Path(root) / "deps/JUCE-9.0.4.zip", Path(directory).resolve()
    if not archive.is_file() or sha256(archive) != expected_hash:
        raise ValueError("JUCE archive missing or SHA-256 mismatch")
    check_local(directory, directory)
    expected_files = set()
    with zipfile.ZipFile(archive) as package:
        for info in package.infolist():
            if info.is_dir():
                continue
            prefix = "JUCE-9.0.4/"
            if not info.filename.startswith(prefix):
                raise ValueError("Unexpected JUCE archive root")
            relative = info.filename[len(prefix):]
            path = directory / relative
            check_local(path, directory)
            if not path.is_file() or sha256(path) != hashlib.sha256(package.read(info)).hexdigest():
                raise ValueError(f"JUCE source differs from official archive: {relative}")
            expected_files.add(relative)
    actual_files = set()
    for current, folders, files in os.walk(directory, followlinks=False):
        for child in folders + files:
            check_local(Path(current) / child, directory)
        actual_files.update((Path(current) / name).relative_to(directory).as_posix() for name in files)
    if actual_files != expected_files:
        raise ValueError("JUCE source tree contains missing or extra files")
    return expected_hash


def stamp(root, binary, expected_source, *, juce_hash=JUCE_HASH):
    current = fingerprint(root)
    if current != expected_source:
        raise ValueError("Release source changed during compilation; rebuild the snapshot")
    binary = Path(binary)
    record = {"version": read_version(root), "source_tree_sha256": current,
              "binary_sha256": sha256(binary), "juce_sha256": juce_hash}
    binary.with_suffix(".build.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return record


def bundle(root, binary, output, *, juce_hash=JUCE_HASH):
    root, binary, output = Path(root).resolve(), Path(binary).resolve(), Path(output).resolve()
    if output.exists():
        raise FileExistsError(output)
    version = read_version(root)
    source_files(root)  # Validate the entire source set before writing anything.
    if not binary.is_file() or binary.is_symlink():
        raise ValueError("A built Windows executable is required")
    with binary.open("rb") as stream:
        if stream.read(2) != b"MZ":
            raise ValueError("Not a Windows executable")
    juce = root / "deps/JUCE-9.0.4.zip"
    check_local(juce, root)
    if not juce.is_file() or sha256(juce) != juce_hash:
        raise ValueError("JUCE archive missing or SHA-256 mismatch; run Setup.ps1")
    build_record = binary.with_suffix(".build.json")
    if not build_record.is_file():
        raise ValueError("Missing binary build record; compile with Build.ps1 first")
    record = json.loads(build_record.read_text(encoding="utf-8"))
    if record.get("binary_sha256") != sha256(binary):
        raise ValueError("The binary changed after compilation")
    if record.get("version") != version or record.get("source_tree_sha256") != fingerprint(root):
        raise ValueError("The source differs from the compiled snapshot; rebuild before packaging")
    if record.get("juce_sha256") != juce_hash:
        raise ValueError("The compiled JUCE dependency differs from the packaged archive")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="dualdeck-package-", dir=output.parent) as temp:
        stage = Path(temp)
        source_name = f"DualDeck-{version}-source.zip"
        source = source_archive(root, stage / source_name)
        if fingerprint(root) != record["source_tree_sha256"]:
            raise ValueError("The source changed during packaging")
        # Validate the archive itself, not only a second read of the live tree.
        archived_digest = hashlib.sha256()
        with zipfile.ZipFile(source) as archived:
            for entry in archived.namelist():
                archived_digest.update(entry.encode("utf-8") + b"\0")
                archived_digest.update(hashlib.sha256(archived.read(entry)).digest())
        if archived_digest.hexdigest() != record["source_tree_sha256"]:
            raise ValueError("Archived source differs from the compiled snapshot")
        name = f"DualDeck-{version}-windows-x64-preview.zip"
        manifest = {
            "version": version, "platform": "windows-x64",
            "status": "preview-not-for-stable-release", "license": "AGPL-3.0-only",
            "binary_sha256": sha256(binary), "source_sha256": sha256(source),
            "source_tree_sha256": record["source_tree_sha256"],
            "juce_sha256": sha256(juce), "interception_bundled": False,
            "limitations": "See RELEASE-CHECKLIST.md before any public release",
        }
        with zipfile.ZipFile(stage / name, "x", compression=zipfile.ZIP_DEFLATED) as package:
            package.write(binary, "DualDeck.exe")
            for doc in ("LICENSE", "COPYRIGHT.md", "GETTING-STARTED.md", "THIRD-PARTY-NOTICES.md",
                        "RELEASE-CHECKLIST.md", "VERIFICA-RELEASE.md", "CHANGELOG.md", "JUCE-LICENSE.md", "BUILDING.md"):
                package.write(root / doc, doc)
            package.write(source, "source/" + source_name)
            package.write(juce, "source/JUCE-9.0.4.zip")
            package.writestr("BUILD-INFO.json", json.dumps(manifest, indent=2) + "\n")
        archives = sorted(stage.glob("*.zip"))
        (stage / "SHA256SUMS.txt").write_text(
            "".join(f"{sha256(path)}  {path.name}\n" for path in archives), encoding="utf-8")
        output.mkdir()  # Reject an existing output; never replace somebody's release.
        for path in stage.iterdir():
            shutil.copyfile(path, output / path.name)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("source", "bundle", "fingerprint", "stamp", "verify-juce"))
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path)
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--expected-source")
    parser.add_argument("--juce-directory", type=Path)
    args = parser.parse_args()
    if args.command == "fingerprint":
        print(fingerprint(args.root))
        return
    if args.command == "stamp":
        if not args.binary or not args.expected_source or not args.juce_directory:
            parser.error("stamp requires --binary, --expected-source and --juce-directory")
        dependency = verify_juce(args.root, args.juce_directory)
        stamp(args.root, args.binary, args.expected_source, juce_hash=dependency)
        return
    if args.command == "verify-juce":
        if not args.juce_directory:
            parser.error("verify-juce requires --juce-directory")
        print(verify_juce(args.root, args.juce_directory))
        return
    if not args.output:
        parser.error("source/bundle requires --output")
    if args.command == "source":
        result = source_archive(args.root, args.output)
    else:
        if not args.binary:
            parser.error("bundle requires --binary")
        result = bundle(args.root, args.binary, args.output)
    print(result)


if __name__ == "__main__":
    main()
