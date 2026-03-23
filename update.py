#!/usr/bin/env python3

from __future__ import annotations

import argparse
import stat
import subprocess
import sys
from pathlib import Path


BASE_DIR = Path(__file__).resolve().parent

# Update these filenames when your shell scripts are ready.
SCRIPT_MAP = {
    "r": BASE_DIR / "updateROSPackage.sh",
    "a": BASE_DIR / "updateArduinoSketch.sh",
    "g": BASE_DIR / "updateGUILauncher.sh",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Apply execute permission to selected shell scripts and run them. "
            "If no option is given, all scripts are run."
        )
    )
    parser.add_argument("-r", action="store_true", help="Run script updateROSPackage.sh")
    parser.add_argument("-a", action="store_true", help="Run script updateArduinoSketch.sh")
    parser.add_argument("-g", action="store_true", help="Run script updateGUILauncher.sh")
    return parser.parse_args()


def selected_keys(args: argparse.Namespace) -> list[str]:
    keys = [key for key in SCRIPT_MAP if getattr(args, key)]
    return keys or list(SCRIPT_MAP.keys())


def ensure_executable(script_path: Path) -> None:
    current_mode = script_path.stat().st_mode
    script_path.chmod(current_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def run_script(script_path: Path) -> int:
    if not script_path.exists():
        print(f"Error: script not found: {script_path}", file=sys.stderr)
        return 1

    ensure_executable(script_path)
    print(f"Running: {script_path.name}")

    completed = subprocess.run(
        ["bash", str(script_path)],
        cwd=BASE_DIR,
        check=False,
    )
    return completed.returncode


def main() -> int:
    args = parse_args()
    targets = [SCRIPT_MAP[key] for key in selected_keys(args)]

    exit_code = 0
    for script_path in targets:
        result = run_script(script_path)
        if result != 0:
            exit_code = result

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
