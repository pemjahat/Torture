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
    depthPSO = nullptr;
    depthAlphaPSO = nullptr;
    alphaTestPipelineState = nullptr;

    m_rootSignature = nullptr;
    depthRootSignature = nullptr;

    meshResource.vertexBuffer.Shutdown();
    meshResource.indexBuffer.Shutdown();
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
    // Only store transform if nodes has meshes, otherwise just pass through
    if (node.mesh >= 0)
    {
        NodeData nodeData;
        nodeData.meshIndex = node.mesh;
        nodeData.transform = nodeTransform;

        modelData.numInstances += model.meshes[node.mesh].primitives.size();
        modelData.nodes.push_back(std::move(nodeData));
    }

    // Recursive process children
    for (int childIndex : node.children)
    {
        ProcessNode(model, childIndex, nodeTransform, modelData);
    }
}

void ProcessMesh(const tinygltf::Model& model, ModelData& modelData)
{
    for (auto& mesh : model.meshes)
    {
        MeshData meshData;
        for (const auto& primitive : mesh.primitives)
        {
            PrimitiveData primitiveData;
            // Get Position
            if (primitive.attributes.find("POSITION") != primitive.attributes.end())
            {
                const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
                const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];

                const unsigned char* bufferData = &posBuffer.data[posView.byteOffset + posAccessor.byteOffset];
                size_t byteStride = posView.byteStride ? posView.byteStride : sizeof(float) * 3;    // Handle interleaved or non-interleaved
                primitiveData.vertices.resize(posAccessor.count);
                for (size_t i = 0; i < posAccessor.count; ++i)
                {
                    const float* postData = reinterpret_cast<const float*>(bufferData + i * byteStride);
                    primitiveData.vertices[i].Position = XMFLOAT3(postData[0], postData[1], postData[2]);
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

                BoundingBox::CreateFromPoints(primitiveData.boundingBox, XMLoadFloat3(&min), XMLoadFloat3(&max));
            }

            // Get Normal
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
            {
                const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.at("NORMAL")];
                const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
                const tinygltf::Buffer& normBuffer = model.buffers[normView.buffer];

                const unsigned char* bufferData = &normBuffer.data[normView.byteOffset + normAccessor.byteOffset];
                size_t byteStride = normView.byteStride ? normView.byteStride : sizeof(float) * 3;

                assert(normAccessor.count == primitiveData.vertices.size());
                for (size_t i = 0; i < normAccessor.count; ++i)
                {
                    const float* normData = reinterpret_cast<const float*>(bufferData + i * byteStride);
                    primitiveData.vertices[i].Normal = XMFLOAT3(normData[0], normData[1], normData[2]);
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

                assert(uvAccessor.count == primitiveData.vertices.size());
                for (size_t i = 0; i < uvAccessor.count; ++i)
                {
                    const float* uvData = reinterpret_cast<const float*>(bufferData + i * byteStride);
                    primitiveData.vertices[i].Uv = XMFLOAT2(uvData[0], uvData[1]);
                }
            }
            // Get tangent
            if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
            {
                primitiveData.hasTangent = true;

                const tinygltf::Accessor& tangentAccessor = model.accessors[primitive.attributes.at("TANGENT")];
                const tinygltf::BufferView& tangentView = model.bufferViews[tangentAccessor.bufferView];
                const tinygltf::Buffer& tangentBuffer = model.buffers[tangentView.buffer];

                const unsigned char* bufferData = &tangentBuffer.data[tangentView.byteOffset + tangentAccessor.byteOffset];
                size_t byteStride = tangentView.byteStride ? tangentView.byteStride : sizeof(float) * 4;

                assert(tangentAccessor.count == primitiveData.vertices.size());
                for (size_t i = 0; i < tangentAccessor.count; ++i)
                {
                    const float* tangentData = reinterpret_cast<const float*>(bufferData + i * byteStride);
                    primitiveData.vertices[i].Tangent = XMFLOAT4(tangentData[0], tangentData[1], tangentData[2], tangentData[3]);
                }
            }

            // Get vertex color
            if (primitive.attributes.find("COLOR_0") != primitive.attributes.end())
            {
                primitiveData.hasVertexColor = true;

                const tinygltf::Accessor& colorAccessor = model.accessors[primitive.attributes.at("COLOR_0")];
                const tinygltf::BufferView& colorView = model.bufferViews[colorAccessor.bufferView];
                const tinygltf::Buffer& colorBuffer = model.buffers[colorView.buffer];

                const unsigned char* bufferData = &colorBuffer.data[colorView.byteOffset + colorAccessor.byteOffset];
                size_t byteStride = colorView.byteStride ? colorView.byteStride : sizeof(float) * 4;

                assert(colorAccessor.count == primitiveData.vertices.size());
                for (size_t i = 0; i < colorAccessor.count; ++i)
                {
                    const float* colorData = reinterpret_cast<const float*>(bufferData + i * byteStride);
                    primitiveData.vertices[i].Color = XMFLOAT4(colorData[0], colorData[1], colorData[2], colorData[3]);
                }
            }

            // Get indices
            if (primitive.indices >= 0)
            {
                const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];

                const unsigned char* bufferData = &indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset];
                primitiveData.indices.resize(indexAccessor.count);

                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    const uint16_t* indexData = reinterpret_cast<const uint16_t*>(bufferData);
                    for (size_t i = 0; i < indexAccessor.count; ++i) {
                        primitiveData.indices[i] = static_cast<uint32_t>(indexData[i]);
                    }
                }
                else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    const uint32_t* indexData = reinterpret_cast<const uint32_t*>(bufferData);
                    for (size_t i = 0; i < indexAccessor.count; ++i) {
                        primitiveData.indices[i] = indexData[i];
                    }
                }
            }

            // Get material index
            modelData.numPrimitives += 1;
            primitiveData.materialIndex = primitive.material;
            meshData.primitives.push_back(std::move(primitiveData));
        }
        modelData.meshes.push_back(std::move(meshData));
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
        material.alphaCutoff = (mat.alphaMode.compare("OPAQUE") == 0) ? 1.f : mat.alphaCutoff;
        material.albedoTextureIndex = pbr.baseColorTexture.index;
        material.metallicTextureIndex = pbr.metallicRoughnessTexture.index;
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
    m_model.numPrimitives = 0;
    m_model.numInstances = 0;
    m_model.nodes.clear();
    m_model.meshes.clear();
    //m_model.textures.clear();

    // start with scene root nodes
    const auto& scene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
    XMMATRIX identity = XMMatrixIdentity();
    for (int nodeIndex : scene.nodes)
    {
        ProcessNode(model, nodeIndex, identity, m_model);
    }

    ProcessMesh(model, m_model);
    ProcessMaterial(model, m_model);

	return S_OK;
}

