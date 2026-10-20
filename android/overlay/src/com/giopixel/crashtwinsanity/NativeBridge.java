package com.giopixel.crashtwinsanity;

/** Hands the game the paths it needs at start-up (src/platform/native/android_glue.cpp). */
final class NativeBridge {
    private NativeBridge() {}

    /** The disc image (a copy in the app's folder) and the folder for settings and saves. */
    static native void setPaths(String discImage, String settingsFolder);
}
