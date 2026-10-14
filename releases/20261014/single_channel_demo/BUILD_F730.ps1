param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot 'build_reproduce'),
    [string]$ToolchainRoot = 'C:/Users/22209/zephyr-sdk-0.17.1/arm-zephyr-eabi',
    [string]$ToolchainPrefix = 'arm-zephyr-eabi-'
)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$build = $BuildDirectory
$source = if (Test-Path (Join-Path $PSScriptRoot 'f730\CMakeLists.txt')) { Join-Path $PSScriptRoot 'f730' } else { $PSScriptRoot }
& 'C:\Program Files\CMake\bin\cmake.exe' -S $source -B $build -G Ninja `
    '-DCMAKE_BUILD_TYPE=Debug' `
    '-DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake' `
    "-DTOOLCHAIN_ROOT=$ToolchainRoot" `
    "-DTOOLCHAIN_PREFIX=$ToolchainPrefix" `
    '-DCMAKE_MAKE_PROGRAM=E:/Xilinx/2025.2/Vivado/bin/ninja.exe'
if ($LASTEXITCODE -ne 0) { throw 'F730 configuration failed.' }
& 'C:\Program Files\CMake\bin\cmake.exe' --build $build
if ($LASTEXITCODE -ne 0) { throw 'F730 build failed.' }
Write-Host ('Build output: ' + $build)
