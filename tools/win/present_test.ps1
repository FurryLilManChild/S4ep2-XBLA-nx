# TFU-nx (2026-11-11): the black-window test, several checks in one go. Two runs of the Renut build:
#   1. D3D12 (the default), 70 s, with two parts switched off and on in turn every 10 s (the in-run A/B):
#      the scan-out from the target's image (banjo_deko3d_scanout_handover, target 74) and the game's gamma ramp
#      (banjo_renut_game_gamma, target 86).
#   2. Vulkan instead of D3D12, 50 s.
# In both, at Present 60, 300, 700, 1000, 1300, 1600 and 1900 the build saves what the game drew (tfu_back_*.ppm)
# and what the window is handed (tfu_scanout_*.ppm). They are kept in out\logs\present_d3d12 and
# out\logs\present_vulkan.
# Usage (from C:\TFU-nx): powershell -ExecutionPolicy Bypass -File tools\win\present_test.ps1
# Watch the window in each run: say whether a picture ever shows, and roughly when.

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$frames = Join-Path $repo 'out\pc\logs'
$boot = Join-Path $PSScriptRoot 'boot_test.ps1'

function Run($name, $seconds, $backend, $extra) {
    Write-Host ''
    Write-Host "== Run: $name ($seconds s)"
    Get-ChildItem $frames -Filter 'tfu_*.ppm' -ErrorAction SilentlyContinue | Remove-Item -Force
    & $boot -Seconds $seconds -Renderer renut -Backend $backend -Extra $extra
    $keep = Join-Path $repo "out\logs\present_$name"
    New-Item -ItemType Directory -Force -Path $keep | Out-Null
    Get-ChildItem $keep -File | Remove-Item -Force
    Get-ChildItem $frames -Filter 'tfu_*.ppm' -ErrorAction SilentlyContinue | Copy-Item -Destination $keep
    Copy-Item (Join-Path $repo 'out\logs\boot_latest.log') (Join-Path $keep 'boot.log')
}

$env:TFU_PRESENT_DIAG = "1"
Run 'd3d12' 70 'd3d12' @('--banjo_renut_ab_targets=74,86', '--banjo_renut_ab_s=10')
Run 'vulkan' 50 'vulkan' @()
Write-Host ''
Write-Host 'Both runs done. Tell Claude whether either window showed a picture, and when.'
