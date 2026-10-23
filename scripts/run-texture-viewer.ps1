param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if (!$SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Target h3ps3_texture_viewer }
if (!(Test-Path -LiteralPath (Join-Path $projectRoot 'HALO 3\maps\mainmenu.map'))) { throw 'Falta la copia local de mainmenu.map' }
$rpcRoot=Join-Path $projectRoot 'tools\RPCS3'
$vfs=Join-Path $rpcRoot 'config\vfs.yml'
$expected=Join-Path $projectRoot 'config\vfs-tag-browser.yml'
if ((Test-Path -LiteralPath $vfs) -and [IO.File]::ReadAllText($vfs) -ne [IO.File]::ReadAllText($expected)) {
 Copy-Item -LiteralPath $vfs -Destination "$vfs.before-texture-$(Get-Date -Format yyyyMMdd-HHmmss).bak"
}
Copy-Item -LiteralPath $expected -Destination $vfs -Force
$started=Get-Date
& (Join-Path $rpcRoot 'rpcs3.exe') --no-gui --config (Join-Path $projectRoot 'config\rpcs3-map-reader.yml') (Join-Path $projectRoot 'dist\texture-viewer\h3ps3_texture_viewer.self') | Out-Host
if ($LASTEXITCODE) { throw "RPCS3 devolvio $LASTEXITCODE" }
$tty=Join-Path $rpcRoot 'log\TTY.log'
if ((Get-Item -LiteralPath $tty).LastWriteTime -lt $started) { throw 'El log pertenece a otra ejecucion' }
$log=Join-Path $projectRoot 'logs\texture-viewer-TTY.log'
Copy-Item -LiteralPath $tty -Destination $log -Force
$text=[IO.File]::ReadAllText($log)
if ($text -notmatch 'H3TEXTURE_TESTS 8/8' -or $text -notmatch 'H3TEXTURE_PASS' -or $text -notmatch 'H3TEXTURE_EXIT 0') { throw 'El visor no confirmo todas las comprobaciones' }
