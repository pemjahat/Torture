#include "Model.h"
#include "d3dx12.h"
#include <tiny_gltf.h>
#include "Helper.h"

Model::Model()
{
    m_vertexBufferView = {};
    m_indexBufferView = {};
}

Model::~Model()
{
    // Unmap vertex + index buffer
    if (m_vertexBuffer && m_mappedVertexBufferData)
    {
        m_vertexBuffer->Unmap(0, nullptr);
    }
    if (m_indexBuffer && m_mappedIndexBufferData)
    {
        m_indexBuffer->Unmap(0, nullptr);
    }
}

HRESULT Model::LoadFromFile(const std::string& filePath)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    // Load .glb file (binary glTF)
    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);
    if (!warn.empty()) { printf("Warning: %s\n", warn.c_str()); }
    if (!err.empty()) { printf("Error: %s\n", err.c_str()); }
    if (!ret) { return E_FAIL; }

    // Clear data
    m_vertices.clear();
    m_indices.clear();

    // Assume the first mesh in the first scene
    const auto& mesh = model.meshes[0];
    const auto& primitive = mesh.primitives[0];

    // Get vertex positions
    const auto& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
    const auto& posView = model.bufferViews[posAccessor.bufferView];
    const auto& posBuffer = model.buffers[posView.buffer];
    const float* posData = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

    // Get vertex colors (if available, otherwise use a default color)
    bool hasColor = primitive.attributes.find("COLOR_0") != primitive.attributes.end();
    const float* colorData = nullptr;
    if (hasColor)
    {
        const auto& colorAccessor = model.accessors[primitive.attributes.at("COLOR_0")];
        const auto& colorView = model.bufferViews[colorAccessor.bufferView];
        const auto& colorBuffer = model.buffers[colorView.buffer];
        colorData = reinterpret_cast<const float*>(&colorBuffer.data[colorView.byteOffset + colorAccessor.byteOffset]);
    }

    // Extract vertices
    m_vertices.resize(posAccessor.count);
    for (size_t i = 0; i < posAccessor.count; ++i)
    {
        m_vertices[i].Position = DirectX::XMFLOAT3(posData[i * 3 + 0], posData[i * 3 + 1], posData[i * 3 + 2]);
        if (hasColor)
        {
            m_vertices[i].Color = DirectX::XMFLOAT4(colorData[i * 4 + 0], colorData[i * 4 + 1], colorData[i * 4 + 2], colorData[i * 4 + 3]);
        }
        else
        {
            m_vertices[i].Color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f); // Default white
        }
    }

    // Get indices
    const auto& indexAccessor = model.accessors[primitive.indices];
    const auto& indexView = model.bufferViews[indexAccessor.bufferView];
    const auto& indexBuffer = model.buffers[indexView.buffer];
    const uint32_t* indexData = reinterpret_cast<const uint32_t*>(&indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset]);

    m_indices.resize(indexAccessor.count);
    for (size_t i = 0; i < indexAccessor.count; ++i)
    {
        m_indices[i] = indexData[i];
    }

	return S_OK;
}

HRESULT Model::UploadGpuResources(
	ID3D12Device* device,
	ID3D12CommandQueue* cmdQueue,
	ID3D12CommandAllocator* cmdAlloc,
	ID3D12GraphicsCommandList* cmdList)
{
    if (m_vertices.empty() || m_indices.empty())
    {
        return E_FAIL;
    }

    //// Create vertex buffer (upload heap)
    //UINT vertexBufferSize = static_cast<UINT>(m_vertices.size() * sizeof(Vertex));
    //D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD };
    //D3D12_RESOURCE_DESC resourceDesc = {};
    //resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    //resourceDesc.Width = vertexBufferSize;
    //resourceDesc.Height = 1;
    //resourceDesc.DepthOrArraySize = 1;
    //resourceDesc.MipLevels = 1;
    //resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    //resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    //HRESULT hr = device->CreateCommittedResource(
    //    &heapProps,
    //    D3D12_HEAP_FLAG_NONE,
    //    &resourceDesc,
    //    D3D12_RESOURCE_STATE_GENERIC_READ,
    //    nullptr,
    //    IID_PPV_ARGS(&m_vertexBuffer));
    //if (FAILED(hr)) { throw std::runtime_error("Failed to create vertex buffer"); }

    //// Map and copy vertex data
    //hr = m_vertexBuffer->Map(0, nullptr, &m_mappedVertexBufferData);
    //if (FAILED(hr)) { throw std::runtime_error("Failed to map vertex buffer"); }
    //memcpy(m_mappedVertexBufferData, m_vertices.data(), vertexBufferSize);

    //// Create vertex buffer view
    //m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    //m_vertexBufferView.SizeInBytes = vertexBufferSize;
    //m_vertexBufferView.StrideInBytes = sizeof(Vertex);

    //// Create index buffer (upload heap)
    //UINT indexBufferSize = static_cast<UINT>(m_indices.size() * sizeof(uint32_t));
    //resourceDesc.Width = indexBufferSize;

    //hr = device->CreateCommittedResource(
    //    &heapProps,
    //    D3D12_HEAP_FLAG_NONE,
    //    &resourceDesc,
    //    D3D12_RESOURCE_STATE_GENERIC_READ,
    //    nullptr,
    //    IID_PPV_ARGS(&m_indexBuffer));
    //if (FAILED(hr)) { throw std::runtime_error("Failed to create index buffer"); }

    //// Map and copy index data
    //hr = m_indexBuffer->Map(0, nullptr, &m_mappedIndexBufferData);
    //if (FAILED(hr)) { throw std::runtime_error("Failed to map index buffer"); }
    //memcpy(m_mappedIndexBufferData, m_indices.data(), indexBufferSize);

    //// Create index buffer view
    //m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    //m_indexBufferView.SizeInBytes = indexBufferSize;
    //m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;


    // RENDER PART
    // Set vertex and index buffers
    //g_commandList->IASetVertexBuffers(0, 1, &g_model.GetVertexBufferView());
    //g_commandList->IASetIndexBuffer(&g_model.GetIndexBufferView());
    //g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //// Draw
    //g_commandList->DrawIndexedInstanced(g_model.GetIndexCount(), 1, 0, 0, 0);
}