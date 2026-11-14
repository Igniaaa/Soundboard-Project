"""Run CTest and the application smoke tests used for DualDeck previews."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD_DIRS = {
    "vs-release": "build-vs-release",
    "vs-foleys": "build-vs-foleys",
    "vs-foleys-designer": "build-vs-foleys-designer",
}
TIMEOUT_SECONDS = 60


def ctest_path() -> str:
    found = shutil.which("ctest")
    if found:
        return found
    cmake = shutil.which("cmake")
    if cmake:
        sibling = Path(cmake).with_name("ctest.exe" if os.name == "nt" else "ctest")
        if sibling.is_file():
            return str(sibling)
    program_files = os.environ.get("ProgramFiles(x86)")
    if program_files:
        installer = Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
        if installer.is_file():
            result = subprocess.run(
                [str(installer), "-version", "[17.0,18.0)", "-latest", "-requires",
                 "Microsoft.VisualStudio.Workload.NativeDesktop",
                 "-property", "installationPath"], capture_output=True, text=True, check=False,
            )
            if result.returncode == 0 and result.stdout.strip():
                cmake_dir = Path(result.stdout.strip()) / "Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin"
                candidate = cmake_dir / "ctest.exe"
                if candidate.is_file():
                    return str(candidate)
    raise RuntimeError("ctest non trovato. Installa i CMake tools con Visual Studio 2022.")


def run_app(executable: Path, arguments: list[str], report: Path | None = None) -> None:
    command = [str(executable), *arguments]
    startupinfo = None
    if os.name == "nt":
        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startupinfo.wShowWindow = 0
    try:
        result = subprocess.run(command, timeout=TIMEOUT_SECONDS, check=False, startupinfo=startupinfo)
    except subprocess.TimeoutExpired as exc:
        print(f"Test scaduto dopo {TIMEOUT_SECONDS} secondi: {' '.join(arguments)}", file=sys.stderr)
        if report and report.is_file():
            print(f"Rapporto del test: {report}")
            if report.suffix.lower() != ".png":
                print(report.read_text(encoding="utf-8", errors="replace"))
        raise RuntimeError("timeout del test applicativo") from exc
    if result.returncode:
        if report and report.is_file():
            print(f"Rapporto del test fallito: {report}")
            if report.suffix.lower() != ".png":
                print(report.read_text(encoding="utf-8", errors="replace"))
        raise RuntimeError(f"Test applicativo fallito con codice {result.returncode}: {' '.join(arguments)}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", choices=tuple(BUILD_DIRS), default="vs-release")
    parser.add_argument("--audio-devices", action="store_true")
    parser.add_argument("--performance", action="store_true")
    parser.add_argument("--skip-keyboard-devices", action="store_true")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()

    build_dir = (args.build_dir or (ROOT / BUILD_DIRS[args.preset])).resolve()
    executable = (args.executable or (build_dir / "bin" / "DualDeckJuce.exe")).resolve()
    output = (args.output_dir or (ROOT / "test-output")).resolve()
    if not executable.is_file():
        print(f"Eseguibile di test assente: {executable}. Compila il preset in Visual Studio.", file=sys.stderr)
        return 2
    output.mkdir(parents=True, exist_ok=True)
    try:
        result = subprocess.run(
            [ctest_path(), "--test-dir", str(build_dir), "-C", "Release", "--output-on-failure"],
            check=False,
        )
        if result.returncode:
            raise RuntimeError("Test CTest non superati.")

        library_report = output / "library-tests.txt"
        run_app(executable, ["--self-test", str(output)], library_report)

        keyboard_report = output / "keyboard-devices.txt"
        if args.skip_keyboard_devices:
            message = ("SKIP: controllo tastiera fisica richiesto escluso; le suite software CTest "
                       "per tastiera e bridge restano obbligatorie.\n")
            keyboard_report.write_text(message, encoding="utf-8")
            print(message.rstrip())
        else:
            run_app(executable, ["--keyboard-smoke", str(keyboard_report)], keyboard_report)

        interaction_report = output / "ui-interactions.txt"
        run_app(executable, ["--interaction-smoke", str(interaction_report)], interaction_report)

        previews = (
            ("board.png", 0, None),
            ("audio.png", 1, None),
            ("keyboard.png", 2, None),
            ("dedicated-keyboard.png", 3, None),
            ("editor.png", 4, None),
            ("board-small.png", 0, (960, 640)),
            ("editor-small.png", 4, (960, 640)),
            ("editor-effects.png", 5, None),
            ("editor-effects-small.png", 5, (960, 640)),
        )
        for name, section, size in previews:
            arguments = ["--smoke-test", str(output / name), str(section)]
            if size:
                arguments.extend(map(str, size))
            run_app(executable, arguments, output / name)

        if args.audio_devices:
            report = output / "audio-devices.txt"
            run_app(executable, ["--audio-smoke", str(report)], report)
        if args.performance:
            report = output / "performance.txt"
            run_app(executable, ["--performance-test", str(report)], report)

        if library_report.is_file():
            print(library_report.read_text(encoding="utf-8", errors="replace"))
        print(f"Anteprime e report: {output}")
        return 0
    except (OSError, RuntimeError) as exc:
        print(f"Errore: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
