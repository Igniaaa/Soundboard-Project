"""Prepare dependencies and finalize locally built DualDeck artifacts."""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

import release

ROOT = Path(__file__).resolve().parents[1]
JUCE_URL = "https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.4.zip"
FOLEYS_COMMIT = "d53d998e2bc3fb3ca394f1ee5b0c2f3886a67331"
FOLEYS_URL = f"https://codeload.github.com/ffAudio/foleys_gui_magic/zip/{FOLEYS_COMMIT}"
FOLEYS_HASH = "2b7797725272193e54c9beb660e30918a6171c00b510979fa7ce86e21aaa3ddf"
INTERCEPTION = (
    ("Interception-v1.0.1.zip",
     "https://github.com/oblitum/Interception/releases/download/v1.0.1/Interception.zip",
     "ad038963d6413055765128b0b931f6e765147c9916dba79e65d872b261f9af10"),
    ("Interception-v1.0.1-source.zip",
     "https://github.com/oblitum/Interception/archive/refs/tags/v1.0.1.zip",
     "43e6a4f71085c8797df3ea1df40cbda72530546f45b697944a237a1a8cd3eaa8"),
)
GPL_URL = "https://raw.githubusercontent.com/gcc-mirror/gcc/master/COPYING3"
GPL_HASH = "8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903"
DLL_HASH = "ab88164c11b1b48488772d4c3bfaa4509d5b0ae9dbc5a691dc4f96f0260443c8"


def digest(path: Path) -> str:
    return release.sha256(path)


def download(url: str, path: Path, expected_hash: str) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        print(f"Download: {url}")
        request = urllib.request.Request(url, headers={"User-Agent": "DualDeck-build-setup"})
        with tempfile.NamedTemporaryFile(prefix=f"{path.name}.", suffix=".download",
                                         dir=path.parent, delete=False) as temporary:
            temporary_path = Path(temporary.name)
            with urllib.request.urlopen(request, timeout=60) as response:
                shutil.copyfileobj(response, temporary)
        actual = digest(temporary_path)
        if actual.lower() != expected_hash.lower():
            temporary_path.unlink(missing_ok=True)
            raise ValueError(f"SHA-256 inatteso per {path}: {actual}")
        try:
            temporary_path.replace(path)
        except OSError:
            temporary_path.unlink(missing_ok=True)
            raise
    actual = digest(path)
    if actual.lower() != expected_hash.lower():
        raise ValueError(f"SHA-256 inatteso per {path}: {actual}")
    return path


def safe_extract(archive: Path, destination: Path, *, strip_prefix: str | None = None) -> None:
    """Extract a ZIP after rejecting traversal, absolute paths, and links."""
    destination = destination.resolve()
    with zipfile.ZipFile(archive) as package:
        for item in package.infolist():
            name = item.filename.replace("\\", "/")
            relative = PurePosixPath(name)
            if relative.is_absolute() or any(part in {"..", ""} for part in relative.parts):
                raise ValueError(f"Invalid path in dependency archive: {item.filename}")
            if relative.parts and ":" in relative.parts[0]:
                raise ValueError(f"Drive path in dependency archive: {item.filename}")
            mode = item.external_attr >> 16
            if mode and (mode & 0o170000) == 0o120000:
                raise ValueError(f"Symbolic link in dependency archive: {item.filename}")
            parts = relative.parts
            if strip_prefix:
                if not parts or parts[0] != strip_prefix:
                    raise ValueError(f"Unexpected archive root: {item.filename}")
                parts = parts[1:]
                if not parts and item.is_dir():
                    continue
            target = destination.joinpath(*parts).resolve()
            if not target.is_relative_to(destination):
                raise ValueError(f"Dependency path escapes destination: {item.filename}")
            if item.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with package.open(item) as source, target.open("xb") as output:
                shutil.copyfileobj(source, output)


def prepare_archive(url: str, archive: Path, expected_hash: str, target: Path,
                    *, prefix: str | None = None, validator=None) -> None:
    download(url, archive, expected_hash)
    if target.exists():
        if validator:
            validator(target)
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="dualdeck-dependency-", dir=target.parent) as temporary:
        staging = Path(temporary) / "staged"
        staging.mkdir()
        safe_extract(archive, staging, strip_prefix=prefix)
        extracted = staging
        if not extracted.is_dir():
            raise ValueError(f"Dependency root missing from archive: {prefix}")
        if validator:
            validator(extracted)
        extracted.rename(target)


