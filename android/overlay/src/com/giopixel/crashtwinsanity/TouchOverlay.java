package com.giopixel.crashtwinsanity;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;

/**
 * The touch controls drawn over the game: the buttons of the PS2's pad (the art in assets/touch) and the movement stick.
 * Each control's centre, size (a fraction of the screen's height) and opacity are saved on the device, and can be
 * changed in the edit mode (the "Editar" button, in play). Outside play the overlay draws nothing and takes no touches.
 */
public class TouchOverlay extends View {
    // The controls, in the order of the layout's arrays
    static final int CROSS = 0, CIRCLE = 1, SQUARE = 2, TRIANGLE = 3, L1 = 4, R1 = 5, START = 6, STICK = 7, COUNT = 8;
    static final String[] FILES = {"X.png", "circulo.png", "cuadrado.png", "triangulo.png", "L1.png", "R1.png",
            "Start-Pausa.png", "Joystick-base.png"};
    static final String[] NAMES = {"X", "Círculo", "Cuadrado", "Triángulo", "L1", "R1", "Start", "Joystick"};
    // The PS2 pad's bits (src/platform/native/ui/bindings.h): cross, circle, square, triangle, L1, R1, start
    static final int[] MASKS = {0x40, 0x20, 0x80, 0x10, 0x04, 0x08, 0x800, 0};
    // Defaults: the PlayStation layout on the right, the stick on the left, L1 and R1 at the top corners
    static final float[] DEF_X = {0.88f, 0.95f, 0.81f, 0.88f, 0.10f, 0.90f, 0.50f, 0.14f};
    static final float[] DEF_Y = {0.72f, 0.55f, 0.55f, 0.38f, 0.16f, 0.16f, 0.10f, 0.62f};
    static final float[] DEF_SIZE = {0.14f, 0.14f, 0.14f, 0.14f, 0.12f, 0.12f, 0.09f, 0.34f};
    static final float[] DEF_ALPHA = {0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f, 0.6f};

    // Edit panel actions
    static final int ACT_NEXT = 0, ACT_SMALLER = 1, ACT_BIGGER = 2, ACT_FADE_LESS = 3, ACT_FADE_MORE = 4,
            ACT_RESET = 5, ACT_DONE = 6, ACT_EDIT = 7;
    static final String[] PANEL_LABELS = {"Control", "Tamaño −", "Tamaño +", "Opacidad −", "Opacidad +", "Restablecer", "Listo"};
    static final int[] PANEL_ACTIONS = {ACT_NEXT, ACT_SMALLER, ACT_BIGGER, ACT_FADE_LESS, ACT_FADE_MORE, ACT_RESET, ACT_DONE};

    private static final String PREFS = "touch_layout";
    private static final int OWNER_EDIT = -2;
    private static final int OWNER_PANEL = -3;

    private final Bitmap[] art = new Bitmap[COUNT];
    private Bitmap knob;
    private final float[] x = new float[COUNT], y = new float[COUNT], size = new float[COUNT], alpha = new float[COUNT];
    private final SharedPreferences prefs;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF tmp = new RectF();
    private final RectF editRect = new RectF();
    private final RectF[] panelRects = new RectF[PANEL_LABELS.length];

    // Pointer id -> control (or OWNER_*), with the grab offset for dragging in the edit mode
    private final Map<Integer, Integer> owners = new HashMap<>();
    private final Map<Integer, float[]> grabs = new HashMap<>();
    private boolean editing = false;
    private int selected = STICK;
    private int pressed = 0;
    private boolean stickHeld = false;
    private float stickX = 0f, stickY = 0f; // -1..1, screen down positive

    public TouchOverlay(Context context) {
        super(context);
        setWillNotDraw(false);
        prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        for (int i = 0; i < COUNT; i++) {
            art[i] = load(context, "touch/" + FILES[i]);
        }
        knob = load(context, "touch/knob.png");
        loadLayout();
    }

    private static Bitmap load(Context context, String path) {
        try (InputStream in = context.getAssets().open(path)) {
            return BitmapFactory.decodeStream(in);
        } catch (IOException e) {
            return null;
        }
    }

    // ---- layout storage

