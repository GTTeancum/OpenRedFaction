param(
    [string]$GameDirectory = 'D:\Programming\GitHub\OpenRedFaction\Installed_Game',
    [string]$GhidraDirectory = 'C:\Programming\ghidra_11.3.2_PUBLIC',
    [string]$JavaDirectory = 'C:\Program Files\Eclipse Adoptium\jdk-21.0.10.7-hotspot',
    [switch]$SkipAnalysis,
    [string[]]$FunctionAddresses = @()
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
    $selectedManifest = Join-Path $evidence 'selected-functions.txt'
    if ($FunctionAddresses.Count) {
        foreach ($address in $FunctionAddresses) {
            if ($address -notmatch '^(0[xX])?[0-9a-fA-F]{1,8}$') { throw "Invalid hexadecimal function address: $address" }
        }
        if (Test-Path -LiteralPath $selectedManifest) { Remove-Item -LiteralPath $selectedManifest }
    }
    $exportArgs = if ($FunctionAddresses.Count) { @('ExportSelected.java', $evidence) + $FunctionAddresses } else { @('ExportBaseline.java', $evidence) }
    & (Join-Path $GhidraDirectory 'support/analyzeHeadless.bat') $database $projectName @modeArgs -analysisTimeoutPerFile 1200 -max-cpu 4 -scriptPath (Join-Path $PSScriptRoot 'ghidra') -postScript @exportArgs -log (Join-Path $evidence 'analysis.log') -scriptlog (Join-Path $evidence 'scripts.log')
    if ($LASTEXITCODE -ne 0) { throw "Ghidra failed: $LASTEXITCODE" }
    # Headless Ghidra can exit zero after a script exception. Require a fresh
    # manifest written only after every requested function was exported.
    if ($FunctionAddresses.Count) {
        if (!(Test-Path -LiteralPath $selectedManifest)) { throw 'Selected export did not complete; inspect analysis.log' }
        $manifest = @(Get-Content -LiteralPath $selectedManifest)
        $expected = @($fingerprint) + @($FunctionAddresses | ForEach-Object { ([Convert]::ToUInt32(($_ -replace '^0[xX]', ''),16)).ToString('x') })
        if (($manifest -join ',') -ne ($expected -join ',')) { throw 'Selected export manifest does not match requested binary/functions' }
    }
} finally { $env:JAVA_HOME = $oldJava }
