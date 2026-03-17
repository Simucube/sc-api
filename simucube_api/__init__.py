"""Simucube API - Python bindings for Simucube device control."""

import sys

if sys.platform != "win32":
    raise ImportError(
        "simucube-api only supports Windows. "
        "The underlying Simucube hardware interface requires Windows IPC mechanisms."
    )

from simucube_api._native import __version__  # noqa: E402

__all__ = ["__version__"]
