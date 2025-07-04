#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#endif

#include <filesystem>
#include <iostream>
// DX12
#include "d3dx12.h"
#include <dxcapi.h>
#include <DirectXCollision.h>

// imgui
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_dx12.h>
// app
#include "RenderApplication.h"
#include "WindowApplication.h"
#include "Helper.h"

static DescriptorHeapAllocator  g_descHeapAllocator;

CommandLineArgs ParseCommandLine(int argc, char** argv)
{
    CommandLineArgs args;
    if (argc > 0)
    {
        // Store application directory from argv[0]
        std::filesystem::path exePath(argv[0]);
        args.exePath = exePath.string();
    }

    // Parse args start from 1
    for (int i = 1; i < argc; ++i)
    {
        std::string token = argv[i];
        if (token.find("-resX=") == 0)
        {
            args.resX = std::stoi(token.substr(6));
            if (args.resX <= 0)
                args.resX = 800;
        }
        else if (token.find("-resY=") == 0)
        {
            args.resY = std::stoi(token.substr(6));
            if (args.resY <= 0)
                args.resY = 600;
        }
        else if (token.find("-model=") == 0)
        {
            args.modelPath = token.substr(7);
        }
    }
    return args;
}

// Convert polar (azimuth, elevation) to Cartesian (XMFLOAT3)
XMFLOAT3 PolarToCartesian(float azimuth, float elevation) {
    float cosPhi = cosf(elevation);
    XMFLOAT3 dir = {
        cosPhi * cosf(azimuth),
        sinf(elevation),
        cosPhi * sinf(azimuth)
    };
    XMVECTOR v = XMLoadFloat3(&dir);
    v = XMVector3Normalize(v); // Ensure normalized
    XMStoreFloat3(&dir, v);
    return dir;
}

RenderApplication::RenderApplication(int argc, char** argv) :
    m_cbvDataBegin(nullptr),
    m_lightDataBegin(nullptr),
    m_constantBufferData{},
    m_frameIndex(0),
    m_fenceValue{},
    m_moveSpeed(20.f),
    m_azimuth(XM_PI),
    m_elevation(0.f)
{
    CommandLineArgs args = ParseCommandLine(argc, argv);
    m_width = args.resX;
    m_height = args.resY;
    m_executablePath = args.exePath;
    m_modelPath = args.modelPath;

    m_aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);

    m_viewport = { 0.f, 0.f, (float)m_width, (float)m_height, 0.f, 1.f };
    m_scissorRect = { 0, 0, (LONG)m_width, (LONG)m_height };
}

void RenderApplication::OnInit(SDL_Window* window)
{
    m_camera.Init({ 0, 0.f, 10 });
    m_camera.SetMoveSpeed(m_moveSpeed);

    // Light
    m_directionalLight.direction = XMFLOAT3(-1.f, -1.f, -1.f);
    XMVECTOR v = XMLoadFloat3(&m_directionalLight.direction);
    v = XMVector3Normalize(v);
    XMStoreFloat3(&m_directionalLight.direction, v);

    m_directionalLight.intensity = 1.f;
    m_directionalLight.color = XMFLOAT3(1.f, 1.f, 1.f);

    LoadPipeline();
    LoadAsset(window);
}

// Helper function for resolving the full path of assets.
std::wstring RenderApplication::GetAssetFullPath(const std::string& relativePath)
{
    std::filesystem::path exePath = std::filesystem::absolute(m_executablePath);

    std::filesystem::path baseDir = exePath.parent_path().parent_path();

    std::filesystem::path absolutePath = baseDir / relativePath;

    return std::filesystem::absolute(absolutePath).wstring();
}

void RenderApplication::LoadPipeline()
{
    // Enable debug layer (optional, for development)
#ifdef DX12_ENABLE_DEBUG_LAYER
    ComPtr<ID3D12Debug3> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();

        const bool bEnableGPUBasedValidation = false;
        if (bEnableGPUBasedValidation)
        {
            debugController->SetEnableGPUBasedValidation(true);
        }
    }
    else
    {
        std::cerr << "Failed to enable D3D12 debug layer" << std::endl;
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    CheckHRESULT(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)));

    CheckHRESULT(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_d3dDevice)));

    // break on warnings/errors
