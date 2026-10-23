param([string]$SdkRoot = (Join-Path (Split-Path $PSScriptRoot -Parent) 'tools\PS3DK'), [string]$Target='')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
$toolchain = Join-Path $SdkRoot 'cmake\ps3-ppu-toolchain.cmake'
if (!(Test-Path -LiteralPath $toolchain)) { throw "Falta el toolchain de PS3DK: $toolchain" }
# Variables solo para este proceso; no se modifica el registro ni el PATH persistente.
$env:PS3DK = $SdkRoot
$env:PS3DEV = $SdkRoot
$env:PSL1GHT = $SdkRoot
$cmakeBin = Join-Path $projectRoot 'tools\cmake-4.4.4-windows-x86_64\bin'
$env:PATH = "$cmakeBin;$projectRoot\tools;$projectRoot\tools\git\cmd;$SdkRoot\bin;$SdkRoot\ppu\bin;$SdkRoot\spu\bin;$env:PATH"
foreach ($tool in @('cmake','ninja','powerpc64-ps3-elf-gcc')) {
    if (!(Get-Command $tool -ErrorAction SilentlyContinue)) { throw "Herramienta ausente: $tool" }
}
& powerpc64-ps3-elf-gcc --version
if ($LASTEXITCODE) { throw 'El compilador PPU no se ejecuta' }
& cmake -S $projectRoot -B "$projectRoot\build" -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
if ($LASTEXITCODE) { throw 'Fallo de configuracion CMake' }
if ($Target) { & cmake --build "$projectRoot\build" --target $Target } else { & cmake --build "$projectRoot\build" }
if ($LASTEXITCODE) { throw 'Fallo de compilacion' }
foreach ($artifactDir in @($projectRoot, (Join-Path $projectRoot 'dist\map-reader'), (Join-Path $projectRoot 'dist\tag-browser'), (Join-Path $projectRoot 'dist\texture-viewer'))) {
    if (Test-Path -LiteralPath $artifactDir) { Get-ChildItem -LiteralPath $artifactDir -File -Filter 'h3ps3*.self' | Select-Object FullName,Length }
}



