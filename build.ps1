<#
.SYNOPSIS
    Builds the fixed dxgi.dll for Wild Hearts (native Windows, no WSL needed).

.DESCRIPTION
    PowerShell equivalent of build.sh. Runs llvm-dlltool, clang-cl and lld-link
    to produce build\dxgi.dll.

.PARAMETER LLVMPath
    Directory containing clang-cl.exe, lld-link.exe and llvm-dlltool.exe.
    Defaults to $env:LLVM, then "C:\Program Files\LLVM\bin".

.EXAMPLE
    .\build.ps1

.EXAMPLE
    .\build.ps1 -LLVMPath 'D:\Tools\LLVM\bin'

.NOTES
    If script execution is blocked, run this in the current window first:
        Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

    Requires LLVM (clang-cl, lld-link, llvm-dlltool). Download:
        https://releases.llvm.org/download.html
#>
[CmdletBinding()]
param(
    [string]$LLVMPath = $env:LLVM
)

$ErrorActionPreference = 'Stop'

$Root = $PSScriptRoot
if (-not $Root) { $Root = Split-Path -Parent $MyInvocation.MyCommand.Path }
Set-Location -LiteralPath $Root

$Src = Join-Path $Root 'src'
$Out = Join-Path $Root 'build'

if (-not $LLVMPath) { $LLVMPath = 'C:\Program Files\LLVM\bin' }

function Resolve-Tool([string]$name) {
    $exe = Join-Path $LLVMPath "$name.exe"
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
        throw "'$exe' not found. Install LLVM from https://releases.llvm.org/download.html or pass -LLVMPath."
    }
    return $exe
}

function Invoke-Step([string]$label, [string]$tool, [string[]]$arguments) {
    Write-Host "==> $label"
    Write-Host ("    {0} {1}" -f (Split-Path -Leaf $tool), ($arguments -join ' '))
    & $tool @arguments
    if ($LASTEXITCODE -ne 0) { throw "$label failed (exit code $LASTEXITCODE)" }
}

Write-Host 'Wild Hearts - dxgi.dll priority fix'
Write-Host ''
Write-Host "==> environment : Windows (native PowerShell)"
Write-Host "==> toolchain   : $LLVMPath"
Write-Host "==> sources     : $Src"
Write-Host "==> output      : $Out"
Write-Host ''

$dlltool = Resolve-Tool 'llvm-dlltool'
$clang   = Resolve-Tool 'clang-cl'
$link    = Resolve-Tool 'lld-link'

$source = Join-Path $Src 'dxgi_proxy.c'
if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
    throw "'$source' is missing."
}
if (-not (Test-Path -LiteralPath $Out -PathType Container)) {
    New-Item -ItemType Directory -Path $Out | Out-Null
}

$lib     = Join-Path $Out 'kernel32.lib'
$obj     = Join-Path $Out 'dxgi_proxy.obj'
$dll     = Join-Path $Out 'dxgi.dll'
$def     = Join-Path $Src 'dxgi.def'
$k32def  = Join-Path $Src 'kernel32.def'

Invoke-Step 'import library' $dlltool @(
    '-m', 'i386:x86-64', '-d', $k32def, '-l', $lib
)

Invoke-Step 'compiling' $clang @(
    '-nologo', '-c', '-O2', '-GS-', '-Zl', '-W4',
    '-Wno-unused-parameter', '-Wno-incompatible-library-redeclaration',
    '-fno-builtin', '--target=x86_64-pc-windows-msvc',
    "-Fo$obj", $source
)

Invoke-Step 'linking' $link @(
    '-nologo', '-dll', '-nodefaultlib', '-entry:DllMain', '-machine:x64', '-brepro',
    "-def=${def}", "-out=${dll}", $obj, $lib
)

$size = (Get-Item -LiteralPath $dll).Length
Write-Host ''
Write-Host "Built $dll ($size bytes)"
Write-Host 'Install it next to the game executable, or as an OptiScaler plugin - see the README.'
