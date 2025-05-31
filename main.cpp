#include <SDL.h>
#include <SDL_syswm.h> // Added for SDL_GetWindowWMInfo
#include <iostream>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <dxgidebug.h> // Added for IDXGIDebug
#include <wrl/client.h> // For ComPtr
#include <stdexcept>
#include <string>
#include <cassert>
#include <sstream> // Added for std::wstringstream
#include <vector>
#include <thread> // For std::this_thread::sleep_for
#include <wrl.h>

using namespace Microsoft::WRL;
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

// Function to capture DXGI debug messages
void CaptureDXGIDebugMessages() {
    ComPtr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(dxgiDebug.As(&dxgiInfoQueue))) {
            UINT64 messageCount = dxgiInfoQueue->GetNumStoredMessages(DXGI_DEBUG_ALL);
            SDL_Log("DXGI debug messages found: %llu", messageCount);
            for (UINT64 i = 0; i < messageCount; ++i) {
                SIZE_T messageLength = 0;
                dxgiInfoQueue->GetMessage(DXGI_DEBUG_ALL, i, nullptr, &messageLength);
                std::vector<char> messageData(messageLength);
                DXGI_INFO_QUEUE_MESSAGE* pMessage = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>(messageData.data());
                dxgiInfoQueue->GetMessage(DXGI_DEBUG_ALL, i, pMessage, &messageLength);
                std::cerr << "DXGI Debug: " << pMessage->pDescription << std::endl;
            }
            dxgiInfoQueue->ClearStoredMessages(DXGI_DEBUG_ALL);
        }
        else {
            std::cerr << "Failed to get IDXGIInfoQueue" << std::endl;
        }
    }
    else {
        std::cerr << "Failed to get IDXGIDebug1" << std::endl;
    }
}

void CaptureD3D12DebugMessages(ComPtr<ID3D12Device> device) {
    if (!device) return;
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        UINT64 messageCount = infoQueue->GetNumStoredMessages();
        SDL_Log("D3D12 debug messages found: %llu", messageCount);
        for (UINT64 i = 0; i < messageCount; ++i) {
            SIZE_T messageLength = 0;
            infoQueue->GetMessage(i, nullptr, &messageLength);
            std::vector<char> messageData(messageLength);
            D3D12_MESSAGE* pMessage = reinterpret_cast<D3D12_MESSAGE*>(messageData.data());
            infoQueue->GetMessage(i, pMessage, &messageLength);
            std::cerr << "D3D12 Debug: " << pMessage->pDescription << std::endl;
        }
        infoQueue->ClearStoredMessages();
    }
    else {
        std::cerr << "Failed to get ID3D12InfoQueue" << std::endl;
    }
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

    //// Ensure window is visible and focused
    //SDL_ShowWindow(window);
    //SDL_RaiseWindow(window);
    //SDL_MaximizeWindow(window); // Try maximizing to ensure visibility
    //SDL_Log("Window shown and maximized");

    //// Check window state
    //Uint32 windowFlags = SDL_GetWindowFlags(window);
    //if (windowFlags & SDL_WINDOW_MINIMIZED) {
    //    std::cerr << "Window is minimized, restoring" << std::endl;
    //    SDL_RestoreWindow(window);
    //}
    //if (windowFlags & SDL_WINDOW_HIDDEN) {
    //    std::cerr << "Window is hidden, showing again" << std::endl;
    //    SDL_ShowWindow(window);
    //}
    //SDL_Log("Window flags: 0x%x", windowFlags);

    //// Validate window dimensions
    //int width, height;
    //SDL_GetWindowSize(window, &width, &height);
    //if (width <= 0 || height <= 0) {
    //    std::cerr << "Invalid window dimensions: " << width << "x" << height << std::endl;
    //    SDL_DestroyWindow(window);
    //    SDL_Quit();
    //    return nullptr;
    //}
    return window;    
}

bool InitD3D12(SDL_Window* window, ComPtr<ID3D12Device>& device, ComPtr<IDXGISwapChain1>& swapChain, ComPtr<ID3D12CommandQueue>& commandQueue)
{
    // Enable debug layer (optional, for development)
#ifdef _DEBUG
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

#ifdef _DEBUG
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
    }
