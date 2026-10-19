#!/usr/bin/env python3
"""Weak stand-ins for what the native platform layer doesn't do yet: every retail function or Platform:: function the game's
objects call that nothing defines gets a weak definition that logs its name the first time it's called and returns 0. A real
definition anywhere (src/platform/native/, the agents' graphics/ and math/) takes its place at link time. The log is the list of
what the game reached that isn't there yet; nothing pretends to work.

    native/tools/make_stubs.py <out.cpp> <objects...>
"""
import json, os, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import host  # noqa: E402

WINDOWS = sys.platform == "win32"
# The host's nm and c++filt: LLVM's next to the compiler on Windows (LLVM-MinGW has no c++filt of that name)
if WINDOWS:
    TOOLS = Path(host.CLANG).parent if os.path.dirname(host.CLANG) else Path(".")
    NM = str(TOOLS / "llvm-nm.exe") if (TOOLS / "llvm-nm.exe").exists() else "llvm-nm"
    CXXFILT = str(TOOLS / "llvm-cxxfilt.exe") if (TOOLS / "llvm-cxxfilt.exe").exists() else "llvm-cxxfilt"
else:
    NM, CXXFILT = "nm", "c++filt"

out, objects = Path(sys.argv[1]), []
for argument in sys.argv[2:]:
    # @file: the objects one a line (native/CMakeLists.txt: Windows' command lines can't hold them all)
    if argument.startswith("@"):
        objects += [line.strip() for line in Path(argument[1:]).read_text().splitlines() if line.strip()]
    else:
        objects.append(argument)
defined, undefined = set(), set()
# Windows' command lines are at most 32767 characters
CHUNK = 40 if WINDOWS else 200
for chunk in range(0, len(objects), CHUNK):
    lines = subprocess.run([NM, "-A"] + objects[chunk:chunk + CHUNK], capture_output=True, text=True).stdout.splitlines()
    for line in lines:
        parts = line.split()
        if len(parts) >= 3 and parts[-2] in "TtDdBbSsCVWR" and len(parts[-2]) == 1:
            defined.add(parts[-1])
        elif len(parts) >= 2 and parts[-2] == "U":
            undefined.add(parts[-1])
missing = undefined - defined

prefix = "_" if sys.platform == "darwin" else ""
types = json.loads((HERE / "build" / "native" / "data_types.json").read_text())
retail_functions = {prefix + label for label, info in types.items() if info["kind"] == "function"}
# The retail data (the split's labels): converted by native/tools/convert_data.py, so none should be missing
split_labels = {line.split()[1] for path in (HERE / "asm" / "data").glob("*.s") for line in path.read_text().splitlines()
                if line.startswith(("dlabel ", "glabel "))}
retail_variables = {prefix + label for label, info in types.items() if info["kind"] == "var" and label in split_labels}

def demangle(names):
    result = subprocess.run([CXXFILT], input="\n".join(names), capture_output=True, text=True).stdout.splitlines()
    return dict(zip(names, result))

# Functions the PS2 platform files define that the game calls by their C names (extern "C", no RETAIL): the console's services
import re
ps2_sources = "\n".join(p.read_text(errors="ignore") for p in (HERE / "src" / "platform" / "ps2").rglob("*.cpp"))
def defined_by_ps2(name):
    bare = name[1:] if sys.platform == "darwin" and name.startswith("_") and not name.startswith("__Z") else name
    return re.match(r"^[A-Za-z_]\w*$", bare) is not None and (
        re.search(r"\b" + re.escape(bare) + r"\s*\(", ps2_sources) is not None
        or re.search(r"RETAIL(?:_N32)?\(" + re.escape(bare) + r"\)", ps2_sources) is not None)