def verify_extracted_archive(archive: Path, target: Path, prefix: str) -> None:
    release.check_local(target, ROOT)
    expected: dict[str, str] = {}
    with zipfile.ZipFile(archive) as package:
        for item in package.infolist():
            if item.is_dir():
                continue
            name = item.filename.replace("\\", "/")
            relative = PurePosixPath(name)
            if not relative.parts or relative.parts[0] != prefix:
                raise ValueError(f"Unexpected archive root: {item.filename}")
            if relative.is_absolute() or any(part in {"..", ""} for part in relative.parts):
                raise ValueError(f"Invalid path in dependency archive: {item.filename}")
            expected[relative.as_posix().removeprefix(prefix + "/")] = hashlib.sha256(package.read(item)).hexdigest()
    actual: dict[str, str] = {}
    for directory, folders, files in os.walk(target, followlinks=False):
        current = Path(directory)
        for folder in folders:
            candidate = current / folder
            if candidate.is_symlink():
                raise ValueError(f"Symbolic link in extracted dependency: {candidate}")
        for filename in files:
            candidate = current / filename
            if candidate.is_symlink():
                raise ValueError(f"Symbolic link in extracted dependency: {candidate}")
            actual[candidate.relative_to(target).as_posix()] = digest(candidate)
    if actual != expected:
        raise ValueError(f"Estratto non corrispondente all'archivio verificato: {target}")


def check_visual_studio() -> None:
    program_files = os.environ.get("ProgramFiles(x86)")
    if not program_files:
        raise RuntimeError("Questo comando di setup richiede Windows x64.")
    vswhere = Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        raise RuntimeError("Installa Visual Studio 2022 con Desktop development with C++ e Windows SDK.")
    result = subprocess.run(
        [str(vswhere), "-version", "[17.0,18.0)", "-latest", "-requires",
         "Microsoft.VisualStudio.Workload.NativeDesktop",
         "-property", "installationPath"], capture_output=True, text=True, check=False,
    )
    if result.returncode or not result.stdout.strip():
        raise RuntimeError("Visual Studio 2022 con workload Desktop development with C++ non trovato.")
    installation = Path(result.stdout.strip())
    cmake_candidates = [
        Path(shutil.which("cmake.exe") or shutil.which("cmake") or ""),
        installation / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "CMake" / "bin" / "cmake.exe",
    ]
    if not any(candidate.is_file() for candidate in cmake_candidates):
        raise RuntimeError("CMake non trovato. Installa i CMake tools con Visual Studio 2022.")
    if sys.version_info < (3, 10):
        raise RuntimeError("È richiesto Python 3.10 o successivo.")


def setup(_args) -> None:
    check_visual_studio()
    archive = ROOT / "deps" / "JUCE-9.0.4.zip"
    target = ROOT / "deps" / "JUCE-9.0.4"
    prepare_archive(JUCE_URL, archive, release.JUCE_HASH, target,
                    prefix="JUCE-9.0.4", validator=lambda path: release.verify_juce(ROOT, path))
    release.verify_juce(ROOT, target)
    print("JUCE 9.0.4 verificato. Visual Studio, CMake tools e Windows SDK sono pronti.")


def setup_foleys(_args) -> None:
    archive = ROOT / "deps" / f"foleys_gui_magic-{FOLEYS_COMMIT[:7]}.zip"
    target = ROOT / "deps" / f"foleys_gui_magic-{FOLEYS_COMMIT}"
    prepare_archive(FOLEYS_URL, archive, FOLEYS_HASH, target, prefix=f"foleys_gui_magic-{FOLEYS_COMMIT}")
    verify_extracted_archive(archive, target, f"foleys_gui_magic-{FOLEYS_COMMIT}")
    header = target / "modules" / "foleys_gui_magic" / "foleys_gui_magic.h"
    if not header.is_file():
        raise ValueError(f"Modulo Foleys GUI Magic non trovato: {header}")
    print(f"Foleys GUI Magic pronto: {FOLEYS_COMMIT}")


def fetch_verified(url: str, path: Path, expected_hash: str) -> Path:
    if not path.exists():
        download(url, path, expected_hash)
    elif digest(path).lower() != expected_hash.lower():
        raise ValueError(f"SHA-256 inatteso per {path}")
    return path


