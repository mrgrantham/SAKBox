#include <SDL2/SDL.h>
#include <iostream>
#include <fmt/core.h>

int main()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        fmt::println("Failed to initialize the SDL2 library");
        return -1;
    }
    #if defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    const int windowHeight = 480;
    const int windowWidth = 680;
    SDL_Window *window = SDL_CreateWindow("SDL2 Window",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          windowWidth, windowHeight,
                                          window_flags);
    SDL_Renderer *sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	SDL_Texture *sdlTexture = SDL_CreateTexture(sdlRenderer,
										SDL_PIXELFORMAT_ARGB8888,
										SDL_TEXTUREACCESS_STREAMING,
										windowWidth, windowHeight);
    if(!window)
    {
        fmt::println("Failed to create window");
        return -1;
    }

    // SDL_Surface *window_surface = SDL_GetWindowSurface(window);

    // if(!window_surface)
    // {
    //     fmt::println("Failed to get the surface from the window");
    //     return -1;
    // }
    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }
        int *pixels = nullptr;
		int pitch = 0;
        const SDL_Rect* SDL_LOCK_ENTIRE_TEXTURE = nullptr;
        if ( SDL_LockTexture(sdlTexture,SDL_LOCK_ENTIRE_TEXTURE,(void **)&pixels,&pitch) < 0 )
		{
			fmt::println("ERROR");
			return 1;
		}
        // SDL_UpdateWindowSurface(window);
        SDL_SetRenderTarget(sdlTexture)
        SDL_UnlockTexture(sdlTexture);
    	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
	    // Update the whole screen
	    SDL_RenderPresent(sdlRenderer);
    }

    // Cleanup
    SDL_DestroyTexture(sdlTexture);
    SDL_DestroyRenderer(sdlRenderer);
    // SDL_Delay(5000);
    SDL_DestroyWindow(window);
    SDL_Quit();
}