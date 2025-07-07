#pragma once

#include "PCH.h"

extern Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice;
extern Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
extern Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
extern Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;