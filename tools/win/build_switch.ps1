# TFU-nx: builds the Switch NRO on Mark's PC with devkitPro (C:\devkitPro), Renut's renderer on deko3d, and puts the
# test folder together in out\sd\switch\s4ep1-nx\ (renut.nro, renut.toml) with the NRO's SHA-256. Every step's full
# output goes to out\logs\.
#
# Usage (from C:\TFU-nx): powershell -ExecutionPolicy Bypass -File tools\win\build_switch.ps1 [-Jobs N] [-Clean]
#   -Jobs   parallel compile jobs (default: half the logical CPUs, at most 4; each game-code file can need 3 GB RAM)
#   -Clean  delete the Switch build folder (out\switch) first
#
# What it needs (Claude puts the first two in place over the link to this PC):
#   out\renut_switch\deko3d_glsl.bin   the game shaders for deko3d (made in the cloud from out\renut_shader_dump)
#   out\renut_switch_host\*.deko3d.h   Renut's own shaders for deko3d (made in the cloud)
#   out\pc\renut_gpu\shader_cache.cpp  the PC build's shader table (build_pc.ps1 -Renut made it)
#   app\generated\                     the game code (the PC build made it; the Switch build cannot run the generator)
#   C:\devkitPro                       devkitA64, libnx, deko3d, and the switch-zlib portlib

param(
    [int]$Jobs = 0,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$logDir = Join-Path $repo 'out\logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$build = Join-Path $repo 'out\switch'
$shaders = Join-Path $repo 'out\renut_switch'
$hostShaders = Join-Path $repo 'out\renut_switch_host'
$sdOut = Join-Path $repo 'out\sd\switch\s4ep1-nx'
$devkitPro = 'C:\devkitPro'

function Step($name, $logName, [scriptblock]$body) {
    $log = Join-Path $logDir $logName
    Write-Host ''
    Write-Host "== $name  (log: out\logs\$logName)"
    $start = Get-Date
    $sw = [System.IO.StreamWriter]::new($log, $false, [System.Text.UTF8Encoding]::new($false))
    $sw.AutoFlush = $true
    try {
        & { $ErrorActionPreference = 'Continue'; & $body } 2>&1 | ForEach-Object { "$_" } |
            ForEach-Object -Begin { $n = 0 } -Process {
                $sw.WriteLine($_)
                $n++
                if ($_ -match '^\[\d+/\d+\]' -and ($n % 25) -eq 0) { Write-Host "   $($_.Substring(0, [Math]::Min(100, $_.Length)))" }
                elseif ($_ -match 'error:|FAILED') { Write-Host "   $_" }
            }
    } finally {
        $sw.Close()
    }
    if ($LASTEXITCODE -ne 0) {
        Write-Host ''
        Write-Host "FAILED at: $name. Tell Claude; the log is out\logs\$logName"
        exit 1
    }
    Write-Host ("   done in {0:n0} min" -f ((Get-Date) - $start).TotalMinutes)
}

# The PC tools (CMake, Ninja, Python, Git) as the PC build finds them. devkitPro's MSYS2 stays off the PATH: the
# toolchain file calls devkitA64's compilers by full path.
. (Join-Path $PSScriptRoot 'env.ps1')
# devkitPro's installer sets DEVKITPRO to /opt/devkitpro (its MSYS2 form); CMake's compiler checks run in fresh
# projects that see only the environment, so it is set to the Windows folder here.
$env:DEVKITPRO = 'C:/devkitPro'
$env:DEVKITA64 = 'C:/devkitPro/devkitA64'

# 1. Everything the build reads is checked first, so a missing piece stops it here and not an hour in.
$missing = @()
$need = @(
    @((Join-Path $devkitPro 'devkitA64\bin\aarch64-none-elf-gcc.exe'), 'devkitA64 (devkitPro installer, Switch development)'),
    @((Join-Path $devkitPro 'libnx\lib\libnx.a'), 'libnx'),
    @((Join-Path $devkitPro 'libnx\lib\libdeko3d.a'), 'deko3d (devkitPro package deko3d)'),
    @((Join-Path $devkitPro 'portlibs\switch\lib\libz.a'), 'switch-zlib'),
    @((Join-Path $devkitPro 'tools\bin\elf2nro.exe'), 'switch-tools (elf2nro, nacptool)'),
    @((Join-Path $shaders 'deko3d_glsl.bin'), 'the deko3d game shader package (Claude makes it)'),
    @((Join-Path $hostShaders 'present_ps.deko3d.h'), 'the deko3d host shaders (Claude makes them)'),
    @((Join-Path $repo 'out\pc\renut_gpu\shader_cache.cpp'), 'the PC build''s shader table (build_pc.ps1 -Renut)'),
    @((Join-Path $repo 'out\glslang\StandAlone\glslangValidator.exe'), 'glslangValidator (build_pc.ps1 makes it)'),
    @((Join-Path $repo 'app\generated\codegen.stamp'), 'the generated game code (build_pc.ps1)')
)
foreach ($item in $need) {
    if (-not (Test-Path $item[0])) { $missing += "   $($item[1]): no $($item[0])" }
}
if ($missing.Count -gt 0) {
    Write-Host 'Missing, so nothing was built:'
    $missing | ForEach-Object { Write-Host $_ }
    Write-Host 'Tell Claude.'
    exit 1
}

if ($Jobs -le 0) { $Jobs = [math]::Max(1, [math]::Min(4, [int]([Environment]::ProcessorCount / 2))) }
if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }

