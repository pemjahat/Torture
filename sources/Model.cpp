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

//
// Helper
//
D3D12_FILTER Model::GetD3D12Filter(int magFilter, int minFilter)
{
    if (magFilter == TINYGLTF_TEXTURE_FILTER_LINEAR && minFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR){
        return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    }
    else if (magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST && minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST) {
        return D3D12_FILTER_MIN_MAG_MIP_POINT;
    }
    else if (magFilter == TINYGLTF_TEXTURE_FILTER_LINEAR && minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST) {
        return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    }
    return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
}

D3D12_TEXTURE_ADDRESS_MODE Model::GetD3D12AddressMode(int wrapMode)
{
    switch (wrapMode)
    {
    case TINYGLTF_TEXTURE_WRAP_REPEAT:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    default:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
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
    m_model.vertices.clear();
    m_model.indices.clear();
    m_model.textures.clear();

    /* Typical buffer view for box.gltf (one per attribute)*/
    /*
    * buffer: 0, byteoffset: 0, bytelength: 96      // Positions -> 8 vert * 3 floats * 4 bytes = 96 bytes
    * buffer: 0, byteoffset: 96, bytelength: 96     // Normals
    * buffer: 0, byteoffset: 192, bytelength: 64    // Texcoord
    */

    /* Typical buffer view for boxinterleaved.gltf (single buffer view, all attribute interleaved)*/
    /*
    * buffer: 0, byteoffset: 0, bytelength: 256, bytestride: 32
    */
    // Assume the first mesh in the first scene
    const auto& mesh = model.meshes[0];
    const auto& primitive = mesh.primitives[0];

    // Get vertex positions
    const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
    const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
    const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];

    // Handle interleaved or non-interleaved
    {
        const unsigned char* bufferData = &posBuffer.data[posView.byteOffset + posAccessor.byteOffset];
        size_t byteStride = posView.byteStride ? posView.byteStride : sizeof(float) * 3;
        m_model.vertices.resize(posAccessor.count);

        for (size_t i = 0; i < posAccessor.count; ++i)
        {
            const float* postData = reinterpret_cast<const float*>(bufferData + i * byteStride);
            m_model.vertices[i].Position = DirectX::XMFLOAT3(postData[0], postData[1], postData[2]);
            m_model.vertices[i].Color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f); // Default white
        }
    }

    // Get normal
    if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
    {
        const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.at("NORMAL")];
        const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
        const tinygltf::Buffer& normBuffer = model.buffers[normView.buffer];

        const unsigned char* bufferData = &normBuffer.data[normView.byteOffset + normAccessor.byteOffset];
        size_t byteStride = normView.byteStride ? normView.byteStride : sizeof(float) * 3;
        
        assert(normAccessor.count == m_model.vertices.size());
        for (size_t i = 0; i < normAccessor.count; ++i)
        {
            const float* normData = reinterpret_cast<const float*>(bufferData + i * byteStride);
            m_model.vertices[i].Normal = DirectX::XMFLOAT3(normData[0], normData[1], normData[2]);
        }
    }
    
    // Get texture coordinates
    if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
    {
        const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
        const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
        const tinygltf::Buffer& uvBuffer = model.buffers[uvView.buffer];

        const unsigned char* bufferData = &uvBuffer.data[uvView.byteOffset + uvAccessor.byteOffset];
        size_t byteStride = uvView.byteStride ? uvView.byteStride : sizeof(float) * 2;

        assert(uvAccessor.count == m_model.vertices.size());
        for (size_t i = 0; i < uvAccessor.count; ++i)
        {
            const float* uvData = reinterpret_cast<const float*>(bufferData + i * byteStride);
            m_model.vertices[i].Uv = DirectX::XMFLOAT2(uvData[0], uvData[1]);
        }
    }
    // Get indices
    const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
    const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
    const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];

    // Indices are non interlace so stride is component size
    {
        const unsigned char* bufferData = &indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset];
        m_model.indices.resize(indexAccessor.count);

        if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
        {
            const uint16_t* indexData = reinterpret_cast<const uint16_t*>(bufferData);
            for (size_t i = 0; i < indexAccessor.count; ++i) {
                m_model.indices[i] = static_cast<uint32_t>(indexData[i]);
            }
        }
        else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
        {
            const uint32_t* indexData = reinterpret_cast<const uint32_t*>(bufferData);
            for (size_t i = 0; i < indexAccessor.count; ++i) {
                m_model.indices[i] = indexData[i];
            }
        }
    }

    // Extract Texture
    if (primitive.material >= 0)
    {
        const auto& material = model.materials[primitive.material];

        // So far only use "pbr metallic-roughness"
        const auto& pbr = material.pbrMetallicRoughness;
        // Load base color
        if (pbr.baseColorTexture.index >= 0)
        {
            const tinygltf::Texture& texture = model.textures[pbr.baseColorTexture.index];
            const tinygltf::Image& image = model.images[texture.source];

            TextureData texData;
            texData.pixels = image.image;
            texData.width = image.width;
            texData.height = image.height;
            texData.channels = image.component;
            m_model.textures.push_back(std::move(texData));
        }
        // Load normal texture
        if (material.normalTexture.index >= 0)
        {
            const tinygltf::Texture& texture = model.textures[material.normalTexture.index];
            const tinygltf::Image& image = model.images[texture.source];

            TextureData texData;
            texData.pixels = image.image;
            texData.width = image.width;
            texData.height = image.height;
            texData.channels = image.component;
            m_model.textures.push_back(std::move(texData));
        }

        /*if (material.values.find("baseColorTexture") != material.values.end())
        {
            const auto& texInfo = material.values.at("baseColorTexture");
            int texIndex = texInfo.TextureIndex();
            if (texIndex >= 0 && texIndex < model.images.size())
            {
                const tinygltf::Texture& texture = model.textures[texIndex];
                const tinygltf::Image& image = model.images[texture.source];

                TextureData texData;
                texData.pixels = image.image;
                texData.width = image.width;
                texData.height = image.height;
                texData.channels = image.component;
                m_model.textures.push_back(std::move(texData));

                if (texture.sampler >= 0)
                {
                    const auto& sampler = model.samplers[texture.sampler];
                    SamplerData samplerData;
                    samplerData.filter = GetD3D12Filter(sampler.magFilter, sampler.minFilter);
                    samplerData.addressU = GetD3D12AddressMode(sampler.wrapS);
                    samplerData.addressV = GetD3D12AddressMode(sampler.wrapT);
                    m_model.samplers.push_back(std::move(samplerData));
                }
            }
        }*/
    }

	return S_OK;
}

