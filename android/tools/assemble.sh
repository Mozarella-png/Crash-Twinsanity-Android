#!/usr/bin/env bash
# Assembles the Android project in android/build/project from SDL3's android-project template, our activity, the touch overlay
# and its art. Run from anywhere; the GitHub Actions workflow runs it and then builds the APK with Gradle.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SDL_TAG="${SDL_TAG:-release-3.2.0}"          # pinned SDL3 release; the template must match it
PKG=com.giopixel.crashtwinsanity
OUT="$ROOT/android/build"

rm -rf "$OUT"
mkdir -p "$OUT"
git clone --depth 1 --branch "$SDL_TAG" https://github.com/libsdl-org/SDL.git "$OUT/SDL"
cp -r "$OUT/SDL/android-project" "$OUT/project"
APP="$OUT/project/app"

# Our activity (extends SDL's SDLActivity) and the overlay's classes, in the app's package
JAVA_DIR="$APP/src/main/java/com/giopixel/crashtwinsanity"
mkdir -p "$JAVA_DIR"
cp "$ROOT"/android/overlay/src/com/giopixel/crashtwinsanity/*.java "$JAVA_DIR/"

# The overlay's art
mkdir -p "$APP/src/main/assets/touch"
cp "$ROOT"/android/overlay/assets/touch/* "$APP/src/main/assets/touch/"

# Package, launcher activity and name
sed -i "s/org\.libsdl\.app\.SDLActivity/$PKG.CrashActivity/g" "$APP/src/main/AndroidManifest.xml"
sed -i "s/applicationId \"org\.libsdl\.app\"/applicationId \"$PKG\"/" "$APP/build.gradle"
sed -i 's|<string name="app_name">[^<]*</string>|<string name="app_name">Crash Twinsanity</string>|' "$APP/src/main/res/values/strings.xml"

# The launcher icon: our PNGs at every density, and no adaptive icon from the template (it would win on Android 8+)
RES="$APP/src/main/res"
for d in mdpi hdpi xhdpi xxhdpi xxxhdpi; do
    mkdir -p "$RES/mipmap-$d"
    cp "$ROOT/android/overlay/res/mipmap-$d/ic_launcher.png" "$RES/mipmap-$d/ic_launcher.png"
done
rm -rf "$RES"/mipmap-anydpi-v26
python3 - "$APP/src/main/AndroidManifest.xml" <<'PYEOF'
import re, sys
path = sys.argv[1]
text = open(path, encoding="utf-8").read()
text = re.sub(r'android:icon="[^"]*"', 'android:icon="@mipmap/ic_launcher"', text)
if 'android:icon=' not in text:
    text = text.replace("<application", '<application android:icon="@mipmap/ic_launcher"', 1)
text = re.sub(r'android:label="[^"]*"', 'android:label="@string/app_name"', text)
open(path, "w", encoding="utf-8").write(text)
PYEOF

# The native build: the template's jni/SDL is SDL3's source (its Android.mk builds SDL3); jni/src holds our module
JNI="$APP/jni"
ln -sfn "$OUT/SDL" "$JNI/SDL"
mkdir -p "$JNI/src"
cp "$ROOT/android/native/stub_main.c" "$JNI/src/stub_main.c"
cat > "$JNI/src/Android.mk" <<'MK'
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

# The stub (android/native) until the game builds for Android; the touch bridge (src/platform/native/touch) is the game's
# own code and is built from the repo, so its JNI functions exist in libmain.so
LOCAL_SRC_FILES := stub_main.c \
    @ROOT@/src/platform/native/touch/touchpad.cpp \
    @ROOT@/src/platform/native/android_glue.cpp

LOCAL_C_INCLUDES := @OUT@/SDL/include @ROOT@/src/platform/native

LOCAL_SHARED_LIBRARIES := SDL3

LOCAL_LDLIBS := -llog -landroid

include $(BUILD_SHARED_LIBRARY)
MK
sed -i "s|@ROOT@|$ROOT|g; s|@OUT@|$OUT|g" "$JNI/src/Android.mk"
# The template's Application.mk: a 64-bit ABI and the same API level as the app (Android 5.0 and up)
cat > "$JNI/Application.mk" <<'APP'
APP_ABI := arm64-v8a
APP_PLATFORM := android-24
APP_STL := c++_static
APP

# minSdk and the NDK's platform level in the template's build.gradle
sed -i 's/minSdkVersion [0-9]*/minSdkVersion 24/; s/android-19/android-24/g; s/android-16/android-24/g' "$APP/build.gradle"

echo "Android project ready in $OUT/project"
