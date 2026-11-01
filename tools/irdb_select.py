#!/usr/bin/env python3
"""List the Flipper-IRDB files that can be uploaded to the SD card with tools/irdb_upload.py.

Usage:  python tools/irdb_select.py <irdb checkout> [--include-converted] [--only Folder1,Folder2]
Prints one absolute path per accepted file on stdout; a summary of the rejected ones goes to stderr.
Rules match the limits of the bench `sdput` command: path /irdb/<relative path> of at most 96 characters made of
letters, digits and / _ - . ( ) and spaces; size 1..65536 bytes. The `_Converted_` folder (about 56 MB of files
converted from other databases) is skipped unless --include-converted is given.
"""
import os
import re
import sys

ALLOWED = re.compile(r"^[A-Za-z0-9/_\-. ()]+$")
MAX_PATH = 96
MAX_SIZE = 65536


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    root = os.path.abspath(args[0])
    include_converted = "--include-converted" in args
    only = None
    if "--only" in args:
        only = set(args[args.index("--only") + 1].split(","))
    rejected = {"bad characters": 0, "path too long": 0, "too big": 0, "empty": 0}
    examples = {}
    accepted = 0
    for folder, dirs, files in os.walk(root):
        dirs[:] = sorted(d for d in dirs if not d.startswith(".") and (include_converted or d != "_Converted_"))
        rel_dir = os.path.relpath(folder, root).replace(os.sep, "/")
        if rel_dir != "." and only and rel_dir.split("/")[0] not in only:
            continue
        for name in sorted(files):
            if not name.endswith(".ir"):
                continue
            full = os.path.join(folder, name)
            rel = (name if rel_dir == "." else rel_dir + "/" + name)
            if rel_dir == "." or (only and rel.split("/")[0] not in only):
                continue
            remote = "/irdb/" + rel
            size = os.path.getsize(full)
            reason = None
            if size == 0:
                reason = "empty"
            elif size > MAX_SIZE:
                reason = "too big"
            elif not ALLOWED.match(remote):
                reason = "bad characters"
            elif len(remote) > MAX_PATH:
                reason = "path too long"
            if reason:
                rejected[reason] += 1
                examples.setdefault(reason, rel)
                continue
            accepted += 1
            print(full)
    sys.stderr.write("accepted: %d\n" % accepted)
    for k, v in rejected.items():
        if v:
            sys.stderr.write("rejected (%s): %d, e.g. %s\n" % (k, v, examples[k]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
