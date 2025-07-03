#define NOMINMAX // Prevent min/max macros
#include "Model.h"
#include <DirectXCollision.h>
#include "d3dx12.h"
#include <tiny_gltf.h>

using namespace DirectX;

Model::Model()
{
    
}

Model::~Model()
{
    // Unmap vertex + index buffer
    /*if (m_vertexBuffer && m_mappedVertexBufferData)
    {
        m_vertexBuffer->Unmap(0, nullptr);
    }
    if (m_indexBuffer && m_mappedIndexBufferData)
    {
        m_indexBuffer->Unmap(0, nullptr);
    }*/
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

//
// Helper
//
XMMATRIX GetNodeTransform(const tinygltf::Node& node)
{
    XMMATRIX transform = XMMatrixIdentity();

    // Matrix
    if (node.matrix.size() == 16)
    {
        XMFLOAT4X4 mat;
        for (int i = 0; i < 16; ++i)
        {
            reinterpret_cast<float*>(&mat)[i] = static_cast<float>(node.matrix[i]);
        }
        transform = XMLoadFloat4x4(&mat);
    }
    else
    {
        XMVECTOR translation = XMVectorSet(0.f, 0.f, 0.f, 0.f);
        XMVECTOR rotation = XMVectorSet(0.f, 0.f, 0.f, 1.f);
        XMVECTOR scale = XMVectorSet(1.f, 1.f, 1.f, 0.f);

        // Translation
        if (!node.translation.empty())
        {
            translation = XMVectorSet(
                static_cast<float>(node.translation[0]),
                static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2]),
                0.f);
        }

        // Rotation (quaternion: x, y, z, 0
        if (!node.rotation.empty())
        {
            rotation = XMVectorSet(
                static_cast<float>(node.rotation[0]),
                static_cast<float>(node.rotation[1]),
                static_cast<float>(node.rotation[2]),
                static_cast<float>(node.rotation[3]));
        }

        // Scale
        if (!node.scale.empty())
        {
            scale = XMVectorSet(
                static_cast<float>(node.scale[0]),
                static_cast<float>(node.scale[1]),
                static_cast<float>(node.scale[2]),
                0.f);
        }

        // Compute TRS
        XMMATRIX tMatrix = XMMatrixTranslationFromVector(translation);
        XMMATRIX rMatrix = XMMatrixRotationQuaternion(rotation);
        XMMATRIX sMatrix = XMMatrixScalingFromVector(scale);
        transform = sMatrix * rMatrix * tMatrix;
    }

    return transform;
}

