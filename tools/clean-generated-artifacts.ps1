param([switch]$Apply)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$artifactRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'artifacts'))
$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'build'))
$expectedRoot = [System.IO.Path]::GetFullPath('D:\Programming\GitHub\OpenRedFaction').TrimEnd('\')
if (-not [string]::Equals($projectRoot.TrimEnd('\'), $expectedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Unexpected project root: $projectRoot"
}
if (-not [string]::Equals($artifactRoot.TrimEnd('\'), (Join-Path $expectedRoot 'artifacts'), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Unexpected artifacts root: $artifactRoot"
}
if ((Test-Path -LiteralPath $artifactRoot) -and ((Get-Item -LiteralPath $artifactRoot -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
    throw 'Artifacts root is a reparse point'
}
if (-not [string]::Equals($buildRoot.TrimEnd('\'), (Join-Path $expectedRoot 'build'), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Unexpected build root: $buildRoot"
}
if ((Test-Path -LiteralPath $buildRoot) -and ((Get-Item -LiteralPath $buildRoot -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
    throw 'Build root is a reparse point'
}

# These are regenerable run payloads. Keep compact JSON reports, validation
# results, text logs, source inputs, and the reusable QCOW2 test HDD.
$minimumByExtension = @{
    '.vpp' = 1MB; '.iso' = 1MB; '.xbe' = 1MB; '.wav' = 1MB
    '.bty' = 1MB; '.map' = 1MB; '.bin' = 1MB; '.obj' = 1MB
    '.ppm' = 0; '.png' = 0
}
$buildExtensions = @('.iso', '.vpp')
$items = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
$pending = [System.Collections.Generic.Stack[System.IO.DirectoryInfo]]::new()
if (Test-Path -LiteralPath $artifactRoot) { $pending.Push((Get-Item -LiteralPath $artifactRoot -Force)) }
if (Test-Path -LiteralPath $buildRoot) { $pending.Push((Get-Item -LiteralPath $buildRoot -Force)) }
$skippedLinks = 0
while ($pending.Count) {
    $directory = $pending.Pop()
    foreach ($entry in Get-ChildItem -LiteralPath $directory.FullName -Force) {
        if ($entry.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            $skippedLinks++
            continue
        }
        if ($entry -is [System.IO.DirectoryInfo]) {
            $pending.Push($entry)
        } else {
            $extension = $entry.Extension.ToLowerInvariant()
            $underArtifacts = $entry.FullName.StartsWith($artifactRoot.TrimEnd('\') + '\', [System.StringComparison]::OrdinalIgnoreCase)
            $underBuild = $entry.FullName.StartsWith($buildRoot.TrimEnd('\') + '\', [System.StringComparison]::OrdinalIgnoreCase)
            if (($underArtifacts -and $minimumByExtension.ContainsKey($extension) -and $entry.Length -ge $minimumByExtension[$extension]) -or
                ($underBuild -and $buildExtensions.Contains($extension) -and $entry.Length -ge 1MB)) {
                $items.Add($entry)
            }
        }
    }
}
$bytes = ($items | Measure-Object -Property Length -Sum).Sum
if (-not $bytes) { $bytes = 0 }
Write-Output ("Generated artifact/build payloads: {0} files, {1:N2} GiB" -f $items.Count, ($bytes / 1GB))
Write-Output ("Skipped {0} junctions/symlinks." -f $skippedLinks)
if (-not $Apply) {
    Write-Output 'Dry run. Pass -Apply to remove these generated payloads.'
    return
}

$busy = @(Get-CimInstance Win32_Process | Where-Object {
    $_.Name -in @('xemu.exe', 'python.exe', 'bash.exe', 'make.exe') -and
    $_.CommandLine -and $_.CommandLine.IndexOf($expectedRoot, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
})
if ($busy.Count) { throw "Project processes are still running: $($busy.ProcessId -join ', ')" }

$removedBytes = [long]0
foreach ($item in $items) {
    $resolved = [System.IO.Path]::GetFullPath($item.FullName)
    if (-not ($resolved.StartsWith($artifactRoot.TrimEnd('\') + '\', [System.StringComparison]::OrdinalIgnoreCase) -or
              $resolved.StartsWith($buildRoot.TrimEnd('\') + '\', [System.StringComparison]::OrdinalIgnoreCase))) {
        throw "Refusing path outside generated roots: $resolved"
    }
    if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Refusing reparse point: $resolved"
    }
    Remove-Item -LiteralPath $resolved -Force
    $removedBytes += $item.Length
}
Write-Output ("Removed {0} generated files, {1:N2} GiB." -f $items.Count, ($removedBytes / 1GB))
