param([switch]$BuildOnly)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$source=Join-Path $root 'HALO 3\maps\mainmenu.map'
$destination=Join-Path $root 'dist\map-reader'
if (!(Test-Path -LiteralPath $source)) { throw "Falta el mapa local: $source" }
New-Item -ItemType Directory -Path $destination -Force | Out-Null
$copy=Join-Path $destination 'mainmenu.map'
$originalHash=(Get-FileHash -LiteralPath $source).Hash
if (!(Test-Path -LiteralPath $copy) -or (Get-FileHash -LiteralPath $copy).Hash -ne $originalHash) {
    Copy-Item -LiteralPath $source -Destination $copy
}
if ((Get-FileHash -LiteralPath $copy).Hash -ne $originalHash) { throw 'La copia del mapa no coincide' }
& (Join-Path $PSScriptRoot 'build.ps1') -Target h3ps3_map_reader
if ($BuildOnly) { return }
$rpcRoot=Join-Path $root 'tools\RPCS3'
Push-Location $rpcRoot
try {
    & (Join-Path $rpcRoot 'rpcs3.exe') --no-gui --config (Join-Path $root 'config\rpcs3-map-reader.yml') (Join-Path $destination 'h3ps3_map_reader.self')
    if ($LASTEXITCODE -ne 0) { throw "RPCS3 termino con codigo $LASTEXITCODE" }
} finally { Pop-Location }