void Model::LoadShader(const std::filesystem::path& shaderPath )
{
    // Load shaders
    std::filesystem::path mainShader = shaderPath / "basic_color.hlsl";
    std::filesystem::path depthShader = shaderPath / "basic.hlsl";
    std::filesystem::path gbufferShader = shaderPath / "gbuffer.hlsl";

    /*CompileShaderFromFile(
        std::filesystem::absolute(mainShader).wstring(),
        std::filesystem::absolute(shaderPath).wstring(),
        L"VSMain",
        m_vertexShader,
        ShaderType::Vertex);*/

    // Depth prepass
    CompileShaderFromFile(
        std::filesystem::absolute(depthShader).wstring(),
        std::filesystem::absolute(shaderPath).wstring(),
        L"VSMain",
        depthVS,
        ShaderType::Vertex);
    CompileShaderFromFile(
        std::filesystem::absolute(depthShader).wstring(),
        std::filesystem::absolute(shaderPath).wstring(),
        L"PSMain",
        depthAlphaPS,
        ShaderType::Pixel);

    // GBuffer
    /*CompileShaderFromFile(
        std::filesystem::absolute(gbufferShader).wstring(),
        std::filesystem::absolute(shaderPath).wstring(),
        L"VSMain",
        gbufferVS,
        ShaderType::Vertex);
    CompileShaderFromFile(
        std::filesystem::absolute(gbufferShader).wstring(),
        std::filesystem::absolute(shaderPath).wstring(),
        L"PSGBuffer",
        gbufferPS,
        ShaderType::Pixel);
    CompileShaderFromFile(
        std::filesystem::absolute(gbufferShader).wstring(),
        std::filesystem::absolute(shaderPath).wstring(),
        L"PSAlphaTest",
        alphaTestPS,
        ShaderType::Pixel);*/

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
        D3D12_DESCRIPTOR_RANGE1 srvDescriptorRange[1] = {};

        // Descriptor range
        srvDescriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvDescriptorRange[0].NumDescriptors = 2;
        srvDescriptorRange[0].BaseShaderRegister = 0;
        srvDescriptorRange[0].RegisterSpace = 0;
        srvDescriptorRange[0].OffsetInDescriptorsFromTableStart = 0;
        srvDescriptorRange[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

        rootParameters[Model_ModelSBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[Model_ModelSBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[Model_ModelSBuffer].DescriptorTable.pDescriptorRanges = srvDescriptorRange;
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

        //CreateRootSignature(m_rootSignature, rootSignatureDesc);
    }
 
    // Root Signature (depth pre-pass)
    {
        D3D12_ROOT_PARAMETER1 rootParameters[5] = {};

        // Bindless texture
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[0].DescriptorTable.pDescriptorRanges = SRVDescriptorRanges();
        rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

        // Scene Constant Buffer
        rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[1].Descriptor.RegisterSpace = 0;
        rootParameters[1].Descriptor.ShaderRegister = 0;
        rootParameters[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        // Model Constant
        rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[2].Constants.Num32BitValues = 2;
        rootParameters[2].Constants.RegisterSpace = 0;
        rootParameters[2].Constants.ShaderRegister = 2;

        // Mesh + Material structured buffer
        rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[3].Descriptor.RegisterSpace = 0;
        rootParameters[3].Descriptor.ShaderRegister = 0;
        rootParameters[3].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

        rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[4].Descriptor.RegisterSpace = 0;
        rootParameters[4].Descriptor.ShaderRegister = 1;
        rootParameters[4].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;

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

        CreateRootSignature(depthRootSignature, rootSignatureDesc);
    }

    // GBuffer root signature
    {
        D3D12_ROOT_PARAMETER1 rootParameters[4] = {};

        // Global srv used to bind bindless texture
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[0].DescriptorTable.pDescriptorRanges = SRVDescriptorRanges();
        rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

        // Scene Constant
        rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[1].Descriptor.RegisterSpace = 0;
        rootParameters[1].Descriptor.ShaderRegister = 0;
        rootParameters[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
        
        // Model Constant
        rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[2].Constants.Num32BitValues = 2;
        rootParameters[2].Constants.RegisterSpace = 0;
        rootParameters[2].Constants.ShaderRegister = 1;

        // Structured buffer
        D3D12_DESCRIPTOR_RANGE1 srvDescriptorRange[1] = {};

        // Descriptor range
        srvDescriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvDescriptorRange[0].NumDescriptors = 2;
        srvDescriptorRange[0].BaseShaderRegister = 0;
        srvDescriptorRange[0].RegisterSpace = 0;
        srvDescriptorRange[0].OffsetInDescriptorsFromTableStart = 0;
        srvDescriptorRange[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

        rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[3].DescriptorTable.pDescriptorRanges = srvDescriptorRange;
        rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;

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

        //CreateRootSignature(gbufferRootSignature, rootSignatureDesc);
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

    // Main pass PSO
    {
        

        /*D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = { m_vertexShader->GetBufferPointer(), m_vertexShader->GetBufferSize() };
        psoDesc.PS = { alphaTestPS->GetBufferPointer(), alphaTestPS->GetBufferSize() };
        psoDesc.RasterizerState = GetRasterizerState(RasterizerState::BackfaceCull);
        psoDesc.BlendState = CD3DX12_BLEND_DESC{ D3D12_DEFAULT };
        psoDesc.DepthStencilState = GetDepthStencilState(DepthStencilState::WriteEnabled);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };
        CheckHRESULT(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&alphaTestPipelineState)));*/
    }

    // Depth only PSO
    {
        /*D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };*/

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = depthRootSignature.Get();
        psoDesc.VS = { depthVS->GetBufferPointer(), depthVS->GetBufferSize() };
        psoDesc.RasterizerState = GetRasterizerState(RasterizerState::BackfaceCull);
        psoDesc.BlendState = CD3DX12_BLEND_DESC{ D3D12_DEFAULT };
        psoDesc.DepthStencilState = GetDepthStencilState(DepthStencilState::WriteEnabled);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 0;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };
        CheckHRESULT(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&depthPSO)));

        psoDesc.PS = { depthAlphaPS->GetBufferPointer(), depthAlphaPS->GetBufferSize() };
        CheckHRESULT(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&depthAlphaPSO)));
    }

    // GBuffer pso
    {
        D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
        };

        /*D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = gbufferRootSignature.Get();
        psoDesc.VS = { gbufferVS->GetBufferPointer(), gbufferVS->GetBufferSize() };
        psoDesc.PS = { gbufferPS->GetBufferPointer(), gbufferPS->GetBufferSize() };
        psoDesc.RasterizerState = GetRasterizerState(RasterizerState::BackfaceCull);
        psoDesc.BlendState = CD3DX12_BLEND_DESC{ D3D12_DEFAULT };
        psoDesc.DepthStencilState = GetDepthStencilState(DepthStencilState::WriteEnabled);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 3;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.RTVFormats[1] = DXGI_FORMAT_R10G10B10A2_UNORM;
        psoDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };*/
        //CheckHRESULT(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&gbufferPipelineState)));

        //psoDesc.PS = { alphaTestPS->GetBufferPointer(), alphaTestPS->GetBufferSize() };
        //CheckHRESULT(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&alphaTestPipelineState)));
    }
}

