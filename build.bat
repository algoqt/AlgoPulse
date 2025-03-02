@echo off
setlocal enabledelayedexpansion

set VCPKG_ROOT=c:\vcpkg
set BUILD_DIR=build

if not exist "%BUILD_DIR%" (
    echo Creating BUILD_DIR: %BUILD_DIR%
    mkdir "%BUILD_DIR%"
)

%VCPKG_ROOT%\vcpkg.exe install --x-install-root=.\build\vcpkg_installed

cmake  -B build -S . -DCMAKE_C_COMPILER_FORCED=ON -DCMAKE_CXX_COMPILER_FORCED=ON ^
rem -DCMAKE_C_COMPILER="cl.exe" -DCMAKE_CXX_COMPILER="cl.exe"  ^
-DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
-DCMAKE_BUILD_TYPE="Release" -DCMAKE_INSTALL_PREFIX=.\bulid


for %%T in (algoPulse clientTest install) do (
    cmake.exe --build . --target %%T --config Release
    if !errorlevel! neq 0 (
        echo Error: CMake Build Failed for target %%T
        exit /b 1
    )
)

echo Build Success
exit /b 0
