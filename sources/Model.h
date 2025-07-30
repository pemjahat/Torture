#pragma once

#include "PCH.h"
#include "Helper.h"
#include "GraphicsTypes.h"
#include "../Shaders/HLSLCompatible.h"

using Microsoft::WRL::ComPtr;

//struct VertexData
//{
//	DirectX::XMFLOAT3 Position;
//	DirectX::XMFLOAT3 Normal;
//	DirectX::XMFLOAT4 Color;
//	DirectX::XMFLOAT4 Tangent;	// tangent (x, y, z) and handedness (w)
//	DirectX::XMFLOAT2 Uv;
//};

// Split between texture resource and texture view
struct TextureResource
{
	std::vector<unsigned char> pixels;
	int width = 0;
	int height = 0;
	int channels = 0;	// for RGBA is 4

	Texture texture;
};

// TODO: No need to store this, runtime view should be fine
struct TextureView
{
	int resourceIndex = -1;
	int viewIndex = -1;
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

struct PrimitiveData
{
	std::vector<MeshVertex> vertices;
	std::vector<uint32_t> indices;
	uint64_t vertexOffset = 0;
	uint64_t indexOffset = 0;
	bool hasVertexColor = false;
	bool hasTangent = false;
	int materialIndex = -1;
	DirectX::BoundingBox boundingBox;
	RawBuffer blasBuffer;
};

struct MeshData
{
	std::vector<PrimitiveData> primitives;
};

struct NodeData
{	
	int meshIndex = -1;
	DirectX::XMMATRIX transform;	// Node hierarchy transform
};

struct ModelData
{
	std::vector<NodeData> nodes;
	std::vector<MeshData> meshes;
	std::vector<SamplerData> samplers;
	std::vector<MaterialData> materials;
	std::vector<TextureView> textures;
	std::vector<TextureResource> images;

	uint32_t numPrimitives;
	uint32_t numInstances;
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
	StructuredBuffer instanceInfoBuffer;	// Raytrace use to retrieve vertices

	std::vector<MeshVertex> vertices;	// Combine vertices (blas)
	std::vector<uint32_t> indices;		// Combine indices

	RawBuffer tlasScratchBuffer;
	RawBuffer blasScratchBuffer;
	RawBuffer tlasBuffer;
	RawBuffer instanceBuffer;
};

class Model
{
public:
	void Initialize();
	void Shutdown();

	void LoadShader(const std::filesystem::path& shaderPath);
	void CreatePSO();

	HRESULT LoadFromFile(const std::string& filePath);
	HRESULT UploadGpuResources();
	void BuildAccelerationStructure();

	HRESULT RenderDepthOnly(
		const ConstantBuffer* sceneCB,
		const DirectX::BoundingFrustum& frustum);

	HRESULT RenderBasePass(
		const ConstantBuffer* sceneCB,
		const ConstantBuffer* lightCB,
		const DirectX::BoundingFrustum& frustum);

	void RenderDepthPrepass();
	void RenderGBuffer(const ConstantBuffer* sceneCB, const DirectX::BoundingFrustum& frustum);

	// Accessor
	const StructuredBuffer& GetVertexBuffer() const { return meshResource.vertexBuffer; }
	const FormattedBuffer& GetIndexBuffer() const { return meshResource.indexBuffer; }

	uint32_t NumPrimitives() const { return m_model.numPrimitives; }
	uint32_t NumInstances() const { return m_model.numInstances; }
	const std::vector<NodeData>& Nodes() const { return m_model.nodes; }
	const std::vector<MeshData>& Meshes() const { return m_model.meshes; }
	const std::vector<MaterialData>& Materials() const { return m_model.materials; }

	const StructuredBuffer& MeshBuffer() const { return m_meshSB; }
	const StructuredBuffer& MaterialBuffer() const { return m_materialSB; }
	const MeshResources& MeshResource() const { return meshResource; }
private:
	// Helper
	D3D12_FILTER GetD3D12Filter(int magFilter, int minFilter);
	D3D12_TEXTURE_ADDRESS_MODE GetD3D12AddressMode(int wrapMode);

	// 
	ModelData m_model;

	MeshResources meshResource;

	StructuredBuffer m_meshSB;
	StructuredBuffer m_materialSB;
	D3D12_CPU_DESCRIPTOR_HANDLE m_materialCpuHandle;

	ComPtr<IDxcBlob> m_vertexShader;
	ComPtr<IDxcBlob> m_depthVertexShader;
	ComPtr<IDxcBlob> gbufferVS;
	ComPtr<IDxcBlob> gbufferPS;
	ComPtr<IDxcBlob> alphaTestPS;

	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12RootSignature> m_depthRootSignature;
	ComPtr<ID3D12RootSignature> gbufferRootSignature;	
	ComPtr<ID3D12PipelineState> m_depthPipelineState;
	ComPtr<ID3D12PipelineState> gbufferPipelineState;
	ComPtr<ID3D12PipelineState> alphaTestPipelineState;
};


