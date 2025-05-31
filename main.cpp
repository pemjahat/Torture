#include <SDL.h>
#include <SDL_syswm.h> // Added for SDL_GetWindowWMInfo
#include <iostream>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h> // For ComPtr
#include <string>
#include <cassert>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#endif

using Microsoft::WRL::ComPtr;

// Helper function to format HRESULT error messages
std::string GetHResultErrorMessage(HRESULT hr) {
    char* errorMsg = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&errorMsg,
        0,
        nullptr
    );
    std::string message = errorMsg ? errorMsg : "Unknown HRESULT error";
    LocalFree(errorMsg);
    return message;
}

// Function to initialize SDL3 and create a window
SDL_Window* InitSDL3() 
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
        800,
        600,
        window_flags // Ensure D3D compatibility
    );

    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return nullptr;
    }

    return window;    
}

bool InitD3D12(SDL_Window* window, ComPtr<ID3D12Device>& device, ComPtr<IDXGISwapChain1>& swapChain, ComPtr<ID3D12CommandQueue>& commandQueue)
{
    // Enable debug layer (optional, for development)
#ifdef DX12_ENABLE_DEBUG_LAYER
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        SDL_Log("D3D12 debug layer enabled");
    }
    else {
        std::cerr << "Failed to enable D3D12 debug layer" << std::endl;
    }

    ComPtr<IDXGIDebug> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
        SDL_Log("DXGI debug layer enabled");
    }
    else {
        std::cerr << "Failed to enable DXGI debug layer" << std::endl;
    }

#endif

    // Create DXGI factory
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        std::cerr << "CreateDXGIFactory2 failed: " << GetHResultErrorMessage(hr);
        return false;
    }


    ComPtr<IDXGIAdapter1> adapter;
    factory->EnumAdapters1(0, &adapter); // Use first hardware adapter

    // Create D3D12 device
    hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr)) {
        std::cerr << "D3D12CreateDevice failed: " << GetHResultErrorMessage(hr);
        return false;
    }

    // Check device removal
    if (device->GetDeviceRemovedReason() != S_OK) {
        std::cerr << "Device removed: " << GetHResultErrorMessage(device->GetDeviceRemovedReason())
            << " (HRESULT: 0x" << std::hex << device->GetDeviceRemovedReason() << std::dec << ")" << std::endl;
        return false;
    }

    // Setup debug interface to break on any warning / error
#ifdef DX12_ENABLE_DEBUG_LAYER
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
    }
#endif

    // Create command queue for fallback swap chain creation
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;    
    hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    if (FAILED(hr)) {
        std::cerr << "CreateCommandQueue failed: " << GetHResultErrorMessage(hr)
            << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
        return false;
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        std::cerr << "SDL_GetWindowWMInfo failed: " << SDL_GetError() << std::endl;
        return false;
    }
    HWND hwnd = wmInfo.info.win.window;
    if (!hwnd || !IsWindow(hwnd)) {
        std::cerr << "Invalid HWND: " << hwnd << std::endl;
        return false;
    }    

    if (!IsWindowVisible(hwnd)) {
        printf("Window is not visible\n");
        SDL_ShowWindow(window);
        SDL_UpdateWindowSurface(window);
        SDL_PumpEvents();
    }

    // Get window dimensions for swap chain
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    SDL_Log("SDL Window size: %dx%d", width, height);
    if (width <= 0 || height <= 0) {
        std::cerr << "Invalid window dimensions: " << width << "x" << height << std::endl;
        return false;
    }

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = 0; // Use window size
    swapChainDesc.Height = 0; // Use window size
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;   // disable MSAA
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapChainDesc.Flags = 0;

    ComPtr<IDXGISwapChain1> tempSwapChain;
    hr = factory->CreateSwapChainForHwnd(
        commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &tempSwapChain
    );
    if (SUCCEEDED(hr)) {
        SDL_Log("Swap chain created successfully");
        // Cast to IDXGISwapChain4
        hr = tempSwapChain.As(&swapChain);
        if (FAILED(hr)) {
            std::cerr << "Failed to cast swap chain to IDXGISwapChain4: " << GetHResultErrorMessage(hr)
                << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
            return false;
        }
        SDL_Log("Swap chain cast to IDXGISwapChain4 successfully");
        return true;
    }
    else {
        std::cerr << "CreateSwapChainForHwnd failed "
            << GetHResultErrorMessage(hr)
            << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
    }

    std::cerr << "All swap chain creation attempts failed" << std::endl;
    return false;
}

// Placeholder for triangle rendering (to be implemented)
void RenderTriangle(ComPtr<ID3D12Device>& device, ComPtr<IDXGISwapChain1>& swapChain) {
    // TODO: Implement D3D12 pipeline setup and triangle rendering
    // Steps:
    // 1. Create command queue and allocator
    // 2. Create render target views for swap chain buffers
    // 3. Create root signature and pipeline state
    // 4. Create vertex buffer for triangle
    // 5. Record command list to clear screen and draw triangle
    // 6. Present swap chain
}

int main(int argc, char* argv[]) {
    // Initialize SDL3 and create window
    SDL_Window* window = InitSDL3();
    if (window == nullptr) {
        return 1;
    }

    // Initialize D3D12
    ComPtr<ID3D12Device> device;
    ComPtr<IDXGISwapChain1> swapChain;
    ComPtr<ID3D12CommandQueue> commandQueue;
    if (!InitD3D12(window, device, swapChain, commandQueue))
    {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Main loop
    bool running = true;
    SDL_Event event;
    while (running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
                break;
            }
        }
        // Render triangle
        RenderTriangle(device, swapChain);
    }

    // Cleanup
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}