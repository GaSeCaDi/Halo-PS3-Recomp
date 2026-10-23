param([switch]$Import, [string]$OutputName='startup', [string]$Entry='82582a98', [string[]]$ExtraSeeds=@())
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$jdk=Get-ChildItem -LiteralPath (Join-Path $root 'tools') -Directory -Filter 'jdk-21*' | Select-Object -First 1
if (!$jdk) { throw 'Falta el JDK portable' }
$env:JAVA_HOME=$jdk.FullName
$env:PATH="$env:JAVA_HOME\bin;$env:PATH"
$userDir=Join-Path $root 'tools\ghidra-user'
New-Item -ItemType Directory -Path $userDir -Force | Out-Null
$env:APPDATA=Join-Path $userDir 'AppData\Roaming'
$env:LOCALAPPDATA=Join-Path $userDir 'AppData\Local'
New-Item -ItemType Directory -Path $env:APPDATA,$env:LOCALAPPDATA -Force | Out-Null
$env:JAVA_TOOL_OPTIONS="-Duser.home=$userDir -Dapplication.settingsdir=$userDir"
$headless=Join-Path $root 'tools\ghidra_12.1.3_PUBLIC\support\analyzeHeadless.bat'
if (!(Test-Path -LiteralPath $headless)) { throw 'Falta Ghidra' }
$projects=Join-Path $root 'research\ghidra-project'
$out=Join-Path (Join-Path $root 'research') $OutputName
New-Item -ItemType Directory -Path $projects,$out -Force | Out-Null
$scriptPath=Join-Path $root 'scripts\ghidra'
$baseArgs=@($projects,'Halo3Research','-max-cpu','2','-noanalysis','-scriptPath',$scriptPath,'-postScript','ModelXboxImports.java',(Join-Path $root 'research\input\default.xex'),$out,'-postScript','RefineControlFlow.java',$out,'-postScript','ExportStartup.java',$out,$Entry)+$ExtraSeeds+@('-log',(Join-Path $root 'logs\ghidra-import.log'),'-scriptlog',(Join-Path $root 'logs\ghidra-script.log'))
if ($Import) {
    $inputFile=Join-Path $root 'research\input\default.xex'
    if (!(Test-Path -LiteralPath $inputFile)) { throw 'Falta la copia de trabajo del XEX' }
    $baseArgs+=@('-import',$inputFile)
} else {
    $baseArgs+=@('-process','default.xex')
}
& $headless @baseArgs
if ($LASTEXITCODE -ne 0) { throw "Ghidra termino con codigo $LASTEXITCODE" }


