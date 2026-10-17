#!/usr/bin/env python3
"""Installs the Linux engine from its manifest and separates a track with it.

The installer in Mixxx does all of this in C++, so what this checks is the data:
the interpreter, the wheels and the model named in ``engine-linux.json``,
together with the worker and the shim that travel with the application. Running
it on a real Linux machine answers the question the manifest cannot: does the
engine start and write a stem file there.

    python3 tools/validate_linux_engine.py [path/to/engine-linux.json]

Everything is left under /tmp/stem-engine-linux, so a second run reuses what was
downloaded and only costs the separation.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

ROOT = "/tmp/stem-engine-linux"
ENGINE = os.path.join(ROOT, "engine")
CACHE = os.path.join(ROOT, "cache")
APP = os.path.join(ENGINE, "app")
SITE = os.path.join(ENGINE, "site-packages")
MODELS = os.path.join(ENGINE, "models")


def download(url: str, name: str, sha256: str | None = None) -> str:
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, name)
    if os.path.isfile(path) and sha256:
        digest = hashlib.sha256()
        with open(path, "rb") as handle:
            for chunk in iter(lambda: handle.read(1 << 20), b""):
                digest.update(chunk)
        if digest.hexdigest() == sha256:
            print("  already there: %s" % name)
            return path
    print("  downloading %s" % name)
    with urllib.request.urlopen(url, timeout=180) as response, open(path, "wb") as out:
        shutil.copyfileobj(response, out)
    if sha256:
        digest = hashlib.sha256()
        with open(path, "rb") as handle:
            for chunk in iter(lambda: handle.read(1 << 20), b""):
                digest.update(chunk)
        if digest.hexdigest() != sha256:
            raise SystemExit("hash mismatch for %s" % name)
    return path


def install_runtime(item: dict) -> None:
    archive = download(item["url"], item["name"], item.get("sha256"))
    target = os.path.join(ENGINE, item.get("unpack_to") or "runtime")
    shutil.rmtree(target, ignore_errors=True)
    os.makedirs(target, exist_ok=True)
    strip = item.get("strip_prefix") or ""
    with tarfile.open(archive) as handle:
        members = handle.getmembers()
        for member in members:
            name = member.name
            if strip:
                if name == strip:
                    continue
                if not name.startswith(strip + "/"):
                    continue
                suffix = name[len(strip) + 1:]
                if not suffix:
                    continue
                member.name = suffix
            handle.extract(member, target, filter="data")
    print("  interpreter at %s" % os.path.join(target, "bin/python3"))


def install_wheels(items: list[dict]) -> None:
    os.makedirs(SITE, exist_ok=True)
    for item in items:
        archive = download(item["url"], os.path.basename(item["url"]), item.get("sha256"))
        with zipfile.ZipFile(archive) as handle:
            handle.extractall(SITE)
    print("  %d wheels in %s" % (len(items), SITE))


def nvidia_library_paths() -> list[str]:
    """The folders of the CUDA libraries that travel inside the wheels.

    On Windows the loader finds them through PATH; on Linux the same list has to
    be given as LD_LIBRARY_PATH, or onnxruntime cannot even load its CUDA
    provider.
    """
    nvidia = os.path.join(SITE, "nvidia")
    if not os.path.isdir(nvidia):
        return []
    paths = []
    for package in sorted(os.listdir(nvidia)):
        for candidate in ("lib", os.path.join("lib", "x86_64")):
            folder = os.path.join(nvidia, package, candidate)
            if os.path.isdir(folder):
                paths.append(folder)
    return paths


def install_model(models: dict, name: str) -> None:
    model = models[name]
    archive = download(model["url"], model["filename"], model.get("sha256"))
    os.makedirs(MODELS, exist_ok=True)
    target = os.path.join(MODELS, model["filename"])
    if not os.path.isfile(target):
        shutil.copyfile(archive, target)
    print("  model at %s" % target)


def install_app(resource_dir: str) -> None:
    shutil.rmtree(APP, ignore_errors=True)
    os.makedirs(APP, exist_ok=True)
    for name in ("stems_worker.py", "stem_mp4.py"):
        shutil.copy(os.path.join(resource_dir, name), os.path.join(APP, name))
    shutil.copytree(os.path.join(resource_dir, "huggingface_hub"),
            os.path.join(APP, "huggingface_hub"))
    print("  worker and shim in %s" % APP)


def mark_ffmpeg_executable() -> str:
    """What the installer does after unpacking: a ZIP does not carry the bit."""
    binaries = os.path.join(SITE, "imageio_ffmpeg", "binaries")
    for entry in sorted(os.listdir(binaries)):
        if entry.startswith("ffmpeg"):
            path = os.path.join(binaries, entry)
            os.chmod(path, os.stat(path).st_mode | 0o111)
            print("  encoder at %s" % path)
            return path
    raise SystemExit("the imageio_ffmpeg wheel brought no encoder")


def make_test_track(ffmpeg: str) -> str:
    track = os.path.join(ROOT, "prueba.wav")
    if not os.path.isfile(track):
        subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-y",
                        "-f", "lavfi", "-i", "sine=frequency=220:duration=12",
                        "-ar", "44100", "-c:a", "pcm_s16le", track], check=True)
    return track


def main() -> int:
    variant = "cpu"
    arguments = [a for a in sys.argv[1:] if not a.startswith("--")]
    for argument in sys.argv[1:]:
        if argument.startswith("--variant="):
            variant = argument.split("=", 1)[1]
    manifest_path = arguments[0] if arguments else os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            "res", "stems", "engine-linux.json")
    with open(manifest_path, encoding="utf-8") as handle:
        manifest = json.load(handle)
    resource_dir = os.path.dirname(os.path.abspath(manifest_path))
    if variant not in manifest["compute"]:
        raise SystemExit("the manifest has no %r variant (has %s)"
                         % (variant, ", ".join(manifest["compute"])))
    print("Manifest: %s (%s), variant %s"
          % (manifest_path, manifest["target"]["platform"], variant))

    print("Interpreter:")
    install_runtime(manifest["runtime"])
    print("Wheels:")
    install_wheels(manifest["common"])
    extra = manifest["compute"][variant]["items"]
    if extra:
        print("Wheels of the %s variant:" % variant)
        install_wheels(extra)
    print("Model:")
    install_model(manifest["models"], "htdemucs_fp16weights")
    print("Application files:")
    install_app(resource_dir)
    ffmpeg = mark_ffmpeg_executable()

    python = os.path.join(ENGINE, "runtime", "bin", "python3")
    if not os.path.isfile(python):
        raise SystemExit("the interpreter is not where the engine expects it: %s" % python)

    track = make_test_track(ffmpeg)
    output = os.path.join(ROOT, "prueba.stem.mp4")
    if os.path.isfile(output):
        os.remove(output)

    environment = dict(os.environ)
    # What Mixxx passes on: the libraries of the engine, where the models are,
    # and for the CUDA variant the folders of its own CUDA libraries.
    environment["PYTHONPATH"] = SITE
    environment["STEM_ENGINE_MODELS"] = MODELS
    environment["PYTHONIOENCODING"] = "utf-8"
    environment["PYTHONUNBUFFERED"] = "1"
    libraries = nvidia_library_paths()
    if libraries:
        environment["LD_LIBRARY_PATH"] = os.pathsep.join(
                libraries + [environment.get("LD_LIBRARY_PATH", "")]).strip(os.pathsep)
        print("CUDA libraries from: %s" % ", ".join(libraries))

    probe = subprocess.run([python, os.path.join(APP, "stems_worker.py"), "probe",
                            "--ffmpeg", ffmpeg],
            env=environment, capture_output=True, text=True)
    print("Probe: %s" % probe.stdout.strip()[:400])

    # The worker speaks of providers, the manifest of variants: cuda and cpu.
    provider = "cuda" if variant == "nvidia" else variant
    command = [python, os.path.join(APP, "stems_worker.py"), "separate",
               "--input", track, "--out", output, "--provider", provider,
               "--codec", "aac", "--model", "htdemucs",
               "--precision", "fp16weights", "--ffmpeg", ffmpeg]
    print("Separating:")
    print("  %s" % " ".join(command))
    result = subprocess.run(command, env=environment, capture_output=True, text=True)
    for line in result.stdout.splitlines():
        print("  %s" % line)
    if result.returncode != 0 or not os.path.isfile(output):
        print(result.stderr[-4000:])
        raise SystemExit("the engine did not write a stem file (exit %d)"
                         % result.returncode)

    size = os.path.getsize(output)
    print("Stem file written: %s (%d bytes, %.1f MB)"
          % (output, size, size / 1e6))
    check = subprocess.run([python, os.path.join(APP, "stem_mp4.py"), "verify", output],
            env=environment, capture_output=True, text=True)
    print("Container report: %s" % check.stdout.strip())
    return 0


if __name__ == "__main__":
    sys.exit(main())
