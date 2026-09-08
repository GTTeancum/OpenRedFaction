param(
    [string]$GameDirectory = 'D:\Programming\GitHub\OpenRedFaction\Installed_Game',
    [string]$GhidraDirectory = 'C:\Programming\ghidra_11.3.2_PUBLIC',
    [string]$JavaDirectory = 'C:\Program Files\Eclipse Adoptium\jdk-21.0.10.7-hotspot',
    [switch]$SkipAnalysis
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$binary = Join-Path $GameDirectory 'RF.exe'
$fingerprint = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
$projectName = 'rf_' + $fingerprint.Substring(0, 12)
$database = Join-Path $projectRoot 'local/ghidra'
$evidence = Join-Path $projectRoot "artifacts/analysis/$projectName"
New-Item -ItemType Directory -Force $database,$evidence | Out-Null
$oldJava = $env:JAVA_HOME
try {
    $env:JAVA_HOME = $JavaDirectory
    $modeArgs = if (Test-Path (Join-Path $database "$projectName.gpr")) { @('-process', 'RF.exe') } else { @('-import', $binary) }
    if ($SkipAnalysis) { $modeArgs += '-noanalysis' }
    & (Join-Path $GhidraDirectory 'support/analyzeHeadless.bat') $database $projectName @modeArgs -analysisTimeoutPerFile 1200 -max-cpu 4 -scriptPath (Join-Path $PSScriptRoot 'ghidra') -postScript ExportBaseline.java $evidence -log (Join-Path $evidence 'analysis.log') -scriptlog (Join-Path $evidence 'scripts.log')
    if ($LASTEXITCODE -ne 0) { throw "Ghidra failed: $LASTEXITCODE" }
} finally { $env:JAVA_HOME = $oldJava }