    private void loadLayout() {
        for (int i = 0; i < COUNT; i++) {
            x[i] = prefs.getFloat("x" + i, DEF_X[i]);
            y[i] = prefs.getFloat("y" + i, DEF_Y[i]);
            size[i] = prefs.getFloat("s" + i, DEF_SIZE[i]);
            alpha[i] = prefs.getFloat("a" + i, DEF_ALPHA[i]);
        }
    }

    private void saveLayout() {
        SharedPreferences.Editor edit = prefs.edit();
        for (int i = 0; i < COUNT; i++) {
            edit.putFloat("x" + i, x[i]).putFloat("y" + i, y[i]).putFloat("s" + i, size[i]).putFloat("a" + i, alpha[i]);
        }
        edit.apply();
    }

    private void resetLayout() {
        for (int i = 0; i < COUNT; i++) {
            x[i] = DEF_X[i];
            y[i] = DEF_Y[i];
            size[i] = DEF_SIZE[i];
            alpha[i] = DEF_ALPHA[i];
        }
        saveLayout();
    }

    // ---- geometry

    /** The control's rectangle in view pixels: its height is size × the screen's height, its width by the art's shape. */
    private RectF rectOf(int i, RectF out) {
        float h = size[i] * getHeight();
        float aspect = (art[i] != null && art[i].getHeight() > 0)
                ? (float) art[i].getWidth() / art[i].getHeight() : 1f;
        float w = h * aspect;
        if (i == STICK) {
            w = h;
        }
        float cx = x[i] * getWidth(), cy = y[i] * getHeight();
        out.set(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2);
        return out;
    }

    private int hitControl(float px, float py) {
        for (int i = COUNT - 1; i >= 0; i--) {
            RectF r = rectOf(i, tmp);
            float pad = i == STICK ? 0f : 0.15f * r.height();
            if (px >= r.left - pad && px <= r.right + pad && py >= r.top - pad && py <= r.bottom + pad) {
                return i;
            }
        }
        return -1;
    }

    private int hitPanel(float px, float py) {
        for (int i = 0; i < panelRects.length; i++) {
            if (panelRects[i] != null && panelRects[i].contains(px, py)) {
                return i;
            }
        }
        return -1;
    }

    private boolean hitEdit(float px, float py) {
        return editRect.contains(px, py);
    }

    // ---- touches

