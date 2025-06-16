#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h> // For ComPtr
#include <string>
#include <SDL.h>
#include <vector>
#include "StepTimer.h"
#include "Model.h"
#include "SimpleCamera.h"

using Microsoft::WRL::ComPtr;


class RenderApplication
{
public:
    RenderApplication(UINT width, UINT height, const char* argv);

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
    static const UINT SrvHeapSize = 256;

    struct SceneConstantBuffer
    {
        XMFLOAT4X4 World;
        XMFLOAT4X4 WorldView;
        XMFLOAT4X4 WorldViewProj;
        float padding[16];  // padd to 256byte aligned
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
    ComPtr<ID3D12Device> m_d3dDevice;
    ComPtr<ID3D12Resource> m_renderTarget[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    ComPtr<ID3D12DescriptorHeap> m_rtvDescHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvDescHeap;
    ComPtr<ID3D12DescriptorHeap> m_samplerDescHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvDescHeap;

    D3D12_CPU_DESCRIPTOR_HANDLE m_dsvCpuHandle;

    D3D12_CPU_DESCRIPTOR_HANDLE m_cbvCpuDescHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_cbvGpuDescHandle;

    D3D12_CPU_DESCRIPTOR_HANDLE m_lightCpuDescHandle;

    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize;

    // Render app resources
    ComPtr<ID3D12Resource> m_depth;
    ComPtr<ID3D12Resource> m_constantBuffer;
    ComPtr<ID3D12Resource> m_lightCB;
    SceneConstantBuffer m_constantBufferData;    
    UINT8* m_cbvDataBegin;
    UINT8* m_lightDataBegin;

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

    // Light
    float m_azimuth;
    float m_elevation;
    LightData m_directionalLight;

    // GLTF
    Model m_model;

    // Root assets path. (helper)
    std::string m_executablePath;

    std::wstring GetAssetFullPath(const std::string& relativePath);

    void LoadPipeline();
    void LoadAsset(SDL_Window* window);
    void PopulateCommandList();
    void MoveToNextFrame();
    void WaitForGpu();
};