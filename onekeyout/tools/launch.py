#!/usr/bin/env python3
"""Self-locating Qbs launcher; upstream files and build outputs are untouched."""
from pathlib import Path
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def main():
    args = sys.argv[1:]
    mode = "run"
    if args and args[0] in ("--help", "-h"):
        print("Usage: ./run.sh [--build-only | --check | --package] [-- game arguments]\n"
              "Examples:\n  ./run.sh\n  ./run.sh -- --map assets/maps/first-room.tmx\n"
              "  ./run.sh -- --capture build/start.png --quit-after 600\n"
              "  ./run.sh --check\n  ./run.sh --package")
        return 0
    if args and args[0] in ("--build-only", "--check", "--package"):
        mode = args.pop(0)[2:]
    if args and args[0] == "--":
        args.pop(0)
    elif args:
        raise SystemExit("Game arguments must follow --. See --help.")
    if mode != "run" and args:
        raise SystemExit("Game arguments are only accepted in run mode.")
    if not shutil.which("qbs"):
        raise SystemExit("Qbs is required. On macOS: brew install qbs qttools")
    products = "onekeyout,test-onekeyout" if mode == "check" else "onekeyout"
    common = ["-f", str(ROOT / "workspace.qbs"), "-d", str(BUILD)]
    subprocess.run(["qbs", "build", *common, "--no-install", "-p", products, "-j", "4", "config:debug"],
                   cwd=ROOT, check=True)
    if mode == "build-only":
        return 0
    if mode == "package":
        subprocess.run([sys.executable, str(ROOT / "tools/package-macos.py")], cwd=ROOT, check=True)
        return 0
    if mode == "check":
        environment = os.environ.copy()
        environment["QT_QPA_PLATFORM"] = "offscreen"
        environment["ONEKEYOUT_CAPTURE_DIR"] = str(BUILD / "evidence")
        subprocess.run(["qbs", "run", *common, "--no-build", "-p", "test-onekeyout", "config:debug"],
                       cwd=ROOT, env=environment, check=True, timeout=45)
        return 0
    if "--map" not in args and not any(arg.startswith("--map=") for arg in args):
        args = ["--map", str(ROOT / "assets/maps/first-room.tmx"), *args]
    # Relative custom map/capture arguments retain the caller's working directory.
    return subprocess.call(["qbs", "run", *common, "--no-build", "-p", "onekeyout", "config:debug", "--", *args])


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
    except subprocess.TimeoutExpired:
        raise SystemExit("Verification exceeded its 45 second limit.")