#endif

    // Capture D3D12 debug messages
    CaptureD3D12DebugMessages(device);

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

    //// Log GPU adapter details
    //ComPtr<IDXGIAdapter> adapter;
    //if (SUCCEEDED(factory->EnumAdapters(0, &adapter))) {
    //    DXGI_ADAPTER_DESC desc;
    //    adapter->GetDesc(&desc);
    //    std::wstringstream wss;
    //    wss << L"GPU Adapter: " << desc.Description;
    //    std::wstring ws = wss.str();
    //    std::string adapterDesc(ws.begin(), ws.end());
    //    SDL_Log("Using GPU: %s", adapterDesc.c_str());
    //}
    //else {
    //    std::cerr << "Failed to enumerate GPU adapters" << std::endl;
    //}


    // Get native window handle (HWND) using SDL3 properties
    /*SDL_PropertiesID props = SDL_GetWindowProperties(window);
    if (props == 0) {
        std::cerr << "SDL_GetWindowProperties failed: " << std::endl;
        return false;
    }
    HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!hwnd) {
        std::cerr << "Failed to get HWND from SDL window properties" << std::endl;
        return false;
    }
    if (!IsWindow(hwnd)) {
        std::cerr << "Invalid HWND: " << hwnd << std::endl;
        return false;
    }*/
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

    // Validate window rectangle
    RECT rect;
    if (!GetWindowRect(hwnd, &rect)) {
        std::cerr << "GetWindowRect failed for HWND: " << hwnd << std::endl;
        return false;
    }
    SDL_Log("HWND rect: left=%ld, top=%ld, right=%ld, bottom=%ld",
        rect.left, rect.top, rect.right, rect.bottom);
    if (rect.right <= rect.left || rect.bottom <= rect.top) {
        std::cerr << "Invalid window rectangle: width or height <= 0" << std::endl;
        return false;
    }

    // Get window dimensions for swap chain
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    SDL_Log("SDL Window size: %dx%d", width, height);
    if (width <= 0 || height <= 0) {
        std::cerr << "Invalid window dimensions: " << width << "x" << height << std::endl;
        return false;
    }


    //// Create swap chain
    //DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    //swapChainDesc.Width = width; // Use window size
    //swapChainDesc.Height = height; // Use window size
    //swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    //swapChainDesc.SampleDesc.Count = 1;
    //swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    //swapChainDesc.BufferCount = 2;
    //swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    //ComPtr<IDXGISwapChain1> tempSwapChain;
    //hr = factory->CreateSwapChainForHwnd(
    //    device.Get(),
    //    hwnd,
    //    &swapChainDesc,
    //    nullptr,
    //    nullptr,
    //    &tempSwapChain);

    /*if (FAILED(hr)) {
        std::cerr << "CreateSwapChainForHwnd failed: " << GetHResultErrorMessage(hr)
            << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
        return false;
    }*/
    //if (FAILED(hr)) {
    //    std::cerr << "CreateSwapChainForHwnd failed: " << GetHResultErrorMessage(hr)
    //        << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
    //    // Try alternative SwapEffect
    //    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    //    SDL_Log("Retrying with SwapEffect=DXGI_SWAP_EFFECT_DISCARD");
    //    hr = factory->CreateSwapChainForHwnd(
    //        device.Get(),
    //        hwnd,
    //        &swapChainDesc,
    //        nullptr,
    //        nullptr,
    //        &tempSwapChain
    //    );
    //    if (FAILED(hr)) {
    //        std::cerr << "CreateSwapChainForHwnd failed with Format=" << formatName
    //            << ", SwapEffect=" << effectName << ": " << GetHResultErrorMessage(hr)
    //            << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
    //        return false;
    //    }
    //}

    //// Cast to IDXGISwapChain4
    //hr = tempSwapChain.As(&swapChain);
    //if (FAILED(hr)) {
    //    std::cerr << "Failed to cast swap chain to IDXGISwapChain4: " << GetHResultErrorMessage(hr)
    //        << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
    //    return false;
    //}
    
    std::vector<std::pair<DXGI_FORMAT, std::string>> formats = {
       {DXGI_FORMAT_R8G8B8A8_UNORM, "R8G8B8A8_UNORM"},
       {DXGI_FORMAT_B8G8R8A8_UNORM, "B8G8R8A8_UNORM"}
    };
    for (const auto& formatPair : formats) {
        DXGI_FORMAT format = formatPair.first;
        D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport = {};
        formatSupport.Format = format;
        if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport)))) {
            if (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) {
                SDL_Log("Format %s is supported as render target", formatPair.second.c_str());
            }
            else {
                SDL_Log("Format %s is NOT supported as render target", formatPair.second.c_str());
            }
        }
        else {
            std::cerr << "Failed to check format support for " << formatPair.second << std::endl;
        }
    }

    // Try multiple swap chain configurations
    std::vector<std::pair<DXGI_SWAP_EFFECT, std::string>> swapEffects = {
        {DXGI_SWAP_EFFECT_FLIP_DISCARD, "FLIP_DISCARD"},
    };

    for (const auto& formatPair : formats) {
        DXGI_FORMAT format = formatPair.first;
        const std::string& formatName = formatPair.second;

        // Validate format
        D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport = {};
        formatSupport.Format = format;
        if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport)))) {
            if (!(formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)) {
                std::cerr << "Swap chain format " << formatName << " not supported as render target" << std::endl;
                continue;
            }
            SDL_Log("Swap chain format %s supported", formatName.c_str());
        }
        else {
            std::cerr << "Failed to check format support for " << formatName << std::endl;
            continue;
        }

        for (const auto& effectPair : swapEffects) {
            DXGI_SWAP_EFFECT swapEffect = effectPair.first;
            const std::string& effectName = effectPair.second;

            // Create swap chain
            DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
            swapChainDesc.Width = width; // Use window size
            swapChainDesc.Height = height; // Use window size
            swapChainDesc.Format = format;
            swapChainDesc.SampleDesc.Count = 1;
            swapChainDesc.SampleDesc.Quality = 0;   // disable MSAA
            swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapChainDesc.BufferCount = 2;
            swapChainDesc.SwapEffect = swapEffect;
            swapChainDesc.Stereo = FALSE;
            swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
            swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            swapChainDesc.Flags = 0;


            // Log swap chain parameters
            SDL_Log("Trying swap chain: Format=%s, SwapEffect=%s, Width=%d, Height=%d, BufferCount=%d",
                formatName.c_str(), effectName.c_str(), swapChainDesc.Width, swapChainDesc.Height, swapChainDesc.BufferCount);

            ComPtr<IDXGISwapChain1> tempSwapChain;
            hr = factory->CreateSwapChainForHwnd(
                device.Get(),
                hwnd,
                &swapChainDesc,
                nullptr,
                nullptr,
                &tempSwapChain
            );
            if (SUCCEEDED(hr)) {
                SDL_Log("Swap chain created successfully with Format=%s, SwapEffect=%s",
                    formatName.c_str(), effectName.c_str());
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
                std::cerr << "CreateSwapChainForHwnd failed with Format=" << formatName
                    << ", SwapEffect=" << effectName << ": " << GetHResultErrorMessage(hr)
                    << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
                CaptureDXGIDebugMessages(); // Capture DXGI debug messages
                CaptureD3D12DebugMessages(device);
            }
        }
    }

    for (const auto& formatPair : formats) {
        DXGI_FORMAT format = formatPair.first;
        const std::string& formatName = formatPair.second;

        D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport = {};
        formatSupport.Format = format;
        if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport)))) {
            if (!(formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)) {
                continue;
            }
        }
        else {
            continue;
        }

        for (const auto& effectPair : swapEffects) {
            DXGI_SWAP_EFFECT swapEffect = effectPair.first;
            const std::string& effectName = effectPair.second;

            DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
            swapChainDesc.BufferDesc.Width = width;
            swapChainDesc.BufferDesc.Height = height;
            swapChainDesc.BufferDesc.Format = format;
            swapChainDesc.SampleDesc.Count = 1;
            swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapChainDesc.BufferCount = 2;
            swapChainDesc.SwapEffect = swapEffect;
            swapChainDesc.OutputWindow = hwnd;
            swapChainDesc.Windowed = TRUE;
            swapChainDesc.Flags = 0;

            SDL_Log("Trying CreateSwapChain: Format=%s, SwapEffect=%s, Width=%d, Height=%d, BufferCount=%d",
                formatName.c_str(), effectName.c_str(), swapChainDesc.BufferDesc.Width, swapChainDesc.BufferDesc.Height, swapChainDesc.BufferCount);

            ComPtr<IDXGISwapChain> tempSwapChain;
            hr = factory->CreateSwapChain(
                commandQueue.Get(),
                &swapChainDesc,
                &tempSwapChain
            );
            if (SUCCEEDED(hr)) {
                SDL_Log("Swap chain created successfully with Format=%s, SwapEffect=%s",
                    formatName.c_str(), effectName.c_str());
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
                std::cerr << "CreateSwapChain failed with Format=" << formatName
                    << ", SwapEffect=" << effectName << ": " << GetHResultErrorMessage(hr)
                    << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
                CaptureDXGIDebugMessages();
                CaptureD3D12DebugMessages(device);
            }
        }
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