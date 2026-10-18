"""Reject filesystem aliases in managed files; never follows a linked directory.

Read-only: reports metadata, never deletes links or reads file payloads.
Missing outputs may be permitted, but existing ancestors are still checked.
"""
import argparse
import json
import os
from pathlib import Path
import stat


def link_kind(info):
    if stat.S_ISLNK(info.st_mode):
        return "symbolic-link"
    if getattr(info, "st_file_attributes", 0) & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400):
        return "reparse-point"
    if stat.S_ISREG(info.st_mode) and info.st_nlink > 1:
        return "hard-linked-file"
    return None


def inspect_paths(paths, *, recursive=False, allow_missing=False):
    issues, cached, unsafe, expanded, checked, skipped_git = [], {}, set(), set(), 0, 0

    def path_key(path):
        return os.path.normcase(os.path.normpath(str(path)))

    def inspect(path, missing_ok=False):
        nonlocal checked
        key = path_key(path)
        if key in cached:
            return cached[key]
        cached[key] = None
        try:
            info = path.lstat()
        except FileNotFoundError:
            if not missing_ok:
                issues.append({"path": str(path), "kind": "missing"})
                unsafe.add(key)
            return None
        except OSError as error:
            issues.append({"path": str(path), "kind": "unreadable-metadata", "error": str(error)})
            unsafe.add(key)
            return None
        checked += 1
        kind = link_kind(info)
        if kind:
            issue = {"path": str(path), "kind": kind}
            if kind == "hard-linked-file":
                issue["link_count"] = info.st_nlink
            else:
                try:
                    issue["target"] = os.readlink(path)
                except OSError:
                    pass
            issues.append(issue)
            unsafe.add(key)
            return None
        if not (stat.S_ISREG(info.st_mode) or stat.S_ISDIR(info.st_mode)):
            issues.append({"path": str(path), "kind": "not-regular-file-or-directory"})
            unsafe.add(key)
            return None
        cached[key] = info
        return info

    roots = [Path(path) if Path(path).is_absolute() else Path.cwd() / path for path in paths]
    for root in roots:
        unsafe_parent = False
        for parent in reversed(root.parents):
            inspect(parent, missing_ok=True)
            if path_key(parent) in unsafe:
                unsafe_parent = True
                break
        if unsafe_parent:
            continue
        info = inspect(root, missing_ok=allow_missing)
        if info is None or not recursive or not stat.S_ISDIR(info.st_mode):
            continue
        pending = [root]
        while pending:
            directory = pending.pop()
            key = path_key(directory)
            if key in expanded:
                continue
            expanded.add(key)
            try:
                with os.scandir(directory) as entries:
                    children = sorted((Path(entry.path) for entry in entries), key=str)
            except OSError as error:
                issues.append({"path": str(directory), "kind": "unreadable-directory", "error": str(error)})
                continue
            for child in children:
                if child.name == ".git":
                    skipped_git += 1
                    continue  # VCS administration is not a runtime/package input.
                child_info = inspect(child)
                if child_info is not None and stat.S_ISDIR(child_info.st_mode):
                    pending.append(child)
    return {"roots": list(map(str, roots)), "checked_entries": checked,
            "skipped_git_metadata": skipped_git, "issues": issues}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recursive", action="store_true")
    parser.add_argument("--allow-missing", action="store_true")
    parser.add_argument("--json", action="store_true", help="metadata-only JSON on standard output")
    parser.add_argument("paths", nargs="+")
    args = parser.parse_args()
    report = inspect_paths(args.paths, recursive=args.recursive, allow_missing=args.allow_missing)
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print(f"Checked {report['checked_entries']} filesystem entries (no linked-directory traversal)")
        for issue in report["issues"]:
            print(f"ERROR {issue['kind']}: {issue['path']}")
    raise SystemExit(bool(report["issues"]))


if __name__ == "__main__":
    main()
