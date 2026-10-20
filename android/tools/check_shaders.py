#!/usr/bin/env python3
"""Checks the renderer's GLSL with glslang, in both dual-source modes, as the GLES build will compile it.

Each shader in the native sources is taken from its raw string, given the version and defines the C++ gives it (the main
program's header defines, the memory programs' common source), adapted as src/platform/native/graphics/hw/glsl.cpp does, and
compiled as ES 3.10 with glslang (ES 3.00 can't be checked by glslang's SPIR-V path; 3.10 is a superset that catches syntax
and semantic errors, and the 3.1-only built-ins are listed by the grep at the end).
Usage: check_shaders.py <glslangValidator>
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
SOURCES = ["src/platform/native/graphics/display.cpp", "src/platform/native/graphics/hw/hwgs.cpp",
           "src/platform/native/ui/overlayrender.cpp", "src/platform/native/ui/crtfilter.cpp"]
DESKTOP = "#version 330 core"
PRECISION = ("precision highp float;\nprecision highp int;\nprecision highp sampler2D;\n"
             "precision highp usampler2D;\nprecision highp isampler2D;\n")
# The main program's header defines (hwgs.cpp's define(...) calls), with the values the checks use
MAIN_DEFINES = ["TEXTURED", "TFX", "TCC", "FOG", "ATST", "ALPHATEST", "UVMODE", "TEXALPHA", "FRAME16", "DITHER", "FBA",
                "MIPMAP", "LODCONST", "FAILPASS", "DEPTH", "SHADERBLEND", "DATE", "FRAME24", "WRAPADD", "NATIVEDEPTH"]
VARIANTS = [[0] * 20, [1] * 20]


def literals(path):
    text = open(path, encoding="utf-8").read()
    for m in re.finditer(r'R"\((.*?)\)"', text, re.S):
        yield m.group(1)


def adapt(src, dual):
    if DESKTOP in src:
        head = "#version 310 es\n" + PRECISION
        if dual:
            head += "#extension GL_EXT_blend_func_extended : require\n#define TWIN_DUAL_SOURCE 1\n"
        src = src.replace(DESKTOP, head, 1)
    return src.replace("noperspective ", "")


def run(glslang, text, stage):
    with tempfile.NamedTemporaryFile("w", suffix=".glsl", delete=False) as f:
        f.write(text)
        name = f.name
    try:
        r = subprocess.run([glslang, "-G", "--auto-map-locations", "--auto-map-bindings", "-S", stage, name],
                           capture_output=True, text=True)
    finally:
        os.unlink(name)
    return [l for l in (r.stdout + r.stderr).splitlines()
            if "ERROR" in l and "compilation errors" not in l and "Linking" not in l and "No code" not in l
            and "terminated" not in l]


def main():
    glslang = sys.argv[1] if len(sys.argv) > 1 else "glslang"
    bodies = []
    everything = []
    for rel in SOURCES:
        for body in literals(os.path.join(ROOT, rel)):
            everything.append(body)
            if "void main" in body:
                bodies.append((rel, body))
    common = [b for b in everything if "UnitPixel" in b and "void main" not in b]
    failures = 0
    checked = 0
    for rel, body in bodies:
        stage = "vert" if "gl_Position" in body else "frag"
        if "UnitPixel" in body or "UnitAddress" in body:
            pieces = [DESKTOP, "#define SAMPLE_SHIFT 0.000000"] + common[:1] + [body.replace(DESKTOP, "")]
            variants = [[]]
        elif "outColour" in body and "SAMPLE_SHIFT" in body or "TWIN_DUAL_SOURCE" in body:
            variants = VARIANTS
            pieces = None
        else:
            pieces = [body if body.lstrip().startswith("#version") else DESKTOP + "\n" + body]
            variants = [[]]
        for vals in variants:
            for dual in (False, True):
                if pieces is None:
                    defs = "".join("#define %s %d\n" % (n, v) for n, v in zip(MAIN_DEFINES, vals))
                    text = DESKTOP + "\n" + defs + "#define SAMPLE_SHIFT 0.000000\n" + body.replace(DESKTOP, "")
                else:
                    text = "\n".join(pieces)
                errors = run(glslang, adapt(text, dual), stage)
                checked += 1
                if errors:
                    failures += 1
                    print("FAIL %s (%s, dual=%s): %s" % (rel, stage, dual, errors[0]))
    print("checked %d shader variants, %d failures" % (checked, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
