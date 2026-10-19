/*
 * Stub de libmain.so para validar la cadena de build (Gradle, NDK, SDL3, puente JNI del overlay) antes de tener el
 * juego compilado para Android. Abre una ventana de SDL3, pinta un fondo y cierra cuando el usuario sale.
 * El juego real sustituye este archivo en la fase C.
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Crash Twinsanity", 1280, 720, SDL_WINDOW_FULLSCREEN);
    if (window == NULL)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return 1;
    }

    int running = 1;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = 0;
            }
        }

        SDL_SetRenderDrawColor(renderer, 230, 200, 40, 255); /* el amarillo del ícono */
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
