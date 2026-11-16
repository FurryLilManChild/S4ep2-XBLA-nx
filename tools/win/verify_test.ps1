# TFU-nx (2026-11-13): build 4's PC check of the 805 localized copies (app/src/cpu/hot/localized_bulk.txt), one run of
# the Renut build (D3D12), 8 minutes, with banjo_cpu_verify on: every call of every copy also runs the recompiled
# original and the two are compared (registers and the guest memory written). Slower than normal play; that is expected.
# Get into gameplay and play normally (fights, Force powers, moving through the level) until it closes.
# Kept: the log as out\logs\verify_test.log, each copy's counts as out\logs\verify_counts.txt (tools/verified_copies.py).
# Usage (from C:\TFU-nx): powershell -ExecutionPolicy Bypass -File tools\win\verify_test.ps1

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$boot = Join-Path $PSScriptRoot 'boot_test.ps1'
& $boot -Seconds 480 -Renderer renut -Extra @('--banjo_cpu_verify=true')
Copy-Item (Join-Path $repo 'out\logs\boot_latest.log') (Join-Path $repo 'out\logs\verify_test.log')
$counts = Join-Path $repo 'out\pc\logs\rr6_hot_counts.txt'
if (Test-Path $counts) {
    Copy-Item $counts (Join-Path $repo 'out\logs\verify_counts.txt')
    Write-Host 'Kept as out\logs\verify_test.log and out\logs\verify_counts.txt'
} else {
    Write-Host 'Kept as out\logs\verify_test.log (no copy counts were written: tell Claude)'
}