void ProcessNode(const tinygltf::Model& model, int nodeIndex, const XMMATRIX& parentTransform, ModelData& modelData)
{
    const tinygltf::Node& node = model.nodes[nodeIndex];
    XMMATRIX nodeTransform = XMMatrixMultiply(GetNodeTransform(node), parentTransform);

    // Interleaved vs non interleaved
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

   // Only process if mesh preset (ignore camera node for now)
    if (node.mesh >= 0)
    {
        const auto& mesh = model.meshes[node.mesh];
        MeshData meshData;
        // Row-Major vs Colum-Major issue
        //XMStoreFloat4x4(&meshData.transform, XMMatrixTranspose(nodeTransform));
        XMStoreFloat4x4(&meshData.transform, nodeTransform);

        for (const auto& primitive : mesh.primitives)
        {
            // Get Position
            if (primitive.attributes.find("POSITION") != primitive.attributes.end())
            {
                const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
                const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];

                const unsigned char* bufferData = &posBuffer.data[posView.byteOffset + posAccessor.byteOffset];
                size_t byteStride = posView.byteStride ? posView.byteStride : sizeof(float) * 3;    // Handle interleaved or non-interleaved
                meshData.vertices.resize(posAccessor.count);
                for (size_t i = 0; i < posAccessor.count; ++i)
                {
                    const float* postData = reinterpret_cast<const float*>(bufferData + i * byteStride);
                    meshData.vertices[i].Position = XMFLOAT3(postData[0], postData[1], postData[2]);
                }
                
                // AABB
                XMFLOAT3 min = XMFLOAT3(
                    static_cast<float>(posAccessor.minValues[0]),
                    static_cast<float>(posAccessor.minValues[1]),
                    static_cast<float>(posAccessor.minValues[2]));

                XMFLOAT3 max = XMFLOAT3(
                    static_cast<float>(posAccessor.maxValues[0]),
                    static_cast<float>(posAccessor.maxValues[1]),
                    static_cast<float>(posAccessor.maxValues[2]));

                BoundingBox::CreateFromPoints(meshData.boundingBox, XMLoadFloat3(&min), XMLoadFloat3(&max));
            }

            // Get Normal
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
            {
                const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.at("NORMAL")];
                const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
                const tinygltf::Buffer& normBuffer = model.buffers[normView.buffer];

                const unsigned char* bufferData = &normBuffer.data[normView.byteOffset + normAccessor.byteOffset];
                size_t byteStride = normView.byteStride ? normView.byteStride : sizeof(float) * 3;

                assert(normAccessor.count == meshData.vertices.size());
                for (size_t i = 0; i < normAccessor.count; ++i)
                {
                    const float* normData = reinterpret_cast<const float*>(bufferData + i * byteStride);
                    meshData.vertices[i].Normal = XMFLOAT3(normData[0], normData[1], normData[2]);
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

                assert(uvAccessor.count == meshData.vertices.size());
                for (size_t i = 0; i < uvAccessor.count; ++i)
                {
                    const float* uvData = reinterpret_cast<const float*>(bufferData + i * byteStride);
                    meshData.vertices[i].Uv = XMFLOAT2(uvData[0], uvData[1]);
                }
            }
            // Get tangent
            if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
            {
                modelData.hasTangent = 1;

                const tinygltf::Accessor& tangentAccessor = model.accessors[primitive.attributes.at("TANGENT")];
                const tinygltf::BufferView& tangentView = model.bufferViews[tangentAccessor.bufferView];
                const tinygltf::Buffer& tangentBuffer = model.buffers[tangentView.buffer];

                const unsigned char* bufferData = &tangentBuffer.data[tangentView.byteOffset + tangentAccessor.byteOffset];
                size_t byteStride = tangentView.byteStride ? tangentView.byteStride : sizeof(float) * 4;

                assert(tangentAccessor.count == meshData.vertices.size());
                for (size_t i = 0; i < tangentAccessor.count; ++i)
                {
                    const float* tangentData = reinterpret_cast<const float*>(bufferData + i * byteStride);
                    meshData.vertices[i].Tangent = XMFLOAT4(tangentData[0], tangentData[1], tangentData[2], tangentData[3]);
                }
            }

            // Get vertex color
            if (primitive.attributes.find("COLOR_0") != primitive.attributes.end())
            {
                modelData.hasVertexColor = 1;

                const tinygltf::Accessor& colorAccessor = model.accessors[primitive.attributes.at("COLOR_0")];
                const tinygltf::BufferView& colorView = model.bufferViews[colorAccessor.bufferView];
                const tinygltf::Buffer& colorBuffer = model.buffers[colorView.buffer];

                const unsigned char* bufferData = &colorBuffer.data[colorView.byteOffset + colorAccessor.byteOffset];
                size_t byteStride = colorView.byteStride ? colorView.byteStride : sizeof(float) * 4;

                assert(colorAccessor.count == meshData.vertices.size());
                for (size_t i = 0; i < colorAccessor.count; ++i)
                {
                    const float* colorData = reinterpret_cast<const float*>(bufferData + i * byteStride);
                    meshData.vertices[i].Color = XMFLOAT4(colorData[0], colorData[1], colorData[2], colorData[3]);
                }
            }

            // Get indices
            if (primitive.indices >= 0)
            {
                const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];

                const unsigned char* bufferData = &indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset];
                meshData.indices.resize(indexAccessor.count);

                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    const uint16_t* indexData = reinterpret_cast<const uint16_t*>(bufferData);
                    for (size_t i = 0; i < indexAccessor.count; ++i) {
                        meshData.indices[i] = static_cast<uint32_t>(indexData[i]);
                    }
                }
                else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    const uint32_t* indexData = reinterpret_cast<const uint32_t*>(bufferData);
                    for (size_t i = 0; i < indexAccessor.count; ++i) {
                        meshData.indices[i] = indexData[i];
                    }
                }
            }

            // Get material index
            meshData.materialIndex = primitive.material;

            modelData.meshes.push_back(std::move(meshData));
        }
    }
    

    // Recursive process children
    for (int childIndex : node.children)
    {
        ProcessNode(model, childIndex, nodeTransform, modelData);
    }
}

