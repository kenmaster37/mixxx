#!/usr/bin/env python3
"""Creates the Python environment the stem generator runs in.

Mixxx looks for an interpreter in this order:

1. the interpreter set in Preferences > Stem Generation,
2. ``<settings folder>/stems/venv`` (this is where the script installs),
3. the ``python`` command on the PATH.

Usage
-----
    python res/stems/setup_venv.py [--cpu] [--settings <folder>]

``--cpu`` installs the plain CPU build of ONNX Runtime instead of the CUDA one
(the separation then takes about four times as long). ``--settings`` points at
the folder Mixxx uses for its configuration, for a portable or a test profile;
by default it is the same folder Mixxx picks.
"""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REQUIREMENTS = os.path.join(HERE, "requirements.txt")
MIN_PYTHON = (3, 9)


def default_settings_folder() -> str:
    if platform.system() == "Windows":
        base = os.environ.get("LOCALAPPDATA") or os.path.expanduser("~")
        return os.path.join(base, "Mixxx")
    return os.path.join(os.path.expanduser("~"), ".mixxx")


def find_python() -> list[str]:
    """The interpreter that will create the environment (not the one inside)."""
    candidates = [sys.executable, "python3", "python"]
    for candidate in candidates:
        if not candidate:
            continue
        path = shutil.which(candidate) if not os.path.isabs(candidate) else candidate
        if not path:
            continue
        try:
            output = subprocess.run(
                [path, "-c", "import sys; print('%d.%d' % sys.version_info[:2])"],
                capture_output=True, check=True, text=True).stdout.strip()
        except (OSError, subprocess.CalledProcessError):
            continue
        version = tuple(int(part) for part in output.split("."))
        if version >= MIN_PYTHON:
            return [path]
    raise SystemExit(
            "No Python %d.%d or newer found. Install it first, then run this again."
            % MIN_PYTHON)


def venv_python(venv_dir: str) -> str:
    if platform.system() == "Windows":
        return os.path.join(venv_dir, "Scripts", "python.exe")
    return os.path.join(venv_dir, "bin", "python")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpu", action="store_true",
            help="install the CPU build of ONNX Runtime instead of the CUDA one")
    parser.add_argument("--settings", default=None,
            help="Mixxx settings folder (default: %s)" % default_settings_folder())
    args = parser.parse_args()

    settings = args.settings or default_settings_folder()
    venv_dir = os.path.join(settings, "stems", "venv")
    print("Carpeta de configuracion: %s" % settings)
    print("Entorno: %s" % venv_dir)

    requirements = REQUIREMENTS
    if args.cpu:
        # Same file, with the CUDA runtime swapped for the CPU one.
        requirements = os.path.join(settings, "stems", "requirements-cpu.txt")
        os.makedirs(os.path.dirname(requirements), exist_ok=True)
        with open(REQUIREMENTS, encoding="utf-8") as source, \
                open(requirements, "w", encoding="utf-8") as target:
            for line in source:
                stripped = line.strip()
                if stripped.startswith("onnxruntime-gpu"):
                    target.write("onnxruntime\n")
                elif stripped.startswith("onnxruntime"):
                    continue
                # The nvidia-* wheels are the CUDA runtime, useless without it.
                elif stripped.startswith("nvidia-"):
                    continue
                else:
                    target.write(line)

    if not os.path.exists(venv_python(venv_dir)):
        print("Creando el entorno virtual...")
        os.makedirs(os.path.dirname(venv_dir), exist_ok=True)
        subprocess.run(find_python() + ["-m", "venv", venv_dir], check=True)
    else:
        print("El entorno virtual ya existe, se actualizan los paquetes.")

    print("Instalando las dependencias (varios cientos de MB, tarda un rato)...")
    subprocess.run([venv_python(venv_dir), "-m", "pip", "install", "--upgrade", "pip"],
            check=True)
    subprocess.run([venv_python(venv_dir), "-m", "pip", "install", "-r", requirements],
            check=True)

    print()
    print("Listo. En Mixxx: Preferencias > Stem Generation > Interprete, pon:")
    print("    %s" % venv_python(venv_dir))
    print("y pulsa COMPROBAR para ver si detecta la GPU.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
