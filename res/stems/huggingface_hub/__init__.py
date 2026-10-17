"""Stand-in for ``huggingface_hub``, limited to what the separator uses.

``demucs_onnx`` looks for its model with ``hf_hub_download``, imported lazily
inside its own helper, so the import never happens on a plain start-up and this
package is only reached when a model file is needed.

Going through the real client would mean installing it plus the fifteen
packages it pulls in (requests, certifi, urllib3, ...), and it would hide a
hundred megabyte download behind a spinner that says nothing. The engine
already knows which model belongs to the install and downloads it as part of
the install list, so the only thing needed at run time is to hand back the file
that is already there.

Layout the engine is installed to, relative to this file:

    engine/runtime/         the interpreter
    engine/site-packages/   the wheels
    engine/app/             this package and the worker
    engine/models/          the .onnx files
"""

from __future__ import annotations

import os
import sys
import urllib.error
import urllib.request
from pathlib import Path

from .errors import HfHubHTTPError, LocalEntryNotFoundError, RepositoryNotFoundError

__all__ = ["hf_hub_download", "HfHubHTTPError", "LocalEntryNotFoundError",
           "RepositoryNotFoundError"]

ENGINE_ROOT = Path(__file__).resolve().parent.parent.parent


def _models_dir() -> Path:
    """Where the model files are.

    The app tells us where they are when it runs the worker, because the worker
    also ships with the application and can be started from there, with this
    package sitting next to it instead of inside the engine. Failing that the
    interpreter is asked: an engine keeps it in ``runtime/``, one level above
    the ``models`` folder, and that holds however the engine was laid out.
    """
    from_env = os.environ.get("STEM_ENGINE_MODELS")
    if from_env:
        return Path(from_env)
    beside_this_file = ENGINE_ROOT / "models"
    if beside_this_file.is_dir():
        return beside_this_file
    beside_interpreter = Path(sys.executable).resolve().parent.parent / "models"
    if beside_interpreter.is_dir():
        return beside_interpreter
    return beside_this_file


MODELS_DIR = _models_dir()

_USER_AGENT = "mixxx-stem-engine/0.1"


def _local_model(filename: str) -> Path | None:
    """The installed file, or the same model at the other precision.

    The install picks one variant, and the variant decides the file name: an
    fp16 install has ``htdemucs_fp16weights.onnx`` on disk while the default
    precision asks for ``htdemucs.onnx``. Both hold the same graph, so the other
    precision of the *same* model is accepted.

    Nothing else is. Handing back whichever file happens to be around would look
    like it worked, and with ``htdemucs_ft`` — four specialists asked for by
    name — it would quietly separate with the wrong weights four times over.
    """
    exact = MODELS_DIR / filename
    if exact.is_file():
        return exact
    stem, extension = os.path.splitext(filename)
    suffix = "_fp16weights"
    alternative = stem[: -len(suffix)] if stem.endswith(suffix) else stem + suffix
    candidate = MODELS_DIR / (alternative + extension)
    if candidate.is_file():
        return candidate
    return None


def _download(repo_id: str, filename: str) -> Path:
    """Fetch the file to the models folder; only used when the install missed it."""
    MODELS_DIR.mkdir(parents=True, exist_ok=True)
    target = MODELS_DIR / filename
    url = f"https://huggingface.co/{repo_id}/resolve/main/{filename}"
    partial = target.with_suffix(target.suffix + ".part")
    request = urllib.request.Request(url, headers={"User-Agent": _USER_AGENT})
    print(f"stem engine: downloading {filename} into {MODELS_DIR}", file=sys.stderr)
    try:
        with urllib.request.urlopen(request, timeout=60) as response, \
                open(partial, "wb") as handle:
            total = int(response.headers.get("Content-Length") or 0)
            done = 0
            while True:
                chunk = response.read(1 << 20)
                if not chunk:
                    break
                handle.write(chunk)
                done += len(chunk)
                if total:
                    print("stem engine: model %d%%" % (done * 100 // total),
                          file=sys.stderr)
    except urllib.error.HTTPError as error:
        if partial.exists():
            partial.unlink()
        if error.code == 404:
            raise RepositoryNotFoundError(
                f"{repo_id} not found, or {filename} is not in it") from error
        raise HfHubHTTPError(f"HTTP {error.code} for {url}") from error
    except (urllib.error.URLError, OSError) as error:
        if partial.exists():
            partial.unlink()
        raise LocalEntryNotFoundError(
            f"could not download {filename} from {repo_id}: {error}") from error
    os.replace(partial, target)
    return target


def hf_hub_download(repo_id: str, filename: str, *, cache_dir=None,
                    token=None, revision=None, **kwargs) -> str:
    """Return the path of the model file, downloading it only if it is missing.

    The arguments the caller passes for the real client (``cache_dir``,
    ``token``, ``revision``, anything else) are accepted and ignored: an
    installed engine does not use a shared cache, and it never authenticates.
    """
    del cache_dir, token, revision, kwargs
    local = _local_model(filename)
    if local is not None:
        return str(local)
    return str(_download(repo_id, filename))
