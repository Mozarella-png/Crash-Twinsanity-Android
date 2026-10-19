#!/usr/bin/env python3
"""A report of a Windows profile ($TWIN_PROFILE, src/platform/native/profiler_win32.cpp): for each thread that was sampled most,
the functions its time was spent in (self: the innermost frame; total: anywhere on the stack), the executable's addresses named by
llvm-symbolizer, other modules' by their name.

    native/tools/profile_report.py PROFILE [--exe build/native/cmake/CrashTwinsanity.exe] [--threads 4] [--top 25]
"""
import argparse
import collections
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
IMAGE_BASE = 0x10000000


def symbolize(exe, offsets):
    tool = shutil.which("llvm-symbolizer")
    if tool is None or not offsets:
        return {}
    names = {}
    offsets = sorted(offsets)
    for start in range(0, len(offsets), 2000):
        chunk = offsets[start:start + 2000]
        result = subprocess.run([tool, "--obj=" + str(exe), "-f", "--no-inlines"], input="\n".join(hex(IMAGE_BASE + o) for o in chunk),
                                capture_output=True, text=True).stdout
        blocks = [b.splitlines() for b in re.split(r"\n\s*\n", result.strip())]
        for offset, block in zip(chunk, blocks):
            function = block[0] if block else "?"
            where = block[1] if len(block) > 1 else ""
            where = re.sub(r"^.*[\\/](src|include|build)[\\/]", r"\1/", where)
            names[offset] = f"{function} ({where})" if function != "??" else f"exe+{offset:x}"
    return names


def function_only(named):
    """A symbolized frame's function without its location and its arguments' list (the last balanced parentheses)"""
    named = re.sub(r"\s\([^()]*:\d+(:\d+)?\)$", "", named).strip()
    if not named.endswith(")"):
        return named
    depth = 0
    for at in range(len(named) - 1, -1, -1):
        depth += {")": 1, "(": -1}.get(named[at], 0)
        if depth == 0:
            return named[:at].strip()
    return named


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("profile")
    parser.add_argument("--exe", default=str(ROOT / "build/native/cmake/CrashTwinsanity.exe"))
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--top", type=int, default=25)
    parser.add_argument("--lines", action="store_true", help="by source line rather than by function")
    arguments = parser.parse_args()
    exe = Path(arguments.exe)
    samples = []
    offsets = set()
    for line in Path(arguments.profile).read_text().splitlines():
        thread, _, rest = line.partition(":")
        frames = rest.split()
        samples.append((thread, frames))
        for frame in frames:
            module, _, offset = frame.rpartition("+")
            if module.lower() == exe.name.lower():
                offsets.add(int(offset, 16))
    names = symbolize(exe, offsets)

    def name(frame):
        module, _, offset = frame.rpartition("+")
        if module.lower() == exe.name.lower():
            named = names.get(int(offset, 16), frame)
            # By function: the location dropped (and with it the arguments' list, long for templates)
            return named if arguments.lines else function_only(named)
        return module

    by_thread = collections.defaultdict(list)
    for thread, frames in samples:
        by_thread[thread].append([name(f) for f in frames])
    # A thread waiting is sampled as much as one working: the busiest are those whose innermost frame is least often the system's
    waiting = {"ntdll.dll", "KERNELBASE.dll", "win32u.dll", "kernel32.dll", "KERNEL32.DLL"}
    busy = lambda stacks: sum(1 for stack in stacks if stack[0] not in waiting)
    busiest = sorted(by_thread.items(), key=lambda item: -busy(item[1]))[:arguments.threads]
    for thread, stacks in busiest:
        total = len(stacks)
        print(f"== thread {thread}: {total} samples, {100.0 * busy(stacks) / total:.0f}% busy")
        own = collections.Counter(stack[0] for stack in stacks)
        anywhere = collections.Counter(n for stack in stacks for n in set(stack))
        print("  self:")
        for function, count in own.most_common(arguments.top):
            print(f"    {100.0 * count / total:5.1f}%  {function}")
        print("  total:")
        for function, count in anywhere.most_common(arguments.top):
            print(f"    {100.0 * count / total:5.1f}%  {function}")


if __name__ == "__main__":
    sys.exit(main())
