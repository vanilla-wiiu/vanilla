#define SCREEN_WIDTH    854
#define SCREEN_HEIGHT   480

#include <stdio.h>
#include <SDL2/SDL.h>
#include <vanilla.h>

int main(int argc, const char **argv)
{
	SDL_Window* window = NULL;
	SDL_Surface* screenSurface = NULL;
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
		fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	window = SDL_CreateWindow(
				"Vanilla Pi",
				SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
				SCREEN_WIDTH, SCREEN_HEIGHT,
				SDL_WINDOW_SHOWN
			);
	if (window == NULL) {
		fprintf(stderr, "could not create window: %s\n", SDL_GetError());
		return 1;
	}

	screenSurface = SDL_GetWindowSurface(window);
	SDL_FillRect(screenSurface, NULL, SDL_MapRGB(screenSurface->format, 0xFF, 0x00, 0xFF));
	SDL_UpdateWindowSurface(window);

	vanilla_start();

	SDL_Delay(10000);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}