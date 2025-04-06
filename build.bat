@echo off
setlocal enabledelayedexpansion

:: 默认参数
set BUILD_DIR=build
set DEFAULT_TARGETS=algoPulse clientTest
set BUILD_MODE=Release

:: 解析命令行参数
set TARGETS=%DEFAULT_TARGETS%
set MODE=%BUILD_MODE%

:parse_args
if "%~1"=="" goto :args_parsed
if /i "%~1"=="--target" (
    set TARGETS=%~2
    shift /2
    goto :parse_args
)
if /i "%~1"=="--mode" (
    set MODE=%~2
    shift /2
    goto :parse_args
)
shift
goto :parse_args

:args_parsed

:: 设置VCPKG路径（如果未设置环境变量）
if not defined VCPKG_ROOT set VCPKG_ROOT=c:\vcpkg

:: 检查构建目录
if not exist "%BUILD_DIR%" (
    echo Creating build directory: %BUILD_DIR%
    mkdir "%BUILD_DIR%"
)

echo Vcpkg root: %VCPKG_ROOT%
echo Build mode: %MODE%
echo Targets: %TARGETS%

:: Vcpkg安装依赖
"%VCPKG_ROOT%\vcpkg.exe" install --x-install-root=.\%BUILD_DIR%\vcpkg_installed
if %errorlevel% neq 0 (
    echo Error: vcpkg install failed
    exit /b 1
)

:: CMake配置
cmake -B "%BUILD_DIR%" -S . ^
    -DCMAKE_C_COMPILER_FORCED=ON ^
    -DCMAKE_CXX_COMPILER_FORCED=ON ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DCMAKE_BUILD_TYPE="%MODE%" ^
    -DCMAKE_INSTALL_PREFIX=.\%BUILD_DIR%

if %errorlevel% neq 0 (
    echo Error: CMake configuration failed
    exit /b 1
)

:: 编译目标
cd "%BUILD_DIR%"
for %%T in (%TARGETS%) do (
    echo Building target: %%T
    cmake --build . --target %%T --config %MODE%
    if !errorlevel! neq 0 (
        echo Error: Build failed for target %%T
        exit /b 1
    )
)

:: 安装
echo Running install...
cmake --build . --target install --config %MODE%
if !errorlevel! neq 0 (
    echo Error: Install failed
    exit /b 1
)

echo Build successful!
exit /b 0