# 1b. S4ep1-nx: TFU's tools\tfu_spin_idle.py step (a TFU-only change to the generated code) removed.
# 2. The NRO's shader table: the PC build's without the DXIL (tools/renut_switch_shader_cache.py), and the deko3d
#    package's source, which points the assembler at deko3d_glsl.bin on this PC.
Step 'Shader table for the Switch' 'switch_shaders.log' {
    python (Join-Path $repo 'tools\renut_switch_shader_cache.py') --pc (Join-Path $repo 'out\pc') --out-dir $shaders
}
$glslBin = (Join-Path $shaders 'deko3d_glsl.bin').Replace('\', '/')
$glslSize = (Get-Item (Join-Path $shaders 'deko3d_glsl.bin')).Length
$glslCpp = @(
    '// TFU-nx: the game shaders'' uam GLSL (tools/deko3d_shader_package.py), embedded by the assembler.',
    '// Written by tools/win/build_switch.ps1 for this PC''s path. Game data: build tree and NRO only.',
    '#include <cstddef>',
    '#include <cstdint>',
    '__asm__(".section .rodata.banjo_deko3d_glsl,\"a\"\n"',
    '        ".global g_deko3dGlslPackage\n"',
    '        ".type g_deko3dGlslPackage, %object\n"',
    '        ".balign 16\n"',
    '        "g_deko3dGlslPackage:\n"',
    "        `".incbin \`"$glslBin\`"\n`"",
    '        ".size g_deko3dGlslPackage, .-g_deko3dGlslPackage\n"',
    '        ".previous\n");',
    "extern const size_t g_deko3dGlslPackageSize = $glslSize;"
)
Set-Content -Path (Join-Path $shaders 'deko3d_glsl_switch.cpp') -Value $glslCpp -Encoding ascii

# 3. Configure. Renut's renderer on deko3d, no Vulkan driver in the NRO (REXGLUE_SWITCH_NVK off, the default).
$py = (Get-Command python).Source
$cfgArgs = @(
    '-S', (Join-Path $repo 'app'), '-B', $build, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release',
    "-DCMAKE_TOOLCHAIN_FILE=$(Join-Path $repo 'tools\switch\cmake\switch-devkitA64.cmake')",
    "-DDEVKITPRO=C:/devkitPro",
    "-DREXSDK_DIR=$(Join-Path $repo 'sdk')",
    '-DBANJO_RENUT_RENDERER=ON',
    # TFU-nx (2026-11-13): line tables (-g1) for Renut's render-thread files, so a console profile names the source
    # lines inside FastDrawLocked and its neighbours. Debug info only: the code is the same.
    '-DBANJO_SWITCH_RENDERER_LINES=ON',
    "-DBANJO_RENUT_SHADER_DIR=$shaders",
    "-DBANJO_DEKO3D_HOST_SHADERS_PREBUILT=$hostShaders",
    "-DPYTHON_EXECUTABLE=$py", "-DPython3_EXECUTABLE=$py",
    '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'
)
Step 'Configuring the Switch build' 'switch_configure.log' { cmake @cfgArgs }

# 4. Build. The first time compiles all the game code for the Switch (long: about as long as the first PC build).
Step "Compiling for the Switch ($Jobs jobs at a time)" 'switch_build.log' { cmake --build $build -- -j $Jobs }

# 5. The test folder: the NRO, the settings, the SHA-256; the ELF kept by build stamp (crash addresses are read from it).
$nro = Join-Path $build 'renut.nro'
$elf = Join-Path $build 'renut'  # the ELF (no extension in this toolchain)
if (-not (Test-Path $nro)) { Write-Host 'The build finished but renut.nro is not where expected. Tell Claude.'; exit 1 }
New-Item -ItemType Directory -Force -Path $sdOut | Out-Null
Copy-Item $nro (Join-Path $sdOut 'renut.nro') -Force
# The settings must parse: the SDK loads them before its log opens, and one bad line (the first test: a duplicate key)
# silently left every setting at its default.
$toml = Join-Path $repo 'app\s4_switch_test.toml'
python -c "import sys, tomllib; tomllib.load(open(sys.argv[1], 'rb'))" $toml
if ($LASTEXITCODE -ne 0) { Write-Host 'app\s4_switch_test.toml does not parse (above). Tell Claude.'; exit 1 }
Copy-Item $toml (Join-Path $sdOut 'renut.toml') -Force
$hash = (Get-FileHash $nro -Algorithm SHA256).Hash.ToLower()
Set-Content -Path (Join-Path $sdOut 'renut.nro.sha256') -Value "$hash  renut.nro" -Encoding ascii
$stampFile = Join-Path $build 'tfu_build_stamp\tfu_build_stamp.h'
$stamp = if (Test-Path $stampFile) { (Get-Content $stampFile -Raw).Trim() } else { 'no stamp' }
$elfDir = Join-Path $repo 'out\nx\elf'
New-Item -ItemType Directory -Force -Path $elfDir | Out-Null
if (Test-Path $elf) { Copy-Item $elf (Join-Path $elfDir ("renut_" + $hash.Substring(0, 12) + ".elf")) -Force }
Write-Host ''
Write-Host "BUILD OK: $sdOut"
Write-Host "   renut.nro  $([math]::Round((Get-Item $nro).Length / 1MB)) MB  sha256 $hash"
Write-Host "   $stamp"
Write-Host 'On the SD card: /switch/s4ep1-nx/ gets renut.nro, renut.toml and a game_root folder holding default.xex and data.cpk (assets\extracted).'
