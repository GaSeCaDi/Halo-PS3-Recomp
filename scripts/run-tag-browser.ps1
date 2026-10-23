param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if (!$SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Target h3ps3_tag_browser }
$mapRoot=Join-Path $projectRoot 'HALO 3\maps'
if (!(Test-Path -LiteralPath $mapRoot)) { throw "Falta la copia local: $mapRoot" }
$dist=Join-Path $projectRoot 'dist\tag-browser'
$maps=Get-ChildItem -LiteralPath $mapRoot -File -Filter '*.map' | Sort-Object Name
if (!$maps -or !($maps.Name -contains 'mainmenu.map')) { throw 'Falta mainmenu.map' }
foreach ($map in $maps) { if ($map.Name -cnotmatch '^[a-z0-9_]+\.map$') { throw "Nombre no admitido: $($map.Name)" } }
[IO.File]::WriteAllLines((Join-Path $dist 'maps.txt'),[string[]]$maps.Name,[Text.Encoding]::ASCII)
$rpcRoot=Join-Path $projectRoot 'tools\RPCS3'
$vfsPath=Join-Path $rpcRoot 'config\vfs.yml'
$expected=Join-Path $projectRoot 'config\vfs-tag-browser.yml'
# Preserve any existing custom emulator mount configuration before replacement.
if (Test-Path -LiteralPath $vfsPath) {
 if ([IO.File]::ReadAllText($vfsPath) -ne [IO.File]::ReadAllText($expected)) {
  Copy-Item -LiteralPath $vfsPath -Destination "$vfsPath.before-h3ps3-$(Get-Date -Format yyyyMMdd-HHmmss).bak"
 }
}
Copy-Item -LiteralPath $expected -Destination $vfsPath -Force
$runStarted=Get-Date
& (Join-Path $rpcRoot 'rpcs3.exe') --no-gui --config (Join-Path $projectRoot 'config\rpcs3-map-reader.yml') (Join-Path $dist 'h3ps3_tag_browser.self') | Out-Host
if ($LASTEXITCODE) { throw "RPCS3 devolvio $LASTEXITCODE" }
if ((Get-Item -LiteralPath (Join-Path $rpcRoot 'log\TTY.log')).LastWriteTime -lt $runStarted) { throw 'El log no pertenece a esta ejecucion' }
Copy-Item -LiteralPath (Join-Path $rpcRoot 'log\TTY.log') -Destination (Join-Path $projectRoot 'logs\tag-browser-TTY.log') -Force
$result=Get-Content -LiteralPath (Join-Path $projectRoot 'logs\tag-browser-TTY.log') -Raw
if ($result -notmatch 'H3INDEX_EXIT 0' -or $result -notmatch 'H3RESOURCE_PASS') { throw 'No se confirmo la prueba completa' }
