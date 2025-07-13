#pragma once

#include "PCH.h"
#include "GraphicsTypes.h"
#include <cassert>

enum class RasterizerState
{
    NoCull = 0,
    BackfaceCull,
    MaxRasterizer
};

enum class DepthStencilState
{
    Disabled = 0,
    Enabled,
    WriteEnabled,
    MaxDepth
};

enum class SamplerState
{
    Linear = 0,
    LinearClamp,
    Point,
    MaxSampler
};

enum class ShaderType
{
    Vertex = 0,
    Pixel,
    Compute,
    MaxShader
};

// External
extern DescriptorHeap rtvDescriptorHeap;
extern DescriptorHeap dsvDescriptorHeap;
extern DescriptorHeap srvDescriptorHeap;
extern DescriptorHeap uavDescriptorHeap;

// Once
void InitializeHelper();
void ShutdownHelper();

const D3D12_DESCRIPTOR_RANGE1* SRVDescriptorRanges();

// States
D3D12_RASTERIZER_DESC GetRasterizerState(RasterizerState rasterState);
D3D12_DEPTH_STENCIL_DESC GetDepthStencilState(DepthStencilState depthState);
D3D12_SAMPLER_DESC GetSamplerState(SamplerState samplerState);

D3D12_STATIC_SAMPLER_DESC GetStaticSamplerState(SamplerState samplerState, uint32_t shaderRegister = 0, uint32_t registerSpace = 0);
D3D12_STATIC_SAMPLER_DESC ConvertToStaticSampler(const D3D12_SAMPLER_DESC samplerDesc, uint32_t shaderRegister, uint32_t registerSpace);

//
void SrvSetAsGfxRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter);
void SrvSetAsComputeRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter);

void CreateRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature, const D3D12_ROOT_SIGNATURE_DESC1& desc);
void CompileShaderFromFile(const std::wstring& filePath, const std::wstring& includePath, Microsoft::WRL::ComPtr<IDxcBlob>& shaderBlob, ShaderType type);

struct DescriptorHeapAllocator
{
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT    HeapHandleIncrement;
    std::vector<int> FreeIndices;

    void Create(
        Microsoft::WRL::ComPtr<ID3D12Device> device, 
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap)
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

//void LogError(const char* message, HRESULT hr = S_OK)
//{
//    std::string errorMsg = message;
//    if (hr != S_OK) {
//        char hrMsg[512];
//        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0, hrMsg, sizeof(hrMsg), nullptr);
//        errorMsg += " HRESULT: 0x" + std::to_string(hr) + " - " + hrMsg;
//    }
//    errorMsg += "\n";
//    std::cerr << errorMsg;
//}
