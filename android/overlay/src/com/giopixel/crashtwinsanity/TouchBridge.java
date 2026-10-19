package com.giopixel.crashtwinsanity;

/** The native side of the touch overlay (src/platform/native/touch/touchpad.cpp). */
final class TouchBridge {
    private TouchBridge() {}

    /** The overlay's buttons (PS2 bits) and stick (0x80 the middle, up 0), read by the game's pads. */
    static native void setPad(int buttons, int leftX, int leftY, boolean stickActive);

    /** Whether the game is in play: the overlay only acts then; in the menus the touches are the mouse's. */
    static native boolean inPlay();
}
