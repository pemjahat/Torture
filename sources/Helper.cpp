#include "Helper.h"

static const uint32_t NumSamplerState = uint32_t(SamplerState::MaxSampler);

static D3D12_SAMPLER_DESC SamplerStateDesc[NumSamplerState] = {};

void InitializeHelper()
{
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
