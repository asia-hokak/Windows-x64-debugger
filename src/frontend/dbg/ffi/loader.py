from __future__ import annotations

import ctypes
import pathlib
import sys

from .errors import DllLoadError


def load_backend_dll(path: str | None = None) -> ctypes.WinDLL:
    if path is None:
        if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"): # for pyinstaller
            dll_path = pathlib.Path(getattr(sys, "_MEIPASS")) / "dbg" / "lib" / "dbg_backend.dll" # pyinstaller 的暫存資料夾
        else:
            dll_path = pathlib.Path("dbg/lib/dbg_backend.dll")
    else:
        dll_path = pathlib.Path(path)

    dll_path = dll_path.resolve()

    if not dll_path.exists():
        raise DllLoadError(f"backend DLL not found: {dll_path}")

    if not dll_path.is_file():
        raise DllLoadError(f"backend DLL path is not a file: {dll_path}")

    try:
        return ctypes.WinDLL(str(dll_path))
    except OSError as e:
        raise DllLoadError(f"failed to load backend DLL: {dll_path}") from e
