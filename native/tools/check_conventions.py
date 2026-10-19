#!/usr/bin/env python3
"""Checks the retail functions' calling conventions on Windows (include/abi.h): every function taking floats with its integers
(a method's object counts) is RETAIL_N32's (System V's there), and lists those RETAIL_N32 that take none.

    check_conventions.py [build/native/data_types.json]

Abi::CallEabi calls a function the System V way when it's RETAIL_N32's and Windows' own way otherwise, leaving a mixed call's
floats out: one that mixes them but isn't RETAIL_N32 would lose its floats there (and be called the wrong way by the retail
code's vtables on every host's EABI stand-in). Exits 1 when there's one.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FLOATS = {"f32", "float", "const f32", "const float"}


def parameters(canonical):
    """The parameters' types of a function type ("void (int *, f32) __attribute__((sysv_abi))")."""
    text = canonical.split(" __attribute__")[0].rstrip()
    if not text.endswith(")"):
        return None
    depth = 0
    for index in range(len(text) - 1, -1, -1):
        if text[index] == ")":
            depth += 1
        elif text[index] == "(":
            depth -= 1
            if depth == 0:
                inner = text[index + 1:-1]
                break
    else:
        return None
    params, depth, start = [], 0, 0
    for index, character in enumerate(inner):
        if character in "(<[":
            depth += 1
        elif character in ")>]":
            depth -= 1
        elif character == "," and depth == 0:
            params.append(inner[start:index].strip())
            start = index + 1
    if inner.strip():
        params.append(inner[start:].strip())
    return [p for p in params if p not in ("void", "...")]


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build" / "native" / "data_types.json"
    types = json.loads(path.read_text())
    if not any(info.get("method") for info in types.values()):
        sys.exit(f"{path} has no methods marked: made before data_types.py recorded them (build again)")
    missing, extra = [], []
    for label, info in sorted(types.items()):
        if info["kind"] != "function":
            continue
        params = parameters(info["canonical"])
        if params is None:
            continue
        floats = sum(p in FLOATS for p in params)
        others = len(params) - floats + (1 if info.get("method") else 0)
        mixed = floats > 0 and others > 0
        sysv = "sysv_abi" in info["canonical"]
        if mixed and not sysv:
            missing.append((label, info["name"], info["canonical"]))
        elif sysv and not mixed:
            extra.append((label, info["name"], info["canonical"]))
    for label, name, canonical in missing:
        print(f"mixed, not RETAIL_N32: {label} ({name}): {canonical}")
    for label, name, canonical in extra:
        print(f"RETAIL_N32 without floats and integers mixed: {label} ({name}): {canonical}")
    print(f"{len(missing)} mixed without RETAIL_N32, {len(extra)} RETAIL_N32 unmixed")
    sys.exit(1 if missing else 0)


if __name__ == "__main__":
    main()
