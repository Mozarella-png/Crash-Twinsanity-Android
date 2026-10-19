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

echo "Android project ready in $OUT/project"
