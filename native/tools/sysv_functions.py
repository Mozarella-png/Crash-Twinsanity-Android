#!/usr/bin/env python3
"""The functions Windows' build calls the System V way (include/abi.h, TWIN_EABI_CC): every RETAIL_N32 declaration's, as a
list of their addresses for Abi::IsSysvFunction (src/platform/native/abi_win32.cpp).

    sysv_functions.py OUTPUT.cpp

A call through a retail vtable or function pointer goes the way of the function behind it, whatever its arguments: a vtable's
slot can hold a function taking floats and integers (System V's) and one that leaves the float out (Windows' own), and the
EABI the retail code was made for passes both alike. The references are weak: a declaration nothing defines is left out (0).
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LABEL = re.compile(r"RETAIL_N32\((\w+)\)")


def labels():
    found = set()
    for folder in (ROOT / "include", ROOT / "src"):
        for path in sorted(folder.rglob("*")):
            if path.suffix not in (".h", ".cpp") or path.name == "abi.h":
                continue
            found.update(LABEL.findall(path.read_text(encoding="utf-8", errors="replace")))
    return sorted(found)


def main():
    output = Path(sys.argv[1])
    names = labels()
    lines = [
        "// Made by native/tools/sysv_functions.py: the RETAIL_N32 functions' addresses (the System V convention's on Windows)",
        '#include "common.h"',
        "",
        'extern "C"',
        "{",
    ]
    lines += [f'    extern const char g_Sysv{index}[] asm("{name}") __attribute__((weak));' for index, name in enumerate(names)]
    lines += [
        "}",
        "",
        "namespace Abi",
        "{",
        "extern const void* const g_SysvFunctionList[];",
        "extern const u32 g_SysvFunctionCount;",
        "const void* const g_SysvFunctionList[] = {",
    ]
    lines += [f"    g_Sysv{index}," for index in range(len(names))]
    lines += ["};", f"const u32 g_SysvFunctionCount = {len(names)};", "}", ""]
    text = "\n".join(lines)
    if not output.exists() or output.read_text(encoding="utf-8") != text:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding="utf-8", newline="\n")
    print(f"{output.name}: {len(names)} functions")


if __name__ == "__main__":
    main()
