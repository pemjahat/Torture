#include <SDL.h>
#include <SDL_syswm.h> // Added for SDL_GetWindowWMInfo
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_dx12.h>
#include <iostream>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
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

// static const
static const int NUM_FRAME = 2; // Not really back buffer, for frame swap
static const int NUM_BACK_BUFFER = 2;
static const int SRV_HEAP_SIZE = 64;

static const UINT WinWidth = 800;
static const UINT WinHeight = 600;

// Vertex structure
struct Vertex {
    float position[3];
    float color[4];
};

// d3d12 structure
struct DescriptorHeapAllocator
{
    ComPtr<ID3D12DescriptorHeap> Heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT    HeapHandleIncrement;
    ImVector<int>   FreeIndices;

    void Create(ComPtr<ID3D12Device> device, ComPtr<ID3D12DescriptorHeap> heap)
    {
        Heap = heap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
        HeapType = desc.Type;
        HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
        HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
        HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
        FreeIndices.reserve(desc.NumDescriptors);
        for (int n = desc.NumDescriptors; n > 0; n--)
            FreeIndices.push_back(n - 1);
    }

    void Destroy()
    {
        Heap = nullptr;
        FreeIndices.clear();
    }

    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
    {
        int idx = FreeIndices.back();
        FreeIndices.pop_back();
        out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
        out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
    }

