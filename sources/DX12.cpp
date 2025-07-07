#pragma once

#include "DX12.h"

Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = nullptr;
Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;