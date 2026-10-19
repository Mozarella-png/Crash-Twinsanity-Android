#!/usr/bin/env python3
"""Finds the casts the native build's 64 bit pointers break and the compiler doesn't warn about: a 32 bit integer made a pointer
(reinterpret_cast<T*>(u32), a C cast), which on the PS2 was an address and on the host loses the top half. Reads clang's AST dump
of every C++ file (with TWIN_NATIVE) and prints each one's place once.

    native/tools/find_narrow_pointer_casts.py [files...]
"""
import re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
FLAGS = ["-std=gnu++2c", "-fsyntax-only", "-DTWIN_NATIVE", "-Inative/include", "-Iinclude", "-fno-exceptions", "-fno-rtti",
         "-Wno-everything", "-Xclang", "-ast-dump"]
NARROW = re.compile(r"'(?:u32|s32|u16|s16|u8|s8|unsigned int|int|unsigned short|short|unsigned char|char)'(?::'[^']*')?")
LOC = re.compile(r"<(?:(/[^:>]+|[^<:>]+\.(?:cpp|h)):(\d+):\d+|line:(\d+):\d+|col:\d+)")

def scan(path):
    out = subprocess.run(["clang++"] + FLAGS + [str(path)], capture_output=True, text=True, cwd=HERE).stdout.splitlines()
    found, file, line = set(), None, None
    for i, text in enumerate(out):
        m = LOC.search(text)
        if m:
            if m.group(1):
                file, line = m.group(1), int(m.group(2))
            elif m.group(3):
                line = int(m.group(3))
        if "<IntegralToPointer>" not in text or file is None:
            continue
        # The cast's operand is the next node: a 32 bit integer is the PS2's address
        if i + 1 < len(out) and NARROW.search(out[i + 1].split("'", 1)[-1] and out[i + 1]):
            rel = str(Path(file).resolve().relative_to(HERE)) if file.startswith(str(HERE)) or not file.startswith("/") else file
            if rel.startswith(("src/", "include/")):
                found.add(f"{rel}:{line}")
    return found

files = sys.argv[1:] or sorted(str(p.relative_to(HERE)) for p in (HERE / "src").rglob("*.cpp")
                               if "platform" not in p.parts and p.name != "abi.cpp")
with ThreadPoolExecutor(10) as pool:
    results = set().union(*pool.map(scan, files))
for place in sorted(results, key=lambda s: (s.rsplit(":", 1)[0], int(s.rsplit(":", 1)[1]))):
    f, l = place.rsplit(":", 1)
    print(f"{place}: {(HERE / f).read_text(errors='ignore').splitlines()[int(l) - 1].strip()[:150]}")
print(f"{len(results)} places", file=sys.stderr)
