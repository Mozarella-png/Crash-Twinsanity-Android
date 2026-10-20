package com.giopixel.crashtwinsanity;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;
import android.widget.FrameLayout;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * The game's activity: SDL's, with the touch overlay over it. Touches on the overlay's controls are taken here and not
 * passed to the game, so pressing a button doesn't also click the mouse (Spin is the left button); the other touches
 * go on to SDL, which turns them into the mouse the menus use.
 *
 * The disc: the player picks their PAL disc image once; it's copied into the app's private folder and its path is handed
 * to the game (NativeBridge). The copy is kept, so the picker only comes back if the copy is gone.
 */
public class CrashActivity extends SDLActivity {
    private static final int PICK_DISC = 7001;
    private static final String PREFS = "disc";
    private static final String KEY_COPIED = "copied";

    private TouchOverlay overlay;
    private final Handler main = new Handler(Looper.getMainLooper());

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        overlay = new TouchOverlay(this);
        addContentView(overlay, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        if (discFile().isFile() && getSharedPreferences(PREFS, MODE_PRIVATE).getBoolean(KEY_COPIED, false)) {
            handOverPaths();
        } else {
            pickDisc();
        }
    }

    private File discFile() {
        return new File(new File(getFilesDir(), "disc"), "disc.iso");
    }

    private void pickDisc() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityForResult(intent, PICK_DISC);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_DISC || resultCode != Activity.RESULT_OK || data == null || data.getData() == null) {
            Toast.makeText(this, "Hace falta tu imagen del disco PAL de Crash Twinsanity (SLES-52568).", Toast.LENGTH_LONG).show();
            return;
        }
        copyDisc(data.getData());
    }

    private void copyDisc(final Uri uri) {
        Toast.makeText(this, "Copiando el disco… puede tardar un momento.", Toast.LENGTH_SHORT).show();
        new Thread(() -> {
            File target = discFile();
            target.getParentFile().mkdirs();
            File partial = new File(target.getParentFile(), "disc.iso.part");
            boolean ok = false;
            try (InputStream in = getContentResolver().openInputStream(uri);
                 OutputStream out = new FileOutputStream(partial)) {
                if (in == null) {
                    throw new java.io.IOException("no se pudo abrir el archivo");
                }
                byte[] buffer = new byte[1 << 20];
                int n;
                while ((n = in.read(buffer)) > 0) {
                    out.write(buffer, 0, n);
                }
                ok = true;
            } catch (java.io.IOException e) {
                partial.delete();
            }
            if (ok && partial.renameTo(target)) {
                getSharedPreferences(PREFS, MODE_PRIVATE).edit().putBoolean(KEY_COPIED, true).apply();
                main.post(() -> {
                    Toast.makeText(this, "Disco importado.", Toast.LENGTH_SHORT).show();
                    handOverPaths();
                });
            } else {
                main.post(() -> Toast.makeText(this, "No se pudo copiar el disco. Vuelve a elegirlo.", Toast.LENGTH_LONG).show());
            }
        }, "disc-copy").start();
    }

    private void handOverPaths() {
        NativeBridge.setPaths(discFile().getAbsolutePath(), new File(getFilesDir(), "settings").getAbsolutePath());
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
