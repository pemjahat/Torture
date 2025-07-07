#pragma once

#include "PCH.h"
#include "Helper.h"

struct DescriptorAlloc
{
	D3D12_CPU_DESCRIPTOR_HANDLE startCpuHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE startGpuHandle = {};
	uint32_t descriptorIndex = uint32_t(-1);
};

struct DescriptorHeap
{
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
	uint32_t numDescriptor = 0;
	uint32_t numAllocated = 0;
	uint32_t heapIncrement = 0;
	std::vector<uint32_t> FreeIndices;
	D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = {};
	D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = {};

	void Initialize(uint32_t size, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible);
	void Shutdown();

	DescriptorAlloc Allocate();
	void Free(uint32_t& index);

	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandleFromIndex(uint32_t descriptorIdx) const;
};

struct Buffer
{
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> upload = nullptr;
	uint64_t gpuAddress = 0;
	UINT8* cpuAddress = 0;	
	
	void Initialize(
		uint64_t size, 
		uint64_t alignment, 
		bool cpuAccessible, 
		const void* initData, 
		D3D12_RESOURCE_STATES initialState, 
		const wchar_t* name);

	MapResult Map();
};

struct ConstantBufferInit
{
	uint64_t size = 0;
	bool cpuAccessible = true;
	const void* initData = nullptr;
	const wchar_t* name = nullptr;
};

struct ConstantBuffer
{
	Buffer internalBuffer;	

	void Initialize(const ConstantBufferInit& init);

	void SetAsGfxRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const;
	void SetAsComputeRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const;

	void MapAndSetData(const void* data, uint64_t dataSize);
};

struct StructuredBufferInit
{
	uint64_t stride = 0;
	uint64_t numElements = 0;
	const void* initData = nullptr;
	D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	const wchar_t* name = nullptr;
};
struct StructuredBuffer
{
	Buffer internalBuffer;
	uint64_t stride;
	uint64_t numElements;

	void Initialize(const StructuredBufferInit& init);

	void SetAsGfxRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const;
	void SetAsComputeRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const;

	D3D12_VERTEX_BUFFER_VIEW VBView() const;
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc() const;
};

struct FormattedBufferInit
{
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	uint32_t bitSize = 0;
	uint64_t numElements = 0;
	const void* initData = nullptr;
	D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_GENERIC_READ;
	const wchar_t* name = nullptr;
};
struct FormattedBuffer
{
	Buffer internalBuffer;
	uint64_t stride = 0;
	uint64_t numElements = 0;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

	void Initialize(const FormattedBufferInit& init);

	D3D12_INDEX_BUFFER_VIEW IBView() const;
};