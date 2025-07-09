#pragma once

#include <stdint.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define STRICT                          // Use strict declarations for Windows types

#define NOMINMAX // Prevent min/max macros

#define IMGUI_DEFINE_MATH_OPERATORS // for imgui

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

// Window header file
#include <Windows.h>
#include <wrl/client.h>

// DirectX
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#endif

// c++
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <algorithm>

// SDL
#include <SDL.h>
#include <SDL_syswm.h> // Added for SDL_GetWindowWMInfo

// Imgui
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_dx12.h>
