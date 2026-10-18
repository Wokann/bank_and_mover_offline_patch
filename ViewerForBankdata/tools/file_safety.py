"""Standalone Viewer file policy; no dependency on the patch-build toolchain."""
from pathlib import Path
import stat


def require_plain_file(path, *, allow_missing=False):
    path = Path(path)
    if not path.is_absolute():
        path = Path.cwd() / path
    # Do not resolve/collapse '..' first: a preceding component could be a link.
    for candidate in (*reversed(path.parents), path):
        try:
            info = candidate.lstat()
        except FileNotFoundError:
            if allow_missing:
                continue
            raise
        if (stat.S_ISLNK(info.st_mode) or
                getattr(info, "st_file_attributes", 0) & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)):
            raise ValueError(f"filesystem links are not permitted: {candidate}")
        if candidate == path:
            if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
                raise ValueError(f"expected an independent regular file: {candidate}")
        elif not stat.S_ISDIR(info.st_mode):
            raise ValueError(f"expected an ordinary directory: {candidate}")
    return path