def setup_keyboard_bridge(args) -> None:
    deps = ROOT / "deps"
    archive = Path(args.archive).resolve() if args.archive else deps / INTERCEPTION[0][0]
    source_archive = Path(args.source_archive).resolve() if args.source_archive else deps / INTERCEPTION[1][0]
    for path, (_, url, checksum) in zip((archive, source_archive), INTERCEPTION):
        fetch_verified(url, path, checksum)
    gpl_path = deps / "GPL-3.0.txt"
    fetch_verified(GPL_URL, gpl_path, GPL_HASH)
    with tempfile.TemporaryDirectory(prefix="dualdeck-interception-") as temporary:
        staging = Path(temporary)
        runtime_tree, source_tree = staging / "runtime", staging / "source"
        safe_extract(archive, runtime_tree)
        safe_extract(source_archive, source_tree)
        dll = runtime_tree / "Interception" / "library" / "x64" / "interception.dll"
        if not dll.is_file() or digest(dll).lower() != DLL_HASH:
            raise ValueError("DLL x64 non verificata.")
        library_license = (source_tree / "Interception-1.0.1" / "licenses" /
                           "non-commercial-usage" / "LGPL 3.0.txt")
        if not library_license.is_file():
            raise ValueError("Licenza LGPL della libreria Interception non trovata.")
        runtime = ROOT / "keyboard-runtime"
        runtime.mkdir(parents=True, exist_ok=True)
        shutil.copy2(dll, runtime / "interception.dll")
        shutil.copy2(library_license, runtime / "LGPL-3.0.txt")
        shutil.copy2(gpl_path, runtime / "GPL-3.0.txt")
        shutil.copy2(source_archive, runtime / "Interception-v1.0.1-source.zip")
    notice = (
        "Interception user-mode library, unmodified x64 release v1.0.1.\n"
        "Author: Francisco Lopes da Silva (oblitum).\n"
        "Project and license information: https://github.com/oblitum/Interception\n"
        "Official release: https://github.com/oblitum/Interception/releases/tag/v1.0.1\n"
        "The library is dynamically loaded through its published C API.\n"
        "For filter writes, DualDeck also uses the pinned x64 v1.0.1 context layout\n"
        "and SET_FILTER IOCTL from library/interception.c to retain the driver\n"
        "acknowledgement; the installed legacy driver does not implement GET_FILTER.\n"
        "LGPL-3.0.txt and GPL-3.0.txt are included, together with the unmodified\n"
        "source archive for tag v1.0.1. See its README and licenses directory.\n"
        "This directory does not install, start, or redistribute kernel-driver binaries.\n"
        "The author's free driver distribution is described for non-commercial use.\n"
        "DualDeck source contains the default trusted DLL SHA-256; a replacement\n"
        "library can be used by adapting that value and rebuilding the app.\n"
    )
    (runtime / "THIRD-PARTY-NOTICES.txt").write_text(notice, encoding="utf-8")
    print(f"Runtime tastiera pronto: {runtime}")
    print("Preparata solo la libreria: nessun driver installato o modificato.")


