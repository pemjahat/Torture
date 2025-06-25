#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <string>
#include <wrl/client.h> 

#include "Helper.h"

using Microsoft::WRL::ComPtr;

struct VertexData
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT4 Color;
	DirectX::XMFLOAT4 Tangent;	// tangent (x, y, z) and handedness (w)
	DirectX::XMFLOAT2 Uv;
};

// Split between texture resource and texture view
struct TextureResource
{
	std::vector<unsigned char> pixels;
	int width = 0;
	int height = 0;
	int channels = 0;	// for RGBA is 4

	ComPtr<ID3D12Resource> texture;
	ComPtr<ID3D12Resource> uploadBuffer;	// TODO: No need store this
};

// TODO: No need to store this, runtime view should be fine
struct TextureView
{
	int resourceIndex = -1;
	// Resource
	D3D12_CPU_DESCRIPTOR_HANDLE srvTextureCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE srvTextureGpuHandle;
};

struct MaterialData
{
	DirectX::XMFLOAT4 baseColorFactor = DirectX::XMFLOAT4(1.f, 1.f, 1.f, 1.f);
	float metallicFactor = 0.f;
	float roughnessFactor = 1.f;
	int albedoTextureIndex = -1;
	int metallicRoughnessTextureIndex = -1;
	int normalTextureIndex = -1;
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

struct MeshData
{
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	int materialIndex = -1;
	DirectX::XMFLOAT4X4 transform;	// Node hierarchy transform
	DirectX::BoundingBox boundingBox;
};

struct ModelData
{
	std::vector<MeshData> meshes;
	std::vector<SamplerData> samplers;
	std::vector<MaterialData> materials;
	std::vector<TextureView> textures;
	std::vector<TextureResource> images;
	bool hasVertexColor = false;
	bool hasTangent = false;
};

struct MaterialConstantBuffer
{
	int useVertexColor = 0;
	int useTangent = 0;
	float metallicFactor = 0.f;
	float roughnessFactor = 1.f;

	int hasAlbedoMap = 0;
	int hasMetallicRoughnessMap = 0;
	int hasNormalMap = 0;
	float paddedMat;

	DirectX::XMFLOAT4 baseColorFactor;
	DirectX::XMFLOAT4X4 meshTransform;
};

struct MeshResources
{
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexUploadBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexUploadBuffer;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;

	D3D12_GPU_DESCRIPTOR_HANDLE constantBufferView;
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
	HRESULT RenderGpu(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const DirectX::BoundingFrustum& frustum);

private:
	// Helper
	D3D12_FILTER GetD3D12Filter(int magFilter, int minFilter);
	D3D12_TEXTURE_ADDRESS_MODE GetD3D12AddressMode(int wrapMode);

	// 
	ModelData m_model;

	std::vector<MeshResources> m_meshResources;

	ComPtr<ID3D12Resource> m_materialCB;
	D3D12_CPU_DESCRIPTOR_HANDLE m_materialCpuHandle;	
};