    /**
     * Takes the pointers that touch the overlay's controls (and, when editing, all of them) and sends the pad's state.
     * Returns, per pointer of the event, whether the overlay took it; the rest go on to the game.
     */
    public boolean[] claim(MotionEvent e) {
        int n = e.getPointerCount();
        boolean[] claimed = new boolean[n];
        if (!editing && !TouchBridge.inPlay()) {
            owners.clear();
            grabs.clear();
            sendPad();
            return claimed;
        }
        int action = e.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            down(e, e.getActionIndex());
        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
            up(e.getPointerId(e.getActionIndex()));
        } else if (action == MotionEvent.ACTION_CANCEL) {
            owners.clear();
            grabs.clear();
        } else if (action == MotionEvent.ACTION_MOVE) {
            for (int p = 0; p < n; p++) {
                move(e.getPointerId(p), e.getX(p), e.getY(p));
            }
        }
        for (int p = 0; p < n; p++) {
            // Editing: the game gets no touches at all; otherwise only the pointers on the controls are taken
            claimed[p] = editing || owners.containsKey(e.getPointerId(p));
        }
        sendPad();
        postInvalidateOnAnimation();
        return claimed;
    }

    private void down(MotionEvent e, int index) {
        int id = e.getPointerId(index);
        float px = e.getX(index), py = e.getY(index);
        if (!editing) {
            if (hitEdit(px, py)) {
                owners.put(id, OWNER_EDIT);
                startEditing();
                return;
            }
            int c = hitControl(px, py);
            if (c >= 0) {
                owners.put(id, c);
                if (c == STICK) {
                    stickFrom(id, px, py);
                }
                return;
            }
            return; // not a control: the game
        }
        // Editing: the panel's buttons, else drag the control under the finger
        int p = hitPanel(px, py);
        if (p >= 0) {
            owners.put(id, OWNER_PANEL);
            act(PANEL_ACTIONS[p]);
            return;
        }
        int c = hitControl(px, py);
        if (c >= 0) {
            selected = c;
            owners.put(id, c);
            grabs.put(id, new float[]{px - x[c] * getWidth(), py - y[c] * getHeight()});
        } else {
            owners.put(id, OWNER_EDIT);
        }
    }

    private void move(int id, float px, float py) {
        Integer owner = owners.get(id);
        if (owner == null) {
            return;
        }
        if (editing && owner >= 0) {
            float[] g = grabs.get(id);
            float dx = g != null ? g[0] : 0f, dy = g != null ? g[1] : 0f;
            x[owner] = clamp((px - dx) / getWidth());
            y[owner] = clamp((py - dy) / getHeight());
        } else if (owner == STICK) {
            stickFrom(id, px, py);
        }
    }

    private void up(int id) {
        Integer owner = owners.remove(id);
        grabs.remove(id);
        if (owner != null && owner == STICK) {
            stickHeld = false;
            stickX = stickY = 0f;
        }
        if (editing && owner != null && owner >= 0) {
            saveLayout();
        }
    }

    private void stickFrom(int id, float px, float py) {
        RectF r = rectOf(STICK, tmp);
        float radius = r.height() / 2f;
        float dx = (px - r.centerX()) / radius;
        float dy = (py - r.centerY()) / radius;
        float len = (float) Math.sqrt(dx * dx + dy * dy);
        if (len > 1f) {
            dx /= len;
            dy /= len;
        }
        stickX = dx;
        stickY = dy;
        stickHeld = true;
    }

    private void act(int action) {
        int c = selected;
        switch (action) {
            case ACT_NEXT: selected = (selected + 1) % COUNT; break;
            case ACT_SMALLER: size[c] = clamp(size[c] - 0.02f, 0.06f, 0.6f); saveLayout(); break;
            case ACT_BIGGER: size[c] = clamp(size[c] + 0.02f, 0.06f, 0.6f); saveLayout(); break;
            case ACT_FADE_LESS: alpha[c] = clamp(alpha[c] - 0.1f, 0.1f, 1f); saveLayout(); break;
            case ACT_FADE_MORE: alpha[c] = clamp(alpha[c] + 0.1f, 0.1f, 1f); saveLayout(); break;
            case ACT_RESET: resetLayout(); break;
            case ACT_DONE: editing = false; saveLayout(); break;
            default: break;
        }
    }

    private static float clamp(float v) {
        return Math.max(0.02f, Math.min(0.98f, v));
    }

    private static float clamp(float v, float lo, float hi) {
        return Math.max(lo, Math.min(hi, v));
    }

    private void sendPad() {
        int mask = 0;
        pressed = 0;
        for (Map.Entry<Integer, Integer> entry : owners.entrySet()) {
            int c = entry.getValue();
            if (c >= 0 && c != STICK) {
                mask |= MASKS[c];
            }
        }
        int lx = 128, ly = 128;
        if (stickHeld) {
            lx = 128 + Math.round(clamp(stickX, -1f, 1f) * 127f);
            ly = 128 + Math.round(clamp(stickY, -1f, 1f) * 127f);
        }
        TouchBridge.setPad(mask, lx, ly, stickHeld);
    }

    // ---- drawing

    @Override
    protected void onDraw(Canvas canvas) {
        if (!editing && !TouchBridge.inPlay()) {
            postInvalidateOnAnimation();
            return;
        }
        // What's held shows full, the rest at its opacity
        boolean[] held = new boolean[COUNT];
        for (int owner : owners.values()) {
            if (owner >= 0) {
                held[owner] = true;
            }
        }
        for (int i = 0; i < COUNT; i++) {
            if (i == STICK) {
                continue;
            }
            drawArt(canvas, art[i], i, held[i] ? 1f : alpha[i]);
        }
        // The stick: its base, then the knob where it's held
        RectF base = rectOf(STICK, new RectF());
        drawArt(canvas, art[STICK], STICK, stickHeld ? 1f : alpha[STICK]);
        if (knob != null) {
            float knobSize = base.height() * 0.45f;
            float kx = base.centerX() + stickX * base.height() * 0.3f;
            float ky = base.centerY() + stickY * base.height() * 0.3f;
            paint.setAlpha((int) (255 * (stickHeld ? 1f : alpha[STICK])));
            tmp.set(kx - knobSize / 2, ky - knobSize / 2, kx + knobSize / 2, ky + knobSize / 2);
            canvas.drawBitmap(knob, null, tmp, paint);
        }
        if (editing) {
            drawEditing(canvas);
        } else {
            drawEditButton(canvas);
        }
        postInvalidateOnAnimation();
    }

    private void drawArt(Canvas canvas, Bitmap bmp, int i, float a) {
        if (bmp == null) {
            return;
        }
        paint.setAlpha((int) (255 * a));
        canvas.drawBitmap(bmp, null, rectOf(i, new RectF()), paint);
    }

    private void drawEditButton(Canvas canvas) {
        float w = getWidth() * 0.12f, h = getHeight() * 0.08f;
        editRect.set(getWidth() - w - 12, 12, getWidth() - 12, 12 + h);
        paint.setColor(Color.argb(140, 0, 0, 0));
        canvas.drawRoundRect(editRect, 14, 14, paint);
        paint.setColor(Color.WHITE);
        paint.setTextSize(h * 0.45f);
        paint.setTextAlign(Paint.Align.CENTER);
        canvas.drawText("Editar", editRect.centerX(), editRect.centerY() + h * 0.16f, paint);
    }

    private void drawEditing(Canvas canvas) {
        // The selected control's outline
        RectF r = rectOf(selected, new RectF());
        paint.setColor(Color.YELLOW);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(4f);
        canvas.drawRect(r, paint);
        paint.setStyle(Paint.Style.FILL);
        // The panel, along the bottom
        float gap = 10f, count = PANEL_LABELS.length;
        float w = (getWidth() - gap * (count + 1)) / count, h = getHeight() * 0.1f;
        float top = getHeight() - h - gap;
        paint.setTextSize(h * 0.36f);
        paint.setTextAlign(Paint.Align.CENTER);
        for (int i = 0; i < PANEL_LABELS.length; i++) {
            float left = gap + i * (w + gap);
            panelRects[i] = new RectF(left, top, left + w, top + h);
            paint.setColor(Color.argb(190, 20, 20, 20));
            canvas.drawRoundRect(panelRects[i], 14, 14, paint);
            paint.setColor(Color.WHITE);
            String label = i == 0 ? PANEL_LABELS[0] + ": " + NAMES[selected] : PANEL_LABELS[i];
            canvas.drawText(label, panelRects[i].centerX(), panelRects[i].centerY() + h * 0.13f, paint);
        }
    }

    /** Edit mode on: the game gets no touches until "Listo". Called by the activity's button. */
    public void startEditing() {
        editing = true;
        owners.clear();
        grabs.clear();
        sendPad();
        postInvalidateOnAnimation();
    }

    /** The touches that the overlay didn't take, for the game (null when none). */
    static MotionEvent without(MotionEvent e, boolean[] claimed) {
        int n = e.getPointerCount();
        int keep = 0;
        for (boolean c : claimed) {
            if (!c) {
                keep++;
            }
        }
        if (keep == n) {
            return e;
        }
        if (keep == 0) {
            return null;
        }
        int action = e.getActionMasked();
        if ((action == MotionEvent.ACTION_POINTER_DOWN || action == MotionEvent.ACTION_POINTER_UP)
                && claimed[e.getActionIndex()]) {
            action = MotionEvent.ACTION_MOVE; // the changing pointer is the overlay's: the rest just move
        }
        MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[keep];
        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[keep];
        int k = 0;
        for (int p = 0; p < n; p++) {
            if (claimed[p]) {
                continue;
            }
            props[k] = new MotionEvent.PointerProperties();
            e.getPointerProperties(p, props[k]);
            coords[k] = new MotionEvent.PointerCoords();
            e.getPointerCoords(p, coords[k]);
            k++;
        }
        return MotionEvent.obtain(e.getDownTime(), e.getEventTime(), action, keep, props, coords,
                e.getMetaState(), e.getButtonState(), e.getXPrecision(), e.getYPrecision(), e.getDeviceId(),
                e.getEdgeFlags(), e.getSource(), e.getFlags());
    }
}
