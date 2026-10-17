#!/usr/bin/env python3
"""Builds ``engine.json``: the exact list of files the stem engine needs.

The engine is meant to run on a machine with no Python installed, so the
installer (in Mixxx, in C++) has to download and unpack everything itself. To
be able to show the user what that costs *before* downloading anything, the
list it works from has to carry real sizes and real hashes. That is what this
script produces; it is a development tool and needs a Python on the build
machine, unlike the engine it describes.

    python tools/make_manifest.py --runtime-url <python zip url>

The result is ``engine.json`` next to this file. Nothing here touches the
engine itself: it only queries:

* pip, for the wheels of the runtime dependencies (resolved for the *target*
  platform, not for the machine that runs this script),
* the file host, for the size of every wheel,
* the Python release, for the size and hash of the interpreter,
* Hugging Face, for the model size and its LFS hash.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ENGINE_ROOT = os.path.dirname(HERE)
MANIFEST_PATH = os.path.join(ENGINE_ROOT, "engine.json")

ENGINE_VERSION = "0.1.0"
PROTOCOL_VERSION = 1

# What the worker imports at run time. demucs_onnx pulls onnxruntime, numpy,
# soundfile (audio files), soxr (resampling) and tqdm; huggingface_hub is only
# imported inside its download helper, which the engine replaces with its own
# downloader (see notes in README.md), so the ~15 packages it drags in are not
# on the list on purpose.
#
# imageio-ffmpeg is there for the encoder: writing the .stem.mp4 runs the ffmpeg
# program, and Mixxx cannot borrow its own copy (it links the libraries, it does
# not ship the tool). The wheel carries a static build, which keeps the install
# to one item instead of "go and find ffmpeg".
COMMON_REQUIREMENTS = ["demucs-onnx", "numpy", "soundfile", "soxr", "tqdm", "imageio-ffmpeg"]
CPU_REQUIREMENTS = ["onnxruntime"]
NVIDIA_REQUIREMENTS = ["onnxruntime-gpu[cuda,cudnn]", "nvidia-cublas", "nvidia-nvjitlink"]

# demucs-onnx depends on huggingface_hub, which brings thirteen more packages
# (click, filelock, fsspec, hf-xet, httpx2, pyyaml, ...). The engine ships its
# own two file stand-in instead (app/huggingface_hub), so downloading them would
# be five megabytes of dead weight: the separator only reaches for that module
# inside its model downloader, and that call is answered locally.
#
# Dropping them is safe but not free: it is only correct as long as demucs-onnx
# keeps importing it lazily. If a future version imports it up front, the
# install fails loudly with an ImportError rather than silently, and these names
# have to go back in the list.
REPLACED_BY_ENGINE = {
    "huggingface_hub", "click", "filelock", "fsspec", "hf-xet", "httpx2",
    "httpcore2", "pyyaml", "anyio", "idna", "truststore", "h11",
}
# typing_extensions was on that list too and had to come back: soundfile
# imports it directly, so it is not only there for huggingface_hub. The self
# check found it (see tests), which is why it exists.


def drop_replaced(items: list[dict]) -> list[dict]:
    return [item for item in items if item["name"] not in REPLACED_BY_ENGINE]


# Wheels that install the same importable package. onnxruntime and
# onnxruntime-gpu both provide `onnxruntime`, and whichever is unpacked last
# wins, so leaving them in the shared part of the list would make the available
# providers depend on the order of two downloads. They stay per variant.
VARIANT_SPECIFIC_WHEELS = {"onnxruntime", "onnxruntime-gpu"}

# Where the worker looks for the model, relative to the engine root.
MODEL_DIR = "models"

# The separator downloads its model from Hugging Face. The engine downloads the
# same files itself so the model is part of the install list instead of a
# surprise on the first job. Only one model is installed, so they are a choice
# and not something to add up.
#
# The fp16 variant holds the same graph with half precision *weights*: the math
# still runs in fp32, so speed and memory are identical and only the download
# and the disk footprint shrink (max difference against fp32 is around 6e-5).
HF_MODELS = {
    "htdemucs": {
        "repo": "StemSplitio/htdemucs-onnx",
        "filename": "htdemucs.onnx",
        "label": "htdemucs (fp32 weights, best fidelity)",
    },
    "htdemucs_fp16weights": {
        "repo": "StemSplitio/htdemucs-onnx",
        "filename": "htdemucs_fp16weights.onnx",
        "label": "htdemucs (fp16 weights, half the download)",
    },
}


def pip_resolve(requirements: list[str], platforms: list[str], python_version: str,
                implementation: str) -> list[dict]:
    """Ask pip which wheels it would install, without installing them.

    ``--target`` with ``--platform`` is what makes pip resolve for another
    platform; ``--only-binary`` is required for that combination and also
    guarantees we never get a source distribution that would need a compiler.

    More than one platform tag can be given, and on Linux it has to be: a wheel
    is tagged with an exact manylinux generation and these packages do not agree
    on one (numpy says manylinux_2_28, imageio-ffmpeg still says
    manylinux2014), and pip does not expand one into the others.
    """
    with tempfile.TemporaryDirectory(prefix="stem-engine-manifest-") as tmp:
        report_path = os.path.join(tmp, "report.json")
        command = [
            sys.executable, "-m", "pip", "install",
            "--dry-run", "--quiet", "--no-input", "--ignore-installed",
            "--only-binary=:all:",
            "--target", os.path.join(tmp, "target"),
        ]
        for platform in platforms:
            command += ["--platform", platform]
        command += [
            "--python-version", python_version,
            "--implementation", implementation,
            "--report", report_path,
            *requirements,
        ]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode != 0:
            raise SystemExit("pip could not resolve the wheels:\n%s\n%s"
                             % (result.stdout, result.stderr))
        with open(report_path, encoding="utf-8") as handle:
            return json.load(handle)["install"]


def item_from_report(entry: dict) -> dict:
    info = entry["download_info"]
    archive = info.get("archive_info", {})
    hashes = archive.get("hashes") or {}
    sha256 = hashes.get("sha256")
    if not sha256 and archive.get("hash", "").startswith("sha256="):
        sha256 = archive["hash"].split("=", 1)[1]
    metadata = entry["metadata"]
    return {
        "name": metadata["name"].lower(),
        "version": metadata["version"],
        "url": info["url"],
        "sha256": sha256,
        "size": None,
        "kind": "wheel",
    }


def head_size(url: str) -> int | None:
    """Size of a file without downloading it."""
    request = urllib.request.Request(url, method="HEAD",
                                     headers={"User-Agent": "stem-engine-manifest"})
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            length = response.headers.get("Content-Length")
            return int(length) if length else None
    except (urllib.error.URLError, OSError) as error:
        print("  warning: could not read the size of %s (%s)" % (url, error))
        return None


def fill_sizes(items: list[dict]) -> None:
    for item in items:
        if item.get("size") is None:
            item["size"] = head_size(item["url"])


def download_and_hash(url: str, path: str) -> tuple[int, str]:
    digest = hashlib.sha256()
    size = 0
    request = urllib.request.Request(url, headers={"User-Agent": "stem-engine-manifest"})
    with urllib.request.urlopen(request, timeout=120) as response, \
            open(path, "wb") as target:
        while True:
            chunk = response.read(1 << 20)
            if not chunk:
                break
            size += len(chunk)
            digest.update(chunk)
            target.write(chunk)
    return size, digest.hexdigest()


def runtime_item(url: str, unpack_to: str, strip_prefix: str = "") -> dict:
    with tempfile.TemporaryDirectory(prefix="stem-engine-runtime-") as tmp:
        name = url.rsplit("/", 1)[-1]
        path = os.path.join(tmp, name)
        print("  downloading %s to hash it" % name)
        size, sha256 = download_and_hash(url, path)
    item = {
        "name": os.path.basename(url),
        "version": "",
        "url": url,
        "sha256": sha256,
        "size": size,
        "kind": "runtime",
        "unpack_to": unpack_to,
    }
    if strip_prefix:
        # The interpreter of python-build-standalone comes inside a single
        # directory (`python/`), and the engine wants `runtime/bin/python3`
        # rather than `runtime/python/bin/python3`.
        item["strip_prefix"] = strip_prefix
    return item


def huggingface_models() -> dict[str, dict]:
    """Size and LFS hash of each model, from the Hub API (no download)."""
    models = {}
    for name, spec in HF_MODELS.items():
        api = "https://huggingface.co/api/models/%s?blobs=true" % spec["repo"]
        request = urllib.request.Request(api, headers={"User-Agent": "stem-engine-manifest"})
        with urllib.request.urlopen(request, timeout=60) as response:
            payload = json.load(response)
        entry = next((s for s in payload.get("siblings", [])
                      if s["rfilename"] == spec["filename"]), None)
        if entry is None:
            print("  warning: %s/%s not found in the repo listing"
                  % (spec["repo"], spec["filename"]))
            continue
        lfs = entry.get("lfs") or {}
        sha256 = lfs.get("sha256") or lfs.get("oid")
        models[name] = {
            "label": spec["label"],
            "version": payload.get("sha", "")[:12],
            "url": "https://huggingface.co/%s/resolve/main/%s"
                   % (spec["repo"], spec["filename"]),
            "sha256": sha256,
            "size": entry.get("size"),
            "filename": spec["filename"],
        }
    return models


def reuse_runtime(url: str, previous_path: str) -> dict | None:
    """Reuse the interpreter entry of an earlier run.

    Hashing the interpreter means downloading it, so an unchanged URL should
    not cost another 11 MB every time the wheel list is regenerated.
    """
    if not os.path.isfile(previous_path):
        return None
    try:
        with open(previous_path, encoding="utf-8") as handle:
            previous = json.load(handle)
    except (OSError, ValueError):
        return None
    runtime = previous.get("runtime")
    if runtime and runtime.get("url") == url and runtime.get("sha256"):
        return runtime
    return None


def total_of(items: list[dict]) -> int:
    return sum(item["size"] or 0 for item in items)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--runtime-url", required=True,
                        help="URL of the interpreter archive (python.org embeddable "
                             "zip on Windows, python-build-standalone on Linux)")
    parser.add_argument("--runtime-unpack-to", default="runtime",
                        help="folder inside the engine the interpreter goes to")
    parser.add_argument("--runtime-strip-prefix", default="",
                        help="directory of the archive to drop, e.g. 'python' "
                             "for the python-build-standalone archives")
    parser.add_argument("--no-nvidia", action="store_true",
                        help="leave out the CUDA variant, for a platform whose "
                             "NVIDIA wheel set does not resolve to the same "
                             "onnxruntime version as the CPU one")
    parser.add_argument("--nvidia-requirements", default="",
                        help="list to use instead of the built in CUDA requirements, "
                             "separated by semicolons because the extras of a "
                             "requirement contain commas, e.g. "
                             "\"onnxruntime-gpu[cuda,cudnn]==1.31.0\"")
    parser.add_argument("--platform", default="win_amd64",
                        help="target platform tag, or several separated by commas "
                             "when the wheels do not share one (pip accepts a "
                             "list and here they are all passed on)")
    parser.add_argument("--python-version", default="3.11")
    parser.add_argument("--implementation", default="cp")
    parser.add_argument("--out", default=MANIFEST_PATH)
    args = parser.parse_args()
    platforms = [p.strip() for p in args.platform.split(",") if p.strip()]

    print("Resolving the CPU install...")
    cpu_all = drop_replaced([item_from_report(entry) for entry in pip_resolve(
        COMMON_REQUIREMENTS + CPU_REQUIREMENTS,
        platforms, args.python_version, args.implementation)])
    if args.no_nvidia:
        nvidia_all = []
    else:
        print("Resolving the NVIDIA install...")
        requirements = NVIDIA_REQUIREMENTS
        if args.nvidia_requirements:
            requirements = [item.strip() for item in args.nvidia_requirements.split(";")
                            if item.strip()]
        nvidia_all = drop_replaced([item_from_report(entry) for entry in pip_resolve(
            COMMON_REQUIREMENTS + requirements,
            platforms, args.python_version, args.implementation)])

    # Splitting by difference rather than by hand: onnxruntime and
    # onnxruntime-gpu install the same import package, so whichever variant
    # gives them to us, the other one must not have them, or one wheel would
    # overwrite the files of the other. Same for anything a variant shares.
    cpu_keys = {(item["name"], item["version"]) for item in cpu_all}
    nvidia_keys = {(item["name"], item["version"]) for item in nvidia_all}
    if args.no_nvidia:
        # Without a second variant to compare against, everything the CPU
        # variant needs is shared.
        common = cpu_all
        cpu_only = []
        nvidia_only = []
    else:
        def shared(item: dict) -> bool:
            """Wheels that both variants need, and that are safe to share."""
            return ((item["name"], item["version"]) in nvidia_keys and
                    item["name"] not in VARIANT_SPECIFIC_WHEELS)

        def normal(item: dict) -> bool:
            return item["name"] not in VARIANT_SPECIFIC_WHEELS

        common = [item for item in cpu_all if shared(item)]
        cpu_only = [item for item in cpu_all if not shared(item) and normal(item)]
        # The CPU build of onnxruntime is named in the CUDA resolution too
        # (demucs-onnx asks for it), and it must not travel with the variant
        # that brings its own: the two libraries would sit on top of each other.
        nvidia_only = [item for item in nvidia_all
                       if (item["name"] == "onnxruntime-gpu" or
                               (normal(item) and
                                       (item["name"], item["version"]) not in cpu_keys))]

    print("Reading the sizes of the wheels...")
    fill_sizes(common)
    fill_sizes(cpu_only)
    fill_sizes(nvidia_only)

    print("Reading the interpreter...")
    runtime = reuse_runtime(args.runtime_url, args.out) or \
        runtime_item(args.runtime_url, args.runtime_unpack_to,
                     args.runtime_strip_prefix)

    print("Reading the models from Hugging Face...")
    models = huggingface_models()

    shared = [runtime] + common
    shared_size = total_of(shared)
    compute = {
        "cpu": {"label": "This computer only (CPU)",
                "items": cpu_only, "size": total_of(cpu_only)},
    }
    if not args.no_nvidia:
        compute["nvidia"] = {"label": "NVIDIA graphics card (CUDA)",
                             "items": nvidia_only,
                             "size": total_of(nvidia_only)}
    totals = {
        variant: {
            model: shared_size + data["size"] + (model_data.get("size") or 0)
            for model, model_data in models.items()
        }
        for variant, data in compute.items()
    }

    manifest = {
        "engine_version": ENGINE_VERSION,
        "protocol": PROTOCOL_VERSION,
        "generated_by": "tools/make_manifest.py",
        "target": {
            "platform": platforms[0],
            "platform_tags": platforms,
            "python_version": args.python_version,
            "implementation": args.implementation,
        },
        "runtime": runtime,
        "common": common,
        "compute": compute,
        "models": models,
        "totals": totals,
    }
    with open(args.out, "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2)
        handle.write("\n")

    def mb(value: int) -> str:
        return "%.1f MB" % (value / 1e6)

    print()
    print("Written to %s" % args.out)
    print("  interpreter  %8s  %s" % (mb(runtime["size"]), runtime["name"]))
    for item in common:
        print("  %-12s %8s  %s %s" % (item["name"], mb(item["size"] or 0),
                                      item["name"], item["version"]))
    print("  --- always installed: %s" % mb(shared_size))
    print()
    for variant, data in compute.items():
        print("  %s (%s):" % (variant, data["label"]))
        for item in data["items"]:
            print("    %-24s %8s  %s %s" % (item["name"], mb(item["size"] or 0),
                                            item["name"], item["version"]))
        print("    total with the runtime: %s" % mb(shared_size + data["size"]))
    print()
    for model, data in models.items():
        print("  model %-24s %8s  %s" % (model, mb(data["size"] or 0), data["label"]))
    print()
    for variant, per_model in totals.items():
        for model, size in per_model.items():
            print("  INSTALL %-8s + %-22s = %s" % (variant, model, mb(size)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
