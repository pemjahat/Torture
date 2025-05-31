cmake_minimum_required(VERSION 3.20)
project(SDL_Window)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable solution folders for Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Include FetchContent module
include(FetchContent)

# Declare SDL2 to be fetched
include(FetchContent)
FetchContent_Declare(
    SDL2
    URL https://www.libsdl.org/release/SDL2-devel-2.32.6-VC.zip    
)
FetchContent_MakeAvailable(SDL2)

# Add executable
add_executable(${PROJECT_NAME} main.cpp)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${sdl2_SOURCE_DIR}/include    
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

# Windows-specific linking
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE user32 gdi32 winmm imm32 ole32 oleaut32 version uuid)
endif()

# Set SDL_Window as the startup project for Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)