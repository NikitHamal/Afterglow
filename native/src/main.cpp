// Afterglow native — main.cpp (SDL entry; SDL_main.h supplies WinMain).
#include "ag_platform.h"
#ifdef AG_WITH_SDL
#include <SDL3/SDL_main.h>
#endif

int main(int argc, char* argv[]) { return ag::runGame(argc, argv); }
