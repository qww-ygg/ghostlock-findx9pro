# GhostLock build for Windows (NDK ships clang only as a .cmd wrapper).
#
#   $env:NDK_ROOT = "C:\android-ndk-r27d"
#   .\build.ps1
#
# Notes:
#  * -DTARGET_CONFIG_H is NOT passed. The wrapper mangles quoted -D values, so
#    offset.h defaults TARGET_CONFIG_H to "target.h" instead.
#  * TMP must point at a writable, short-ish path; some NDK toolchains fail with
#    very long temp names.

param(
    [string]$NdkRoot = $env:NDK_ROOT,
    [string]$Api     = "35",
    [string]$Out     = "ghostlock"
)

$ErrorActionPreference = "Stop"

if (-not $NdkRoot) {
    throw "Set NDK_ROOT (or pass -NdkRoot) to an unpacked Android NDK r26+."
}

$clang = Join-Path $NdkRoot "toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android$Api-clang.cmd"
if (-not (Test-Path $clang)) {
    throw "clang wrapper not found: $clang"
}

$srcs = @(
    "main.c", "util.c", "slide.c", "fops.c",
    "pipe_physrw.c", "root.c", "miniadb.c", "umh_root.c"
) | ForEach-Object { Join-Path "src\core" $_ }

$common = @(
    "-O2", "-Wall", "-Wno-unused-parameter", "-Wno-sign-compare",
    "-Wno-unused-function", "-Isrc/core", "-Isrc/devices",
    "-fPIE", "-pie", "-pthread"
)

Write-Host "==> $clang"
& $clang @common @srcs -o $Out
if ($LASTEXITCODE -ne 0) { throw "compile failed ($LASTEXITCODE)" }

$f = Get-Item $Out
$md5 = (Get-FileHash $Out -Algorithm MD5).Hash.ToLower()
Write-Host ("==> {0}  {1} bytes  md5={2}" -f $f.Name, $f.Length, $md5)

# Sanity checks that have caught real regressions:
#  * the runtime offset table must be the only source of per-device values
#  * the root script must not unload the module (it panics the kernel)
$bytes = [IO.File]::ReadAllBytes((Resolve-Path $Out))
# Windows PowerShell 5.1 has no Latin1 property; 28591 is ISO-8859-1.
$text = [Text.Encoding]::GetEncoding(28591).GetString($bytes)

if ($text -notmatch "ghostlock_findx9pro") { Write-Warning "variant label missing" }
# `ksud unload` panics this kernel (the kallsyms loader skips load_module()'s
# sysfs/RCU registration). Only an actual command counts — comments are stripped
# first, because the script explains the hazard in a comment right next to it.
$scriptLines = $text -split "`n" | Where-Object { $_ -notmatch '^\s*#' }
$unloadCall = $scriptLines | Where-Object { $_ -match '\bunload\b' }
if ($unloadCall) {
    throw "root script still invokes unload: $($unloadCall -join ' | ')"
}
if ($text -notmatch "ksud insmod allow_shell=1 \(first load\)") {
    Write-Warning "first-load allow_shell path missing"
}

foreach ($f2 in "fops.c", "util.c", "slide.c", "root.c", "umh_root.c") {
    $pp = & $clang -E -O2 "-Isrc/core" "-Isrc/devices" (Join-Path "src\core" $f2) 2>$null | Out-String
    if ($pp -match "active_offsets->off_") {
        Write-Host ("    ok   {0} binds runtime offsets" -f $f2)
    } else {
        throw "$f2 does not use active_offsets — compile-time offsets regressed"
    }
}
