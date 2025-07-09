#pragma once

#include "PCH.h"

#include "StepTimer.h"
#include "Model.h"
#include "SimpleCamera.h"
#include "DX12.h"
#include "GraphicsTypes.h"

using Microsoft::WRL::ComPtr;

struct CommandLineArgs
{
    int resX = 800;
    int resY = 600;
    std::string modelPath = "content/Sponza/Sponza.gltf";
    std::string exePath = "";
};

class RenderApplication
{
public:
    RenderApplication(int argc, char** argv);

    void OnInit(SDL_Window* window);
    void OnUpdate();
    void OnRender();
    void OnDestroy();

    void OnKeyUp(SDL_KeyboardEvent& key);
    void OnKeyDown(SDL_KeyboardEvent& key);

    UINT GetWidth() { return m_width; }
    UINT GetHeight() { return m_height; }

private:
    static const UINT FrameCount = 2;
    static const UINT SrvCbvHeapSize = 1000;
    
    // Total shared descriptor for main pass
    //static const UINT DescriptorCBCount = 2;
    static const UINT DescriptorSBCount = 2;
    static const UINT DescriptorTexCount = 500;

    //static const UINT DescriptorCBVCount = DescriptorCBCount;
    static const UINT DescriptorSRVCount = DescriptorSBCount + DescriptorTexCount;
    //static const UINT DescriptorSrvCbvCount = DescriptorSRVCount + DescriptorCBVCount;

    // Offset allocated for model resource
    //static const UINT DescriptorModelSBBase = DescriptorCBCount;   // reserve 5 from CB
    static const UINT DescriptorModelSBBase = 0;   // reserve 5 from CB
    static const UINT DescriptorModelTexBase = DescriptorModelSBBase + DescriptorSBCount;

    struct SceneConstantBuffer
    {
        XMFLOAT4X4 World;
        XMFLOAT4X4 WorldView;
        XMFLOAT4X4 WorldViewProj;
        XMFLOAT2 InvTextureSize;
        XMFLOAT2 HiZDimension;
        //float padding[12];  // padd to 256byte aligned
    };

    struct LightData
    {
        XMFLOAT3 direction;
        float intensity;
        XMFLOAT3 color;
        float padding;
    };

    // Pipeline object
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;    
    ComPtr<ID3D12Resource> m_renderTarget[FrameCount];
    //ComPtr<ID3D12RootSignature> m_rootSignature;
    //ComPtr<ID3D12PipelineState> m_pipelineState;

    ComPtr<ID3D12DescriptorHeap> m_rtvDescHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvcbvDescHeap;    
    ComPtr<ID3D12DescriptorHeap> m_dsvDescHeap;
    ComPtr<ID3D12DescriptorHeap> m_imguiDescHeap;

    D3D12_CPU_DESCRIPTOR_HANDLE m_dsvCpuHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cbvCpuDescHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_cbvGpuDescHandle;

    D3D12_CPU_DESCRIPTOR_HANDLE m_lightCpuDescHandle;

    
    UINT m_rtvDescriptorSize;

    // Render app resources
    ComPtr<ID3D12Resource> m_depth;
    //ComPtr<ID3D12Resource> m_constantBuffer;
    //ComPtr<ID3D12Resource> m_lightCB;
    ConstantBuffer m_sceneCB;
    ConstantBuffer m_lightCB;
    SceneConstantBuffer m_constantBufferData;    
    UINT8* m_cbvDataBegin;
    UINT8* m_lightDataBegin;

    // Depth Passes resource
    //ComPtr<ID3D12PipelineState> m_depthOnlyPipelineState;

    // HiZ Passes resource
    ComPtr<ID3D12Resource> m_hiZBuffer;
    ComPtr<ID3D12RootSignature> m_hiZRootSignature;
    ComPtr<ID3D12PipelineState> m_hiZPipelineState;
    D3D12_CPU_DESCRIPTOR_HANDLE m_hiZCpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_hiZGpuHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_hiZDepthSrvCpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_hiZDepthSrvGpuHandle;
    UINT m_hiZDescriptorSize;

    // Synchronization
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;

    // Window
    UINT m_width;
    UINT m_height;
    float m_aspectRatio;

    // Camera
    StepTimer m_timer;
    SimpleCamera m_camera;    
    float m_moveSpeed;

    // Light
    float m_azimuth;
    float m_elevation;
    LightData m_directionalLight;

    // GLTF
    Model m_model;

    // Root assets path. (helper)
    std::string m_executablePath;
    std::string m_modelPath;

    std::wstring GetAssetFullPath(const std::string& relativePath);

    void LoadPipeline();
    void LoadAsset(SDL_Window* window);
    void PopulateCommandList();
    void MoveToNextFrame();
    void WaitForGpu();
};