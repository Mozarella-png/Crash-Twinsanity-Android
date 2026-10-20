package com.giopixel.crashtwinsanity;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
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
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (android.os.Build.VERSION.SDK_INT >= 28) {
            // The game draws into the notch's area too: the screen is the whole of it
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
        hideSystemBars();
        overlay = new TouchOverlay(this);
        addContentView(overlay, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        if (discFile().isFile() && getSharedPreferences(PREFS, MODE_PRIVATE).getBoolean(KEY_COPIED, false)) {
            handOverPaths();
        } else {
            pickDisc();
        }
    }

    /** Immersive full screen: the bars hide, and come back only for a swipe from their edge (then they hide again). */
    private void hideSystemBars() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemBars();
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

    /**
     * Copies the picked disc into the app's folder, wherever it is: the picker can return a file on the phone, on the SD
     * card, or one a cloud provider has to download first. A failure says what went wrong, so the player knows what to do.
     */
    private void copyDisc(final Uri uri) {
        Toast.makeText(this, "Copiando el disco… puede tardar un momento.", Toast.LENGTH_SHORT).show();
        new Thread(() -> {
            File target = discFile();
            target.getParentFile().mkdirs();
            File partial = new File(target.getParentFile(), "disc.iso.part");
            final String problem = copyTo(uri, partial);
            if (problem == null && partial.renameTo(target)) {
                getSharedPreferences(PREFS, MODE_PRIVATE).edit().putBoolean(KEY_COPIED, true).apply();
                main.post(() -> {
                    Toast.makeText(this, "Disco importado.", Toast.LENGTH_SHORT).show();
                    handOverPaths();
                });
            } else {
                partial.delete();
                final String message = problem != null ? problem : "No se pudo guardar la copia del disco. Vuelve a elegirlo.";
                main.post(() -> Toast.makeText(this, message, Toast.LENGTH_LONG).show());
            }
        }, "disc-copy").start();
    }

    /** Copies the picked file to {@code to}. Returns null when it's all there, or what went wrong, for the player to read. */
    private String copyTo(Uri uri, File to) {
        long size = sizeOf(uri);
        long free = to.getParentFile().getUsableSpace();
        if (size > 0 && free < size + (16L << 20)) {
            return "No hay espacio: el disco ocupa " + megabytes(size) + " y quedan " + megabytes(free)
                    + ". Libera espacio y vuelve a elegirlo.";
        }
        InputStream in = null;
        try {
            in = openDisc(uri);
            try (OutputStream out = new FileOutputStream(to)) {
                byte[] buffer = new byte[1 << 20];
                int n;
                while ((n = in.read(buffer)) != -1) {
                    out.write(buffer, 0, n);
                }
            }
            return null;
        } catch (IOException | RuntimeException e) {
            return "No se pudo leer el archivo elegido (" + e.getMessage() + "). Si está en la nube, "
                    + "descárgalo al teléfono y vuelve a elegirlo.";
        } finally {
            if (in != null) {
                try {
                    in.close();
                } catch (IOException ignored) {
                    // nothing left to read
                }
            }
        }
    }

    /** The picked file as a stream: the content resolver's, or a descriptor when the provider gives no stream. */
    private InputStream openDisc(Uri uri) throws IOException {
        InputStream in = getContentResolver().openInputStream(uri);
        if (in != null) {
            return in;
        }
        ParcelFileDescriptor descriptor = getContentResolver().openFileDescriptor(uri, "r");
        if (descriptor == null) {
            throw new IOException("el proveedor no dio acceso al archivo");
        }
        return new ParcelFileDescriptor.AutoCloseInputStream(descriptor);
    }

    /** The picked file's size in bytes, or -1 when the provider doesn't say. */
    private long sizeOf(Uri uri) {
        try (Cursor cursor = getContentResolver().query(uri, new String[] {OpenableColumns.SIZE}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) {
                return cursor.getLong(0);
            }
        } catch (RuntimeException ignored) {
            // the size is only for the space check
        }
        return -1;
    }

    private static String megabytes(long bytes) {
        return (bytes >> 20) + " MB";
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
