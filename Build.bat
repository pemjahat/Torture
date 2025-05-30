@echo off
setlocal

:: Set project name and directories
set PROJECT_NAME=SDL_Window
set SOURCE_DIR=%~dp0
set BUILD_DIR=%SOURCE_DIR%build

:: Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

:: Navigate to build directory
cd "%BUILD_DIR%"

:: Check if CMake is available
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: CMake not found. Ensure CMake is installed and added to PATH.
    exit /b 1
)

:: Run CMake to configure the project for Visual Studio (64-bit)
echo Configuring project with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    exit /b %ERRORLEVEL%
)

echo Build and run successful!
endlocal