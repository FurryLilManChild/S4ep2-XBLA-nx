# S4ep1-nx (from TFU-nx), originally from LoS1-nx tools/win/boot_test.ps1.
# Starts the PC build, lets it run for a while, then closes it and keeps the log.
# -Renderer renut uses Renut's renderer, -Renderer xenos the emulated GPU (default: what los.toml says).
# -Backend vulkan or d3d12 picks Renut's graphics API (default: D3D12 on Windows).
# Usage (from C:\S4ep1-nx): powershell -ExecutionPolicy Bypass -File tools\win\boot_test.ps1 [-Seconds 60]
# The game window opens; you can watch it (and play if it gets that far). It is closed after -Seconds.
# Each run's log: out\logs\boot_<date>_<time>.log, and out\logs\boot_latest.log is always the newest.

param([int]$Seconds = 60, [string]$Renderer = '', [string]$Backend = '', [string[]]$Extra = @())

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exe = Join-Path $repo 'out\pc\renut.exe'
$game = Join-Path $repo 'assets\extracted'
$logDir = Join-Path $repo 'out\logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

if (-not (Test-Path $exe)) { Write-Host 'out\pc\renut.exe is missing: run tools\win\build_pc.ps1 first.'; exit 1 }

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$log = Join-Path $logDir "boot_$stamp.log"

$runArgs = @("`"--game_data_root=$game`"", "`"--log_file=$log`"", "`"--user_data_root=$repo\out\pc_user`"", "--log_file_writer=false")
if ($Renderer) { $runArgs += "--banjo_renderer=$Renderer" }
# TFU-nx: without Renut (the first builds) the SDK's emulated GPU draws; rr6-nx never loads it (gpu_plugin empty).
if (-not $Renderer) { $runArgs += "--banjo_renderer=emulated"; $runArgs += "--gpu_plugin=xenos" }  # S4ep1-nx: a -Renut build otherwise falls back to Renut (banjo_renderer defaults to "native")
if ($Backend) { $runArgs += "--renut_gpu_backend=$Backend" }  # Renut: d3d12 or vulkan
if ($Extra) { $runArgs += $Extra }  # TFU-nx: more flags (present_test.ps1)
Write-Host "Starting the game for $Seconds seconds..."
$p = Start-Process -FilePath $exe -ArgumentList $runArgs -WorkingDirectory (Split-Path $exe) -PassThru

$exited = $p.WaitForExit($Seconds * 1000)
if ($exited) {
    $result = "The game closed by itself after less than $Seconds s, exit code $($p.ExitCode) (a crash, most likely)."
} else {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    $result = "Still running after $Seconds s: closed it."
}

Add-Content -Path $log -Value "`n[boot_test] $result"
Copy-Item $log (Join-Path $logDir 'boot_latest.log') -Force
Write-Host $result
Write-Host "Log: out\logs\boot_$stamp.log (Claude can read it from the connected folder)."
