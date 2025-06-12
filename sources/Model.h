#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <wrl/client.h> 

#include "Helper.h"

using Microsoft::WRL::ComPtr;

struct VertexData
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT4 Color;
	DirectX::XMFLOAT2 Uv;
};

struct TextureData
{
	std::vector<unsigned char> pixels;
	int width = 0;
	int height = 0;
	int channels = 0;	// for RGBA is 4

	// Resource
	ComPtr<ID3D12Resource> texture;
	D3D12_CPU_DESCRIPTOR_HANDLE srvTextureCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE srvTextureGpuHandle;
};

struct SamplerData
{
	D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;	// Default
	D3D12_TEXTURE_ADDRESS_MODE addressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	D3D12_TEXTURE_ADDRESS_MODE addressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

	// Resource
	D3D12_CPU_DESCRIPTOR_HANDLE samplerCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE samplerGpuHandle;
};

struct ModelData
{
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	std::vector<TextureData> textures;
	std::vector<SamplerData> samplers;
};

class Model
{
public:
	Model();
	~Model();

	HRESULT LoadFromFile(const std::string& filePath);
	HRESULT UploadGpuResources(
		ID3D12Device* device,
		DescriptorHeapAllocator& heapAlloc,	// For srv
		ID3D12DescriptorHeap* samplerHeap,	// For sampler
		ID3D12GraphicsCommandList* cmdList);
	HRESULT RenderGpu(ID3D12GraphicsCommandList* cmdList);

private:
	// Helper
	D3D12_FILTER GetD3D12Filter(int magFilter, int minFilter);
	D3D12_TEXTURE_ADDRESS_MODE GetD3D12AddressMode(int wrapMode);

	// 
	ModelData m_model;

	// GPU resources
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
	void* m_mappedVertexBufferData;
	void* m_mappedIndexBufferData;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_textureUploadHeap;

	// Buffer views
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
};