HRESULT Model::UploadGpuResources()
{
    if (m_model.meshes.empty())
    {
        return E_FAIL;
    }

    uint64_t numVertices = 0;
    uint64_t numIndices = 0;

    for (const auto& mesh : m_model.meshes)
    {
        for (const auto& primitive : mesh.primitives)
        {
            numVertices += primitive.vertices.size();
            numIndices += primitive.indices.size();
        }
    }

    // Create mesh resources
    meshResource.vertices.resize(numVertices);
    meshResource.indices.resize(numIndices);
    uint64_t vtxOffset = 0;
    uint64_t idxOffset = 0;

    for (auto& mesh : m_model.meshes)
    {
        for (auto& primitive : mesh.primitives)
        {
            // Copy vertex data (byte offset)
            memcpy(meshResource.vertices.data() + vtxOffset, primitive.vertices.data(), primitive.vertices.size() * sizeof(MeshVertex));

            // Copy index data (byte offset)
            memcpy(meshResource.indices.data() + idxOffset, primitive.indices.data(), primitive.indices.size() * sizeof(uint32_t));

            // Copy the offset
            primitive.vertexOffset = vtxOffset;
            primitive.indexOffset = idxOffset;

            // Advance it
            vtxOffset += primitive.vertices.size();
            idxOffset += primitive.indices.size();
        }
    }

    StructuredBufferInit sbi;
    sbi.stride = sizeof(MeshVertex);
    sbi.numElements = numVertices;
    sbi.initData = meshResource.vertices.data();
    sbi.name = L"ModelVertexBuffer";
    meshResource.vertexBuffer.Initialize(sbi);

    FormattedBufferInit fbi;
    fbi.format = DXGI_FORMAT_R32_UINT;
    fbi.bitSize = 32;
    fbi.numElements = numIndices;
    fbi.initData = meshResource.indices.data();
    fbi.name = L"ModelIndexBuffer";
    meshResource.indexBuffer.Initialize(fbi);

    // Create texture
    if (!m_model.images.empty())
    {
        // Texture resources
        for (TextureResource& texResource : m_model.images)
        {
            TextureInit ti;
            ti.width = texResource.width;
            ti.height = texResource.height;
            ti.initData = texResource.pixels.data();
            
            texResource.texture.Initialize(ti);
        }

        // Texture views (load from Materials)
        for (TextureView& texView : m_model.textures)
        {
            TextureResource& texResource = m_model.images[texView.resourceIndex];
            texView.viewIndex = texResource.texture.SRV;
        }
    }

    // Create mesh structured buffer
    std::vector<MeshStructuredBuffer> meshes;
    {
        for (const NodeData& node : m_model.nodes)
        {
            const MeshData& mesh = m_model.meshes[node.meshIndex];
            for (const PrimitiveData& primitive : mesh.primitives)
            {
                MeshStructuredBuffer meshSB;
                DirectX::BoundingBox boundingBox;

                // Transform bounding box to world space
                BoundingBox worldBox;
                primitive.boundingBox.Transform(worldBox, node.transform);

                meshSB.meshTransform = node.transform;
                meshSB.centerBound = worldBox.Center;
                meshSB.extentsBound = worldBox.Extents;
                meshSB.useVertexColor = primitive.hasVertexColor ? 1 : 0;
                meshSB.useTangent = primitive.hasTangent ? 1 : 0;
                meshes.push_back(std::move(meshSB));
            }
        }

        StructuredBufferInit sbi;
        sbi.stride = sizeof(MeshStructuredBuffer);
        sbi.numElements = meshes.size();
        sbi.initData = meshes.data();
        sbi.name = L"MeshStructuredBuffer";

        meshSB.Initialize(sbi);
    }

    // Create material structured buffer
    {
        // Repeated information on material
        for (MaterialData& mat : m_model.materials)
        {
            mat.albedoViewTextureIndex = (mat.albedoTextureIndex >= 0) ? m_model.textures[mat.albedoTextureIndex].viewIndex : -1;
            mat.metallicViewTextureIndex = (mat.metallicTextureIndex >= 0) ? m_model.textures[mat.metallicTextureIndex].viewIndex : -1;
            mat.normalViewTextureIndex = (mat.normalTextureIndex >= 0) ? m_model.textures[mat.normalTextureIndex].viewIndex : -1;            
        }

        StructuredBufferInit sbi;
        sbi.stride = sizeof(MaterialData);
        sbi.numElements = m_model.materials.size();
        sbi.initData = m_model.materials.data();
        sbi.name = L"MaterialStructuredBuffer";

        materialSB.Initialize(sbi);
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

        // Move to static sampler
        /*samplerData.samplerCpuHandle = samplerHeap->GetCPUDescriptorHandleForHeapStart();
        samplerData.samplerGpuHandle = samplerHeap->GetGPUDescriptorHandleForHeapStart();
        device->CreateSampler(&samplerDesc, samplerData.samplerCpuHandle);*/
    }
    return S_OK;
}

