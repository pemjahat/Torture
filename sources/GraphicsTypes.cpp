#include "GraphicsTypes.h"

#include "Utility.h"
#include "DX12.h"
#include "Helper.h"

//
// Descriptor  Heap
//
void DescriptorHeap::Initialize(uint32_t size, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
	assert(size > 0);
	numDescriptor = size;
	heapType = type;

	FreeIndices.reserve(numDescriptor);
	for (int n = 0; n < numDescriptor; n++)
		FreeIndices.push_back(n);

	if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
		shaderVisible = false;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptor;
	desc.Type = heapType;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	CheckHRESULT(d3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));

	cpuStart = heap->GetCPUDescriptorHandleForHeapStart();

	if (shaderVisible)
		gpuStart = heap->GetGPUDescriptorHandleForHeapStart();
	heapIncrement = d3dDevice->GetDescriptorHandleIncrementSize(heapType);
}

void DescriptorHeap::Shutdown()
{
	if (heap != nullptr)
	{
		heap = nullptr;
	}
}

DescriptorAlloc DescriptorHeap::Allocate()
{
	assert(numAllocated < numDescriptor);

	uint32_t idx = FreeIndices[numAllocated];
	++numAllocated;

	DescriptorAlloc alloc;
	alloc.descriptorIndex = idx;
	alloc.cpuHandle = cpuStart;
	alloc.cpuHandle.ptr += idx * heapIncrement;

	alloc.gpuHandle = gpuStart;
	alloc.gpuHandle.ptr += idx * heapIncrement;
	return alloc;
}

void DescriptorHeap::Free(uint32_t& index)
{
	if (index == uint32_t(-1))
		return;

	assert(index < numDescriptor);
	assert(numAllocated > 0);

	FreeIndices[numAllocated - 1] = index;
	--numAllocated;

	index = uint32_t(-1);
}

void DescriptorHeap::Free(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	uint32_t idx = IndexFromHandle(handle);
	Free(idx);
}

void DescriptorHeap::Free(D3D12_GPU_DESCRIPTOR_HANDLE handle)
{
	assert(shaderVisible);
	uint32_t idx = IndexFromHandle(handle);
	Free(idx);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::CPUHandleFromIndex(uint32_t descriptorIdx) const
{
	assert(descriptorIdx < numDescriptor);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = cpuStart;
	handle.ptr += descriptorIdx * heapIncrement;
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GPUHandleFromIndex(uint32_t descriptorIdx) const
{
	assert(descriptorIdx < numDescriptor);
	assert(shaderVisible);
	D3D12_GPU_DESCRIPTOR_HANDLE handle = gpuStart;
	handle.ptr += descriptorIdx * heapIncrement;
	return handle;
}

uint32_t DescriptorHeap::IndexFromHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	return uint32_t(handle.ptr - cpuStart.ptr) / heapIncrement;
}

uint32_t DescriptorHeap::IndexFromHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle)
{
	assert(shaderVisible);
	return uint32_t(handle.ptr - gpuStart.ptr) / heapIncrement;
}
//
// Buffer
//
void Buffer::Initialize(
	uint64_t size, 
	uint64_t alignment, 
	bool cpuAccessible,
	bool allowUAV,
	const void* initData, 
	D3D12_RESOURCE_STATES initState,
	const wchar_t* name)
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
	resourceDesc.Flags = allowUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

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
	if (initData)
	{
		CheckHRESULT(d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&upload)));

		void* cpuMapped;
		upload->Map(0, nullptr, &cpuMapped);
		memcpy(cpuMapped, initData, size);
		upload->Unmap(0, nullptr);

		commandList->CopyBufferRegion(resource.Get(), 0, upload.Get(), 0, size);

		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			resource.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			initState);
		commandList->ResourceBarrier(1, &barrier);
	}
}

