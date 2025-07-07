#pragma once

#include "PCH.h"
#include "Helper.h"

struct Buffer
{
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	uint64_t gpuAddress = 0;
	UINT8* cpuAddress = 0;	
	void Initialize(uint64_t size, uint64_t alignment, bool cpuAccessible, const wchar_t* name);

	MapResult Map();
};

struct ConstantBufferInit
{
	uint64_t size = 0;
	bool cpuAccessible = true;
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

	void Upload();
};