void Model::BuildAccelerationStructure()
{
    // Blas generate from Primitives
    // Tlas generate from Instances
    uint32_t numPrimitives = m_model.numPrimitives;
    uint32_t numInstances = m_model.numInstances;
    uint64_t ResultDataMaxSizeInBytes = 0;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs(numPrimitives);
    uint32_t primitiveIdx = 0;
    for (const MeshData& mesh : m_model.meshes)
    {
        for (const PrimitiveData& primitive : mesh.primitives)
        {
            const MaterialData& material = m_model.materials[primitive.materialIndex];
            const bool nonOpaque = (material.alphaCutoff < 1.f) ? true : false;

            D3D12_RAYTRACING_GEOMETRY_DESC& geomDesc = geometryDescs[primitiveIdx];
            geomDesc = {};
            geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            geomDesc.Triangles.IndexBuffer = meshResource.indexBuffer.internalBuffer.gpuAddress + primitive.indexOffset * meshResource.indexBuffer.Stride;
            geomDesc.Triangles.IndexCount = primitive.indices.size();
            geomDesc.Triangles.IndexFormat = meshResource.indexBuffer.format;
            geomDesc.Triangles.Transform3x4 = 0;
            geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            geomDesc.Triangles.VertexCount = primitive.vertices.size();
            geomDesc.Triangles.VertexBuffer.StartAddress = meshResource.vertexBuffer.internalBuffer.gpuAddress + primitive.vertexOffset * meshResource.vertexBuffer.Stride;
            geomDesc.Triangles.VertexBuffer.StrideInBytes = meshResource.vertexBuffer.Stride;
            geomDesc.Flags = nonOpaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_NONE : D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
            
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input = {};
            input.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            input.Flags = buildFlags;
            input.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
            input.pGeometryDescs = &geomDesc;
            input.NumDescs = 1;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
            d3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&input, &prebuildInfo);
            assert(prebuildInfo.ResultDataMaxSizeInBytes > 0);
            ResultDataMaxSizeInBytes = std::max(ResultDataMaxSizeInBytes, prebuildInfo.ResultDataMaxSizeInBytes);
            primitiveIdx++;
        }
    }

    // Blas scratch buffer
    {
        RawBufferInit rbi;
        rbi.numElements = ResultDataMaxSizeInBytes / RawBuffer::Stride;
        rbi.allowUAV = true;
        rbi.initState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        rbi.name = L"RT Blas ScratchBuffer";
        meshResource.blasScratchBuffer.Initialize(rbi);
    }

    // Build blas
    primitiveIdx = 0;
    for (MeshData& mesh : m_model.meshes)
    {
        for (PrimitiveData& primitive : mesh.primitives)
        {
            D3D12_RAYTRACING_GEOMETRY_DESC& geomDesc = geometryDescs[primitiveIdx];

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input = {};
            input.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            input.Flags = buildFlags;
            input.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
            input.pGeometryDescs = &geomDesc;
            input.NumDescs = 1;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
            d3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&input, &prebuildInfo);
            assert(prebuildInfo.ResultDataMaxSizeInBytes > 0);

            RawBufferInit rbi;
            rbi.numElements = prebuildInfo.ResultDataMaxSizeInBytes / RawBuffer::Stride;
            rbi.allowUAV = true;
            rbi.initState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            rbi.name = L"RT Bot Level Acceleration Structure";
            primitive.blasBuffer.Initialize(rbi);

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc = {};
            blasDesc.Inputs = input;
            blasDesc.ScratchAccelerationStructureData = meshResource.blasScratchBuffer.internalBuffer.gpuAddress;
            blasDesc.DestAccelerationStructureData = primitive.blasBuffer.internalBuffer.gpuAddress;

            commandList->BuildRaytracingAccelerationStructure(&blasDesc, 0, nullptr);
            primitive.blasBuffer.internalBuffer.UAVBarrier(commandList.Get());
            primitiveIdx++;
        }
    }

    // Build tlas instancing
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs(numInstances);
    uint32_t instanceIndex = 0;
    for (const NodeData& node : m_model.nodes)
    {
        const MeshData& mesh = m_model.meshes[node.meshIndex];
        for (const PrimitiveData& primitive : mesh.primitives)
        {
            D3D12_RAYTRACING_INSTANCE_DESC& instanceDesc = instanceDescs[instanceIndex];
            instanceDesc = {};
            instanceDesc.InstanceID = instanceIndex;
            instanceDesc.InstanceMask = 0xFF;
            instanceDesc.AccelerationStructure = primitive.blasBuffer.internalBuffer.gpuAddress;
            XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), node.transform);
            instanceIndex++;
        }
    }

    RawBufferInit rbi;
    rbi.numElements = (instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC)) / RawBuffer::Stride;
    rbi.cpuAccessible = true;
    rbi.initData = instanceDescs.data();
    rbi.name = L"RT InstanceDesc";
    meshResource.instanceBuffer.Initialize(rbi);

    // Build tlas scratch buffer
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input = {};
    input.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    input.Flags = buildFlags;
    input.NumDescs = instanceDescs.size();
    //input.InstanceDescs = meshResource.instanceBuffer->GetGPUVirtualAddress();
    input.InstanceDescs = meshResource.instanceBuffer.internalBuffer.gpuAddress;
    d3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&input, &prebuildInfo);
    assert(prebuildInfo.ResultDataMaxSizeInBytes > 0);
    {
        RawBufferInit rbi;
        rbi.numElements = prebuildInfo.ResultDataMaxSizeInBytes / RawBuffer::Stride;
        rbi.allowUAV = true;
        rbi.initState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        rbi.name = L"RT Tlas ScratchBuffer";
        meshResource.tlasScratchBuffer.Initialize(rbi);
    }

    // Build tlas buffer
    {
        RawBufferInit rbi;
        rbi.numElements = prebuildInfo.ResultDataMaxSizeInBytes / RawBuffer::Stride;
        rbi.allowUAV = true;
        rbi.initState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        rbi.name = L"RT Top Level Acceleration Structure";
        meshResource.tlasBuffer.Initialize(rbi);
    }
    
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = input;
    buildDesc.ScratchAccelerationStructureData = meshResource.tlasScratchBuffer.internalBuffer.gpuAddress;
    buildDesc.DestAccelerationStructureData = meshResource.tlasBuffer.internalBuffer.gpuAddress;
    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    meshResource.tlasBuffer.internalBuffer.UAVBarrier(commandList.Get());

    // Build instance info buffer
    std::vector<InstanceInfo> instanceInfo(numInstances);
    instanceIndex = 0;
    for (const NodeData& node : m_model.nodes)
    {
        const MeshData& mesh = m_model.meshes[node.meshIndex];
        for (const PrimitiveData& primitive : mesh.primitives)
        {
            InstanceInfo& instInfo = instanceInfo[instanceIndex];
            instInfo = {};
            instInfo.VtxOffset = primitive.vertexOffset;
            instInfo.IdxOffsetByBytes = primitive.indexOffset * sizeof(uint32_t);
            instInfo.MaterialIdx = primitive.materialIndex;
            instInfo.UseTangent = primitive.hasTangent ? 1 : 0;
            instInfo.UseVertexColor = primitive.hasVertexColor ? 1 : 0;
            instanceIndex++;
        }
    }

    StructuredBufferInit sbi;
    sbi.stride = sizeof(InstanceInfo);
    sbi.numElements = instanceInfo.size();
    sbi.initData = instanceInfo.data();
    sbi.name = L"Instance info buffer";
    meshResource.instanceInfoBuffer.Initialize(sbi);
}

