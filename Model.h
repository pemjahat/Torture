#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <wrl/client.h> 

struct VertexModel
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT4 Color;
};

class Model
{
public:
	Model();
	~Model();

	HRESULT LoadFromFile(const std::string& filePath);
	HRESULT UploadGpuResources(
		ID3D12Device* device, 
		ID3D12CommandQueue* cmdQueue, 
		ID3D12CommandAllocator* cmdAlloc, 
		ID3D12GraphicsCommandList* cmdList);
	HRESULT RenderGpu(
		ID3D12CommandQueue* cmdQueue,
		ID3D12CommandAllocator* cmdAlloc,
		ID3D12GraphicsCommandList* cmdList);

private:
	// Vertex and index data
	std::vector<VertexModel> m_vertices;
	std::vector<uint32_t> m_indices;

	// GPU resources
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
	void* m_mappedVertexBufferData;
	void* m_mappedIndexBufferData;

	// Buffer views
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
};


