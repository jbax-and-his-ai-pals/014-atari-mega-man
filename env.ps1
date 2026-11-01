# Put the project toolchain on PATH for this PowerShell session.
# Usage:  . .\env.ps1
$root = $PSScriptRoot
$tc = Join-Path $root "toolchain"
$dirs = @(
    (Join-Path $tc "arm\bin"),
    (Join-Path $tc "dasm"),
    (Join-Path $tc "make\bin"),
    (Join-Path $tc "stella")
)
foreach ($d in $dirs) {
    if (-not (Test-Path $d)) { Write-Warning "missing $d - run: py -3 tools\setup_toolchain.py" }
}
$env:PATH = ($dirs -join ";") + ";" + $env:PATH
$env:MEGA_ROOT = $root
Write-Host "Toolchain on PATH: arm-none-eabi-gcc, dasm, make, Stella"
