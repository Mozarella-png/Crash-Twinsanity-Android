#!/usr/bin/env python3
"""The C++ type of every retail symbol the C++ declares (RETAIL(name): `extern T x asm("name")`), from clang's AST dump of each
file built natively. Writes build/native/data_types.json: {label: {"name": C++ name, "type": type, "canonical": canonical type,
"kind": "var" | "function", "method": a member function's}}. The data converter (native/tools/convert_data.py) lays the split's data out by these types."""
import json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import host  # noqa: E402
FLAGS = ["-std=gnu++2c", "-fsyntax-only", "-DTWIN_NATIVE", "-Inative/include", "-Iinclude", "-fno-exceptions", "-fno-rtti",
         "-Wno-everything", "-Xclang", "-ast-dump"] + host.host_includes() + host.native_flags()
DECL = re.compile(r"^[\s|`-]*(VarDecl|FunctionDecl|CXXMethodDecl)\s+0x[0-9a-f]+\s.*?\s(\w+)\s'([^']*)'(?::'([^']*)')?")
# Some clang builds (Debian's 21.1) print IsLiteralLabel after the label
LABEL = re.compile(r"^[\s|`-]*AsmLabelAttr\s+0x[0-9a-f]+\s.*\s\"([^\"]+)\"(?:\s+IsLiteralLabel)?\s*$")

def scan(path):
    out = subprocess.run([host.CLANG] + FLAGS + [path], capture_output=True, text=True, cwd=HERE).stdout.splitlines()
    found, pending = {}, None

    def commit_unlabelled():
        # A variable declared without RETAIL in extern "C" is the retail symbol of its own name (G_GameController); a C++
        # one's mangled name matches no retail label, so taking every variable's name is harmless
        if pending and pending[0] == "VarDecl" and pending[1] not in found:
            kind, name, typ, canonical, defined = pending
            found[name] = {"name": name, "type": typ, "canonical": canonical, "kind": "var", "defined": defined,
                           "unlabelled": True}

    for line in out:
        m = DECL.match(line)
        if m:
            commit_unlabelled()
            # A variable declared without extern is defined by the C++ (the converter leaves it out)
            rest = line[m.end():]
            defined = m.group(1) == "VarDecl" and not re.search(r"\bextern\b", rest)
            pending = (m.group(1), m.group(2), m.group(3), m.group(4) or m.group(3), defined)
            continue
        m = LABEL.match(line)
        if m and pending:
            # common.h's RETAIL puts Mach-O's underscore in front of the retail name
            label = m.group(1)[1:] if sys.platform == "darwin" and m.group(1).startswith("_") else m.group(1)
            kind, name, typ, canonical, defined = pending
            previous = found.get(label, {})
            if previous.get("unlabelled"):
                previous = {}
            found[label] = {"name": name, "type": typ, "canonical": canonical,
                                 "kind": "var" if kind == "VarDecl" else "function",
                                 "defined": defined or previous.get("defined", False)}
            # A method's type leaves its object out (native/tools/check_conventions.py counts it)
            if kind == "CXXMethodDecl":
                found[label]["method"] = True
            pending = None
    commit_unlabelled()
    return found

# The native build's C++: the game's, the native platform's and the PS2 platform files it reuses (native/reused_ps2.txt)
reused = [l.strip() for l in (HERE / "native" / "reused_ps2.txt").read_text().splitlines() if l.strip() and not l.startswith("#")]
files = sorted(str(p.relative_to(HERE)) for p in (HERE / "src").rglob("*.cpp")
               if (p.relative_to(HERE / "src").parts[0] != "platform" or p.relative_to(HERE / "src").parts[1] == "native")
               and p.name != "abi.cpp") + reused
# Each file's scan kept between runs (native/tools/host.py's FileCache): only the files that changed are scanned again
cache = host.FileCache("data_types_cache", " ".join([host.CLANG] + FLAGS) + (HERE / "native" / "tools" / "data_types.py").read_text())


def scan_cached(path):
    found = cache.get(HERE / path)
    if found is None:
        found = scan(path)
        cache.put(HERE / path, found)
    return found


def extent_rank(canonical):
    """How much of the retail data a declaration covers, to keep the widest of a label's: a sized array over a scalar (the C++
    reads several retail labels as one: g_ShaderType0BEntries[2] over g_ShaderType0BSize, the stream's 2048 byte scratch
    buffer over the decals' int), the larger count first; an array without its size last (the split's size is all it says)"""
    m = re.search(r"\[(\d*)\]$", canonical.strip())
    if m is None:
        return (1, 1)
    if m.group(1) == "":
        return (0, 0)
    return (2, int(m.group(1)))


types = {}
with ThreadPoolExecutor(10) as pool:
    for found in pool.map(scan_cached, files):
        for label, info in found.items():
            if label in types and types[label].get("defined"):
                info["defined"] = True
            if label in types and types[label]["canonical"] != info["canonical"]:
                # Declared differently in two files: the widest kept
                if extent_rank(info["canonical"]) <= extent_rank(types[label]["canonical"]):
                    types[label]["defined"] = types[label].get("defined", False) or info.get("defined", False)
                    continue
            types[label] = info
cache.save()
out = HERE / "build" / "native" / "data_types.json"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(types, indent=1, sort_keys=True))
print(f"{len(types)} retail symbols ({sum(t['kind'] == 'var' for t in types.values())} variables) -> {out.relative_to(HERE)}")
