#include "Helper.h"
#include "Utility.h"
#include "DX12.h"

static const uint32_t NumRasterizerState = uint32_t(RasterizerState::MaxRasterizer);
static const uint32_t NumDepthState = uint32_t(DepthStencilState::MaxDepth);
static const uint32_t NumSamplerState = uint32_t(SamplerState::MaxSampler);

static D3D12_RASTERIZER_DESC RasterizerStateDesc[NumRasterizerState] = {};
static D3D12_DEPTH_STENCIL_DESC DepthStateDesc[NumDepthState] = {};
static D3D12_SAMPLER_DESC SamplerStateDesc[NumSamplerState] = {};

DescriptorHeap rtvDescriptorHeap;
DescriptorHeap dsvDescriptorHeap;
DescriptorHeap srvDescriptorHeap;

static D3D12_DESCRIPTOR_RANGE1 srvDescriptorRange = {};

void InitializeHelper()
{
	// Descriptor heap
	rtvDescriptorHeap.Initialize(10, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	dsvDescriptorHeap.Initialize(1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	srvDescriptorHeap.Initialize(1024, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Descriptor range
	srvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvDescriptorRange.NumDescriptors = 1024;
	srvDescriptorRange.BaseShaderRegister = 0;
	srvDescriptorRange.RegisterSpace = 1;
	srvDescriptorRange.OffsetInDescriptorsFromTableStart = 0;
	srvDescriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

	// Rasterizer state
	{
		D3D12_RASTERIZER_DESC& rasterDesc = RasterizerStateDesc[uint32_t(RasterizerState::NoCull)];
		rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
		rasterDesc.DepthClipEnable = true;
		rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterDesc.MultisampleEnable = false;
		rasterDesc.FrontCounterClockwise = true; // Matches glTF CCW convention
	}
	{
		D3D12_RASTERIZER_DESC& rasterDesc = RasterizerStateDesc[uint32_t(RasterizerState::BackfaceCull)];
		rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasterDesc.DepthClipEnable = true;
		rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterDesc.MultisampleEnable = false;
		rasterDesc.FrontCounterClockwise = true; // Matches glTF CCW convention
	}

	// Depth Stencil state
	{
		D3D12_DEPTH_STENCIL_DESC& depthDesc = DepthStateDesc[uint32_t(DepthStencilState::Disabled)];
		depthDesc.DepthEnable = false;
		depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	}
	{
		D3D12_DEPTH_STENCIL_DESC& depthDesc = DepthStateDesc[uint32_t(DepthStencilState::Enabled)];
		depthDesc.DepthEnable = true;
		depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	}
	{
		D3D12_DEPTH_STENCIL_DESC& depthDesc = DepthStateDesc[uint32_t(DepthStencilState::WriteEnabled)];
		depthDesc.DepthEnable = true;
		depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	}

	// Sampler state
	{
		D3D12_SAMPLER_DESC& samplerDesc = SamplerStateDesc[uint32_t(SamplerState::Linear)];

		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MipLODBias = 0.f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		samplerDesc.BorderColor[0] = samplerDesc.BorderColor[1] = samplerDesc.BorderColor[2] = samplerDesc.BorderColor[3] = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	}
	{
		D3D12_SAMPLER_DESC& samplerDesc = SamplerStateDesc[uint32_t(SamplerState::LinearClamp)];
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.MipLODBias = 0.f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		samplerDesc.BorderColor[0] = samplerDesc.BorderColor[1] = samplerDesc.BorderColor[2] = samplerDesc.BorderColor[3] = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	}
	{
		D3D12_SAMPLER_DESC& samplerDesc = SamplerStateDesc[uint32_t(SamplerState::Point)];
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.MipLODBias = 0.f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		samplerDesc.BorderColor[0] = samplerDesc.BorderColor[1] = samplerDesc.BorderColor[2] = samplerDesc.BorderColor[3] = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	}

	// Initialize dxc
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));

	// Initialize Include Shaders (with include path)
	CheckHRESULT(dxcUtils->CreateDefaultIncludeHandler(&includeHandler));
}

void ShutdownHelper()
{
	srvDescriptorHeap.Shutdown();
	dsvDescriptorHeap.Shutdown();
	rtvDescriptorHeap.Shutdown();
}

const D3D12_DESCRIPTOR_RANGE1* SRVDescriptorRanges()
{
	return &srvDescriptorRange;
}

D3D12_RASTERIZER_DESC GetRasterizerState(RasterizerState rasterState)
{
	return RasterizerStateDesc[uint32_t(rasterState)];
}

D3D12_DEPTH_STENCIL_DESC GetDepthStencilState(DepthStencilState depthState)
{
	return DepthStateDesc[uint32_t(depthState)];
}

D3D12_SAMPLER_DESC GetSamplerState(SamplerState samplerState)
{
	assert(uint32_t(samplerState) < _countof(SamplerStateDesc));
	return SamplerStateDesc[uint32_t(samplerState)];
}

D3D12_STATIC_SAMPLER_DESC GetStaticSamplerState(SamplerState samplerState, uint32_t shaderRegister, uint32_t registerSpace)
{
	assert(uint32_t(samplerState) < _countof(SamplerStateDesc));
	return ConvertToStaticSampler(SamplerStateDesc[uint32_t(samplerState)], shaderRegister, registerSpace);
}

D3D12_STATIC_SAMPLER_DESC ConvertToStaticSampler(const D3D12_SAMPLER_DESC samplerDesc, uint32_t shaderRegister, uint32_t registerSpace)
{
	D3D12_STATIC_SAMPLER_DESC staticDesc = {};
	staticDesc.Filter = samplerDesc.Filter;
	staticDesc.AddressU = samplerDesc.AddressU;
	staticDesc.AddressV = samplerDesc.AddressV;
	staticDesc.AddressW = samplerDesc.AddressW;
	staticDesc.MipLODBias = samplerDesc.MipLODBias;
	staticDesc.MaxAnisotropy = samplerDesc.MaxAnisotropy;
	staticDesc.ComparisonFunc = samplerDesc.ComparisonFunc;
	staticDesc.MinLOD = samplerDesc.MinLOD;
	staticDesc.MaxLOD = samplerDesc.MaxLOD;
	staticDesc.ShaderRegister = shaderRegister;
	staticDesc.RegisterSpace = registerSpace;
	staticDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	staticDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	
	return staticDesc;
}

void SrvSetAsGfxRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handle = srvDescriptorHeap.gpuStart;
	cmdList->SetGraphicsRootDescriptorTable(rootParameter, handle);
}

