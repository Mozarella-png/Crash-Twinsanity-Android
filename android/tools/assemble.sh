#!/usr/bin/env bash
# Assembles the Android project in android/build/project from SDL3's android-project template, our activity, the touch overlay
# and its art, and the NDK module for the game (jni/src/Android.mk, from android/tools/make_android_mk.py). Run from anywhere; the
# GitHub Actions workflow runs it, then gen_data.sh (the game's data), then builds the APK with Gradle.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SDL_TAG="${SDL_TAG:-release-3.4.18}"         # pinned SDL3 release (the desktop build's); the template must match it
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

# The menus' fonts (overlay.cpp reads them from the assets on Android): without them the options screen has no text
mkdir -p "$APP/src/main/assets/fonts"
cp "$ROOT"/native/third_party/fonts/*.ttf "$APP/src/main/assets/fonts/"
ls "$APP/src/main/assets/fonts/LuckiestGuy-Regular.ttf" "$APP/src/main/assets/fonts/Fredoka-SemiBold.ttf" \
   "$APP/src/main/assets/fonts/Fredoka-Bold.ttf" >/dev/null || { echo "the menus' fonts weren't copied" >&2; exit 1; }

# Package, launcher activity and name. The template writes the namespace as `namespace "org.libsdl.app"` (3.2) or
# `namespace = "org.libsdl.app"` (3.4), so both spellings are matched.
sed -i "s|<activity android:name=\"SDLActivity\"|<activity android:name=\"$PKG.CrashActivity\"|" "$APP/src/main/AndroidManifest.xml"
sed -i -E "s|namespace[ =]*\"org\.libsdl\.app\"|namespace \"$PKG\"|" "$APP/build.gradle"
sed -i 's|<string name="app_name">[^<]*</string>|<string name="app_name">Crash Twinsanity</string>|' "$APP/src/main/res/values/strings.xml"

# Each edit has to have landed: a template that changed would otherwise build the wrong activity without a word
grep -q "android:name=\"$PKG.CrashActivity\"" "$APP/src/main/AndroidManifest.xml" || { echo "the launcher activity wasn't replaced in the manifest" >&2; exit 1; }
grep -q "namespace \"$PKG\"" "$APP/build.gradle" || { echo "the namespace wasn't replaced in build.gradle" >&2; exit 1; }
grep -q "<string name=\"app_name\">Crash Twinsanity</string>" "$APP/src/main/res/values/strings.xml" || { echo "the app name wasn't set" >&2; exit 1; }

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

# The native module: the template's jni/SDL is SDL3's source (its Android.mk builds SDL3); jni/src holds the game's module
JNI="$APP/jni"
ln -sfn "$OUT/SDL" "$JNI/SDL"
mkdir -p "$JNI/src"
python3 "$ROOT/android/tools/make_android_mk.py" "$ROOT" "$JNI/src" "$OUT/SDL"
grep -q "src/platform/native/touch/touchpad.cpp" "$JNI/src/Android.mk" || { echo "touchpad.cpp isn't in Android.mk" >&2; exit 1; }
grep -q "src/game/" "$JNI/src/Android.mk" || { echo "the game's sources aren't in Android.mk" >&2; exit 1; }

# The template's Application.mk: a 64-bit ABI and the same API level as the app (Android 7.0 and up)
cat > "$JNI/Application.mk" <<'APP'
APP_ABI := arm64-v8a
APP_PLATFORM := android-24
APP_STL := c++_static
APP

# minSdk, the NDK's platform level, and the Java sources' encoding (the overlay's Spanish text is UTF-8: say so, rather than
# trust the runner's locale)
sed -i 's/minSdkVersion [0-9]*/minSdkVersion 24/; s/android-[0-9][0-9]*/android-24/g' "$APP/build.gradle"
cat >> "$APP/build.gradle" <<'GRADLE'

tasks.withType(JavaCompile).configureEach {
    options.encoding = 'UTF-8'
}
GRADLE
grep -q "options.encoding = 'UTF-8'" "$APP/build.gradle" || { echo "the Java encoding wasn't set in build.gradle" >&2; exit 1; }
grep -q "android-24" "$APP/build.gradle" || { echo "the platform level wasn't set in build.gradle" >&2; exit 1; }

echo "Android project ready in $OUT/project"
