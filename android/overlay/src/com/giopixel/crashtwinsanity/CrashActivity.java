package com.giopixel.crashtwinsanity;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.media.AudioManager;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.system.Os;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.util.DisplayMetrics;
import android.widget.FrameLayout;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

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

    /** The log is started once per process: an activity re-created by a rotation keeps appending to the same file. */
    private static boolean logStarted = false;

    private TouchOverlay overlay;
    private final Handler main = new Handler(Looper.getMainLooper());

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        startLog();
        installFonts();
        // The disc's path (it may not be copied yet: the game waits for it), the settings' and the log's folders, before the
        // game starts (its start-up reads them)
        NativeBridge.setPaths(discFile().getAbsolutePath(), new File(getFilesDir(), "settings").getAbsolutePath(),
                logDir().getAbsolutePath());
        // The game is a landscape TV: held upright, its picture was a 16:9 band in the middle of the phone (the rest black)
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
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

    @Override
    protected void onResume() {
        super.onResume();
        note("Actividad en primer plano");
        hideSystemBars();
        hideSoon();
    }

    @Override
    protected void onPause() {
        note("Actividad en segundo plano");
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        note("Actividad destruida");
        super.onDestroy();
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        note("Memoria: el sistema pide liberar (nivel " + level + ")");
    }

    /**
     * The log, in the app's private folder (files/settings/log.txt): this part's lines and the game's own (stdout and stderr
     * go to the same file, from the game's start). A new process rotates the last log to log.previous.txt. The log never
     * stops the app: a write that fails is dropped.
     */
    private void startLog() {
        File dir = logDir();
        dir.mkdirs();
        if (!logStarted) {
            logStarted = true;
            File last = new File(dir, "log.txt");
            if (last.exists()) {
                last.renameTo(new File(dir, "log.previous.txt"));
            }
            note("=== Proceso nuevo ===");
            DisplayMetrics metrics = getResources().getDisplayMetrics();
            float refresh = getWindowManager().getDefaultDisplay().getRefreshRate();
            note("Dispositivo: " + Build.MANUFACTURER + " " + Build.MODEL + " (" + Build.HARDWARE + "), Android "
                    + Build.VERSION.RELEASE + ", API " + Build.VERSION.SDK_INT);
            note("Pantalla: " + metrics.widthPixels + "x" + metrics.heightPixels + " px, " + metrics.densityDpi
                    + " dpi, " + refresh + " Hz");
            note("Procesadores: " + Runtime.getRuntime().availableProcessors() + ", memoria máxima de la app: "
                    + (Runtime.getRuntime().maxMemory() >> 20) + " MB");
            ActivityManager activities = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
            ActivityManager.MemoryInfo memory = new ActivityManager.MemoryInfo();
            activities.getMemoryInfo(memory);
            note("Memoria del teléfono: " + (memory.availMem >> 20) + " MB libres de " + (memory.totalMem >> 20)
                    + " MB" + (memory.lowMemory ? " (memoria baja)" : ""));
        }
        note("Actividad creada");
    }

    /**
     * The folder of the log: the app's folder on the phone's storage, Android/data/com.giopixel.crashtwinsanity/files, which
     * a file manager or a USB connection shows (the private folder doesn't). The private one when there's no such storage.
     */
    private File logDir() {
        File external = getExternalFilesDir(null);
        File dir = external != null ? external : new File(getFilesDir(), "settings");
        dir.mkdirs();
        return dir;
    }

    /** One line in the log, with the time. Synchronized: the disc copy writes from its own thread. */
    private synchronized void note(String text) {
        try {
            String line = new SimpleDateFormat("HH:mm:ss.SSS", Locale.US).format(new Date()) + " [java] " + text + "\n";
            try (FileOutputStream out = new FileOutputStream(new File(logDir(), "log.txt"), true)) {
                out.write(line.getBytes(StandardCharsets.UTF_8));
            }
        } catch (IOException | RuntimeException ignored) {
            // the log is a help, not a need
        }
    }

    /**
     * SDL sets up its own view and flags as the surface comes and goes, and those can land after the calls above. Hiding the
     * bars once more a moment later keeps them away whichever one ran last.
     */
    private void hideSoon() {
        main.postDelayed(this::hideSystemBars, 500);
    }

    /**
     * Immersive full screen: the navigation and status bars hide, and come back only for a swipe from their edge (then they
     * hide again). Android 11 and up hide them through the window's insets controller; older ones take the flags below.
     */
    private void hideSystemBars() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
            }
        }
        getWindow().getDecorView().setOnSystemUiVisibilityChangeListener(visibility -> {
            // The bars came back (a swipe, a call, the volume panel): hide them again
            if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
                main.post(this::hideSystemBars);
            }
        });
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
        note("Ventana con foco: " + hasFocus);
        if (hasFocus) {
            hideSystemBars();
            hideSoon();
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
            note("Importando el disco elegido");
            final String problem = copyTo(uri, partial);
            note(problem == null ? "Disco copiado" : "Disco no copiado: " + problem);
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

    /**
     * The game's fonts (the overlay's text): copied from the APK's assets (fonts/) to the app's folder, where the game looks
     * for them (TWIN_UI_FONTS, read when the first text is drawn). Without them the menus' text is not drawn at all.
     */
    private void installFonts() {
        File dir = new File(getFilesDir(), "fonts");
        dir.mkdirs();
        try {
            String[] names = getAssets().list("fonts");
            if (names == null) {
                note("Fuentes: no hay fonts/ en el APK");
                return;
            }
            for (String name : names) {
                try (InputStream in = getAssets().open("fonts/" + name);
                     OutputStream out = new FileOutputStream(new File(dir, name))) {
                    byte[] buffer = new byte[65536];
                    int count;
                    while ((count = in.read(buffer)) > 0) {
                        out.write(buffer, 0, count);
                    }
                }
            }
            Os.setenv("TWIN_UI_FONTS", dir.getAbsolutePath(), true);
            note("Fuentes: " + names.length + " en " + dir.getAbsolutePath());
        } catch (Exception e) {
            note("Fuentes: no se pudieron copiar: " + e);
        }
    }

    private static String megabytes(long bytes) {
        return (bytes >> 20) + " MB";
    }

    private void handOverPaths() {
        note("Entregando al juego la ruta del disco y de los ajustes");
        NativeBridge.setPaths(discFile().getAbsolutePath(), new File(getFilesDir(), "settings").getAbsolutePath(),
                logDir().getAbsolutePath());
    }

    /**
     * The volume keys change the game's sound without the system's volume panel (which would sit over the picture with the
     * navigation buttons): the change is made with no UI, and the keys are taken here.
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int code = event.getKeyCode();
        if (code == KeyEvent.KEYCODE_VOLUME_UP || code == KeyEvent.KEYCODE_VOLUME_DOWN) {
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                AudioManager audio = (AudioManager) getSystemService(AUDIO_SERVICE);
                audio.adjustStreamVolume(AudioManager.STREAM_MUSIC,
                        code == KeyEvent.KEYCODE_VOLUME_UP ? AudioManager.ADJUST_RAISE : AudioManager.ADJUST_LOWER, 0);
            }
            return true;
        }
        return super.dispatchKeyEvent(event);
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