#ifdef DX12_ENABLE_DEBUG_LAYER
    {
        ComPtr<ID3D12InfoQueue> debugInfoQueue;
        CheckHRESULT(m_d3dDevice->QueryInterface(__uuidof(ID3D12InfoQueue), (LPVOID *)&debugInfoQueue));

        // NOTE: add whatever d3d12 filters here when needed
        D3D12_INFO_QUEUE_FILTER newFilter{};

        // Turn off info msgs as these get really spewy
        D3D12_MESSAGE_SEVERITY denySeverity = D3D12_MESSAGE_SEVERITY_INFO;
        newFilter.DenyList.NumSeverities = 1;
        newFilter.DenyList.pSeverityList = &denySeverity;

        CheckHRESULT(debugInfoQueue->PushStorageFilter(&newFilter));
        CheckHRESULT(debugInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
        CheckHRESULT(debugInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true));
        CheckHRESULT(debugInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
    }
#endif // DX12_ENABLE_DEBUG_LAYER

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    CheckHRESULT(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = 0; // Use window size
    swapChainDesc.Height = 0; // Use window size
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;   // disable MSAA
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> tempSwapChain;
    CheckHRESULT(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        WindowApplication::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &tempSwapChain));

    CheckHRESULT(tempSwapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    CheckHRESULT(m_d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvDescHeap)));
    m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // One for apps, one for Imgui
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = DescriptorSrvCbvCount;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    CheckHRESULT(m_d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvcbvDescHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
    samplerHeapDesc.NumDescriptors = 1;
    samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    CheckHRESULT(m_d3dDevice->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_samplerDescHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    CheckHRESULT(m_d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvDescHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC imguiHeapDesc = {};
    imguiHeapDesc.NumDescriptors = 255;
    imguiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    imguiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    CheckHRESULT(m_d3dDevice->CreateDescriptorHeap(&imguiHeapDesc, IID_PPV_ARGS(&m_imguiDescHeap)));
    g_descHeapAllocator.Create(m_d3dDevice, m_imguiDescHeap);

    // Create frame resources
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < FrameCount; ++i)
    {
        ComPtr<ID3D12Resource> renderTarget;
        CheckHRESULT(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTarget[i])));
        m_d3dDevice->CreateRenderTargetView(m_renderTarget[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);
    }

    // Command allocator
    CheckHRESULT(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

void RenderApplication::LoadAsset(SDL_Window* window)
{
    // Setup dear imgui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup dear imgui
    ImGui::StyleColorsDark();

    // Initialize DXC
    ComPtr<IDxcUtils> dxcUtils;
    ComPtr<IDxcCompiler3> dxcCompiler;
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));

    // Root signature
    // Define descriptor table for SRV, as well as sampler
    // INPUT_ASSEMBLER_INPUT_LAYOUT - using input layout for vertex data (vs with input assembler)
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    // Root signature (base pass)
    {
        // Root parameter in root signature
        // Tell GPU how access SRV via descriptor table
        // 1 - number of descriptor range in table
        // VISIBILITY_PIXEL - descriptor table only visible to pixel shader stage
        CD3DX12_DESCRIPTOR_RANGE1 ranges[5];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, DescriptorCBVCount, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Scene (cb0) + Light (cb1)
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, DescriptorSBCount, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);  // Model sb start at t0
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, DescriptorTexCount, DescriptorSBCount, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Model tex start at t2
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0); // Sampler        
        
        // Shared destriptor table between srv + cbv, and visibility all
        CD3DX12_ROOT_PARAMETER1 rootParameters[5];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);    // cbv for vs/ps
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);    // srv : Structured buffer, Bindless
        rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_ALL);    // sampler
        rootParameters[3].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_ALL);    // sampler
        rootParameters[4].InitAsConstants(2, 2, 0, D3D12_SHADER_VISIBILITY_ALL);    // 2x 32 bit value, base register: b2

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        //rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
        if (FAILED(hr))
        {
            std::string errorMsg = std::string(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize());
            assert(false && errorMsg.c_str());
        }
        //CheckHRESULT(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
        CheckHRESULT(m_d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }
    
