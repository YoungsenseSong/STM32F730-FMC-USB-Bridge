[CmdletBinding()]
param(
    [string]$HostGcc = '',
    [string]$ToolchainRoot = $env:WCH_TOOLCHAIN_ROOT
)

$ErrorActionPreference = 'Stop'
$demoRoot = $PSScriptRoot
$buildRoot = Join-Path $demoRoot 'build\release'
$hostRoot = Join-Path $demoRoot 'build\host'
$gccCommand = Get-Command gcc -ErrorAction SilentlyContinue
$gcc = if ($HostGcc) {
    $HostGcc
} elseif ($gccCommand) {
    $gccCommand.Source
} else {
    ''
}
if (-not $ToolchainRoot) {
    $ToolchainRoot = 'C:\MounRiver\MounRiver_Studio\toolchain\RISC-V Embedded GCC12'
}
$wchBin = Join-Path $ToolchainRoot 'bin'
$wchSize = Join-Path $wchBin 'riscv-none-elf-size.exe'

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'Missing required tool: cmake'
}
if (-not $gcc -or -not (Test-Path -LiteralPath $gcc)) {
    throw 'A native host gcc is required for protocol tests. Install it or pass -HostGcc.'
}
if (-not (Test-Path -LiteralPath $wchSize)) {
    throw "Missing WCH size utility: $wchSize. Set WCH_TOOLCHAIN_ROOT or pass -ToolchainRoot."
}

Push-Location $demoRoot
try {
    & cmake --preset release --fresh
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    & cmake --build --preset release
    if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed' }
} finally {
    Pop-Location
}

New-Item -ItemType Directory -Path $hostRoot -Force | Out-Null
& $gcc -std=c11 -Wall -Wextra -Werror `
    -I (Join-Path $demoRoot 'protocol') `
    (Join-Path $demoRoot 'tests\test_bridge_protocol.c') `
    (Join-Path $demoRoot 'protocol\bridge_protocol.c') `
    (Join-Path $demoRoot 'protocol\bridge_crc_combine.c') `
    -o (Join-Path $hostRoot 'test_bridge_protocol.exe')
if ($LASTEXITCODE -ne 0) { throw 'Protocol test compile failed' }
& (Join-Path $hostRoot 'test_bridge_protocol.exe') (Join-Path $demoRoot 'tests\golden')
if ($LASTEXITCODE -ne 0) { throw 'Protocol golden-vector test failed' }

& $gcc -std=c11 -Wall -Wextra -Werror `
    -I (Join-Path $demoRoot 'protocol') `
    (Join-Path $demoRoot 'tests\test_crc_combine.c') `
    (Join-Path $demoRoot 'protocol\bridge_protocol.c') `
    (Join-Path $demoRoot 'protocol\bridge_crc_combine.c') `
    -o (Join-Path $hostRoot 'test_crc_combine.exe')
if ($LASTEXITCODE -ne 0) { throw 'CRC test compile failed' }
& (Join-Path $hostRoot 'test_crc_combine.exe')
if ($LASTEXITCODE -ne 0) { throw 'CRC combine test failed' }

& $gcc -std=c11 -Wall -Wextra -Werror `
    -I (Join-Path $demoRoot 'app') `
    (Join-Path $demoRoot 'tests\test_bridge_state.c') `
    (Join-Path $demoRoot 'app\bridge_state.c') `
    -o (Join-Path $hostRoot 'test_bridge_state.exe')
if ($LASTEXITCODE -ne 0) { throw 'State test compile failed' }
& (Join-Path $hostRoot 'test_bridge_state.exe')
if ($LASTEXITCODE -ne 0) { throw 'State-machine test failed' }

& $gcc -std=c11 -Wall -Wextra -Werror `
    -I (Join-Path $demoRoot 'app') `
    -I (Join-Path $demoRoot 'protocol') `
    (Join-Path $demoRoot 'tests\test_sim_source.c') `
    (Join-Path $demoRoot 'app\bridge_sim.c') `
    (Join-Path $demoRoot 'protocol\bridge_protocol.c') `
    (Join-Path $demoRoot 'protocol\bridge_crc_combine.c') `
    -o (Join-Path $hostRoot 'test_sim_source.exe')
if ($LASTEXITCODE -ne 0) { throw 'Sim-source test compile failed' }
& (Join-Path $hostRoot 'test_sim_source.exe')
if ($LASTEXITCODE -ne 0) { throw 'Sim-source test failed' }

$elf = Join-Path $buildRoot 'WchBridgeDemo.elf'
$sections = @{}
& $wchSize -A $elf | ForEach-Object {
    if ($_ -match '^([.][A-Za-z0-9_]+)\s+(\d+)\s+') {
        $sections[$matches[1]] = [int64]$matches[2]
    }
}
$staticRam = $sections['.data'] + $sections['.bss']
$stack = $sections['.stack']
$totalRam = $staticRam + $stack
if ($staticRam -gt 56KB) { throw "Static RAM gate failed: $staticRam > 57344" }
if ($stack -lt 8KB) { throw "Stack reserve gate failed: $stack < 8192" }
if ($totalRam -gt 64KB) { throw "Physical RAM gate failed: $totalRam > 65536" }

Write-Output "RAM_GATE_OK static=$staticRam stack=$stack total=$totalRam free=$((64KB)-$totalRam)"
Write-Output 'VERIFY_ALL_OK'
