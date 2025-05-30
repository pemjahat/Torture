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
    SDL3
    URL https://www.libsdl.org/release/SDL3-devel-3.2.14-VC.zip    
)
FetchContent_MakeAvailable(SDL3)

# Add executable
add_executable(${PROJECT_NAME} main.cpp)

# Set include directories for SDL2
target_include_directories(${PROJECT_NAME} PRIVATE
    ${sdl3_SOURCE_DIR}/include
)

# Link SDL3 library
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${sdl3_SOURCE_DIR}/lib/x64/SDL3.lib    
)

# Copy SDL3.dll to the output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${sdl3_SOURCE_DIR}/lib/x64/SDL3.dll"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

# Set SDL_Window as the startup project for Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)