void SrvSetAsComputeRootParameter(ID3D12GraphicsCommandList* cmdList, uint32_t rootParameter)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handle = srvDescriptorHeap.gpuStart;
	cmdList->SetComputeRootDescriptorTable(rootParameter, handle);
}

void CreateRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature, const D3D12_ROOT_SIGNATURE_DESC1& desc)
{
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc = {};
	versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versionedDesc.Desc_1_1 = desc;

	Microsoft::WRL::ComPtr<ID3DBlob> signature;
	Microsoft::WRL::ComPtr<ID3DBlob> error;
	HRESULT hr = D3D12SerializeVersionedRootSignature(&versionedDesc, &signature, &error);
	if (FAILED(hr))
	{
		std::string errorMsg = std::string(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize());
		assert(false && errorMsg.c_str());
	}
	CheckHRESULT(d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf())));
}

void CompileShaderFromFile(
	const std::wstring& filePath, 
	const std::wstring& includePath, 
	Microsoft::WRL::ComPtr<IDxcBlob>& shaderBlob, 
	ShaderType type)
{
	// Load shader source
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
	dxcUtils->LoadFile(filePath.c_str(), nullptr, &sourceBlob);

	// Compile
	DxcBuffer sourceBuffer = { sourceBlob->GetBufferPointer(), sourceBlob->GetBufferSize(), 0 };

	// Compile arguments
	LPCWSTR arguments[] =
	{
		L"-E", L"VSMain",
		L"-T", L"vs_6_0",
		L"-Zi", L"-WX",
		L"-I", includePath.c_str()
	};

	if (type == ShaderType::Pixel)
	{
		// Compile arguments
		arguments[1] = L"PSMain";
		arguments[3] = L"ps_6_0";
	}
	else if (type == ShaderType::Compute)
	{
		// Compile arguments
		arguments[1] = L"CSMain";
		arguments[3] = L"cs_6_0";
	}
	else if (type == ShaderType::Library)
	{
		arguments[1] = L"";
		arguments[3] = L"lib_6_5";
	}

	Microsoft::WRL::ComPtr<IDxcResult> compileResult;
	dxcCompiler->Compile(&sourceBuffer, arguments, _countof(arguments), includeHandler.Get(), IID_PPV_ARGS(&compileResult));
	HRESULT status;
	compileResult->GetStatus(&status);
	if (FAILED(status))
	{
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> errorBlob;
		compileResult->GetErrorBuffer(&errorBlob);
		std::string errorMsg = std::string(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
		assert(false && errorMsg.c_str());
	}
	else
	{
		compileResult->GetResult(shaderBlob.GetAddressOf());
	}
}

//
// Raytrace
//
static const uint64_t MaxSubObjectDescSize = sizeof(D3D12_HIT_GROUP_DESC);

void StateObjectBuilder::Init(uint64_t max)
{
	maxSubObjects = max;
	subObjectData.resize(maxSubObjects * MaxSubObjectDescSize, 0);

	D3D12_STATE_SUBOBJECT defSubObj = {};
	subObjects.resize(maxSubObjects, defSubObj);
}

const D3D12_STATE_SUBOBJECT* StateObjectBuilder::AddSubObject(const void* subObjDesc, uint64_t subObjDescSize, D3D12_STATE_SUBOBJECT_TYPE type)
{
	const uint64_t subObjOffset = numSubObjects * MaxSubObjectDescSize;
	memcpy(subObjectData.data() + subObjOffset, subObjDesc, subObjDescSize);

	D3D12_STATE_SUBOBJECT& newSubObj = subObjects[numSubObjects];
	newSubObj.Type = type;
	newSubObj.pDesc = subObjectData.data() + subObjOffset;

	numSubObjects += 1;

	return &newSubObj;
}

void StateObjectBuilder::BuildDesc(D3D12_STATE_OBJECT_TYPE type, D3D12_STATE_OBJECT_DESC& desc)
{
	desc.Type = type;
	desc.NumSubobjects = uint32_t(numSubObjects);
	desc.pSubobjects = numSubObjects ? subObjects.data() : nullptr;
}

ID3D12StateObject* StateObjectBuilder::CreateStateObject(D3D12_STATE_OBJECT_TYPE type)
{
	D3D12_STATE_OBJECT_DESC desc = {};
	BuildDesc(type, desc);

	ID3D12StateObject* stateObj = nullptr;
	CheckHRESULT(d3dDevice->CreateStateObject(&desc, IID_PPV_ARGS(&stateObj)));

	return stateObj;
}