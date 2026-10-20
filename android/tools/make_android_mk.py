#!/usr/bin/env python3
"""Writes the NDK's jni/src/Android.mk for the game: the sources the desktop build lists (native/CMakeLists.txt's globs of
src/game, the core, native/reused_ps2.txt and src/platform/native), with the Android stand-in for the movie decoder in place
of movie/movie.cpp, and the files the converters make in build/native (gen_data.sh makes them before the NDK runs).

Usage: make_android_mk.py <repo root> <jni/src folder> <SDL's folder>
"""
import glob
import os
import sys

ROOT, JNI_SRC, SDL = (os.path.abspath(p) for p in sys.argv[1:4])

# Desktop sources that aren't the game's: movie/movie.cpp needs FFmpeg, so the Android build uses movie_off.cpp instead
MOVIE_DECODER = os.path.join(ROOT, "src/platform/native/movie/movie.cpp")
MOVIE_OFF = os.path.join(ROOT, "android/native/movie_off.cpp")

game = sorted(glob.glob(os.path.join(ROOT, "src/game/*.cpp")))
core = [os.path.join(ROOT, p) for p in ("src/debug.cpp", "src/gcc2.cpp", "src/main.cpp")]
with open(os.path.join(ROOT, "native/reused_ps2.txt"), encoding="utf-8") as f:
    reused = [os.path.join(ROOT, line.strip()) for line in f if line.startswith("src/")]
platform = sorted(glob.glob(os.path.join(ROOT, "src/platform/native/**/*.cpp"), recursive=True))
platform = [p for p in platform if p != MOVIE_DECODER] + [MOVIE_OFF]

generated = [os.path.join(ROOT, "build/native", name) for name in
             ["data.cpp", "scriptlayouts.cpp", "vu1programs.cpp", "vu0programs.cpp", "vu0programs_0.cpp", "vu0programs_1.cpp"]
             + ["vu1programs_%d.cpp" % i for i in range(8)]]

sources = game + core + reused + platform + generated
for path in sources:
    # The generated ones don't exist yet (gen_data.sh makes them); everything else must
    if not path.startswith(os.path.join(ROOT, "build/native")) and not os.path.isfile(path):
        sys.exit("missing source: " + path)

def rel(path):
    return os.path.relpath(path, JNI_SRC).replace(os.sep, "/")

FLAGS = ("-std=gnu++2c -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-strict-aliasing -fno-delete-null-pointer-checks "
         "-fwrapv -ffp-contract=off -Wall -Wno-invalid-offsetof -Wno-unused-variable -Wno-unused-function "
         "-Wno-unused-but-set-variable -Wno-unknown-pragmas -Wno-missing-braces -Wno-mismatched-tags -DTWIN_NATIVE -DTWIN_GLES")

lines = [
    "# Written by android/tools/make_android_mk.py: the game's sources as the desktop build lists them (see its docstring)",
    "LOCAL_PATH := $(call my-dir)",
    "",
    "include $(CLEAR_VARS)",
    "",
    "LOCAL_MODULE := main",
    "",
    "LOCAL_SRC_FILES := \\",
]
lines += ["    %s \\" % rel(p) for p in sources[:-1]] + ["    %s" % rel(sources[-1]), ""]
lines += [
    "LOCAL_C_INCLUDES := %s %s %s %s %s" % (
        os.path.join(SDL, "include"), os.path.join(ROOT, "native/include"), os.path.join(ROOT, "include"),
        os.path.join(ROOT, "src/platform/native"), os.path.join(ROOT, "build/native")),
    "LOCAL_CPPFLAGS := " + FLAGS,
    "LOCAL_SHARED_LIBRARIES := SDL3",
    "LOCAL_LDLIBS := -lGLESv3 -lEGL -llog -landroid",
    "",
    "include $(BUILD_SHARED_LIBRARY)",
    "",
]
with open(os.path.join(JNI_SRC, "Android.mk"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
print("Android.mk: %d sources (%d generated)" % (len(sources), len(generated)))