def finalize(args) -> None:
    if args.output_name:
        name = Path(args.output_name)
        if name.name != args.output_name or name.suffix.lower() != ".exe":
            raise ValueError("--output-name deve essere un nome .exe senza cartelle.")
    build_dir = Path(args.build_dir).resolve()
    binary = build_dir / "bin" / "DualDeckJuce.exe"
    if not binary.is_file() or binary.is_symlink():
        raise ValueError(f"Eseguibile non trovato: {binary}; compila prima in Visual Studio.")
    version = release.read_version(ROOT)
    cache = build_dir / "CMakeCache.txt"
    if not cache.is_file():
        raise ValueError(f"Cache CMake non trovato: {cache}; seleziona e compila un preset Visual Studio.")
    cache_text = cache.read_text(encoding="utf-8", errors="replace")
    cache_values = {}
    for line in cache_text.splitlines():
        if line and not line.startswith("//") and not line.startswith("#") and "=" in line:
            key, value = line.split("=", 1)
            cache_values[key.split(":", 1)[0]] = value
    configured_source = cache_values.get("CMAKE_HOME_DIRECTORY")
    if not configured_source:
        raise ValueError("Cache CMake senza percorso sorgenti.")
    configured_root = Path(configured_source).resolve()
    if configured_root != ROOT.resolve():
        raise ValueError("La directory di build appartiene a un altro progetto.")
    if cache_values.get("CMAKE_GENERATOR") != "Visual Studio 17 2022":
        raise ValueError("Finalizzazione prevista per un preset Visual Studio 2022.")
    if "Release" not in cache_values.get("CMAKE_CONFIGURATION_TYPES", "").split(";"):
        raise ValueError("La finalizzazione richiede una configurazione Release.")
    foleys = (cache_values.get("DUALDECK_FOLEYS_AUDIO_UI") == "ON" or
              cache_values.get("DUALDECK_FOLEYS_DESIGNER") == "ON")
    ui_debug = cache_values.get("DUALDECK_UI_EDITOR") == "ON"
    if args.promote and (foleys or ui_debug):
        raise ValueError("Le build Foleys e UI Debug sono varianti di sviluppo e non possono sostituire la build standard.")
    if args.output_name and not args.promote and args.output_name.lower() in {
            "dualdeckjuce.exe", f"dualdeckjuce-{version}.exe"}:
        raise ValueError("Usa --promote per sostituire gli eseguibili approvati.")
    configured_juce = cache_values.get("JUCE_DIR")
    if not configured_juce:
        raise ValueError("JUCE_DIR non registrato nella cache CMake; riconfigura il preset e ricompila.")
    juce_dir = Path(configured_juce).resolve()
    if args.juce_directory and Path(args.juce_directory).resolve() != juce_dir:
        raise ValueError("--juce-directory deve corrispondere al JUCE_DIR usato dalla configurazione CMake.")
    juce_hash = release.verify_juce(ROOT, juce_dir)
    configured_fingerprint = build_dir / "dualdeck-source-fingerprint.txt"
    if not configured_fingerprint.is_file():
        raise ValueError("Impronta sorgenti configurata assente; riconfigura il preset e ricompila.")
    expected_source = configured_fingerprint.read_text(encoding="ascii").strip()
    if not re.fullmatch(r"[0-9a-fA-F]{64}", expected_source):
        raise ValueError("Impronta sorgenti configurata non valida; riconfigura il preset e ricompila.")
    if release.fingerprint(ROOT) != expected_source:
        raise ValueError("I sorgenti differiscono dallo snapshot configurato; riconfigura e ricompila.")
    record = release.stamp(ROOT, binary, expected_source, juce_hash=juce_hash)
    if release.fingerprint(ROOT) != expected_source:
        raise ValueError("I sorgenti sono cambiati durante la registrazione della build.")
    print(f"Build registrata: {binary}")
    print(f"Versione: {record['version']}; SHA-256: {record['binary_sha256']}")
    if args.output_name:
        name = Path(args.output_name)
        destination = ROOT / name
        if destination.exists():
            raise FileExistsError(f"La copia di prova esiste già: {destination}")
        shutil.copy2(binary, destination)
        shutil.copy2(binary.with_suffix(".build.json"), destination.with_suffix(".build.json"))
        print(f"Copia di prova pronta: {destination}")
    if args.promote:
        versioned = ROOT / f"DualDeckJuce-{version}.exe"
        shutil.copy2(binary, versioned)
        shutil.copy2(binary.with_suffix(".build.json"), versioned.with_suffix(".build.json"))
        approved = ROOT / "DualDeckJuce.exe"
        shutil.copy2(binary, approved)
        shutil.copy2(binary.with_suffix(".build.json"), approved.with_suffix(".build.json"))
        print(f"Build promossa esplicitamente: {versioned} e {approved}")
    else:
        print("Copie nella cartella progetto lasciate invariate. Usa --promote per aggiornarle esplicitamente.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("setup", help="verifica Visual Studio e prepara JUCE")
    commands.add_parser("setup-foleys", help="prepara il modulo Foleys verificato")
    keyboard = commands.add_parser("setup-keyboard-bridge", help="prepara la libreria Interception opzionale")
    keyboard.add_argument("--archive")
    keyboard.add_argument("--source-archive")
    finish = commands.add_parser("finalize", help="registra la build fatta in Visual Studio")
    finish.add_argument("--build-dir", default="build-vs-release")
    finish.add_argument("--juce-directory")
    finish.add_argument("--output-name")
    finish.add_argument("--promote", action="store_true")
    audit = commands.add_parser("inspect-driver", help="controlla staticamente le firme del pacchetto Interception")
    audit.add_argument("--archive", required=True)
    for name, function in (("setup", setup), ("setup-foleys", setup_foleys),
                           ("setup-keyboard-bridge", setup_keyboard_bridge), ("finalize", finalize)):
        commands.choices[name].set_defaults(function=function)
    args = parser.parse_args()
    if args.command == "inspect-driver":
        command = [sys.executable, str(ROOT / "scripts" / "inspect_driver.py"), "--archive", args.archive]
        return subprocess.run(command, check=False).returncode
    try:
        args.function(args)
    except (OSError, ValueError, RuntimeError, zipfile.BadZipFile) as exc:
        print(f"Errore: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
