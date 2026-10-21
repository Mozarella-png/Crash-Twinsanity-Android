package com.giopixel.crashtwinsanity;

/** Hands the game the paths it needs at start-up (src/platform/native/android_glue.cpp). */
final class NativeBridge {
    private NativeBridge() {}

    /** The disc image (a copy in the app's folder, or null for none yet), the folder for settings and saves, and the log's. */
    static native void setPaths(String discImage, String settingsFolder, String logFolder);
}