void ProcessMaterial(const tinygltf::Model& model, ModelData& modelData)
{
    // 1st Load all TextureResource (tinygltf::image)
    for (const tinygltf::Image& image : model.images)
    {
        TextureResource texResource;

        texResource.pixels = image.image;
        texResource.width = image.width;
        texResource.height = image.height;
        texResource.channels = image.component;

        modelData.images.push_back(std::move(texResource));
    }

    // Load all Texture Views (tinygltf::textures)
    for (const tinygltf::Texture& tex : model.textures)
    {
        TextureView texView;
        texView.resourceIndex = tex.source;

        modelData.textures.push_back(std::move(texView));
    }

    // Load all Material (point to texture view)
    for (const tinygltf::Material& mat : model.materials)
    {
        const auto& pbr = mat.pbrMetallicRoughness;

        MaterialData material;
        if (!pbr.baseColorFactor.empty())
        {
            material.baseColorFactor = XMFLOAT4(
                static_cast<float>(pbr.baseColorFactor[0]),
                static_cast<float>(pbr.baseColorFactor[1]),
                static_cast<float>(pbr.baseColorFactor[2]),
                static_cast<float>(pbr.baseColorFactor[3]));
        }
        material.metallicFactor = static_cast<float>(pbr.metallicFactor);
        material.roughnessFactor = static_cast<float>(pbr.roughnessFactor);
        material.albedoTextureIndex = pbr.baseColorTexture.index;
        material.metallicRoughnessTextureIndex = pbr.metallicRoughnessTexture.index;
        material.normalTextureIndex = mat.normalTexture.index;

        modelData.materials.push_back(std::move(material));
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
    m_model.meshes.clear();
    //m_model.textures.clear();

    // start with scene root nodes
    const auto& scene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
    XMMATRIX identity = XMMatrixIdentity();
    for (int nodeIndex : scene.nodes)
    {
        ProcessNode(model, nodeIndex, identity, m_model);
    }

    // Process material
    ProcessMaterial(model, m_model);

	return S_OK;
}

HRESULT Model::UploadGpuResources(
	ID3D12Device* device,
    UINT srvBaseIndex,
    UINT cbvBaseIndex,
    ID3D12DescriptorHeap* srvcbvHeap,
    ID3D12DescriptorHeap* samplerHeap,	// For sampler
	ID3D12GraphicsCommandList* cmdList)
{
    if (m_model.meshes.empty())
    {
        return E_FAIL;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuHandle(srvcbvHeap->GetCPUDescriptorHandleForHeapStart());
    srvCpuHandle.Offset(srvBaseIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle(srvcbvHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvGpuHandle(srvcbvHeap->GetGPUDescriptorHandleForHeapStart());
    cbvCpuHandle.Offset(cbvBaseIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    cbvGpuHandle.Offset(cbvBaseIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    
    m_meshResources.resize(m_model.meshes.size());

    for (size_t i = 0; i < m_model.meshes.size(); ++i)
    {
        auto& mesh = m_model.meshes[i];
        auto& resource = m_meshResources[i];

        // Create vertex buffer (upload heap)
        UINT vertexBufferSize = static_cast<UINT>(mesh.vertices.size() * sizeof(VertexData));

        CheckHRESULT(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&resource.vertexBuffer)));

        CheckHRESULT(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&resource.vertexUploadBuffer)));

        void* mappedData;
        resource.vertexUploadBuffer->Map(0, nullptr, &mappedData);
        memcpy(mappedData, mesh.vertices.data(), vertexBufferSize);
        resource.vertexUploadBuffer->Unmap(0, nullptr);

        cmdList->CopyBufferRegion(resource.vertexBuffer.Get(), 0, resource.vertexUploadBuffer.Get(), 0, vertexBufferSize);
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            resource.vertexBuffer.Get(), 
            D3D12_RESOURCE_STATE_COPY_DEST, 
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        cmdList->ResourceBarrier(1, &barrier);

        resource.vertexBufferView.BufferLocation = resource.vertexBuffer->GetGPUVirtualAddress();
        resource.vertexBufferView.SizeInBytes = vertexBufferSize;
        resource.vertexBufferView.StrideInBytes = sizeof(VertexData);

        // Create index buffer (upload heap)
        UINT indexBufferSize = static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t));

        CheckHRESULT(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&resource.indexBuffer)));

        CheckHRESULT(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&resource.indexUploadBuffer)));

        resource.indexUploadBuffer->Map(0, nullptr, &mappedData);
        memcpy(mappedData, mesh.indices.data(), indexBufferSize);
        resource.indexUploadBuffer->Unmap(0, nullptr);

        cmdList->CopyBufferRegion(resource.indexBuffer.Get(), 0, resource.indexUploadBuffer.Get(), 0, indexBufferSize);
        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            resource.indexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER);
        cmdList->ResourceBarrier(1, &barrier);

        resource.indexBufferView.BufferLocation = resource.indexBuffer->GetGPUVirtualAddress();
        resource.indexBufferView.SizeInBytes = indexBufferSize;
        resource.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    }
    
    // Create buffer for material data (large enough to handle multi meshes)
    {
        UINT numMeshes = m_meshResources.size();
        const UINT materialCBSize = (sizeof(MaterialConstantBuffer) + 255) & ~255;
        const UINT materialBufferSize = materialCBSize * numMeshes;  // Align to 256 byte
        CheckHRESULT(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_materialCB)));

        for (size_t i = 0; i < m_model.meshes.size(); ++i)
        {
            auto& resource = m_meshResources[i];

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = m_materialCB->GetGPUVirtualAddress() + i * materialCBSize;
            cbvDesc.SizeInBytes = materialCBSize;
            device->CreateConstantBufferView(&cbvDesc, cbvCpuHandle);
            resource.constantBufferView = cbvGpuHandle;
            cbvCpuHandle.Offset(1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
            cbvGpuHandle.Offset(1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
        }
    }

    // Create texture
    if (!m_model.images.empty())
    {
        // Texture resources
        for (TextureResource& texResource : m_model.images)
        {
            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.MipLevels = 1;
            textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.Width = texResource.width;
            textureDesc.Height = texResource.height;
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
                IID_PPV_ARGS(&texResource.texture)));

            // Staging buffer to upload
            const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texResource.texture.Get(), 0, 1);

            CheckHRESULT(device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&texResource.uploadBuffer)));

            D3D12_SUBRESOURCE_DATA textureResourceData = {};
            textureResourceData.pData = &texResource.pixels[0];
            textureResourceData.RowPitch = texResource.width * sizeof(float);
            textureResourceData.SlicePitch = textureResourceData.RowPitch * texResource.height;

            UpdateSubresources(cmdList, texResource.texture.Get(), texResource.uploadBuffer.Get(), 0, 0, 1, &textureResourceData);

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = texResource.texture.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            // Need this if descriptor table is shared = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            cmdList->ResourceBarrier(1, &barrier);

            // SRV
           /* D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;

            device->CreateShaderResourceView(texResource.texture.Get(), &srvDesc, srvCpuHandle);
            srvCpuHandle.Offset(1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));*/
        }

        // Texture views (load from Materials)
        UINT viewIndex = 0;
        for (TextureView& texView: m_model.textures)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            texView.viewIndex = viewIndex;

            TextureResource& texResource = m_model.images[texView.resourceIndex];
            device->CreateShaderResourceView(texResource.texture.Get(), &srvDesc, srvCpuHandle);
            srvCpuHandle.Offset(1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
            viewIndex++;
        }
        /*for (const MaterialData& material : m_model.materials)
        {            
            if (material.albedoTextureIndex >= 0)
            {
                TextureView& texView = m_model.textures[material.albedoTextureIndex];
                
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = 1;                
                TextureResource& texResource = m_model.images[texView.resourceIndex];
                device->CreateShaderResourceView(texResource.texture.Get(), &srvDesc, srvCpuHandle);
                srvCpuHandle.Offset(1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
            }
            if (material.metallicRoughnessTextureIndex >= 0)
            {
                TextureView& texView = m_model.textures[material.metallicRoughnessTextureIndex];

                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = 1;

                TextureResource& texResource = m_model.images[texView.resourceIndex];
                device->CreateShaderResourceView(texResource.texture.Get(), &srvDesc, srvCpuHandle);
                srvCpuHandle.Offset(1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
            }
            if (material.normalTextureIndex >= 0)
            {
                TextureView& texView = m_model.textures[material.normalTextureIndex];

                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = 1;

                TextureResource& texResource = m_model.images[texView.resourceIndex];
                device->CreateShaderResourceView(texResource.texture.Get(), &srvDesc, srvCpuHandle);
                srvCpuHandle.Offset(1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
            }
        }*/
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