# What the system's libraries define (the C library, libc++, SDL): never stood in for, the linker finds them there
def system_symbols():
    found = set()
    if sys.platform == "darwin":
        sdk = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True).stdout.strip()
        for tbd in list(Path(sdk, "usr", "lib").rglob("*.tbd")):
            found.update(re.findall(r"\b_[A-Za-z_][\w$]*", tbd.read_text(errors="ignore")))
        for library in Path("/opt/homebrew/lib").glob("libSDL3*.dylib"):
            found.update(l.split()[-1] for l in subprocess.run(["nm", "-gU", str(library)], capture_output=True,
                                                               text=True).stdout.splitlines() if l.strip())
    elif WINDOWS:
        # The MinGW runtime's and C++ library's (the toolchain's lib folder), Windows' own import libraries, SDL's and FFmpeg's
        # ($TWIN_HOST_LIBS, the import libraries the build links)
        libraries = []
        for folder in (TOOLS.parent / "x86_64-w64-mingw32" / "lib", TOOLS.parent / "lib"):
            for name in ("libucrt.a", "libucrtbase.a", "libmsvcrt.a", "libmingwex.a", "libmingw32.a", "libc++.a", "libunwind.a",
                         "libkernel32.a", "libuser32.a", "libwinpthread.a", "libpthread.a", "libm.a", "libmoldname.a",
                         "libapi-ms-win-crt-runtime-l1-1-0.a"):
                if (folder / name).exists():
                    libraries.append(str(folder / name))
        libraries += host.host_libs()
        for library in libraries:
            for l in subprocess.run([NM, "--defined-only", library], capture_output=True, text=True).stdout.splitlines():
                parts = l.split()
                if len(parts) >= 2 and not l.endswith(":"):
                    found.add(parts[-1])
    else:
        for library in ("/lib/x86_64-linux-gnu/libc.so.6", "/lib/x86_64-linux-gnu/libm.so.6",
                        "/lib/x86_64-linux-gnu/libstdc++.so.6"):
            if Path(library).exists():
                found.update(l.split()[-1].split("@")[0] for l in subprocess.run(
                    ["nm", "-D", "--defined-only", library], capture_output=True, text=True).stdout.splitlines() if l.strip())
    return found

system = system_symbols()
missing -= system
names = sorted(missing)
readable = demangle([n[1:] if sys.platform == "darwin" and n.startswith("__Z") else n for n in names]) if names else {}
stubs, variables = [], []
for name in names:
    pretty = readable.get(name[1:] if sys.platform == "darwin" and name.startswith("__Z") else name, name)
    bare_name = name[1:] if sys.platform == "darwin" and name.startswith("_") else name
    if name in retail_variables or bare_name in split_labels:
        # Retail data is never stood in for by a function: a missing object fails the build
        variables.append(pretty)
    elif name in retail_functions or pretty.startswith("Platform::") or "Pool" in pretty or defined_by_ps2(name):
        stubs.append((name, pretty))

lines = ["// Generated by native/tools/make_stubs.py: what the native platform layer doesn't do yet. Don't edit",
         '#include "native.h"', "", "#include <atomic>", "#include <cstdint>", "", "namespace", "{",
         "void Missing(std::atomic<bool>& said, const char* name)", "{",
         "    if (!said.exchange(true) && Native::GetSettings().logStubs)", "    {",
         '        Native::Log("not implemented yet: %s", name);', "    }", "}", "}", "", 'extern "C"', "{"]
for index, (name, pretty) in enumerate(stubs):
    raw = name[1:] if sys.platform == "darwin" else name
    escaped = pretty.replace("\\", "\\\\").replace('"', '\\"')
    lines += [f'__attribute__((weak)) std::uintptr_t Stub{index}() asm("{name}");',
              f"std::uintptr_t Stub{index}()", "{", f"    static std::atomic<bool> said;",
              f'    Missing(said, "{escaped}");', "    return 0;", "}"]
lines.append("}")
out.write_text("\n".join(lines) + "\n")
print(f"{out.name}: {len(stubs)} stand-ins", file=sys.stderr)
if variables:
    sys.exit("retail variables nothing defines (native/tools/convert_data.py should have): " + ", ".join(variables))
