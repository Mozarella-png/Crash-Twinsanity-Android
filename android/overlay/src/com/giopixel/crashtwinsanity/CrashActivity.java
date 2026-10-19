package com.giopixel.crashtwinsanity;

import android.os.Bundle;
import android.view.MotionEvent;
import android.widget.FrameLayout;

import org.libsdl.app.SDLActivity;

/**
 * The game's activity: SDL's, with the touch overlay over it. Touches on the overlay's controls are taken here and not
 * passed to the game, so pressing a button doesn't also click the mouse (Spin is the left button); the other touches
 * go on to SDL, which turns them into the mouse the menus use.
 */
public class CrashActivity extends SDLActivity {
    private TouchOverlay overlay;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        overlay = new TouchOverlay(this);
        addContentView(overlay, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (overlay == null) {
            return super.dispatchTouchEvent(event);
        }
        boolean[] claimed = overlay.claim(event);
        MotionEvent rest = TouchOverlay.without(event, claimed);
        if (rest == null) {
            return true;
        }
        boolean handled = super.dispatchTouchEvent(rest);
        if (rest != event) {
            rest.recycle();
        }
        return handled;
    }
}
