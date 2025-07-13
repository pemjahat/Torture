#pragma once

#include "PCH.h"

extern Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice;
extern Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
extern Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
extern Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
// Shaders
extern Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils;
extern Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler;
extern Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;

// Raytracing
extern Microsoft::WRL::ComPtr<ID3D12Device5> dxrDevice;
extern Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> dxrCommandList;
extern Microsoft::WRL::ComPtr<ID3D12StateObject> dxrStateObject;