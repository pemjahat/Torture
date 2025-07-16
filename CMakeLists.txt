cmake_minimum_required(VERSION 3.20)

# Project settings
project(SDL_Window)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SDL_VERSION "2.32.6")
set(DXC_VERSION "v1.8.2505/dxc_2025_05_24")

# multithreading compilation
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

# Enable solution folders for Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set CMP0135 to NEW to use extraction timestamps
# Ensure rebuild occur when archive content change
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Helper to download and unzip a package from a URL
# Uses a zero-length file to identify the version of the package
# copied from https://github.com/NVIDIAGameWorks/RTXGI-DDGI/blob/main/CMakeLists.txt
function(CheckAndDownloadPackage NAME VERSION LOCAL_PATH URL)
    # Do we already have the correct version?
    if(NOT EXISTS ${LOCAL_PATH}/${VERSION}.ver)
        # Was there a previous version that we need to delete?
        if(EXISTS ${LOCAL_PATH})
            message(STATUS "Deleting old " ${NAME})
            file(REMOVE_RECURSE ${LOCAL_PATH})
        endif()
        message(STATUS "Obtaining " ${NAME} " " ${VERSION})
        file(DOWNLOAD ${URL} ${LOCAL_PATH}.zip)
        message(STATUS "Extracting " ${NAME})
        file(ARCHIVE_EXTRACT INPUT ${LOCAL_PATH}.zip DESTINATION ${LOCAL_PATH})
        file(REMOVE ${LOCAL_PATH}.zip)
        # Create an empty file so we know which version we have
        file(WRITE ${LOCAL_PATH}/${VERSION}.ver)
    endif()
endfunction()

set(EXTERN_DIR "${CMAKE_SOURCE_DIR}/thirdparty")
set(DXC_DIR "${EXTERN_DIR}/dxc")
set(SDL_DIR "${EXTERN_DIR}/sdl/SDL2-${SDL_VERSION}")
CheckAndDownloadPackage("DXC" ${DXC_VERSION} ${EXTERN_DIR}/dxc https://github.com/microsoft/DirectXShaderCompiler/releases/download/${DXC_VERSION}.zip)
CheckAndDownloadPackage("SDL" ${SDL_VERSION} ${EXTERN_DIR}/sdl https://www.libsdl.org/release/SDL2-devel-${SDL_VERSION}-VC.zip)

# Include FetchContent module
include(FetchContent)

# D3DX12
FetchContent_Declare(
    DirectXHeaders
    GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
    GIT_TAG main
)
FetchContent_MakeAvailable(DirectXHeaders)

# Imgui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
)
FetchContent_MakeAvailable(imgui)

# TinyGltf
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git    
    GIT_TAG release
)
FetchContent_MakeAvailable(tinygltf)

# Automatically collect all .cpp and .h files
file(GLOB SOURCES
    "${CMAKE_SOURCE_DIR}/sources/*.cpp"
    "${CMAKE_SOURCE_DIR}/sources/*.h"
)

# Set Output to binary
set(BIN_DIR "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR})

# Add executable
add_executable(${PROJECT_NAME} ${SOURCES})
target_precompile_headers(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/sources/PCH.h)

# Add imgui source file (need to compile source)
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)
target_sources(${PROJECT_NAME} PRIVATE ${IMGUI_SOURCES})

# Add hlsl into project(no compile)
file(GLOB SHADER_FILES "${CMAKE_SOURCE_DIR}/shaders/*.*")
source_group("Shader Files" FILES ${SHADER_FILES})
target_sources(${PROJECT_NAME} PRIVATE ${SHADER_FILES})

# Copy hlsl to output directory
foreach(SHADER ${SHADER_FILES})
    # prevent vs from compile it
    set_source_files_properties(${SHADER} PROPERTIES VS_TOOL_OVERRIDE "None")
endforeach()

message(STATUS "Check DXC " ${DXC_DIR})
message(STATUS "Check SDL " ${SDL_DIR})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${SDL_DIR}/include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${directxheaders_SOURCE_DIR}/include/directx
    ${tinygltf_SOURCE_DIR}
    ${DXC_DIR}/include
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${SDL_DIR}/lib/x64/SDL2.lib 
    ${SDL_DIR}/lib/x64/SDL2main.lib 
    d3d12
    dxgi
    dxguid   #dxgi debug layer
    ${DXC_DIR}/lib/x64/dxcompiler.lib
)

# Symlink
message(STATUS "Check TargetDir " ${BIN_DIR})

file(CREATE_LINK "${DXC_DIR}/bin/x64/dxcompiler.dll" "${BIN_DIR}/dxcompiler.dll" SYMBOLIC)
file(CREATE_LINK "${SDL_DIR}/lib/x64/SDL2.dll" "${BIN_DIR}/SDL2.dll" SYMBOLIC)

# Define TinyGltf implementation flags
target_compile_definitions(${PROJECT_NAME} PRIVATE
    TINYGLTF_IMPLEMENTATION
    STB_IMAGE_IMPLEMENTATION
    STB_IMAGE_WRITE_IMPLEMENTATION
)

# Windows-specific linking
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE user32 gdi32 winmm imm32 ole32 oleaut32 version uuid)
endif()

# Set SDL_Window as the startup project for Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
