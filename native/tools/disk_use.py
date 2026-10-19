#!/usr/bin/env python3
"""The disk manager's pool through a log of $TWIN_DISK_TRACE (game/disk.cpp): how much of it was in use when each chunk's files
started loading and unloading and at each of the game's states, the most it held, every allocation it had no room for, and with
--by-caller what held it at the first of those (or at the end), by the code that asked for it (llvm-symbolizer names it).

    native/tools/disk_use.py LOG [--by-caller] [--exe build/native/cmake/CrashTwinsanity.exe]
"""
import argparse
import collections
import re
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BLOCK = 64
ALLOCATE = re.compile(r"disk: allocate (-?\d+) \((\d+) bytes at [0-9A-Fa-fx]+\) for ([0-9A-Fa-fx]+)")
RELEASE = re.compile(r"disk: release (-?\d+) \(state (\d+)")
# A block let go of while in use: released at once or a couple of frames later (the frames' releases aren't logged)
USED = "2"


def megabytes(size):
    return f"{size / 1048576:.2f} MB"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("log")
    parser.add_argument("--by-caller", action="store_true")
    parser.add_argument("--exe", default=str(ROOT / "build" / "native" / "cmake" / "CrashTwinsanity.exe"))
    args = parser.parse_args()

    live = {}
    total = peak = 0
    held = None
    for line in open(args.log, errors="replace"):
        if (match := ALLOCATE.search(line)):
            size = (int(match[2]) + BLOCK - 1) // BLOCK * BLOCK
            live[int(match[1])] = (size, hex(int(match[3], 16)))
            total += size
            peak = max(peak, total)
        elif (match := RELEASE.search(line)):
            if match[2] == USED and int(match[1]) in live:
                total -= live.pop(int(match[1]))[0]
        elif "disk: no room" in line:
            print(f"{megabytes(total)} in use, {line.strip()}")
            held = held or dict(live)
        elif "chunk: " in line or ("game: " in line and "the player at" not in line):
            print(f"{megabytes(total)} in use | {line.strip()}")
    print(f"the most in use: {megabytes(peak)}")

    if args.by_caller:
        held = held or live
        by = collections.Counter()
        count = collections.Counter()
        for size, caller in held.values():
            by[caller] += size
            count[caller] += 1
        names = {}
        tool = shutil.which("llvm-symbolizer")
        if tool is not None:
            result = subprocess.run([tool, "--obj=" + args.exe, "-f", "-i"], input="\n".join(by), capture_output=True,
                                    text=True).stdout
            for caller, block in zip(by, re.split(r"\n\s*\n", result.strip())):
                names[caller] = " < ".join(block.splitlines()[0::2])
        for caller, size in by.most_common():
            print(f"{megabytes(size):>9} in {count[caller]:5d} blocks for {caller} {names.get(caller, '')}")


if __name__ == "__main__":
    main()