void Buffer::Shutdown()
{
	if (resource)
	{
		resource = nullptr;
	}
	if (upload)
	{
		upload = nullptr;
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

void Buffer::UAVBarrier(ID3D12GraphicsCommandList* cmdList) const
{
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.UAV.pResource = resource.Get();
	cmdList->ResourceBarrier(1, &barrier);
}
//
// Constant Buffer
//
void ConstantBuffer::Initialize(const ConstantBufferInit& init)
{
	internalBuffer.Initialize(init.size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, init.cpuAccessible, false, nullptr, D3D12_RESOURCE_STATE_GENERIC_READ, init.name);
}

void ConstantBuffer::Shutdown()
{
	internalBuffer.Shutdown();
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
	NumElements = init.numElements;

	internalBuffer.Initialize(stride * NumElements, stride, false, false, init.initData, init.initState, init.name);

	DescriptorAlloc srvAlloc = srvDescriptorHeap.Allocate();
	SRV = srvAlloc.descriptorIndex;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc.Buffer.NumElements = NumElements;
	srvDesc.Buffer.StructureByteStride = stride;
	d3dDevice->CreateShaderResourceView(internalBuffer.resource.Get(), &srvDesc, srvAlloc.cpuHandle);
}

void StructuredBuffer::Shutdown()
{
	srvDescriptorHeap.Free(SRV);
	internalBuffer.Shutdown();
}

void StructuredBuffer::SetAsGfxRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const
{
	assert(SRV != uint32_t(-1));
	D3D12_GPU_DESCRIPTOR_HANDLE handle = srvDescriptorHeap.GPUHandleFromIndex(SRV);
	cmdList->SetGraphicsRootDescriptorTable(rootParameter, handle);
}

void StructuredBuffer::SetAsComputeRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter) const
{
	assert(SRV != uint32_t(-1));
	D3D12_GPU_DESCRIPTOR_HANDLE handle = srvDescriptorHeap.GPUHandleFromIndex(SRV);
	cmdList->SetComputeRootDescriptorTable(rootParameter, handle);
}

D3D12_VERTEX_BUFFER_VIEW StructuredBuffer::VBView() const
{
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = internalBuffer.gpuAddress;
	vbView.StrideInBytes = stride;
	vbView.SizeInBytes = stride * NumElements;
	return vbView;
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE StructuredBuffer::ShaderTable(uint64_t startElement, uint64_t numElements) const
{
	numElements = std::min(numElements, NumElements - startElement);

	D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE result = {};
	result.StartAddress = internalBuffer.gpuAddress + stride * startElement;
	result.SizeInBytes = numElements * stride;
	result.StrideInBytes = stride;

	return result;
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE StructuredBuffer::ShaderRecord(uint64_t element) const
{
	D3D12_GPU_VIRTUAL_ADDRESS_RANGE result = {};
	result.StartAddress = internalBuffer.gpuAddress + stride * element;
	result.SizeInBytes = stride;

	return result;
}
//
// Formatted Buffer
//
void FormattedBuffer::Initialize(const FormattedBufferInit& init)
{
	assert(init.format != DXGI_FORMAT_UNKNOWN);
	assert(init.numElements > 0);
	stride = init.bitSize / 8;
	numElements = init.numElements;
	format = init.format;

	internalBuffer.Initialize(stride * numElements, stride, false, false, init.initData, init.initState, init.name);
}

void FormattedBuffer::Shutdown()
{
	internalBuffer.Shutdown();
}

D3D12_INDEX_BUFFER_VIEW FormattedBuffer::IBView() const
{
	assert(format == DXGI_FORMAT_R16_UINT || format == DXGI_FORMAT_R32_UINT);
	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = internalBuffer.gpuAddress;
	ibView.Format = format;
	ibView.SizeInBytes = stride * numElements;
	return ibView;
}

//
// Raw Buffer
//
void RawBuffer::Initialize(const RawBufferInit& init)
{
	assert(init.numElements > 0);
	NumElements = init.numElements;
	// Stride is 4 byte
	internalBuffer.Initialize(Stride * NumElements, Stride, init.cpuAccessible, init.allowUAV, init.initData, init.initState, init.name);

	DescriptorAlloc srvAlloc = srvDescriptorHeap.Allocate();
	SRV = srvAlloc.descriptorIndex;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};	
	srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	srvDesc.Buffer.NumElements = uint32_t(NumElements);
	d3dDevice->CreateShaderResourceView(internalBuffer.resource.Get(), &srvDesc, srvAlloc.cpuHandle);

	DescriptorAlloc uavAlloc = uavDescriptorHeap.Allocate();
	UAV = uavAlloc.cpuHandle;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	uavDesc.Buffer.NumElements = uint32_t(NumElements);
	d3dDevice->CreateUnorderedAccessView(internalBuffer.resource.Get(), nullptr, &uavDesc, UAV);
}

void RawBuffer::Shutdown()
{
	srvDescriptorHeap.Free(SRV);
	uavDescriptorHeap.Free(UAV);
	internalBuffer.Shutdown();
}

//
// Texture
//
void Texture::Initialize(const TextureInit& init)
{
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.Width = init.width;
	textureDesc.Height = init.height;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureDesc.Alignment = 0;

	CheckHRESULT(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)));

	const uint64_t uploadBufferSize = GetRequiredIntermediateSize(resource.Get(), 0, 1);
	CheckHRESULT(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&upload)));

	D3D12_SUBRESOURCE_DATA textureResourceData = {};
	textureResourceData.pData = init.initData;
	textureResourceData.RowPitch = init.width * sizeof(float);
	textureResourceData.SlicePitch = textureResourceData.RowPitch * init.height;

	UpdateSubresources(commandList.Get(), resource.Get(), upload.Get(), 0, 0, 1, &textureResourceData);

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	// Need this if descriptor table is shared = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	DescriptorAlloc srvAlloc = srvDescriptorHeap.Allocate();
	SRV = srvAlloc.descriptorIndex;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	d3dDevice->CreateShaderResourceView(resource.Get(), &srvDesc, srvAlloc.cpuHandle);
}

