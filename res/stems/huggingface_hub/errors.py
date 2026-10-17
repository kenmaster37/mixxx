"""The exception names ``demucs_onnx`` imports from ``huggingface_hub.errors``.

Only the identity matters: the separator catches these to turn a failure into a
readable message. They are subclasses of ``RuntimeError`` so anything that
catches broadly still works.
"""

from __future__ import annotations


class HfHubHTTPError(RuntimeError):
    """The server answered with an error status."""


class RepositoryNotFoundError(HfHubHTTPError):
    """The repository or the file in it does not exist."""


class LocalEntryNotFoundError(RuntimeError):
    """The file is not installed and could not be fetched."""