#ifdef DX12_ENABLE_DEBUG_LAYER
    //UINT compileFlags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FrontCounterClockwise = TRUE; // Matches glTF CCW convention
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Create pipeline state, compile, load shaders (base shader)
    {
        ComPtr<IDxcBlob> vertexShader;
        ComPtr<IDxcBlob> pixelShader;
        
        // Load shader source
        ComPtr<IDxcBlobEncoding> sourceBlob;
        dxcUtils->LoadFile(GetAssetFullPath("shaders/basic_color.hlsl").c_str(), nullptr, &sourceBlob);

        // Compile
        DxcBuffer sourceBuffer = { sourceBlob->GetBufferPointer(), sourceBlob->GetBufferSize(), 0 };

        // Vertex shader
        {
            // Compile arguments
            LPCWSTR arguments[] =
            {
                L"-E", L"VSMain",
                L"-T", L"vs_6_0",
                L"-Zi"
            };

            ComPtr<IDxcResult> compileResult;
            dxcCompiler->Compile(&sourceBuffer, arguments, _countof(arguments), nullptr, IID_PPV_ARGS(&compileResult));            
            HRESULT status;
            compileResult->GetStatus(&status);
            if (FAILED(status))
            {
                ComPtr<IDxcBlobEncoding> errorBlob;
                compileResult->GetErrorBuffer(&errorBlob);
                std::string errorMsg = std::string(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
                assert(false && errorMsg.c_str());
            }
            else
            {
                compileResult->GetResult(&vertexShader);
            }
        }
        // Pixel shader
        {
            // Compile arguments
            LPCWSTR arguments[] =
            {
                L"-E", L"PSMain",
                L"-T", L"ps_6_0",
                L"-Zi"
            };
            ComPtr<IDxcResult> compileResult;
            dxcCompiler->Compile(&sourceBuffer, arguments, _countof(arguments), nullptr, IID_PPV_ARGS(&compileResult));
            HRESULT status;
            compileResult->GetStatus(&status);
            if (FAILED(status))
            {
                ComPtr<IDxcBlobEncoding> errorBlob;
                compileResult->GetErrorBuffer(&errorBlob);
                std::string errorMsg = std::string(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
                assert(false && errorMsg.c_str());
            }
            else
            {
                compileResult->GetResult(&pixelShader);
            }
        }

        D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
        };

        D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
        depthStencilDesc.DepthEnable = TRUE;
        depthStencilDesc.StencilEnable = FALSE;
        //depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
        psoDesc.RasterizerState = rasterizerDesc; // CD3DX12_RASTERIZER_DESC{ D3D12_DEFAULT };
        psoDesc.BlendState = CD3DX12_BLEND_DESC{ D3D12_DEFAULT };
        psoDesc.DepthStencilState = depthStencilDesc;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        CheckHRESULT(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }
   
    // Root signature and Pipeline state (depth only pass)
    {
        //CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
        //ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Scene (cb0)
        //ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE); // Material (cb1)

        //CD3DX12_ROOT_PARAMETER1 rootParameters[2];
        //rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);    // cbv for vs/ps
        //rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);    // cbv material

        //CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        //rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        //ComPtr<ID3DBlob> signature;
        //ComPtr<ID3DBlob> error;
        //CheckHRESULT(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
        //CheckHRESULT(m_d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_depthOnlyRootSignature)));

        //ComPtr<ID3DBlob> vertexShader;
        //CheckHRESULT(D3DCompileFromFile(GetAssetFullPath("shaders/basic.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &error));

        //D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
        //{
        //    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        //};

        //D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        //psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };
        //psoDesc.pRootSignature = m_depthOnlyRootSignature.Get();
        //psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        //psoDesc.RasterizerState = rasterizerDesc;
        //psoDesc.BlendState = CD3DX12_BLEND_DESC{ D3D12_DEFAULT };
        //psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        //psoDesc.SampleMask = UINT_MAX;
        //psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        //psoDesc.NumRenderTargets = 0;
        //psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        //psoDesc.SampleDesc.Count = 1;
        //CheckHRESULT(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_depthOnlyPipelineState)));
    }
    
    // Root signature & Pipeline state (for HiZ)
    {
        //CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
        //ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);  // Input depth (t0)
        //ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);  // Output depth (u0)

        //CD3DX12_ROOT_PARAMETER1 rootParameters[3];
        //rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
        //rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);
        //rootParameters[2].InitAsConstants(5, 0);  // 4 32 bit HiZ Constant Buffer

        //CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        //rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

        //ComPtr<ID3DBlob> signature;
        //ComPtr<ID3DBlob> error;
        //CheckHRESULT(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error));
        //CheckHRESULT(m_d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_hiZRootSignature)));

        //ComPtr<ID3DBlob> computeShader;
        //CheckHRESULT(D3DCompileFromFile(GetAssetFullPath("shaders/hiz.hlsl").c_str(), nullptr, nullptr, "CSMain", "cs_5_0", compileFlags, 0, &computeShader, nullptr));

        //D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        //psoDesc.pRootSignature = m_hiZRootSignature.Get();
        //psoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());
        //CheckHRESULT(m_d3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_hiZPipelineState)));
    }

    // Create command list
    CheckHRESULT(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

    // Note: ComPtr's are CPU objects but this resource needs to stay in scope until
    // the command list that references it has finished executing on the GPU.
    // We will flush the GPU at the end of this method to ensure the resource is not
    // prematurely destroyed.
    ComPtr<ID3D12Resource> textureUploadHeap;

    // Create depth
    {
        D3D12_RESOURCE_DESC depthDesc = {};
        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width = m_width;
        depthDesc.Height = m_height;
        depthDesc.DepthOrArraySize = 1;
        depthDesc.MipLevels = 1;
        depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE depthClear = {};
        depthClear.Format = DXGI_FORMAT_D32_FLOAT;
        depthClear.DepthStencil.Depth = 1.f;
        depthClear.DepthStencil.Stencil = 0;

        CheckHRESULT(m_d3dDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthClear,
            IID_PPV_ARGS(&m_depth)));

        // Depth View
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        m_dsvCpuHandle = m_dsvDescHeap->GetCPUDescriptorHandleForHeapStart();
        m_d3dDevice->CreateDepthStencilView(m_depth.Get(), &dsvDesc, m_dsvCpuHandle);
    }

    // Create hiz pass
    {
        //D3D12_RESOURCE_DESC hizDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        //    DXGI_FORMAT_R32_FLOAT,
        //    m_width,
        //    m_height,
        //    1, 0, 1, 0,
        //    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        ////hizDesc.MipLevels = static_cast<UINT>(floor(log2(max(m_width, m_height))) + 1);
        //hizDesc.MipLevels = static_cast<UINT>(floor(log2(max(m_width, m_height))));

        //CheckHRESULT(m_d3dDevice->CreateCommittedResource(
        //    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        //    D3D12_HEAP_FLAG_NONE,
        //    &hizDesc,
        //    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        //    nullptr,
        //    IID_PPV_ARGS(&m_hiZBuffer)));

        //// Views
        //// SRV for depth
        //D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc = {};
        //depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        //depthSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        //depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        //depthSrvDesc.Texture2D.MipLevels = 1;
        //g_descHeapAllocator.Alloc(&m_hiZDepthSrvCpuHandle, &m_hiZDepthSrvGpuHandle);
        //m_d3dDevice->CreateShaderResourceView(m_depth.Get(), &depthSrvDesc, m_hiZDepthSrvCpuHandle);

        //// SRV
        //D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        //srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        //srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        //srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        //srvDesc.Texture2D.MipLevels = hizDesc.MipLevels;
        //g_descHeapAllocator.Alloc(&m_hiZCpuHandle, &m_hiZGpuHandle);
        //m_d3dDevice->CreateShaderResourceView(m_hiZBuffer.Get(), &srvDesc, m_hiZCpuHandle);
        //m_hiZDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        //// UAV
        //// UAV CpuHandle is start from SRV + DescriptorSize * (MipIndex + 1)
        //D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        //uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
        //uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        //for (UINT i = 0; i < hizDesc.MipLevels; ++i)
        //{
        //    uavDesc.Texture2D.MipSlice = i;
        //    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        //    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
        //    g_descHeapAllocator.Alloc(&cpuHandle, &gpuHandle);
        //    m_d3dDevice->CreateUnorderedAccessView(m_hiZBuffer.Get(), nullptr, &uavDesc, cpuHandle);
        //}
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvcbvCpuHandle(m_srvcbvDescHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvcbvGpuHandle(m_srvcbvDescHeap->GetGPUDescriptorHandleForHeapStart());

    // Create constant buffer for scene
    {
        const UINT constantBufferSize = sizeof(SceneConstantBuffer);

        CheckHRESULT(m_d3dDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_constantBuffer)));

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = constantBufferSize;
        m_d3dDevice->CreateConstantBufferView(&cbvDesc, srvcbvCpuHandle);
        srvcbvCpuHandle.Offset(1, m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
        srvcbvGpuHandle.Offset(1, m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

        // Map and initialize constant buffer
        CD3DX12_RANGE readRange(0, 0);
        CheckHRESULT(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvDataBegin)));
        memcpy(m_cbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));        
    }

    // Constant buffer for light
    {
        const UINT lightBufferSize = (sizeof(LightData) + 255) & ~255;  // Align to 256 byte
        CheckHRESULT(m_d3dDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(lightBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_lightCB)));

        // buffer view
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_lightCB->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = lightBufferSize;

        m_d3dDevice->CreateConstantBufferView(&cbvDesc, srvcbvCpuHandle);
        srvcbvCpuHandle.Offset(1, m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
        srvcbvGpuHandle.Offset(1, m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

        // Map and initialize constant buffer
        CheckHRESULT(m_lightCB->Map(0, nullptr, reinterpret_cast<void**>(&m_lightDataBegin)));
        memcpy(m_lightDataBegin, &m_directionalLight, sizeof(LightData));
        //m_lightCB->Unmap(0, nullptr);
    }

    // Model
    //std::wstring gltfPath = GetAssetFullPath("content/Box With Spaces.gltf");
    //std::wstring gltfPath = GetAssetFullPath("content/BoxVertexColors.gltf");
    //std::wstring gltfPath = GetAssetFullPath("content/Cube.gltf");
    //std::wstring gltfPath = GetAssetFullPath("content/Duck.gltf");
    //std::wstring gltfPath = GetAssetFullPath("content/2CylinderEngine.gltf");
    //std::wstring gltfPath = GetAssetFullPath("content/GearboxAssy.gltf");
    //std::wstring gltfPath = GetAssetFullPath("content/CesiumMilkTruck.gltf");
    //std::wstring gltfPath = GetAssetFullPath("content/Sponza/Sponza.gltf");
    std::wstring gltfPath = GetAssetFullPath(m_modelPath);
        
    m_model.LoadFromFile(WStringToString(gltfPath));
    m_model.UploadGpuResources(
        m_d3dDevice.Get(),
        DescriptorModelSBBase,
        DescriptorModelTexBase,
        m_srvcbvDescHeap.Get(),        
        m_samplerDescHeap.Get(), 
        m_commandList.Get());

    // ImGui
    // ImGui Renderer backend
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = m_d3dDevice.Get();
    init_info.CommandQueue = m_commandQueue.Get();
    init_info.NumFramesInFlight = FrameCount;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
    // (current version of the backend will only allocate one descriptor, future versions will need to allocate more)
    init_info.SrvDescriptorHeap = m_imguiDescHeap.Get();
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return g_descHeapAllocator.Alloc(out_cpu_handle, out_gpu_handle); };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return g_descHeapAllocator.Free(cpu_handle, gpu_handle); };
    ImGui_ImplDX12_Init(&init_info);
    
    // Clsoe command list and execute to begin initial GPU setup
    CheckHRESULT(m_commandList->Close());
    ID3D12CommandList* ppCommandList[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandList), ppCommandList);
    
    // Create synchronization object
    CheckHRESULT(m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValue = 1;

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
    {
        CheckHRESULT(HRESULT_FROM_WIN32(GetLastError()));
    }

    // Wait for the command list to execute; we are reusing the same command 
    // list in our main loop but for now, we just want to wait for setup to 
    // complete before continuing.
    MoveToNextFrame();
}

