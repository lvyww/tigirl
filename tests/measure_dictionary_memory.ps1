param([int]$Readers = 4)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$processes = @()
try {
    for ($i = 0; $i -lt $Readers; $i++) {
        $info = New-Object Diagnostics.ProcessStartInfo
        $info.FileName = "$root\build\tests\ARM64\dictionary_probe.exe"
        $info.Arguments = '"' + "$root\data\tiger-v2.tcd" + '" memory'
        $info.UseShellExecute = $false
        $info.RedirectStandardInput = $true
        $info.RedirectStandardOutput = $true
        $info.CreateNoWindow = $true
        $process = [Diagnostics.Process]::Start($info)
        $processes += $process
        $summary = $process.StandardOutput.ReadLine()
        if ($process.StandardOutput.ReadLine() -ne 'ready') { throw "Reader $i failed: $summary" }
    }
    $results = @()
    foreach ($process in $processes) {
        $process.StandardInput.WriteLine('')
        $process.StandardInput.Flush()
        $result = $process.StandardOutput.ReadLine() | ConvertFrom-Json
        if ($result.resident_pages -lt 1 -or $result.multiply_shared_pages -lt $result.pages * 0.9) {
            throw "Expected at least 90% of mapped pages to have multiple sharers: $($result | ConvertTo-Json -Compress)"
        }
        if ($result.private_bytes -ge $result.mapped_bytes / 2) { throw 'Private memory unexpectedly approaches table size.' }
        $results += $result
    }
    $results | ConvertTo-Json | Set-Content "$root\build\dictionary-memory-arm64.json" -Encoding UTF8
    $results | Format-Table pid,mapped_bytes,private_bytes,pages,multiply_shared_pages
} finally {
    foreach ($process in $processes) {
        if (!$process.HasExited) {
            $process.StandardInput.WriteLine('')
            $process.StandardInput.Flush()
            if (!$process.WaitForExit(5000)) { $process.Kill() }
        }
        $process.Dispose()
    }
}
