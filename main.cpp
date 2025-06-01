#include <SDL.h>
#include <SDL_syswm.h> // Added for SDL_GetWindowWMInfo
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

// Vertex structure
struct Vertex {
    float position[3];
    float color[4];
};

bool InitD3D12(SDL_Window* window, ComPtr<ID3D12Device>& device, ComPtr<IDXGISwapChain3>& swapChain, ComPtr<ID3D12CommandQueue>& commandQueue)
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

    

    return true;
}

// Placeholder for triangle rendering (to be implemented)
void RenderTriangle(ComPtr<ID3D12Device>& device, ComPtr<IDXGISwapChain3>& swapChain) {
    // TODO: Implement D3D12 pipeline setup and triangle rendering
    // Steps:
    // 1. Create command queue and allocator
    // 2. Create render target views for swap chain buffers
    // 3. Create root signature and pipeline state
    // 4. Create vertex buffer for triangle
    // 5. Record command list to clear screen and draw triangle
    // 6. Present swap chain
    UINT frameIndex = swapChain->GetCurrentBackBufferIndex();

    // Reset command allocator and list
    //commandAllocators[frameIndex]->Reset();
    //commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get());

}

int main(int argc, char* argv[]) {
    // Initialize SDL3 and create window
    SDL_Window* window = InitSDL3();
    if (window == nullptr) {
        return 1;
    }

    // Initialize D3D12
    ComPtr<ID3D12Device> device;
    ComPtr<IDXGISwapChain3> swapChain;
    ComPtr<ID3D12CommandQueue> commandQueue;
    if (!InitD3D12(window, device, swapChain, commandQueue))
    {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create RTV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 2;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    HRESULT hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));
    if (FAILED(hr)) {
        std::cerr << "Create descriptor heap failed "
            << GetHResultErrorMessage(hr)
            << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create RTV for swap chain buffer
    ComPtr<ID3D12Resource> renderTargets[2];
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT i = 0; i < 2; ++i) {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
        if (FAILED(hr)) {
            std::cerr << "Create descriptor heap failed "
                << GetHResultErrorMessage(hr)
                << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += rtvDescriptorSize;
    }

    // Create command allocators
    ComPtr<ID3D12CommandAllocator> commandAllocators[2];
    for (UINT i = 0; i < 2; ++i) {
        hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[i]));
        if (FAILED(hr)) {
            std::cerr << "Create command allocator failed "
                << GetHResultErrorMessage(hr)
                << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
    }

    // Create command list
    ComPtr<ID3D12GraphicsCommandList> commandList;
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&commandList));
    if (FAILED(hr)) {
        std::cerr << "Create command list failed "
            << GetHResultErrorMessage(hr)
            << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    commandList->Close();

    // Create vertex buffer
    Vertex vertices[] = {
        { {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } }, // Top (red)
        { {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } }, // Bottom right (green)
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }  // Bottom left (blue)
    };
    const UINT vertexBufferSize = sizeof(vertices);

    ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = vertexBufferSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.SampleDesc.Quality = 0;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer)
    );
    if (FAILED(hr)) {
        std::cerr << "Create commited resource failed "
            << GetHResultErrorMessage(hr)
            << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Copy vertex data to buffer
    void* data;
    vertexBuffer->Map(0, nullptr, &data);
    memcpy(data, vertices, vertexBufferSize);
    vertexBuffer->Unmap(0, nullptr);

    // Create vertex buffer view
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = vertexBufferSize;
    vertexBufferView.StrideInBytes = sizeof(Vertex);

    // Create root signature
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr)) {
        std::cerr << "D3DSerializeRootSignature failed "
            << GetHResultErrorMessage(hr)
            << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
        if (error) {
            std::cerr << static_cast<const char*>(error->GetBufferPointer());
        }
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    ComPtr<ID3D12RootSignature> rootSignature;
    hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) {
        std::cerr << "CreateRootSignature failed "
            << GetHResultErrorMessage(hr)
            << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Compile shaders
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;

    hr = D3DCompileFromFile(L"basic.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", 0, 0, &vertexShader, &error);
    if (FAILED(hr)) {
        std::cerr << "Vertex shader compilation failed "
            << GetHResultErrorMessage(hr)
            << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
        if (error) {
            std::cerr << static_cast<const char*>(error->GetBufferPointer());
        }
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    hr = D3DCompileFromFile(L"basic.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", 0, 0, &pixelShader, &error);
    if (FAILED(hr)) {
        std::cerr << "Pixel shader compilation failed "
            << GetHResultErrorMessage(hr)
            << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
        if (error) {
            std::cerr << static_cast<const char*>(error->GetBufferPointer());
        }
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create pipeline state
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = 0;
    rasterizerDesc.DepthBiasClamp = 0.0f;
    rasterizerDesc.SlopeScaledDepthBias = 0.0f;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Manually initialize blend state (replacing CD3DX12_BLEND_DESC)
    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
        blendDesc.RenderTarget[i].BlendEnable = FALSE;
        blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    ComPtr<ID3D12PipelineState> pipelineState;
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) {
        std::cerr << "CreateGraphicsPipelineState failed "
            << GetHResultErrorMessage(hr)
            << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    // Create viewport and scissor rect
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
    D3D12_RECT scissorRect = { 0, 0, width, height };

    // Create fence for synchronization
    ComPtr<ID3D12Fence> fence;
    UINT64 fenceValues[2] = { 0, 0 };
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr)) {
        std::cerr << "CreateFence failed "
            << GetHResultErrorMessage(hr)
            << " (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent) {
        //LogError("CreateEvent failed", HRESULT_FROM_WIN32(GetLastError()));
        std::cerr << "CreateEvent failed " << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Main loop
    UINT frameIndex = swapChain->GetCurrentBackBufferIndex();
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

        if (!running)
            break;

        // Render triangle
        RenderTriangle(device, swapChain);

        // Reset command allocator and list
        commandAllocators[frameIndex]->Reset();
        commandList->Reset(commandAllocators[frameIndex].Get(), pipelineState.Get());

        // Transition back buffer to RENDER_TARGET
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = renderTargets[frameIndex].Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);

        // Set render target
        rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += frameIndex * rtvDescriptorSize;
        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        // Clear render target
        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

        // Set pipeline state and resources
        commandList->SetGraphicsRootSignature(rootSignature.Get());
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissorRect);
        commandList->SetPipelineState(pipelineState.Get());
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

        // Draw triangle
        commandList->DrawInstanced(3, 1, 0, 0);

        // Transition back buffer to PRESENT
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        commandList->ResourceBarrier(1, &barrier);

        commandList->Close();

        // Execute command list
        ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
        commandQueue->ExecuteCommandLists(1, ppCommandLists);

        // Present
        swapChain->Present(1, 0);

        // Wait for GPU
        const UINT64 fenceValue = fenceValues[frameIndex] + 1;
        commandQueue->Signal(fence.Get(), fenceValue);
        fenceValues[frameIndex] = fenceValue;
        if (fence->GetCompletedValue() < fenceValue) {
            fence->SetEventOnCompletion(fenceValue, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }

        frameIndex = swapChain->GetCurrentBackBufferIndex();
    }
    // Wait for GPU to finish all work before cleanup
    WaitForGPU(commandQueue.Get(), fence.Get(), fenceValues[frameIndex], fenceEvent);

    // Cleanup
    CloseHandle(fenceEvent);
    commandList.Reset();
    for (auto& allocator : commandAllocators) allocator.Reset();
    for (auto& target : renderTargets) target.Reset();
    rtvHeap.Reset();
    swapChain.Reset();
    commandQueue.Reset();
    device.Reset();
    vertexBuffer.Reset();
    rootSignature.Reset();
    pipelineState.Reset();    

    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}