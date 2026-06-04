@echo off
setlocal enabledelayedexpansion

:: PrimeBDS Build Script for Windows
:: Requires: CMake 3.15+, clang-cl (LLVM/Clang with MSVC frontend)
::   Install via: winget install LLVM.LLVM
::   Or install the "C++ Clang tools for Windows" workload in Visual Studio

set "BUILD_DIR=build\windows"
set "BUILD_TYPE=Release"
set "COMPILER_FLAGS="

:: Parse arguments
:parse_args
if "%~1"=="" goto done_args
if /i "%~1"=="--debug"   (set "BUILD_TYPE=Debug"   & shift & goto parse_args)
if /i "%~1"=="--release" (set "BUILD_TYPE=Release"  & shift & goto parse_args)
if /i "%~1"=="--clean"   (
    echo Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    shift & goto parse_args
)
if /i "%~1"=="--help" (
    echo Usage: build.bat [options]
    echo   --debug     Build in Debug mode
    echo   --release   Build in Release mode ^(default^)
    echo   --clean     Remove build directory before building
    echo   --help      Show this help
    exit /b 0
)
echo Unknown option: %~1
exit /b 1
:done_args

:: Check for CMake
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake not found. Install CMake 3.15+ and add it to PATH.
    exit /b 1
)

:: Detect C++ compiler and set up MSVC environment
set "COMPILER_FOUND="
set "VCVARS_FOUND="

:: 1) Try to set up MSVC sysroot via vcvarsall.bat — required by clang-cl even
::    when clang-cl itself comes from a standalone LLVM install.
::    Also add VS-bundled LLVM (VC\Tools\Llvm\x64\bin) to PATH while here.
for %%V in (18 2022 2019 2017) do (
    for %%E in (Community Professional Enterprise BuildTools) do (
        if not defined VCVARS_FOUND (
            if exist "C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat" (
                echo Setting up MSVC environment from VS %%V %%E...
                call "C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
                set "VCVARS_FOUND=1"
            )
            if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat" (
                echo Setting up MSVC environment from VS %%V %%E...
                call "C:\Program Files (x86)\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
                set "VCVARS_FOUND=1"
            )
        )
        :: Add VS-bundled LLVM to PATH so clang-cl can be found below
        if exist "C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Tools\Llvm\x64\bin\clang-cl.exe" (
            if not defined VSLLVM_ADDED (
                set "PATH=C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Tools\Llvm\x64\bin;!PATH!"
                set "VSLLVM_ADDED=1"
            )
        )
        if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%V\%%E\VC\Tools\Llvm\x64\bin\clang-cl.exe" (
            if not defined VSLLVM_ADDED (
                set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\%%V\%%E\VC\Tools\Llvm\x64\bin;!PATH!"
                set "VSLLVM_ADDED=1"
            )
        )
    )
)

:: 2) Add standalone LLVM install to PATH if clang-cl not yet visible
where clang-cl >nul 2>&1
if errorlevel 1 (
    if exist "C:\Program Files\LLVM\bin\clang-cl.exe" (
        set "PATH=C:\Program Files\LLVM\bin;!PATH!"
    )
)

:: 3) Confirm clang-cl is available — it is the only supported compiler on Windows
where clang-cl >nul 2>&1
if not errorlevel 1 (
    set "COMPILER_FOUND=clang-cl"
    set "COMPILER_FLAGS=-DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl"
    goto :compiler_done
)

:compiler_done
if not defined COMPILER_FOUND (
    echo [ERROR] clang-cl not found. Endstone requires clang-cl on Windows.
    echo.
    echo Install one of:
    echo   A) Standalone LLVM:  winget install LLVM.LLVM
    echo   B) Visual Studio workload: "C++ Clang tools for Windows"
    echo.
    echo Then re-run this script.
    exit /b 1
)
echo Compiler: %COMPILER_FOUND%
if defined VCVARS_FOUND echo MSVC sysroot: loaded (required by clang-cl)

:: Pick generator - prefer Ninja if available, fall back to NMake
set "GENERATOR=NMake Makefiles"
where ninja >nul 2>&1
if not errorlevel 1 set "GENERATOR=Ninja"

echo ============================================
echo  PrimeBDS Build - Windows (%BUILD_TYPE%)
echo  Generator: %GENERATOR%
echo ============================================

:: Configure
echo [1/2] Configuring...
cmake -S . -B "%BUILD_DIR%" -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_EXPORT_COMPILE_COMMANDS=ON %COMPILER_FLAGS%
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    exit /b 1
)

:: Build
echo [2/2] Building...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel -- -k 0
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo.
echo ============================================
echo  Build succeeded!
echo  Output: %BUILD_DIR%\%BUILD_TYPE%\output\
echo ============================================