HRESULT Model::RenderDepthOnly(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const BoundingFrustum& frustum)
{
    if (m_model.meshes.empty())
    {
        return E_FAIL;
    }

    void* mappedData;
    UINT materialCBSize = (sizeof(MaterialConstantBuffer) + 255) & ~255;
    m_materialCB->Map(0, nullptr, &mappedData);
    for (size_t i = 0; i < m_model.meshes.size(); ++i)
    {
        const auto& mesh = m_model.meshes[i];
        const auto& resource = m_meshResources[i];

        MaterialConstantBuffer materialCB = {};
        // Transpose is for purpose row major(gltf) - column major(directX)
        XMStoreFloat4x4(&materialCB.meshTransform, XMMatrixTranspose(XMLoadFloat4x4(&mesh.transform)));
        materialCB.useVertexColor = m_model.hasVertexColor ? 1 : 0;
        materialCB.useTangent = m_model.hasTangent ? 1 : 0;

        // Find material
        const auto& material = m_model.materials[mesh.materialIndex];

        materialCB.metallicFactor = material.metallicFactor;
        materialCB.roughnessFactor = material.roughnessFactor;
        materialCB.albedoTextureIndex = (material.albedoTextureIndex >= 0) ? 1 : 0;
        materialCB.metallicTextureIndex = (material.metallicRoughnessTextureIndex >= 0) ? 1 : 0;
        materialCB.normalTextureIndex = (material.normalTextureIndex >= 0) ? 1 : 0;
        materialCB.baseColorFactor = material.baseColorFactor;

        XMMATRIX world = XMLoadFloat4x4(&mesh.transform);
        BoundingBox worldBox;
        mesh.boundingBox.Transform(worldBox, world);
        materialCB.centerBound = worldBox.Center;
        materialCB.extentsBound = worldBox.Extents;

        memcpy(static_cast<char*>(mappedData) + i * materialCBSize, &materialCB, sizeof(MaterialConstantBuffer));
    }
    m_materialCB->Unmap(0, nullptr);

    for (size_t i = 0; i < m_model.meshes.size(); ++i)
    {
        const auto& mesh = m_model.meshes[i];
        const auto& resource = m_meshResources[i];

        XMMATRIX world = XMLoadFloat4x4(&mesh.transform);

        // transform bounding box to world space
        BoundingBox worldBox;
        mesh.boundingBox.Transform(worldBox, world);

        if (frustum.Contains(worldBox) == DISJOINT)
        {
            continue;
        }

        cmdList->SetGraphicsRootDescriptorTable(1, resource.constantBufferView);

        // Set vertex and index buffers
        cmdList->IASetVertexBuffers(0, 1, &resource.vertexBufferView);
        cmdList->IASetIndexBuffer(&resource.indexBufferView);
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
    }

    return S_OK;
}

