#pragma once

#include "DX12.h"

Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = nullptr;
Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;

Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils = nullptr;
Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler = nullptr;
Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler = nullptr;