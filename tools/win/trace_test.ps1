# TFU-nx (2026-11-12): build 2's PC check, one run of the Renut build (D3D12), 4 minutes:
#   - every 30 s one frame traced: each draw with its vertex and pixel shaders and its target (with the fast draw
#     path off, so every draw is listed), to find TFU's final composite (the 540p handheld setting needs it);
#   - the job workers' idle wait (tools/tfu_spin_idle.py) is on: its "[tfu] job workers" line every 10 s shows it runs.
# Get into gameplay and play normally until it closes. The log is kept as out\logs\trace_test.log.
# Usage (from C:\TFU-nx): powershell -ExecutionPolicy Bypass -File tools\win\trace_test.ps1

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$boot = Join-Path $PSScriptRoot 'boot_test.ps1'
& $boot -Seconds 240 -Renderer renut -Extra @('--banjo_renut_diag_resolve_trace=true', '--banjo_renut_fast_draw=false')
Copy-Item (Join-Path $repo 'out\logs\boot_latest.log') (Join-Path $repo 'out\logs\trace_test.log')
Write-Host 'Kept as out\logs\trace_test.log'
