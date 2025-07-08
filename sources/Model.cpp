#include "Model.h"

using namespace DirectX;

// Tinygltf
#include <tiny_gltf.h>
#include "Utility.h"
#include "DX12.h"

enum ModelRootParams
{
    Model_GlobalSRV,
    Model_SceneCBuffer,
    Model_LightCBuffer,
    Model_ModelConstant,
    Model_ModelSBuffer,
    Model_MaxRootParams
};

void Model::Initialize()
{
    // Load shaders
    
    // Create Root Signature
}

void Model::Shutdown()
{
    m_pipelineState = nullptr;
    m_depthPipelineState = nullptr;

    m_rootSignature = nullptr;
    m_depthRootSignature = nullptr;

    for (size_t i = 0; i < m_model.meshes.size(); ++i)
    {
        //auto& mesh = m_model.meshes[i];
        auto& resource = m_meshResources[i];

        resource.vertexBuffer.Shutdown();
        resource.indexBuffer.Shutdown();
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

void Model::LoadShader()
{
    // Root signature (main pass)
    {
        D3D12_ROOT_PARAMETER1 rootParameters[Model_MaxRootParams] = {};

        // Global srv used to bind bindless texture
        rootParameters[Model_GlobalSRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[Model_GlobalSRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[Model_GlobalSRV].DescriptorTable.pDescriptorRanges = SRVDescriptorRanges();
        rootParameters[Model_GlobalSRV].DescriptorTable.NumDescriptorRanges = 1;

        // Scene + Light Constant Buffer
        rootParameters[Model_SceneCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[Model_SceneCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[Model_SceneCBuffer].Descriptor.RegisterSpace = 0;
        rootParameters[Model_SceneCBuffer].Descriptor.ShaderRegister = 0;
        rootParameters[Model_SceneCBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        rootParameters[Model_LightCBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[Model_LightCBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[Model_LightCBuffer].Descriptor.RegisterSpace = 0;
        rootParameters[Model_LightCBuffer].Descriptor.ShaderRegister = 1;
        rootParameters[Model_LightCBuffer].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        // Model Constant
        rootParameters[Model_ModelConstant].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParameters[Model_ModelConstant].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[Model_ModelConstant].Constants.Num32BitValues = 2;
        rootParameters[Model_ModelConstant].Constants.RegisterSpace = 0;
        rootParameters[Model_ModelConstant].Constants.ShaderRegister = 2;

        // Structured buffer
        D3D12_DESCRIPTOR_RANGE1 srvDescriptorRange = {};

        // Descriptor range
        srvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvDescriptorRange.NumDescriptors = 2;
        srvDescriptorRange.BaseShaderRegister = 0;
        srvDescriptorRange.RegisterSpace = 1;
        srvDescriptorRange.OffsetInDescriptorsFromTableStart = 0;
        srvDescriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

        rootParameters[Model_ModelSBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[Model_ModelSBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[Model_ModelSBuffer].DescriptorTable.pDescriptorRanges = &srvDescriptorRange;
        rootParameters[Model_ModelSBuffer].DescriptorTable.NumDescriptorRanges = 1;

        // Static sampler
        D3D12_STATIC_SAMPLER_DESC staticSampler[1] = {};
        staticSampler[0] = GetStaticSamplerState(SamplerState::Linear, 0, 0);

        // Root pipeline
        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = _countof(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = _countof(staticSampler);
        rootSignatureDesc.pStaticSamplers = staticSampler;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc = {};
        versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        versionedDesc.Desc_1_1 = rootSignatureDesc;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&versionedDesc, &signature, &error);
        if (FAILED(hr))
        {
            std::string errorMsg = std::string(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize());
            assert(false && errorMsg.c_str());
        }
        CheckHRESULT(d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }
 
    // Root Signature (depth pre-pass)
    {
        D3D12_ROOT_PARAMETER1 rootParameters[3] = {};

        // Scene Constant Buffer
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[0].Descriptor.RegisterSpace = 0;
        rootParameters[0].Descriptor.ShaderRegister = 0;
        rootParameters[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        // Model Constant
        rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[1].Constants.Num32BitValues = 2;
        rootParameters[1].Constants.RegisterSpace = 0;
        rootParameters[1].Constants.ShaderRegister = 2;

        // Structured buffer
        D3D12_DESCRIPTOR_RANGE1 srvDescriptorRange = {};

        // Descriptor range
        srvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvDescriptorRange.NumDescriptors = 1;
        srvDescriptorRange.BaseShaderRegister = 0;
        srvDescriptorRange.RegisterSpace = 1;
        srvDescriptorRange.OffsetInDescriptorsFromTableStart = 0;
        srvDescriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

        rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[2].DescriptorTable.pDescriptorRanges = &srvDescriptorRange;
        rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

        // Root pipeline
        D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
        rootSignatureDesc.NumParameters = _countof(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc = {};
        versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        versionedDesc.Desc_1_1 = rootSignatureDesc;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&versionedDesc, &signature, &error);
        if (FAILED(hr))
        {
            std::string errorMsg = std::string(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize());
            assert(false && errorMsg.c_str());
        }
        CheckHRESULT(d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_depthRootSignature)));
    }
}

void Model::CreatePSO()
{
    D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.FrontCounterClockwise = TRUE; // Matches glTF CCW convention

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // Main pass PSO
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = { m_vertexShader->GetBufferPointer(), m_vertexShader->GetBufferSize() };
        psoDesc.PS = { m_pixelShader->GetBufferPointer(), m_pixelShader->GetBufferSize() };
        psoDesc.RasterizerState = rasterizerDesc; // CD3DX12_RASTERIZER_DESC{ D3D12_DEFAULT
        psoDesc.BlendState = CD3DX12_BLEND_DESC{ D3D12_DEFAULT };
        psoDesc.DepthStencilState = depthStencilDesc;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };
        CheckHRESULT(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }

    // Depth only PSO
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_depthRootSignature.Get();
        psoDesc.VS = { m_depthVertexShader->GetBufferPointer(), m_vertexShader->GetBufferSize() };
        psoDesc.RasterizerState = rasterizerDesc; // CD3DX12_RASTERIZER_DESC{ D3D12_DEFAULT
        psoDesc.BlendState = CD3DX12_BLEND_DESC{ D3D12_DEFAULT };
        psoDesc.DepthStencilState = depthStencilDesc;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 0;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };
        CheckHRESULT(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_depthPipelineState)));
    }
}

HRESULT Model::UploadGpuResources(
    ID3D12Device* device,
    UINT sbBaseIndex,
    UINT texBaseIndex,    
    ID3D12DescriptorHeap* srvcbvHeap,    
    ID3D12GraphicsCommandList* cmdList)
{
    if (m_model.meshes.empty())
    {
        return E_FAIL;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuHandle(srvcbvHeap->GetCPUDescriptorHandleForHeapStart());    
    
    // Advance heap offset based on based index before used
    /*CD3DX12_CPU_DESCRIPTOR_HANDLE sbCpuHandle(srvcbvHeap->GetCPUDescriptorHandleForHeapStart());
    sbCpuHandle.Offset(sbBaseIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    CD3DX12_CPU_DESCRIPTOR_HANDLE texCpuHandle(srvcbvHeap->GetCPUDescriptorHandleForHeapStart());
    texCpuHandle.Offset(texBaseIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));*/

    m_meshResources.resize(m_model.meshes.size());

    for (size_t i = 0; i < m_model.meshes.size(); ++i)
    {
        auto& mesh = m_model.meshes[i];
        auto& resource = m_meshResources[i];

        // Create vertex buffer
        StructuredBufferInit sbi;
        sbi.stride = sizeof(VertexData);
        sbi.numElements = mesh.vertices.size();
        sbi.initData = mesh.vertices.data();
        sbi.initState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        sbi.name = L"ModelVertexBuffer";

        resource.vertexBuffer.Initialize(sbi);
        
        // Create index buffer
        FormattedBufferInit fbi;
        fbi.format = DXGI_FORMAT_R32_UINT;
        fbi.bitSize = 32;
        fbi.numElements = mesh.indices.size();
        fbi.initData = mesh.indices.data();
        fbi.initState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
        fbi.name = L"ModelIndexBuffer";

        resource.indexBuffer.Initialize(fbi);
    }

    // Create mesh structured buffer
    std::vector<MeshStructuredBuffer> meshes;
    {
        for (const MeshData& mesh : m_model.meshes)
        {
            MeshStructuredBuffer meshSB;
            DirectX::BoundingBox boundingBox;

            // Transpose is for purpose row major(gltf) - column major(directX)
            XMStoreFloat4x4(&meshSB.meshTransform, XMMatrixTranspose(XMLoadFloat4x4(&mesh.transform)));

            // Transform bounding box to world space
            XMMATRIX world = XMLoadFloat4x4(&mesh.transform);
            BoundingBox worldBox;
            mesh.boundingBox.Transform(worldBox, world);

            meshSB.centerBound = worldBox.Center;
            meshSB.extentsBound = worldBox.Extents;
            meshes.push_back(std::move(meshSB));
        }

        /*StructuredBufferInit sbi;
        sbi.stride = sizeof(MeshStructuredBuffer);
        sbi.numElements = meshes.size();
        sbi.initData = meshes.data();
        sbi.initState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        sbi.name = L"MeshStructuredBuffer";

        m_meshSB.Initialize(sbi);*/

        UINT bufferSize = sizeof(MeshStructuredBuffer) * meshes.size();
        D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_NONE);
        CheckHRESULT(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_meshSB)));

        // Upload buffer
        CheckHRESULT(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_meshUploadSB)));

        // Copy material data to upload buffer
        void* mappedData;
        m_meshUploadSB->Map(0, nullptr, &mappedData);
        memcpy(mappedData, meshes.data(), bufferSize);
        m_meshUploadSB->Unmap(0, nullptr);

        // Copy from upload buffer to structured buffer
        cmdList->CopyResource(m_meshSB.Get(), m_meshUploadSB.Get());
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_meshSB.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        // Srv
        /*D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = meshes.size();
        srvDesc.Buffer.StructureByteStride = sizeof(MeshStructuredBuffer);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        device->CreateShaderResourceView(m_meshSB.Get(), &srvDesc, sbCpuHandle);
        sbCpuHandle.Offset(1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));*/
    }
    
    // Create texture
    if (!m_model.images.empty())
    {
        // Texture resources
        for (TextureResource& texResource : m_model.images)
        {
            /*TextureInit ti;
            ti.width = texResource.width;
            ti.height = texResource.height;
            ti.initData = texResource.pixels.data();
            
            texResource.texture.Initialize(ti);*/
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
        }

        // Texture views (load from Materials)
        UINT viewIndex = 0;
        for (TextureView& texView : m_model.textures)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            texView.viewIndex = viewIndex;

            TextureResource& texResource = m_model.images[texView.resourceIndex];
            //texView.viewIndex = texResource.texture.SRV;
            device->CreateShaderResourceView(texResource.texture.Get(), &srvDesc, srvCpuHandle);
            srvCpuHandle.Offset(1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
            viewIndex++;
        }
    }

    // Create material structured buffer
    std::vector<MaterialStructuredBuffer> materials;
    {
        // Repeated information on material
        const bool useVertexColor = m_model.hasVertexColor;
        const bool useTangent = m_model.hasTangent;

        for (const MaterialData& material : m_model.materials)
        {
            MaterialStructuredBuffer matSB;

            matSB.useVertexColor = useVertexColor;
            matSB.useTangent = useTangent;
            matSB.metallicFactor = material.metallicFactor;
            matSB.roughnessFactor = material.roughnessFactor;

            matSB.albedoTextureIndex = (material.albedoTextureIndex >= 0) ? m_model.textures[material.albedoTextureIndex].viewIndex : -1;
            matSB.metallicTextureIndex = (material.metallicRoughnessTextureIndex >= 0) ? m_model.textures[material.metallicRoughnessTextureIndex].viewIndex : -1;
            matSB.normalTextureIndex = (material.normalTextureIndex >= 0) ? m_model.textures[material.normalTextureIndex].viewIndex : -1;

            matSB.baseColorFactor = material.baseColorFactor;

            materials.push_back(std::move(matSB));
        }

        /*StructuredBufferInit sbi;
        sbi.stride = sizeof(MaterialStructuredBuffer);
        sbi.numElements = materials.size();
        sbi.initData = materials.data();
        sbi.initState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        sbi.name = L"MaterialStructuredBuffer";

        m_materialSB.Initialize(sbi);*/

        UINT bufferSize = sizeof(MaterialStructuredBuffer) * m_model.materials.size();
        D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_NONE);
        CheckHRESULT(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_materialSB)));

        // Upload buffer
        CheckHRESULT(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_materialUploadSB)));

        // Copy material data to upload buffer
        void* mappedData;
        m_materialUploadSB->Map(0, nullptr, &mappedData);
        memcpy(mappedData, materials.data(), bufferSize);
        m_materialUploadSB->Unmap(0, nullptr);

        // Copy from upload buffer to structured buffer
        cmdList->CopyResource(m_materialSB.Get(), m_materialUploadSB.Get());
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_materialSB.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        // Srv
       /* D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = materials.size();
        srvDesc.Buffer.StructureByteStride = sizeof(MaterialStructuredBuffer);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        device->CreateShaderResourceView(m_materialSB.Get(), &srvDesc, sbCpuHandle);
        sbCpuHandle.Offset(1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));*/
    }

    // Resource binding (order important)
    
    // t0 - bindless texture, space1
    //    
    // t0 - mesh SB, space0
    // t1 - material SB
    // Mesh SB
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = meshes.size();
    srvDesc.Buffer.StructureByteStride = sizeof(MeshStructuredBuffer);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    device->CreateShaderResourceView(m_meshSB.Get(), &srvDesc, srvCpuHandle);
    srvCpuHandle.Offset(1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

    // Material SB
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = materials.size();
    srvDesc.Buffer.StructureByteStride = sizeof(MaterialStructuredBuffer);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    device->CreateShaderResourceView(m_materialSB.Get(), &srvDesc, srvCpuHandle);
    srvCpuHandle.Offset(1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

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

        // Move to static sampler
        /*samplerData.samplerCpuHandle = samplerHeap->GetCPUDescriptorHandleForHeapStart();
        samplerData.samplerGpuHandle = samplerHeap->GetGPUDescriptorHandleForHeapStart();
        device->CreateSampler(&samplerDesc, samplerData.samplerCpuHandle);*/
    }
    return S_OK;
}