void Model::RenderModel(const BoundingFrustum& frustum, bool AlphaFilter)
{
    UINT constantIndex = 0;
    for (const NodeData& node : m_model.nodes)
    {
        const MeshData& mesh = m_model.meshes[node.meshIndex];
        for (const PrimitiveData& primitive : mesh.primitives)
        {
            const MaterialData& material = m_model.materials[primitive.materialIndex];
            const bool nonOpaque = (material.alphaCutoff < 1.f) ? true : false;

            if (AlphaFilter)
            {
                if (!nonOpaque)
                {
                    constantIndex++;
                    continue;
                }
            }
            else
            {
                if (nonOpaque)
                {
                    constantIndex++;
                    continue;
                }
            }

            // transform bounding box to world space
            BoundingBox worldBox;
            primitive.boundingBox.Transform(worldBox, node.transform);
            if (frustum.Contains(worldBox) == DISJOINT)
            {
                constantIndex++;
                continue;
            }

            // constant
            ModelConstants constant = { static_cast<UINT>(constantIndex), static_cast<UINT>(primitive.materialIndex) };
            commandList->SetGraphicsRoot32BitConstants(2, 2, &constant, 0);

            // Set vertex and index buffers
            commandList->DrawIndexedInstanced(primitive.indices.size(), 1, primitive.indexOffset, primitive.vertexOffset, 0);
            constantIndex++;
        }
    }
}
HRESULT Model::RenderDepthOnly(
    const ConstantBuffer* sceneCB,
    const BoundingFrustum& frustum)
{
    if (m_model.nodes.empty())
    {
        return E_FAIL;
    }
    commandList->SetGraphicsRootSignature(depthRootSignature.Get());

    // Bind srv descriptor heaps contain sb/srv
    ID3D12DescriptorHeap* ppDescHeaps[] = { srvDescriptorHeap.heap.Get() };
    commandList->SetDescriptorHeaps(_countof(ppDescHeaps), ppDescHeaps);
    SrvSetAsGfxRootParameter(commandList.Get(), 0); // bindless
    sceneCB->SetAsGfxRootParameter(commandList.Get(), 1);
    commandList->SetGraphicsRootShaderResourceView(3, meshSB.internalBuffer.gpuAddress);
    commandList->SetGraphicsRootShaderResourceView(4, materialSB.internalBuffer.gpuAddress);

    commandList->IASetVertexBuffers(0, 1, &meshResource.vertexBuffer.VBView());
    commandList->IASetIndexBuffer(&meshResource.indexBuffer.IBView());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Render opaque first
    commandList->SetPipelineState(depthPSO.Get());
    RenderModel(frustum, false);

    // Render alpha test
    commandList->SetPipelineState(depthAlphaPSO.Get());
    RenderModel(frustum, true);
    return S_OK;
}

