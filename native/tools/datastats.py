#!/usr/bin/env python3
"""Statistics of the data the native build has to bring over from the split (asm/data/*.s): each object's size, how many of
its words are pointers (symbols), whether the C++ names it (RETAIL) and whether other data points at it"""
import re, sys, collections
from pathlib import Path

HERE = Path(__file__).resolve().parents[2]
objects = {}          # label -> dict(section, size, pointers, targets)
order = []
for path in sorted((HERE / "asm" / "data").glob("*.s")):
    section, current = path.stem, None
    for line in path.read_text().splitlines():
        line = line.strip()
        m = re.match(r"(?:dlabel|glabel|jlabel)\s+(\S+)", line)
        if m:
            current = m.group(1); objects[current] = dict(section=section, size=0, pointers=0, targets=[]); order.append(current)
            continue
        if line.startswith("enddlabel") or line.startswith("endlabel"):
            current = None; continue
        if current is None:
            continue
        m = re.search(r"\.(word|short|half|byte|space|skip|float|asciz|ascii|zero)\s+(.*)$", line)
        if not m:
            continue
        kind, arg = m.groups()
        o = objects[current]
        if kind == "word":
            for v in arg.split(","):
                v = v.strip(); o["size"] += 4
                if re.match(r"[A-Za-z_.]", v):
                    o["pointers"] += 1; o["targets"].append(re.split(r"[+-]", v)[0])
        elif kind in ("short", "half"): o["size"] += 2 * len(arg.split(","))
        elif kind == "byte": o["size"] += len(arg.split(","))
        elif kind in ("space", "skip", "zero"): o["size"] += int(arg.split(",")[0], 0)
        elif kind == "float": o["size"] += 4 * len(arg.split(","))

cxx = set()
for path in list((HERE / "src").rglob("*.cpp")) + list((HERE / "include").rglob("*.h")):
    cxx.update(re.findall(r"RETAIL\((\w+)\)", path.read_text(errors="ignore")))

pointed = collections.Counter(t for o in objects.values() for t in o["targets"])
by = collections.defaultdict(lambda: collections.Counter())
for name, o in objects.items():
    s = by[o["section"]]
    s["objects"] += 1; s["bytes"] += o["size"]
    used = name in cxx; ref = pointed[name] > 0
    s["used_by_cxx"] += used; s["only_from_data"] += (not used and ref); s["unreferenced"] += (not used and not ref)
    if o["pointers"]:
        s["with_pointers"] += 1; s["pointer_words"] += o["pointers"]
        s["with_pointers_used"] += used
print(f"{'section':10} " + " ".join(f"{k:>18}" for k in ["objects","bytes","used_by_cxx","only_from_data","unreferenced","with_pointers","with_pointers_used","pointer_words"]))
for sec, s in by.items():
    print(f"{sec:10} " + " ".join(f"{s[k]:>18}" for k in ["objects","bytes","used_by_cxx","only_from_data","unreferenced","with_pointers","with_pointers_used","pointer_words"]))
# What the pointers point at: functions (FUN_/named text) vs data
kinds = collections.Counter()
for o in objects.values():
    for t in o["targets"]:
        kinds["data" if t in objects else "code/other"] += 1
print("pointer targets:", dict(kinds))
if "-v" in sys.argv:
    for name in order:
        o = objects[name]
        if o["pointers"] and name in cxx:
            print(name, o["section"], o["size"], o["pointers"])