HRESULT Model::UploadGpuResources(
	ID3D12Device* device,
    DescriptorHeapAllocator& heapAlloc, // for texture
    ID3D12DescriptorHeap* samplerHeap,	// For sampler
	ID3D12GraphicsCommandList* cmdList)
{
    if (m_model.vertices.empty() || m_model.indices.empty())
    {
        return E_FAIL;
    }

    // Create vertex buffer (upload heap)
    UINT vertexBufferSize = static_cast<UINT>(m_model.vertices.size() * sizeof(VertexData));

    //Note: using upload heaps to transfer static data like vert buffers is not
    // recommended. Every time the GPU needs it, the upload heap will be marshalled 
    // over. Please read up on Default Heap usage. An upload heap is used here for 
    // code simplicity and because there are very few verts to actually transfer.
    CheckHRESULT(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)));

    // Map and copy vertex data
    CheckHRESULT(m_vertexBuffer->Map(0, nullptr, &m_mappedVertexBufferData));
    memcpy(m_mappedVertexBufferData, m_model.vertices.data(), vertexBufferSize);

    // Create vertex buffer view
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.SizeInBytes = vertexBufferSize;
    m_vertexBufferView.StrideInBytes = sizeof(VertexData);

    // Create index buffer (upload heap)
    UINT indexBufferSize = static_cast<UINT>(m_model.indices.size() * sizeof(uint32_t));

    CheckHRESULT(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_indexBuffer)));

    // Map and copy index data
    CheckHRESULT(m_indexBuffer->Map(0, nullptr, &m_mappedIndexBufferData));
    memcpy(m_mappedIndexBufferData, m_model.indices.data(), indexBufferSize);

    // Create index buffer view
    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.SizeInBytes = indexBufferSize;
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    // Create texture
    if (!m_model.textures.empty())
    {
        for (auto& textureData : m_model.textures)
        {
            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.MipLevels = 1;
            textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.Width = textureData.width;
            textureDesc.Height = textureData.height;
            textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            CheckHRESULT(device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &textureDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&textureData.texture)));

            // Staging buffer to upload
            const UINT64 uploadBufferSize = GetRequiredIntermediateSize(textureData.texture.Get(), 0, 1);

            CheckHRESULT(device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&textureData.uploadBuffer)));

            D3D12_SUBRESOURCE_DATA textureResourceData = {};
            textureResourceData.pData = &textureData.pixels[0];
            textureResourceData.RowPitch = textureData.width * sizeof(float);
            textureResourceData.SlicePitch = textureResourceData.RowPitch * textureData.height;

            UpdateSubresources(cmdList, textureData.texture.Get(), textureData.uploadBuffer.Get(), 0, 0, 1, &textureResourceData);

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = textureData.texture.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            // Need this if descriptor table is shared = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            cmdList->ResourceBarrier(1, &barrier);

            // Srv for texture
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = textureDesc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            heapAlloc.Alloc(&textureData.srvTextureCpuHandle, &textureData.srvTextureGpuHandle);
            device->CreateShaderResourceView(textureData.texture.Get(), &srvDesc, textureData.srvTextureCpuHandle);
        }
    }

    // Create sampler (create dummy if it's empty)
    if (m_model.samplers.empty())
    {
        SamplerData samplerData{};
        m_model.samplers.push_back(std::move(samplerData));
    }
    {
        // take the first one for now
        auto& samplerData = m_model.samplers[0];

        D3D12_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = samplerData.filter;
        samplerDesc.AddressU = samplerData.addressU;
        samplerDesc.AddressV = samplerData.addressV;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.MipLODBias = 0.f;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplerDesc.MinLOD = 0.f;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        samplerData.samplerCpuHandle = samplerHeap->GetCPUDescriptorHandleForHeapStart();
        samplerData.samplerGpuHandle = samplerHeap->GetGPUDescriptorHandleForHeapStart();
        device->CreateSampler(&samplerDesc, samplerData.samplerCpuHandle);
    }
    return S_OK;
}

HRESULT Model::RenderGpu(ID3D12GraphicsCommandList* cmdList)
{
    if (m_model.vertices.empty() || m_model.indices.empty())
    {
        return E_FAIL;
    }
    // descriptor table
    if (!m_model.textures.empty())
    {
        // take the first one for now
        auto& textureData = m_model.textures[0];
        cmdList->SetGraphicsRootDescriptorTable(0, textureData.srvTextureGpuHandle);
    }
    if (!m_model.samplers.empty())
    {
        // take the first one for now
        auto& samplerData = m_model.samplers[0];
        cmdList->SetGraphicsRootDescriptorTable(1, samplerData.samplerGpuHandle);
    }
    
    // Set vertex and index buffers
    cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    cmdList->IASetIndexBuffer(&m_indexBufferView);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawIndexedInstanced(m_model.indices.size(), 1, 0, 0, 0);

    return S_OK;
}