HRESULT Model::RenderBasePass(
    const ConstantBuffer* sceneCB,
    const ConstantBuffer* lightCB,
    const BoundingFrustum& frustum)
{
    if (m_model.nodes.empty())
    {
        return E_FAIL;
    }
    
    //commandList->SetPipelineState(m_pipelineState.Get());
    //commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    //// Bind srv descriptor heaps contain texture srv
    //ID3D12DescriptorHeap* ppDescHeaps[] = { srvDescriptorHeap.heap.Get() };
    //commandList->SetDescriptorHeaps(_countof(ppDescHeaps), ppDescHeaps);
    //SrvSetAsGfxRootParameter(commandList.Get(), Model_GlobalSRV);
    //sceneCB->SetAsGfxRootParameter(commandList.Get(), Model_SceneCBuffer);
    //lightCB->SetAsGfxRootParameter(commandList.Get(), Model_LightCBuffer);
    //m_meshSB.SetAsGfxRootParameter(commandList.Get(), Model_ModelSBuffer);

    //// For now :
    //// App + IMGUI share the same descriptor heap
    //// App reserve on static slot of heap, while UI build slot of heap dynamically
    //// is fine because UI using different root descriptor table than App descriptor table
    //// They only share descriptor heap

    //for (size_t i = 0; i < m_model.meshes.size(); ++i)
    //{
    //    const auto& mesh = m_model.meshes[i];
    //    const auto& resource = m_meshResources[i];

    //    XMMATRIX world = XMLoadFloat4x4(&mesh.transform);

    //    // transform bounding box to world space
    //    BoundingBox worldBox;
    //    mesh.boundingBox.Transform(worldBox, world);

    //    if (frustum.Contains(worldBox) == DISJOINT)
    //    {
    //        continue;
    //    }

    //    // Constant
    //    ModelConstants constant = {static_cast<UINT>(i), static_cast<UINT>(mesh.materialIndex)};
    //    commandList->SetGraphicsRoot32BitConstants(Model_ModelConstant, 2, &constant, 0);

    //    // Set vertex and index buffers
    //    commandList->IASetVertexBuffers(0, 1, &resource.vertexBuffer.VBView());
    //    commandList->IASetIndexBuffer(&resource.indexBuffer.IBView());
    //    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    //    commandList->DrawIndexedInstanced(mesh.indices.size(), 1, 0, 0, 0);
    //}

    return S_OK;
}

