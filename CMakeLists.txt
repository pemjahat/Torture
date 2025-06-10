cmake_minimum_required(VERSION 3.20)

# Project settings
project(SDL_Window)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# multithreading compilation
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

# Enable solution folders for Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set CMP0135 to NEW to use extraction timestamps
# Ensure rebuild occur when archive content change
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Include FetchContent module
include(FetchContent)

# D3DX12
FetchContent_Declare(
    DirectXHeaders
    GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
    GIT_TAG main
)
FetchContent_MakeAvailable(DirectXHeaders)

# Declare SDL2 to be fetched
FetchContent_Declare(
    SDL2
    URL https://www.libsdl.org/release/SDL2-devel-2.32.6-VC.zip    
)
FetchContent_MakeAvailable(SDL2)

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

# Add executable
add_executable(${PROJECT_NAME} ${SOURCES})

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
file(GLOB SHADER_FILES "${CMAKE_SOURCE_DIR}/shaders/*.hlsl")
source_group("Shader Files" FILES ${SHADER_FILES})
target_sources(${PROJECT_NAME} PRIVATE ${SHADER_FILES})

# Copy hlsl to output directory
foreach(SHADER ${SHADER_FILES})
    # prevent vs from compile it
    set_source_files_properties(${SHADER} PROPERTIES VS_TOOL_OVERRIDE "None")
    message(STATUS "Found shader: ${SHADER}")
    # get file name for use in output directory
    #get_filename_component(SHADER_NAME ${SHADER} NAME)
    # configure file to be copied to output directory
    #configure_file(${SHADER} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SHADER_NAME} COPYONLY)
    #add_custom_command(
        #TARGET ${PROJECT_NAME} POST_BUILD
        #COMMAND ${CMAKE_COMMAND} -E copy_if_different
            #${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_NAME}
            #${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${SHADER_NAME}
    #)

    #add_custom_command(
        #TARGET ${PROJECT_NAME} POST_BUILD
        #COMMAND ${CMAKE_COMMAND} -E copy_if_different
            #${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_NAME}
            #${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}
    #)
endforeach()

# Copy asset to
#file(GLOB GLTF_ASSETS "${CMAKE_SOURCE_DIR}/content/*.gltf")

# Copy assets 

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${sdl2_SOURCE_DIR}/include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${directxheaders_SOURCE_DIR}/include/directx
    ${tinygltf_SOURCE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${sdl2_SOURCE_DIR}/lib/x64/SDL2.lib 
    ${sdl2_SOURCE_DIR}/lib/x64/SDL2main.lib 
    d3d12
    dxgi
    d3dcompiler
    dxguid #dxgi debug layer
)

# Copy SDL2.dll to the output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${sdl2_SOURCE_DIR}/lib/x64/SDL2.dll"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

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

# Set Output to binary
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin")
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)