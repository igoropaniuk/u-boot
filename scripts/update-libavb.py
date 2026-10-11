#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
"""Resync lib/libavb with AOSP external/avb.

Usage: scripts/update-libavb.py [--rev REV] /path/to/external/avb

Copies the libavb sources from an external/avb checkout (the given revision,
default HEAD, checked out with 'git -C <avb> show') into lib/libavb, replacing
the MIT license blocks with SPDX identifiers the way the rest of U-Boot does.
The U-Boot glue stays untouched:

  avb_sysdeps.h, avb_sysdeps_posix.c   U-Boot sysdeps (printf, malloc, hang)
  crypto/avb_crypto_ops_impl.h         SHA contexts from lib/sha256.c, sha512.c
  crypto/sha_impl.c                    avb_sha* on top of U-Boot's SHA library

and the files U-Boot does not build are skipped (avb_crc32.c, the BoringSSL
and the bundled SHA implementations). Prints the synced revision for the
commit message and lists files that exist upstream but are not imported.
"""
import argparse
import os
import re
import subprocess
import sys

# Kept as U-Boot specific glue, never overwritten from upstream.
UBOOT_GLUE = {
    "avb_sysdeps.h",
    "avb_sysdeps_posix.c",
    "crypto/avb_crypto_ops_impl.h",
    "crypto/sha_impl.c",
}

# Upstream files U-Boot does not use.
SKIP = {
    "avb_crc32.c",             # unused, Apache-2.0
    "crypto/sha256_impl.c",    # lib/sha256.c is used instead
    "crypto/sha512_impl.c",    # lib/sha512.c is used instead
}
SKIP_DIRS = ("boringssl/",)

MIT_RE = re.compile(
    r"\A/\*\n \* Copyright \(C\) (?P<year>[0-9]{4}) The Android Open Source Project\n"
    r" \*\n \* Permission is hereby granted.*?SOFTWARE\.\n \*/\n",
    re.S)
CHROMIUM_RE = re.compile(r"Copyright \(c\) 20[0-9]{2} The Chromium OS Authors")


def convert_header(name, text):
    """Replace the MIT license block with an SPDX identifier."""
    m = MIT_RE.match(text)
    if not m:
        raise ValueError(f"{name}: no MIT license block at the top")
    spdx = "MIT"
    if CHROMIUM_RE.search(text):
        # avb_rsa.[ch] carry libmincrypt code under a BSD-style license
        spdx = "MIT OR BSD-3-Clause"
    if name.endswith(".h"):
        head = f"/* SPDX-License-Identifier: {spdx} */\n"
    else:
        head = f"// SPDX-License-Identifier: {spdx}\n"
    head += f"/*\n * Copyright (C) {m.group('year')} The Android Open Source Project\n */\n"
    return head + text[m.end():]


def git(avb, *args):
    return subprocess.run(["git", "-C", avb, *args], check=True,
                          capture_output=True, text=True).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--rev", default="HEAD", help="revision to sync (default HEAD)")
    parser.add_argument("avb", help="path to an external/avb git checkout")
    args = parser.parse_args()

    dest = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "lib", "libavb")
    dest = os.path.normpath(dest)
    rev = git(args.avb, "rev-parse", args.rev).strip()
    files = git(args.avb, "ls-tree", "-r", "--name-only", rev, "libavb").split()

    imported, skipped, kept = [], [], []
    for path in files:
        name = path[len("libavb/"):]
        if not name.endswith((".c", ".h")):
            skipped.append(name)
            continue
        if name in UBOOT_GLUE:
            kept.append(name)
            continue
        if name in SKIP or name.startswith(SKIP_DIRS):
            skipped.append(name)
            continue
        text = git(args.avb, "show", f"{rev}:{path}")
        text = convert_header(name, text)
        out = os.path.join(dest, name)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w") as f:
            f.write(text)
        imported.append(name)

    # files in lib/libavb that upstream no longer has
    stale = []
    for root, _dirs, names in os.walk(dest):
        for n in names:
            rel = os.path.relpath(os.path.join(root, n), dest)
            if rel.endswith((".c", ".h")) and rel not in imported and rel not in UBOOT_GLUE:
                stale.append(rel)

    version = git(args.avb, "show", f"{rev}:libavb/avb_version.h")
    ver = re.findall(r"#define AVB_VERSION_(?:MAJOR|MINOR|SUB) (\d+)", version)
    print(f"Synced lib/libavb with external/avb {rev[:12]} (libavb {'.'.join(ver)})")
    print(f"  imported: {len(imported)} files")
    print(f"  U-Boot glue kept: {', '.join(sorted(kept))}")
    print(f"  not imported: {', '.join(sorted(skipped))}")
    if stale:
        print("  in lib/libavb but not upstream (remove or keep deliberately):")
        for s in sorted(stale):
            print(f"    {s}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