HRESULT Model::RenderDepthOnly(
    ID3D12Device* device, 
    ID3D12GraphicsCommandList* cmdList,
    UINT sbBaseIndex,
    ID3D12DescriptorHeap* srvcbvHeap,
    const BoundingFrustum& frustum)
{
    if (m_model.meshes.empty())
    {
        return E_FAIL;
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle(srvcbvHeap->GetGPUDescriptorHandleForHeapStart());
    CD3DX12_GPU_DESCRIPTOR_HANDLE sbGpuHandle(srvcbvHeap->GetGPUDescriptorHandleForHeapStart());
    sbGpuHandle.Offset(m_model.textures.size(), device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    //CD3DX12_GPU_DESCRIPTOR_HANDLE sbGpuHandle(srvcbvHeap->GetGPUDescriptorHandleForHeapStart());
    

    // srv for material non texture (structured buffer)
    //cmdList->SetGraphicsRootDescriptorTable(3, sbGpuHandle);
    cmdList->SetGraphicsRootDescriptorTable(3, srvGpuHandle);   // t0, space1
    cmdList->SetGraphicsRootDescriptorTable(4, sbGpuHandle);
    //m_meshSB.SetAsGfxRootParameter(cmdList, 3);

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

        // Constant
        ModelConstants constant = { static_cast<UINT>(i), static_cast<UINT>(mesh.materialIndex) };
        cmdList->SetGraphicsRoot32BitConstants(2, 2, &constant, 0);

        // Set vertex and index buffers
        cmdList->IASetVertexBuffers(0, 1, &resource.vertexBuffer.VBView());
        cmdList->IASetIndexBuffer(&resource.indexBuffer.IBView());
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
    }

    return S_OK;
}

HRESULT Model::RenderBasePass(
    ID3D12Device* device, 
    ID3D12GraphicsCommandList* cmdList, 
    UINT sbBaseIndex,
    UINT texBaseIndex,
    ID3D12DescriptorHeap* srvcbvHeap,
    const BoundingFrustum& frustum)
{
    if (m_model.meshes.empty())
    {
        return E_FAIL;
    }
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle(srvcbvHeap->GetGPUDescriptorHandleForHeapStart());
    CD3DX12_GPU_DESCRIPTOR_HANDLE sbGpuHandle(srvcbvHeap->GetGPUDescriptorHandleForHeapStart());
    sbGpuHandle.Offset(m_model.textures.size(), device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    //sbGpuHandle.Offset(sbBaseIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    //CD3DX12_GPU_DESCRIPTOR_HANDLE sbGpuHandle(srvcbvHeap->GetGPUDescriptorHandleForHeapStart());
    //sbGpuHandle.Offset(sbBaseIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    //CD3DX12_GPU_DESCRIPTOR_HANDLE texGpuHandle(srvcbvHeap->GetGPUDescriptorHandleForHeapStart());
    //texGpuHandle.Offset(texBaseIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

    // srv for material non texture (structured buffer)
    //m_meshSB.SetAsGfxRootParameter(cmdList, 3);
    cmdList->SetGraphicsRootDescriptorTable(3, srvGpuHandle);   // t0, space1
    cmdList->SetGraphicsRootDescriptorTable(4, sbGpuHandle);

    //cmdList->SetGraphicsRootDescriptorTable(3, sbGpuHandle);
    // srv for materials texture
    //cmdList->SetGraphicsRootDescriptorTable(4, texGpuHandle);

    // descriptor table
    if (!m_model.samplers.empty())
    {
        // take the first one for now
        /*auto& samplerData = m_model.samplers[0];
        cmdList->SetGraphicsRootDescriptorTable(3, samplerData.samplerGpuHandle);*/
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

        // Constant
        ModelConstants constant = {static_cast<UINT>(i), static_cast<UINT>(mesh.materialIndex)};
        cmdList->SetGraphicsRoot32BitConstants(2, 2, &constant, 0);

        // Set vertex and index buffers
        cmdList->IASetVertexBuffers(0, 1, &resource.vertexBuffer.VBView());
        cmdList->IASetIndexBuffer(&resource.indexBuffer.IBView());
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
    }

    return S_OK;
}