void RenderApplication::OnUpdate()
{
    m_timer.Tick();

    m_camera.SetMoveSpeed(m_moveSpeed);
    m_camera.Update(static_cast<float>(m_timer.GetElapsedSeconds()));
    //m_frustum = m_camera.GetFrustum(m_aspectRatio, XM_PI / 3, 1.f, 1000.f);

    XMMATRIX world = XMMATRIX(g_XMIdentityR0, g_XMIdentityR1, g_XMIdentityR2, g_XMIdentityR3);
    XMMATRIX view = m_camera.GetViewMatrix();
    XMMATRIX proj = m_camera.GetProjectionMatrix(XM_PI / 3, m_aspectRatio);

    // DirectXMath library is row major matrices
    // Hlsl by default is column major matrices
    // Either use row_major explicitly on hlsl, or compile shader with ROW_MAJOR, or use XMMatrixTranspose
    XMStoreFloat4x4(&m_constantBufferData.World, XMMatrixTranspose(world));
    XMStoreFloat4x4(&m_constantBufferData.WorldView, XMMatrixTranspose(world * view));
    XMStoreFloat4x4(&m_constantBufferData.WorldViewProj, XMMatrixTranspose(world * view * proj));
    m_constantBufferData.InvTextureSize = XMFLOAT2(1.f / static_cast<float>(m_width), 1.f / static_cast<float>(m_height));
    m_constantBufferData.HiZDimension = XMFLOAT2(static_cast<float>(m_width), static_cast<float>(m_height));

    // Constant buffer created with upload heap
    // Need to map buffer once during creation, udpate directly in mapped memory
    // No need to unmap unless we're done with buffer
    memcpy(m_cbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));

    m_directionalLight.direction = PolarToCartesian(m_azimuth, m_elevation);

    memcpy(m_lightDataBegin, &m_directionalLight, sizeof(LightData));
}

