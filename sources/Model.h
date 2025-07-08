#pragma once

#include "PCH.h"
#include "Helper.h"
#include "GraphicsTypes.h"

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

	//Texture texture;
	ComPtr<ID3D12Resource> texture;
	ComPtr<ID3D12Resource> uploadBuffer;	// TODO: No need store this
};

// TODO: No need to store this, runtime view should be fine
struct TextureView
{
	int resourceIndex = -1;
	int viewIndex = -1;
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

struct MaterialStructuredBuffer
{
	int useVertexColor = 0;
	int useTangent = 0;
	float metallicFactor = 0.f;
	float roughnessFactor = 1.f;

	int albedoTextureIndex = 0;
	int metallicTextureIndex = 0;
	int normalTextureIndex = 0;
	float paddedMat;

	DirectX::XMFLOAT4 baseColorFactor;
};

struct MeshStructuredBuffer
{
	DirectX::XMFLOAT3 centerBound;
	float padBound1;
	
	DirectX::XMFLOAT3 extentsBound;
	float padBound2;

	DirectX::XMFLOAT4X4 meshTransform;
};

// Constant must be aligned to 256 bytes
struct ModelConstants
{
	UINT meshIndex;		// index for mesh structured buffer
	UINT materialIndex;	// index for material structured buffer
};

struct MeshResources
{
	StructuredBuffer vertexBuffer;
	FormattedBuffer indexBuffer;
};

class Model
{
public:
	void Initialize();
	void Shutdown();

	void LoadShader();
	void CreatePSO();

	HRESULT LoadFromFile(const std::string& filePath);
	HRESULT UploadGpuResources(
		ID3D12Device* device,
		UINT sbBaseIndex,
		UINT texBaseIndex,		
		ID3D12DescriptorHeap* srvcbvHeap,		
		ID3D12GraphicsCommandList* cmdList);

	HRESULT RenderDepthOnly(
		ID3D12Device* device, 
		ID3D12GraphicsCommandList* cmdList, 
		UINT sbBaseIndex,
		ID3D12DescriptorHeap* srvHeap,
		const DirectX::BoundingFrustum& frustum);

	HRESULT RenderBasePass(
		ID3D12Device* device, 
		ID3D12GraphicsCommandList* cmdList,
		UINT sbBaseIndex,
		UINT texBaseIndex,		
		ID3D12DescriptorHeap* srvHeap,
		const DirectX::BoundingFrustum& frustum);

private:
	// Helper
	D3D12_FILTER GetD3D12Filter(int magFilter, int minFilter);
	D3D12_TEXTURE_ADDRESS_MODE GetD3D12AddressMode(int wrapMode);

	// 
	ModelData m_model;

	std::vector<MeshResources> m_meshResources;

	ComPtr<ID3D12Resource> m_meshSB;
	ComPtr<ID3D12Resource> m_meshUploadSB;
	ComPtr<ID3D12Resource> m_materialSB;
	ComPtr<ID3D12Resource> m_materialUploadSB;
	//StructuredBuffer m_meshSB;
	//StructuredBuffer m_materialSB;
	D3D12_CPU_DESCRIPTOR_HANDLE m_materialCpuHandle;

	ComPtr<IDxcBlob> m_vertexShader;
	ComPtr<IDxcBlob> m_pixelShader;
	ComPtr<IDxcBlob> m_depthVertexShader;

	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12RootSignature> m_depthRootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12PipelineState> m_depthPipelineState;
};


