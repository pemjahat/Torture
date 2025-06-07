#include <SDL.h>
#include <SDL_syswm.h> // Added for SDL_GetWindowWMInfo
#include <imgui.h>
#include <imgui_impl_sdl2.h>
//#include <cassert>
//#include <string>
#include <iostream>
#include "WindowApplication.h"

HWND WindowApplication::m_hwnd = nullptr;

// Function to initialize SDL3 and create a window
SDL_Window* InitSDL2(UINT width, UINT height)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return nullptr;
    }
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d12");;

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    SDL_Window* window = SDL_CreateWindow(
        "D3D12 Triangle",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        window_flags // Ensure D3D compatibility
    );

    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return nullptr;
    }

    return window;
}

int WindowApplication::Run(RenderApplication* pApp)
{
    // Initialize window
    SDL_Window* window = InitSDL2(pApp->GetWidth(), pApp->GetHeight());
    if (window == nullptr) {
        return 1;
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        std::cerr << "SDL_GetWindowWMInfo failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    m_hwnd = wmInfo.info.win.window;
    if (!m_hwnd || !IsWindow(m_hwnd)) {
        std::cerr << "Invalid HWND: " << m_hwnd << std::endl;
        return 1;
    }

    // Initialize render application
    pApp->OnInit(window);

    // Show window
    SDL_ShowWindow(window);
    SDL_UpdateWindowSurface(window);
    SDL_PumpEvents();

    // ImGui Renderer backend
    ImGui_ImplSDL2_InitForD3D(window);

    // Window main loop
    bool done = false;
    SDL_Event event;
    while (!done) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            switch (event.type) {
            case SDL_QUIT:
                done = true;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    done = true;
                }
                else
                {
                    pApp->OnKeyDown(event.key);
                }
                break;
            case SDL_KEYUP:
                pApp->OnKeyUp(event.key);
                break;
            }
        }

        if (done)
            break;

        // Start dear imgui frame
        ImGui_ImplSDL2_NewFrame();

        pApp->OnUpdate();
        pApp->OnRender();
    }

    pApp->OnDestroy();

    // Cleanup
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    return 0;
}