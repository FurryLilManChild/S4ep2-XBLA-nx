# S4ep1-nx (from TFU-nx): builds the Windows PC test build. Extracts the XBLA package the first time, configures, runs the code generator
# through the build, and compiles everything. Every step's full output goes to out\logs\.
#
# Usage (from C:\S4ep1-nx): powershell -ExecutionPolicy Bypass -File tools\win\build_pc.ps1 [-Jobs N] [-Clean] [-Renut]
#   -Jobs   parallel compile jobs (default: half the logical CPUs, at most 8; each translated file needs 1-2 GB RAM)
#   -Clean  delete the build folder first
#   -Renut  build Renut's renderer in (the shaders come from out\renut_shader_dump, plus what Renut captured last run)

param(
    [int]$Jobs = 0,
    [switch]$Clean,
    [switch]$Renut
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$logDir = Join-Path $repo 'out\logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$sdk = Join-Path $repo 'sdk'
$build = Join-Path $repo 'out\pc'
$gameRoot = Join-Path $repo 'assets\extracted'

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
                # Show progress lines like [212/1480] now and then, plus anything that looks like an error.
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

. (Join-Path $PSScriptRoot 'env.ps1')

# 1. The game files (one time): the XBLA package (the LIVE file over 100 MB anywhere under assets\) extracted by our
#    own tools\stfs_extract.py into assets\extracted (default.xex, data.cpk).
if (-not (Test-Path (Join-Path $gameRoot 'default.xex'))) {
    $pkg = Get-ChildItem (Join-Path $repo 'assets') -Recurse -File | Where-Object { $_.Length -gt 100MB } | Select-Object -First 1
    if (-not $pkg) { Write-Host 'No XBLA package under assets\ and no assets\extracted: put the package in assets\.'; exit 1 }
    Step 'Extracting the XBLA package (one time, about 190 MB)' 'extract.log' {
        python (Join-Path $repo 'tools\stfs_extract.py') $pkg.FullName $gameRoot
    }
}

# 2. app\assets points at the game folder (a junction: no second copy, no admin rights needed).
$appAssets = Join-Path $repo 'app\assets'
if (Test-Path $appAssets) {
    $item = Get-Item $appAssets -Force
    if (-not ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        # A plain folder (made by an earlier step): replace it with the junction.
        Remove-Item -Recurse -Force $appAssets
    }
}
if (-not (Test-Path $appAssets)) {
    cmd /c mklink /J "$appAssets" "$gameRoot" | Out-Null
    Write-Host "app\assets now points at assets\extracted"
}

if ($Jobs -le 0) { $Jobs = [math]::Max(1, [math]::Min(8, [int]([Environment]::ProcessorCount / 2))) }
if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }

$cfgArgs = @(
    '-S', (Join-Path $repo 'app'), '-B', $build, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
    "-DREXSDK_DIR=$sdk",
    '-DCMAKE_C_COMPILER=clang', '-DCMAKE_CXX_COMPILER=clang++',
    '-DCMAKE_C_FLAGS=-march=x86-64-v2', '-DCMAKE_CXX_FLAGS=-march=x86-64-v2',
    '-DREXGLUE_ENABLE_TRACY=OFF',
    '-DREXGLUE_USE_D3D12=ON', '-DREXGLUE_USE_VULKAN=ON',
    "-DBANJO_RENUT_RENDERER=$(if ($Renut) { 'ON' } else { 'OFF' })",
    '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'
)

# The code generator writes the loaded game image here (post-codegen passes read it). Game data: never committed.
$env:REXGLUE_DUMP_IMAGE = Join-Path $repo 'out\renut_image.bin'

