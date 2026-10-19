#pragma once

// The window the game's picture is shown in: the native platform makes it (SDL3, with SDL_WINDOW_OPENGL) and hands it over at
// start-up; the graphics make their OpenGL 3.3 core context on it and present each frame there. Without a window the frames are
// kept to be read back (NativeGraphicsLastPicture, display.h)

struct SDL_Window;

void NativeGraphicsAttachWindow(SDL_Window* window);