void Texture::Shutdown()
{
	srvDescriptorHeap.Free(SRV);

	if (resource)
	{
		resource = nullptr;
	}
	if (upload)
	{
		upload = nullptr;
	}
}

//
// Depth Buffer
//
void DepthBuffer::Initialize(const DepthBufferInit& init)
{
	// srv format based on init
	DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;
	if (init.format == DXGI_FORMAT_D32_FLOAT)
	{
		srvFormat = DXGI_FORMAT_R32_FLOAT;
	}
	
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = init.format;
	textureDesc.Width = init.width;
	textureDesc.Height = init.height;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = init.format;
	clearValue.DepthStencil.Depth = 1.f;
	clearValue.DepthStencil.Stencil = 0;

	CheckHRESULT(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&texture.resource)));

	DescriptorAlloc dsvAlloc = dsvDescriptorHeap.Allocate();
	dsv = dsvAlloc.cpuHandle;

	// Depth View
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = init.format;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	d3dDevice->CreateDepthStencilView(texture.resource.Get(), &dsvDesc, dsv);
}

void DepthBuffer::Shutdown()
{
	dsvDescriptorHeap.Free(dsv);
	texture.Shutdown();
}

//
// Render Target
//
void RenderTexture::Initialize(const RenderTextureInit& init)
{
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = init.format;
	textureDesc.Width = init.width;
	textureDesc.Height = init.height;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (init.allowUAV)
		textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = init.format;

	CheckHRESULT(d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(&texture.resource)));

	DescriptorAlloc srvAlloc = srvDescriptorHeap.Allocate();
	texture.SRV = srvAlloc.descriptorIndex;

	d3dDevice->CreateShaderResourceView(texture.resource.Get(), nullptr, srvAlloc.cpuHandle);

	DescriptorAlloc rtvAlloc = rtvDescriptorHeap.Allocate();
	rtv = rtvAlloc.cpuHandle;

	d3dDevice->CreateRenderTargetView(texture.resource.Get(), nullptr, rtv);

	if (init.allowUAV)
	{
		DescriptorAlloc uavAlloc = uavDescriptorHeap.Allocate();
		uav = uavAlloc.cpuHandle;
		uavGpuAddress = uavAlloc.gpuHandle;

		d3dDevice->CreateUnorderedAccessView(texture.resource.Get(), nullptr, nullptr, uav);
	}
}

void RenderTexture::Shutdown()
{
	uavDescriptorHeap.Free(uav);
	rtvDescriptorHeap.Free(rtv);
	texture.Shutdown();
}