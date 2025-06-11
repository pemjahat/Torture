#include "Model.h"

#include <d3d12.h>
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
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);
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
        // Add dummy uv value
        m_vertices[i].Uv = DirectX::XMFLOAT2(0.f, 0.f);
    }

    // Get indices
    const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
    const auto& indexView = model.bufferViews[indexAccessor.bufferView];
    const auto& indexBuffer = model.buffers[indexView.buffer];

    m_indices.resize(indexAccessor.count);
    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
    {
        const uint16_t* indexData = reinterpret_cast<const uint16_t*>(&indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset]);
        for (size_t i = 0; i < indexAccessor.count; ++i){
            m_indices[i] = static_cast<uint32_t>(indexData[i]);
        }
    }
    else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
    {
        const uint32_t* indexData = reinterpret_cast<const uint32_t*>(&indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset]);
        for (size_t i = 0; i < indexAccessor.count; ++i) {
            m_indices[i] = indexData[i];
        }
    }

	return S_OK;
}

HRESULT Model::UploadGpuResources(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList)
{
    if (m_vertices.empty() || m_indices.empty())
    {
        return E_FAIL;
    }

    // Create vertex buffer (upload heap)
    UINT vertexBufferSize = static_cast<UINT>(m_vertices.size() * sizeof(VertexModel));

    CheckHRESULT(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)));

    // Map and copy vertex data
    CheckHRESULT(m_vertexBuffer->Map(0, nullptr, &m_mappedVertexBufferData));
    memcpy(m_mappedVertexBufferData, m_vertices.data(), vertexBufferSize);

    // Create vertex buffer view
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.SizeInBytes = vertexBufferSize;
    m_vertexBufferView.StrideInBytes = sizeof(VertexModel);

    // Create index buffer (upload heap)
    UINT indexBufferSize = static_cast<UINT>(m_indices.size() * sizeof(uint32_t));

    CheckHRESULT(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_indexBuffer)));

    // Map and copy index data
    CheckHRESULT(m_indexBuffer->Map(0, nullptr, &m_mappedIndexBufferData));
    memcpy(m_mappedIndexBufferData, m_indices.data(), indexBufferSize);

    // Create index buffer view
    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.SizeInBytes = indexBufferSize;
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    return S_OK;
}

HRESULT Model::RenderGpu(ID3D12GraphicsCommandList* cmdList)
{
    if (m_vertices.empty() || m_indices.empty())
    {
        return E_FAIL;
    }

    // Set vertex and index buffers
    cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    cmdList->IASetIndexBuffer(&m_indexBufferView);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawIndexedInstanced(m_indices.size(), 1, 0, 0, 0);

    return S_OK;
}