HRESULT Model::RenderBasePass(
    ID3D12Device* device, 
    ID3D12GraphicsCommandList* cmdList, 
    UINT srvBaseIndex,
    UINT cbvBaseIndex,
    ID3D12DescriptorHeap* srvcbvHeap,
    const BoundingFrustum& frustum)
{
    if (m_model.meshes.empty())
    {
        return E_FAIL;
    }
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle(srvcbvHeap->GetGPUDescriptorHandleForHeapStart());
    srvGpuHandle.Offset(srvBaseIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

    void* mappedData;
    UINT materialCBSize = (sizeof(MaterialConstantBuffer) + 255) & ~255;
    m_materialCB->Map(0, nullptr, &mappedData);
    for (size_t i = 0; i < m_model.meshes.size(); ++i)
    {
        const auto& mesh = m_model.meshes[i];
        const auto& resource = m_meshResources[i];

        MaterialConstantBuffer materialCB = {};
        // Transpose is for purpose row major(gltf) - column major(directX)
        XMStoreFloat4x4(&materialCB.meshTransform, XMMatrixTranspose(XMLoadFloat4x4(&mesh.transform)));
        materialCB.useVertexColor = m_model.hasVertexColor ? 1 : 0;
        materialCB.useTangent = m_model.hasTangent ? 1 : 0;

        // Find material
        const auto& material = m_model.materials[mesh.materialIndex];

        materialCB.metallicFactor = material.metallicFactor;
        materialCB.roughnessFactor = material.roughnessFactor;
        if (material.albedoTextureIndex >= 0)
        {
            TextureView& texView = m_model.textures[material.albedoTextureIndex];
            materialCB.albedoTextureIndex = texView.viewIndex;
        }
        else
            materialCB.albedoTextureIndex = -1;
        if (material.metallicRoughnessTextureIndex >= 0)
        {
            TextureView& texView = m_model.textures[material.metallicRoughnessTextureIndex];
            materialCB.metallicTextureIndex = texView.viewIndex;
        }
        else
            materialCB.metallicTextureIndex = -1;
        if (material.normalTextureIndex >= 0)
        {
            TextureView& texView = m_model.textures[material.normalTextureIndex];
            materialCB.normalTextureIndex = texView.viewIndex;
        }
        else
            materialCB.normalTextureIndex = -1;
        materialCB.baseColorFactor = material.baseColorFactor;

        XMMATRIX world = XMLoadFloat4x4(&mesh.transform);
        BoundingBox worldBox;
        mesh.boundingBox.Transform(worldBox, world);
        materialCB.centerBound = worldBox.Center;
        materialCB.extentsBound = worldBox.Extents;

        memcpy(static_cast<char*>(mappedData) + i * materialCBSize, &materialCB, sizeof(MaterialConstantBuffer));
    }
    m_materialCB->Unmap(0, nullptr);

    // srv for materials
    cmdList->SetGraphicsRootDescriptorTable(2, srvGpuHandle);

    // descriptor table
    if (!m_model.samplers.empty())
    {
        // take the first one for now
        auto& samplerData = m_model.samplers[0];
        cmdList->SetGraphicsRootDescriptorTable(3, samplerData.samplerGpuHandle);
    }
    
    for (size_t i = 0; i < m_model.meshes.size(); ++i)
    {
        const auto& mesh = m_model.meshes[i];
        const auto& resource = m_meshResources[i];

        XMMATRIX world = XMLoadFloat4x4(&mesh.transform);

        // transform bounding box to world space
        BoundingBox worldBox;
        mesh.boundingBox.Transform(worldBox, world);

        if (frustum.Contains(worldBox) == DISJOINT)
        {
            continue;
        }

        // Find material
        const auto& material = m_model.materials[mesh.materialIndex];
        cmdList->SetGraphicsRootDescriptorTable(1, resource.constantBufferView);

        // Set vertex and index buffers
        cmdList->IASetVertexBuffers(0, 1, &resource.vertexBufferView);
        cmdList->IASetIndexBuffer(&resource.indexBufferView);
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
    }

    return S_OK;
}