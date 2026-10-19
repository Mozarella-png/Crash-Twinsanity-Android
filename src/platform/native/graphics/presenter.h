#pragma once

// The presenter's settings for the native UI (display.cpp). None of them changes the PS2's picture, only how the window shows it.

// The rectangle the game's picture is shown in, in the window's points (not pixels: HiDPI windows have more pixels than points),
// after the letterbox or pillarbox for the TV's shape. All zero before a window is attached
void NativeGraphicsPictureRect(float* x, float* y, float* w, float* h);

// The picture's scaling to the window: linear (the default) or nearest
void NativeGraphicsSetPresentFilter(bool linear);

// A shader drawing the picture in place of the plain one: 0 none, 1 the UI's CRT filter (NativeUiCrtShaderSource). Its fragment
// shader (GLSL 330 core) gets `in vec2 place` (the picture's coordinates, 0-1, top row first) and writes `out vec4 colour`; the
// presenter sets `uniform sampler2D picture` (unit 0), `uniform vec2 pictureSize` (the picture's pixels), `uniform vec2 viewSize`
// (the pixels it's drawn over) and `uniform float time` (seconds), then calls NativeUiCrtShaderUniforms for anything else. A
// shader that doesn't compile is reported and the plain one stays
void NativeGraphicsSetPostFilter(int which);
// The post filter drawing now (0 none: none asked for, or it didn't compile); it's made at the next picture shown
int NativeGraphicsPostFilterInUse();

// The UI's side (src/platform/native/ui/crtfilter.cpp); weak fallbacks here: no source, nothing to set
const char* NativeUiCrtShaderSource();
// With the filter's program in use: getProcAddress is SDL_GL_GetProcAddress, for the GL entry points it wants
void NativeUiCrtShaderUniforms(unsigned int program, void* (*getProcAddress)(const char* name));

// A drawing over the window after each picture is presented (the UI's screens drawn at the window's own resolution): called on
// the presenter's thread with its OpenGL 3.3 core context current, the window's framebuffer bound, the viewport the whole window
// (its pixels), depth, stencil and scissor tests off and the blend state left to it; pixelScale is the window's pixels a point.
// It leaves GL's state as it likes: the presenter and the hardware renderer set what they need. Null: none
void NativeGraphicsSetOverlay(void (*draw)(int windowPixelWidth, int windowPixelHeight, float pixelScale));

// Testing: the window as the next picture shows it (the picture and the overlay over it, at the window's pixels) written to a PNG
// (none without a window: a headless run)
void NativeGraphicsShootWindow(const char* path);