    void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle)
    {
        int cpu_idx = (int)((cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
        int gpu_idx = (int)((gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
        FreeIndices.push_back(cpu_idx);
    }
};

// Global variable
static ComPtr<ID3D12Device>         g_d3dDevice = nullptr;
static ComPtr<ID3D12DescriptorHeap> g_rtvDescHeap = nullptr;
static ComPtr<ID3D12DescriptorHeap> g_srvDescHeap = nullptr;
static DescriptorHeapAllocator      g_descHeapAllocator;

static ComPtr<ID3D12CommandAllocator>   g_commandAllocator = nullptr;
static ComPtr<ID3D12CommandQueue>   g_commandQueue = nullptr;
static ComPtr<ID3D12GraphicsCommandList>    g_commandList = nullptr;

// Synchronization
static ComPtr<ID3D12Fence>  g_fence = nullptr;
static HANDLE   g_fenceEvent = nullptr;
static UINT64   g_fenceValue = 0;
//static UINT64   g_fenceLastSignaledValue = 0;
static UINT g_frameIndex = 0;

static ComPtr<IDXGISwapChain3>  g_swapChain = nullptr;
static ComPtr<ID3D12Resource>   g_rtvResource[NUM_BACK_BUFFER] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE  g_rtvDescHandle[NUM_BACK_BUFFER] = {};

// Fwd declaration
bool CreateDeviceD3D(HWND hwnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void WaitForPreviousFrame();

void LogError(const char* message, HRESULT hr = S_OK)
{
    std::string errorMsg = message;
    if (hr != S_OK) {
        char hrMsg[512];
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0, hrMsg, sizeof(hrMsg), nullptr);
        errorMsg += " HRESULT: 0x" + std::to_string(hr) + " - " + hrMsg;
    }
    errorMsg += "\n";
    std::cerr << errorMsg;
}

// Wait GPU finish all works
void WaitForGPU(ID3D12CommandQueue* commandQueue, ID3D12Fence* fence, UINT64& fenceValue, HANDLE fenceEvent)
{
    HRESULT hr = commandQueue->Signal(fence, fenceValue);
    if (FAILED(hr)) {
        LogError("Signal fence failed", hr);
        return;
    }
    if (fence->GetCompletedValue() < fenceValue) {
        hr = fence->SetEventOnCompletion(fenceValue, fenceEvent);
        if (FAILED(hr)) {
            LogError("SetEventOnCompletion failed", hr);
            return;
        }
        WaitForSingleObject(fenceEvent, INFINITE);
    }
    fenceValue++;
}

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

HWND GetHwnd(SDL_Window* window)
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        std::cerr << "SDL_GetWindowWMInfo failed: " << SDL_GetError() << std::endl;
        return nullptr;
    }
    HWND hwnd = wmInfo.info.win.window;
    if (!hwnd || !IsWindow(hwnd)) {
        std::cerr << "Invalid HWND: " << hwnd << std::endl;
        return nullptr;
    }
    return hwnd;
}

// Main
int main(int, char**)
{
    SDL_Window* window = InitSDL2(WinWidth, WinHeight);
    if (window == nullptr) {
        return 1;
    }

    HWND hwnd = GetHwnd(window);
    if (hwnd == nullptr)
    {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Show window
    SDL_ShowWindow(window);
    SDL_UpdateWindowSurface(window);
    SDL_PumpEvents();

    // Setup dear imgui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup dear imgui
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // ImGui Renderer backend
    ImGui_ImplSDL2_InitForD3D(window);    

    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = g_d3dDevice.Get();
    init_info.CommandQueue = g_commandQueue.Get();
    init_info.NumFramesInFlight = NUM_FRAME;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
    // (current version of the backend will only allocate one descriptor, future versions will need to allocate more)
    init_info.SrvDescriptorHeap = g_srvDescHeap.Get();
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return g_descHeapAllocator.Alloc(out_cpu_handle, out_gpu_handle); };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return g_descHeapAllocator.Free(cpu_handle, gpu_handle); };
    ImGui_ImplDX12_Init(&init_info);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    WaitForPreviousFrame();

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
                break;
            }
        }

        if (done)
            break;

        // Start dear imgui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // Simple window
        {
            static float f = 0.f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");

            ImGui::Text("This is some useful text.");
            ImGui::Checkbox("Demo window", &show_demo_window);
            ImGui::Checkbox("Another window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.f, 1.f);
            ImGui::ColorEdit3("clear color", (float*)&clear_color);

            if (ImGui::Button("Button"))
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        if (show_another_window)
        {
            ImGui::Begin("Another window", &show_another_window);
            ImGui::Text("Hello from another window");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Render
        ImGui::Render();

        g_commandAllocator->Reset();
        g_commandList->Reset(g_commandAllocator.Get(), nullptr);

        // Transition back buffer to RENDER_TARGET
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = g_rtvResource[g_frameIndex].Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        g_commandList->ResourceBarrier(1, &barrier);

        // Render ImGui graphics
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_commandList->ClearRenderTargetView(g_rtvDescHandle[g_frameIndex], clear_color_with_alpha, 0, nullptr);
        g_commandList->OMSetRenderTargets(1, &g_rtvDescHandle[g_frameIndex], FALSE, nullptr);
            
        ID3D12DescriptorHeap* ppDescHeaps[] = { g_srvDescHeap.Get() };
        g_commandList->SetDescriptorHeaps(1, ppDescHeaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_commandList.Get());

        // Transition back to PRESENT
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        g_commandList->ResourceBarrier(1, &barrier);
        g_commandList->Close();

        ID3D12CommandList* ppCommandLists[] = { g_commandList.Get() };
        g_commandQueue->ExecuteCommandLists(1, ppCommandLists);

        // Present
        HRESULT hr = g_swapChain->Present(1, 0);
            
        WaitForPreviousFrame();
    }

    //WaitForLastSubmittedFrame();
    //WaitForPreviousFrame();

    // Cleanup
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

bool CreateDeviceD3D(HWND hwnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = 0; // Use window size
    swapChainDesc.Height = 0; // Use window size
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;   // disable MSAA
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = NUM_BACK_BUFFER;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

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
#endif

    // Create D3D12 device
    HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_d3dDevice));
    if (FAILED(hr)) {
        LogError("D3D12CreateDevice failed", hr);
        return false;
    }

    // Setup debug interface to break on any warning / error
#ifdef DX12_ENABLE_DEBUG_LAYER
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(g_d3dDevice->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
        infoQueue.Reset();
        debugController.Reset();
    }
#endif

    // Create descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = NUM_BACK_BUFFER;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = g_d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_rtvDescHeap));
    if (FAILED(hr)) {
        LogError("Create rtv descriptor heap failed", hr);
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = SRV_HEAP_SIZE;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = g_d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&g_srvDescHeap));
    if (FAILED(hr))
    {
        LogError("Create srv descriptor heap failed", hr);
        return false;
    }
    g_descHeapAllocator.Create(g_d3dDevice, g_srvDescHeap);

    SIZE_T rtvDescriptorSize = g_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < NUM_BACK_BUFFER; ++i)
    {
        g_rtvDescHandle[i] = rtvHandle;
        rtvHandle.ptr += rtvDescriptorSize;
    }

    // Command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    hr = g_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue));
    if (FAILED(hr)) {
        LogError("CreateCommandQueue failed", hr);
        return false;
    }

    // Command allocator, command list
    hr = g_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocator));
    if (FAILED(hr)) {
        LogError("Create command allocator failed", hr);
        return false;
    }

    hr = g_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&g_commandList));
    if (FAILED(hr)) {
        LogError("Create command list failed", hr);
        return false;
    }
    g_commandList->Close();

    // Synchronization -> Fence and event
    hr = g_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence));
    if (FAILED(hr)) {
        LogError("CreateFence failed", hr);
        return false;
    }
    g_fenceValue = 1;

    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_fenceEvent) {
        std::cerr << "CreateEvent failed " << std::endl;
        return false;
    }

    // Factory and swap chain
    ComPtr<IDXGIFactory4> factory;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        LogError("CreateDXGIFactory2 failed", hr);
        return false;
    }

    ComPtr<IDXGISwapChain1> tempSwapChain;
    hr = factory->CreateSwapChainForHwnd(
        g_commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &tempSwapChain
    );
    if (SUCCEEDED(hr)) {
        SDL_Log("Swap chain created successfully");
        hr = tempSwapChain.As(&g_swapChain);
        g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

        if (FAILED(hr)) {
            LogError("Cast swap chain failed", hr);
            return false;
        }
    }
    else {
        LogError("CreateSwapChainForHwnd failed", hr);
        return false;
    }
    
    tempSwapChain.Reset();
    factory.Reset();

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    g_descHeapAllocator.Destroy();
    if (g_swapChain) { g_swapChain->SetFullscreenState(false, nullptr); g_swapChain.Reset(); }
    if (g_commandQueue) { g_commandQueue.Reset(); }
    if (g_commandAllocator) { g_commandAllocator.Reset(); }
    if (g_commandList) { g_commandList.Reset(); }
    if (g_rtvDescHeap) { g_rtvDescHeap.Reset(); }
    if (g_srvDescHeap) { g_srvDescHeap.Reset(); }
    if (g_fence) { g_fence.Reset(); }
    if (g_fenceEvent) { CloseHandle(g_fenceEvent); g_fenceEvent = nullptr; }
    if (g_d3dDevice) { g_d3dDevice.Reset(); }

#ifdef DX12_ENABLE_DEBUG_LAYER
    ComPtr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
        SDL_Log("DXGI debug layer enabled");
        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        dxgiDebug.Reset();
    }
#endif
}

void CreateRenderTarget()
{
    for (UINT i = 0; i < NUM_BACK_BUFFER; ++i)
    {
        ComPtr<ID3D12Resource> renderTarget;
        g_swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTarget));
        g_d3dDevice->CreateRenderTargetView(renderTarget.Get(), nullptr, g_rtvDescHandle[i]);
        g_rtvResource[i] = renderTarget;
    }
}

void CleanupRenderTarget()
{
    WaitForPreviousFrame();
    for (UINT i = 0; i < NUM_BACK_BUFFER; ++i)
        if (g_rtvResource[i]) { g_rtvResource[i].Reset(); }
}

void WaitForPreviousFrame()
{
    // Signal and increment fence value
    const UINT64 fence = g_fenceValue;
    g_commandQueue->Signal(g_fence.Get(), fence);
    g_fenceValue++;

    if (g_fence->GetCompletedValue() < fence)
    {
        g_fence->SetEventOnCompletion(fence, g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }

    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();
}