"""Inspect embedded Interception drivers without executing the installer or drivers."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "scripts" / "inspect-interception-resources.py"


def find_signtool() -> Path:
    program_files = os.environ.get("ProgramFiles(x86)")
    if program_files:
        kit_bin = Path(program_files) / "Windows Kits" / "10" / "bin"
        candidates = list(kit_bin.glob("*/x64/signtool.exe"))
        if candidates:
            def version(path: Path) -> tuple[int, ...]:
                match = re.fullmatch(r"\d+(?:\.\d+)+", path.parents[1].name)
                return tuple(map(int, match.group().split("."))) if match else (0,)
            return max(candidates, key=version)
        installer = Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
        if installer.is_file():
            result = subprocess.run([str(installer), "-latest", "-property", "installationPath"],
                                    capture_output=True, text=True, check=False)
            if result.returncode == 0 and result.stdout.strip():
                vs_root = Path(result.stdout.strip())
                candidates = list(vs_root.glob("**/signtool.exe"))
                candidates = [path for path in candidates if path.parent.name.lower() == "x64"]
                if candidates:
                    return candidates[0]
    found = shutil.which("signtool.exe") or shutil.which("signtool")
    if found:
        return Path(found)
    raise RuntimeError("signtool.exe non trovato. Installa un Windows SDK con Visual Studio.")


def verify_signature(signtool: Path, path: Path) -> dict:
    result = subprocess.run(
        [str(signtool), "verify", "/pa", "/all", "/v", str(path)],
        capture_output=True, text=True, check=False,
    )
    output = (result.stdout + result.stderr).strip()
    issued_to = re.search(r"(?im)^\s*Issued to:\s*(.+?)\s*$", output)
    issued_by = re.search(r"(?im)^\s*Issued by:\s*(.+?)\s*$", output)
    status = "Valid" if result.returncode == 0 else (
        "NotSigned" if re.search(r"(?i)no signature found|not signed", output) else "NotVerified"
    )
    return {
        "AuthenticodeStatus": status,
        "Signer": issued_to.group(1) if issued_to else None,
        "Issuer": issued_by.group(1) if issued_by else None,
        "VerificationOutput": output,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True, type=Path)
    args = parser.parse_args()
    archive = args.archive.resolve()
    if not archive.is_file():
        parser.error(f"Archivio non trovato: {archive}")
    report_dir = ROOT / "test-output" / "keyboard-driver-audit"
    embedded = report_dir / "embedded"
    report_dir.mkdir(parents=True, exist_ok=True)
    extraction = subprocess.run(
        [sys.executable, str(SOURCE), str(archive), str(embedded)],
        capture_output=True, text=True, check=False,
    )
    if extraction.returncode:
        print(extraction.stdout, end="")
        print(extraction.stderr, end="", file=sys.stderr)
        return extraction.returncode
    drivers = json.loads(extraction.stdout)
    signtool = find_signtool()
    for record in drivers:
        record.update(verify_signature(signtool, embedded / record["file"]))
    report = {
        "Source": "https://github.com/oblitum/Interception/releases/tag/v1.0.1",
        "ArchiveSHA256": hashlib.sha256(archive.read_bytes()).hexdigest().upper(),
        "Verification": (
            "Static resource extraction and SignTool Authenticode verification only; "
            "kernel loading, policy eligibility and hardware compatibility are NOT verified."
        ),
        "Drivers": [
            {
                "Resource": driver["resource"],
                "File": driver["file"],
                "Machine": driver["machine"],
                "SHA256": driver["sha256"],
                "AuthenticodeStatus": driver["AuthenticodeStatus"],
                "Signer": driver["Signer"],
                "Issuer": driver["Issuer"],
                "VerificationOutput": driver["VerificationOutput"],
            }
            for driver in drivers
        ],
    }
    report_path = report_dir / "report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Analizzati {len(drivers)} driver incorporati senza eseguirli.")
    print("Firma Authenticode valida non equivale ad autorizzazione al caricamento del driver.")
    print(f"Report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