# 3. glslangValidator (one time): the renderer's EDRAM transfer shaders are compiled to SPIR-V with it at build
#    time. Built from the SDK's own glslang sources into out\glslang (rr6-nx's tools\build_glslang.bat, as PowerShell).
$glslang = Join-Path $repo 'out\glslang\StandAlone\glslangValidator.exe'
if (-not (Test-Path $glslang)) {
    $glslangBuild = Join-Path $repo 'out\glslang'
    Step 'Building glslangValidator (one time, a few minutes)' 'glslang.log' {
        # Every argument quoted: PowerShell splits an unquoted -DNAME=3.5 at the dot.
        $glArgs = @('-S', (Join-Path $sdk 'thirdparty\glslang'), '-B', $glslangBuild, '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=clang', '-DCMAKE_CXX_COMPILER=clang++',
            '-DENABLE_OPT=OFF', '-DGLSLANG_TESTS=OFF', '-DENABLE_GLSLANG_BINARIES=ON', '-DENABLE_HLSL=OFF',
            '-DGLSLANG_ENABLE_INSTALL=OFF', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5')
        cmake @glArgs
        if ($LASTEXITCODE -eq 0) { cmake --build $glslangBuild --target glslangValidator -- -j $Jobs }
    }
    if (-not (Test-Path $glslang)) { Write-Host "glslangValidator built but not at $glslang. Tell Claude."; exit 1 }
}

# 3b. Renut's shader tools need Python's xxhash module (one time).
# S4ep1-nx (2026-11-14): one Python for everything. CMake's find_package(Python3) picked a different python.exe than the
# `python` on PATH (Mark's first -Renut configure: xxhash installed for one, checked in the other), so the build is
# told which one to use, and the module checks and installs go to that same one.
$py = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $py) { Write-Host 'No python on PATH. Tell Claude.'; exit 1 }
$cfgArgs += "-DPython3_EXECUTABLE=$py"
Write-Host "Python: $py"
if ($Renut) {
    & $py -c "import xxhash" 2>$null
    if ($LASTEXITCODE -ne 0) {
        Step 'Installing the Python xxhash module (one time)' 'pip_xxhash.log' { & $py -m pip install --user xxhash }
        & $py -c "import xxhash" 2>$null
        if ($LASTEXITCODE -ne 0) { Write-Host "xxhash still missing for $py after the install. Tell Claude."; exit 1 }
    }
    $cfgArgs += "-DRENUT_SHADER_DUMP_DIR=$(Join-Path $repo 'out\renut_shader_dump')"
    # S4ep1-nx (2026-11-14): the shader dump directory must exist before the configure: when it does not, Renut's
    # gpu.cmake runs Renut's own tools/dump_shaders.py, which is not in this tree (TFU-nx always had the directory).
    # Ours: tools/renut_shader_dump.py takes the containers compiled into default.xex (8 in Sonic 4) from the code
    # generator's image; the rest Renut captures at run time and the next build adds them.
    $dump = Join-Path $repo 'out\renut_shader_dump'
    if (-not (Test-Path $dump)) {
        $image = Join-Path $repo 'out\renut_image.bin'
        if (Test-Path $image) {
            Step 'Dumping the shaders in default.xex (one time)' 'shader_dump.log' {
                & $py (Join-Path $repo 'tools\renut_shader_dump.py') --image $image --out $dump
            }
        } else {
            New-Item -ItemType Directory -Force -Path $dump | Out-Null
            Write-Host 'out\renut_shader_dump made empty (no code image yet): Renut captures the shaders at run time.'
        }
    }

    # 3c. XenosRecomp links Microsoft's DXC (Windows x64 binaries, kept out of git as rr6-nx did). One time: taken
    #     from renderbag/dxc-bin at the pinned commit (docs/upstream.md).
    $dxc = Join-Path $repo 'app\thirdparty\reblue-XenosRecomp\thirdparty\dxc-bin'
    if (-not (Test-Path (Join-Path $dxc 'lib\x64\dxcompiler.lib'))) {
        $dxcSrc = Join-Path $repo 'out\dxc-bin'
        Step 'Fetching the DXC binaries (one time)' 'dxc_fetch.log' {
            if (-not (Test-Path $dxcSrc)) { git clone -q https://github.com/renderbag/dxc-bin $dxcSrc }
            git -C $dxcSrc checkout -q 12395e36295f950361ca842d8cc461492412facc
        }
        foreach ($sub in 'bin\x64', 'lib\x64') {
            New-Item -ItemType Directory -Force -Path (Join-Path $dxc $sub) | Out-Null
            Copy-Item (Join-Path $dxcSrc "$sub\*") (Join-Path $dxc $sub) -Force
        }
    }
}

# 3d. The post-codegen passes (tools/post_codegen.py) make calls to unhooked game functions direct, so a function
#     hooked after the last code generation is still called past its hook (the first Renut boot: the game's own
#     CreateDevice ran, Renut's never did). The build runs the passes only with a new code generation; here they run
#     again when the set of functions the sources hook (tools/direct_calls.py hooked()) differs from the last run's.
#     (Until 2026-11-12 any newer source file triggered them, and their rewrite of the generated files recompiled all
#     the game code: 20+ minutes for a renderer change.)
$genDir = Join-Path $repo 'app\generated'
$hookList = Join-Path $repo 'out\post_codegen.hooks'
if (Test-Path (Join-Path $genDir 'codegen.stamp')) {
    $tools = Join-Path $repo 'tools'
    $now = (python -c "import sys; sys.path.insert(0, r'$tools'); import direct_calls; print(','.join(sorted(direct_calls.hooked())))" | Out-String).Trim()
    $last = if (Test-Path $hookList) { (Get-Content $hookList -Raw).Trim() } else { '' }
    if ($now -and $now -ne $last) {
        Step 'Post-codegen passes (the hooked functions changed; a few minutes, then the game code recompiles)' 'post_codegen.log' {
            $env:REXSDK_DIR = $sdk
            python (Join-Path $repo 'tools\post_codegen.py')
        }
        Set-Content -Path $hookList -Value $now -NoNewline
    }
}

# 4. Configure. (app\generated\rexglue.cmake is in git, so the build itself runs the code generator: no
#    separate translation step.)
Step 'Configuring' 'pc_configure.log' { cmake @cfgArgs }

# 5. Everything: the SDK, the code generator, the translation (about 20 min), the translated game code and the app.
Step "Translating and compiling ($Jobs jobs at a time; the translation step prints nothing for about 20 min)" 'pc_build.log' {
    cmake --build $build -- -j $Jobs
}

$exe = Join-Path $build 'renut.exe'
Write-Host ''
if (Test-Path $exe) {
    Write-Host "BUILD OK: $exe"
    Write-Host 'Next: powershell -ExecutionPolicy Bypass -File tools\win\boot_test.ps1'
} else {
    Write-Host 'The build finished but renut.exe is not where expected. Tell Claude.'
    exit 1
}