void RenderApplication::OnRender()
{
    // Record all command
    PopulateCommandList();

    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    CheckHRESULT(m_swapChain->Present(0, 0));

    MoveToNextFrame();
}

void RenderApplication::OnDestroy()
{
    // Ensure GPU no longer reference resource before cleanup
    MoveToNextFrame();

    CloseHandle(m_fenceEvent);

    // ImGui
    g_descHeapAllocator.Destroy();
    ImGui_ImplDX12_Shutdown();
}

void RenderApplication::OnKeyDown(SDL_KeyboardEvent& key)
{
    m_camera.OnKeyDown(key);
}

void RenderApplication::OnKeyUp(SDL_KeyboardEvent& key)
{
    m_camera.OnKeyUp(key);
}

void RenderApplication::PopulateCommandList()
{
    // Start dear imgui frame
    ImGui_ImplDX12_NewFrame();
    ImGui::NewFrame();

    static bool show_another_window = false;
    static bool show_demo_window = true;
    static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    //ImGui::ShowDemoWindow(&show_demo_window);

    // Simple window
    {
        static float f = 0.f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");

        ImGui::Text("This is some useful text.");
        ImGui::Checkbox("Another window", &show_another_window);

        ImGui::Text("Camera");
        ImGui::SliderFloat("MoveSpeed", &m_moveSpeed, 1.f, 1000.f);

        ImGui::ColorEdit3("clear color", (float*)&clear_color);

        ImGui::Text("Directional Light");
        ImGui::ColorEdit3("Light Color", (float*)&m_directionalLight.color);
        ImGui::SliderFloat("Light Azimuth", &m_azimuth, 0.f, XM_2PI, "%.2f");
        ImGui::SliderFloat("Light Elevation", &m_elevation, -XM_PIDIV2, XM_PIDIV2, "%.2f");

        /*if (ImGui::Button("Button"))
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);*/

        ImGuiIO& io = ImGui::GetIO(); (void)io;
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

    CheckHRESULT(m_commandAllocator->Reset());
    CheckHRESULT(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    BoundingFrustum frustum = m_camera.GetFrustum(XM_PI / 3, m_aspectRatio);

    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Depth only pass
    {
        //m_commandList->SetPipelineState(m_depthOnlyPipelineState.Get());
        //m_commandList->SetGraphicsRootSignature(m_depthOnlyRootSignature.Get());

        //m_commandList->OMSetRenderTargets(0, nullptr, FALSE, &m_dsvCpuHandle);
        //m_commandList->ClearDepthStencilView(m_dsvCpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        //// Bind CBV descriptor heap for constant buffer view
        //ID3D12DescriptorHeap* ppDescHeaps[] = { m_srvDescHeap.Get() };
        //m_commandList->SetDescriptorHeaps(_countof(ppDescHeaps), ppDescHeaps);
        //m_commandList->SetGraphicsRootDescriptorTable(0, m_cbvGpuDescHandle);

        //m_model.RenderDepthOnly(m_d3dDevice.Get(), m_commandList.Get(), frustum);
    }

    // HiZ pass
    {
        //D3D12_RESOURCE_BARRIER barriers[2];

        //barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        //    m_hiZBuffer.Get(),
        //    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        //    D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        //barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        //    m_depth.Get(),
        //    D3D12_RESOURCE_STATE_DEPTH_WRITE,
        //    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        //m_commandList->ResourceBarrier(2, barriers);

        //m_commandList->SetPipelineState(m_hiZPipelineState.Get());
        //m_commandList->SetComputeRootSignature(m_hiZRootSignature.Get());
        //// Bind CBV descriptor heap for constant buffer view
        //ID3D12DescriptorHeap* ppDescHeaps[] = { m_srvDescHeap.Get() };
        //m_commandList->SetDescriptorHeaps(_countof(ppDescHeaps), ppDescHeaps);

        //struct HiZConstantBuffer
        //{
        //    XMFLOAT2 InvTextureSize;
        //    XMFLOAT2 OutDimensions;
        //    UINT MipLevel;
        //    XMFLOAT3 Padding;
        //} hiZCB = { {1.f / static_cast<float>(m_width), 1.f / static_cast<float>(m_height)}, 
        //    {static_cast<float>(m_width), static_cast<float>(m_height)}, 0 };

        //// Copy depth to HiZ level 0
        //m_commandList->SetComputeRootDescriptorTable(0, m_hiZDepthSrvGpuHandle);    // srv
        //m_commandList->SetComputeRootDescriptorTable(1, CD3DX12_GPU_DESCRIPTOR_HANDLE(m_hiZGpuHandle).Offset(1, m_hiZDescriptorSize));    // uav
        //m_commandList->SetComputeRoot32BitConstants(2, 5, &hiZCB, 0);
        //m_commandList->Dispatch((m_width + 7) / 8, (m_height + 7) / 8, 1);

        //UINT width = m_width, height = m_height;
        //for (UINT mip = 1; mip < m_hiZBuffer->GetDesc().MipLevels; ++mip)
        //{
        //    width = max(width / 2, 1u);
        //    height = max(height / 2, 1u);
        //    hiZCB.MipLevel = mip;
        //    // In: mip - 1 * increment size
        //    // Out: mip * increment size
        //    m_commandList->SetComputeRootDescriptorTable(0, CD3DX12_GPU_DESCRIPTOR_HANDLE(m_hiZGpuHandle).Offset(mip, m_hiZDescriptorSize));  // srv
        //    m_commandList->SetComputeRootDescriptorTable(1, CD3DX12_GPU_DESCRIPTOR_HANDLE(m_hiZGpuHandle).Offset(mip + 1, m_hiZDescriptorSize));  // uav
        //    m_commandList->SetComputeRoot32BitConstants(2, 5, &hiZCB, 0);
        //    m_commandList->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
        //    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_hiZBuffer.Get()));
        //}

        //barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        //    m_hiZBuffer.Get(),
        //    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        //    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        //barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        //    m_depth.Get(),
        //    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        //    D3D12_RESOURCE_STATE_DEPTH_READ);

        //m_commandList->ResourceBarrier(2, barriers);
    }
    // Main pass
    {
        m_commandList->SetPipelineState(m_pipelineState.Get());
        m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

        // Bind srv descriptor heaps contain texture srv
        ID3D12DescriptorHeap* ppDescHeaps[] = { m_srvcbvDescHeap.Get(), m_samplerDescHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(ppDescHeaps), ppDescHeaps);

        // For now :
        // App + IMGUI share the same descriptor heap
        // App reserve on static slot of heap, while UI build slot of heap dynamically
        // is fine because UI using different root descriptor table than App descriptor table
        // They only share descriptor heap

        // Shared descriptor table, pass the first index
        //m_commandList->SetGraphicsRootDescriptorTable(0, m_srvDescHeap->GetGPUDescriptorHandleForHeapStart());
        // Remove the shared, since root descriptor table using visiblity = ALL, so Texture SRV need extra flag
        //m_commandList->SetGraphicsRootDescriptorTable(0, m_srvTextureGpuHandle);
        m_commandList->SetGraphicsRootDescriptorTable(0, m_srvcbvDescHeap->GetGPUDescriptorHandleForHeapStart());
        //m_commandList->SetGraphicsRootDescriptorTable(2, m_srvDescHeap->GetGPUDescriptorHandleForHeapStart());

        // Transition back buffer to RENDER_TARGET
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTarget[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &barrier);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &m_dsvCpuHandle);
        m_commandList->ClearDepthStencilView(m_dsvCpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        m_commandList->ClearRenderTargetView(rtvHandle, clear_color_with_alpha, 0, nullptr);

        m_model.RenderBasePass(
            m_d3dDevice.Get(), 
            m_commandList.Get(), 
            DescriptorModelSBBase,
            DescriptorModelTexBase,
            m_srvcbvDescHeap.Get(),
            frustum);
    }

    // ImGui
    {
        ID3D12DescriptorHeap* ppDescHeaps[] = { m_imguiDescHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(ppDescHeaps), ppDescHeaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
    }

    // Transition back to PRESENT
    /*D3D12_RESOURCE_BARRIER barriers[2];
    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_depth.Get(),
        D3D12_RESOURCE_STATE_DEPTH_READ,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);

    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTarget[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    m_commandList->ResourceBarrier(2, barriers);*/
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTarget[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    m_commandList->ResourceBarrier(1, &barrier);
    
    CheckHRESULT(m_commandList->Close());
}

// Wait for pending gpu work to complete
void RenderApplication::WaitForGpu()
{
    // Schedule signal command in queue
    CheckHRESULT(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));

    // Wait until fence processed
    CheckHRESULT(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);

    // Increment fence value for current frame
    m_fenceValue++;
}

// Prepare to render next frame
void RenderApplication::MoveToNextFrame()
{
    // Schedule signal command in queue (Increment fence value for next frame)
    const UINT64 fence = m_fenceValue;
    CheckHRESULT(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // If next frame is not ready to be renderer, wait until ready
    if (m_fence->GetCompletedValue() < fence)
    {
        CheckHRESULT(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Update frame index
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}