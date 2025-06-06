#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h> // For ComPtr
#include <string>
#include <SDL.h>
#include <vector>

using Microsoft::WRL::ComPtr;

struct DescriptorHeapAllocator
{
    ComPtr<ID3D12DescriptorHeap> Heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT    HeapHandleIncrement;
    std::vector<int> FreeIndices;

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

class RenderApplication
{
public:
    RenderApplication(UINT width, UINT height);

    void OnInit(SDL_Window* window);
    void OnUpdate();
    void OnRender();
    void OnDestroy();

    UINT GetWidth() { return m_width; }
    UINT GetHeight() { return m_height; }

private:
    static const UINT FrameCount = 2;
    static const UINT SrvHeapSize = 256;
    static const UINT TextureWidth = 256;
    static const UINT TextureHeight = 256;
    static const UINT TexturePixelSize = 4; // Byte per pixel on texture

    struct Vertex
    {
        float position[3];
        float uv[2];
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
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvTextureCpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvTextureGpuHandle;

    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize;

    // Render app resources
    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_texture;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;    

    // Synchronization
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    //UINT64 m_fenceValue[FrameCount];
    UINT64 m_fenceValue;

    // Window
    UINT m_width;
    UINT m_height;

    void LoadPipeline();
    void LoadAsset(SDL_Window* window);
    std::vector<UINT8> GenerateTextureData();
    void PopulateCommandList();
    void MoveToNextFrame();
    void WaitForGpu();
};