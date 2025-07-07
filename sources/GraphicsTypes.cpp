#include "GraphicsTypes.h"

#include "Utility.h"
#include "DX12.h"

//
// Buffer
//
void Buffer::Initialize(uint64_t size, uint64_t alignment, bool cpuAccessible, const wchar_t* name)
{
	assert(size > 0);

	size = AlignTo(size, alignment);

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = size;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Alignment = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	CheckHRESULT(d3dDevice->CreateCommittedResource(
		cpuAccessible ? &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD) : &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		cpuAccessible ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&resource)));

	resource->SetName(name);

	gpuAddress = resource->GetGPUVirtualAddress();

	if (cpuAccessible)
	{
		D3D12_RANGE readRange = {};
		resource->Map(0, &readRange, reinterpret_cast<void**>(&cpuAddress));
	}
}

MapResult Buffer::Map()
{
	MapResult mapResult = {};
	mapResult.cpuAddress = cpuAddress;
	mapResult.gpuAddress = gpuAddress;
	mapResult.resource = resource;
	return mapResult;
}

//
// Constant Buffer
//
void ConstantBuffer::Initialize(const ConstantBufferInit& init)
{
	internalBuffer.Initialize(init.size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, init.cpuAccessible, init.name);
}

void ConstantBuffer::SetAsGfxRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const
{
	cmdList->SetGraphicsRootConstantBufferView(rootParameter, internalBuffer.gpuAddress);
}

void ConstantBuffer::SetAsComputeRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const
{
	cmdList->SetComputeRootConstantBufferView(rootParameter, internalBuffer.gpuAddress);
}

void ConstantBuffer::MapAndSetData(const void* data, uint64_t dataSize)
{
	MapResult mapResult = internalBuffer.Map();
	memcpy(mapResult.cpuAddress, data, dataSize);
}

//
// Structured Buffer
//
void StructuredBuffer::Initialize(const StructuredBufferInit& init)
{
	assert(init.stride > 0);
	assert(init.numElements > 0);

	stride = init.stride;
	numElements = init.numElements;

	internalBuffer.Initialize(stride * numElements, stride, false, init.name);

	// create shader resource views
}

void StructuredBuffer::SetAsGfxRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const
{

}

void StructuredBuffer::SetAsComputeRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const
{

}

void StructuredBuffer::Upload()
{

}