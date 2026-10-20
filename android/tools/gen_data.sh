#!/usr/bin/env bash
# Converts the game's data for Android: build/native/data.cpp, from asm/ (the split, copied from the private repo) with the
# NDK's clang for arm64 as the host (its record layouts are the ones the Android build uses) and the PS2 toolchain's headers
# for the PS2's layouts. The game's data itself is not in the output: data.cpp has where each run of it sits in the disc's
# executable, and the game reads the bytes from the player's disc image at start-up (system.cpp, NativeLoadRetailData).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
API="${ANDROID_API:-24}"

for needed in "asm/data" "assets/vutext.textbin.bin" "SLES_525.68"; do
    if [ ! -e "$ROOT/$needed" ]; then
        echo "$needed is missing: the private repo must have asm/, assets/ and SLES_525.68 at its root" >&2
        exit 1
    fi
done
NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_LATEST_HOME:-}}"
if [ -z "$NDK" ]; then
    echo "The NDK isn't set (ANDROID_NDK_HOME)" >&2
    exit 1
fi
CLANG="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${API}-clang++"
if [ ! -x "$CLANG" ]; then
    echo "No clang for arm64 at $CLANG" >&2
    exit 1
fi
if [ -z "${PS2DEV:-}" ] || [ ! -d "$PS2DEV" ]; then
    echo "PS2DEV isn't a folder with the PS2 toolchain (set it to where ps2dev was extracted)" >&2
    exit 1
fi

export TWIN_CLANG="$CLANG"
# The platform code's headers for the host layouts (SDL3, as the Android build has it)
export TWIN_HOST_INCLUDES="$ROOT/android/build/SDL/include"
export PS2DEV
mkdir -p "$ROOT/build/native"
cd "$ROOT"
python3 native/tools/data_types.py
python3 native/tools/convert_data.py
ls -l build/native/data.cpp