void Model::RenderDepthPrepass()
{

}

void Model::RenderGBuffer(const ConstantBuffer* sceneCB, const DirectX::BoundingFrustum& frustum)
{
    if (m_model.nodes.empty())
    {
        return;
    }

    commandList->SetPipelineState(gbufferPipelineState.Get());
    commandList->SetGraphicsRootSignature(gbufferRootSignature.Get());

    // Bind srv descriptor heaps contain texture srv
    ID3D12DescriptorHeap* ppDescHeaps[] = { srvDescriptorHeap.heap.Get() };
    commandList->SetDescriptorHeaps(_countof(ppDescHeaps), ppDescHeaps);
    SrvSetAsGfxRootParameter(commandList.Get(), 0);
    sceneCB->SetAsGfxRootParameter(commandList.Get(), 1);
    meshSB.SetAsGfxRootParameter(commandList.Get(), 3);

    commandList->IASetVertexBuffers(0, 1, &meshResource.vertexBuffer.VBView());
    commandList->IASetIndexBuffer(&meshResource.indexBuffer.IBView());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ComPtr<ID3D12PipelineState> currPSO = gbufferPipelineState;

    UINT constantIndex = 0;
    for (const auto& node : m_model.nodes)
    {
        const MeshData& mesh = m_model.meshes[node.meshIndex];
        for (const auto& primitive : mesh.primitives)
        {
            // transform bounding box to world space
            BoundingBox worldBox;
            primitive.boundingBox.Transform(worldBox, node.transform);

            if (frustum.Contains(worldBox) == DISJOINT)
            {
                constantIndex++;
                continue;
            }

            // Constant
            ModelConstants constant = { static_cast<UINT>(constantIndex), static_cast<UINT>(primitive.materialIndex) };
            commandList->SetGraphicsRoot32BitConstants(2, 2, &constant, 0);

            ComPtr<ID3D12PipelineState> newPSO = gbufferPipelineState;
            const MaterialData& material = m_model.materials[primitive.materialIndex];
            if (material.alphaCutoff < 1.f)
                newPSO = alphaTestPipelineState;

            if (currPSO != newPSO)
            {
                commandList->SetPipelineState(newPSO.Get());
                currPSO = newPSO;
            }
            // Set vertex and index buffers
            commandList->DrawIndexedInstanced(primitive.indices.size(), 1, primitive.indexOffset, primitive.vertexOffset, 0);
            constantIndex++;
        }
    }
}