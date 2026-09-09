param([int]$Readers = 3, [switch]$Mixed, [string]$Dictionary = "", [string]$Model = "")
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$processes = @()
$platforms = @()
try {
    for ($i = 0; $i -lt $Readers; $i++) {
        $info = New-Object Diagnostics.ProcessStartInfo
        $platform = if ($Mixed) { @('ARM64','x64','Win32')[$i % 3] } else { 'ARM64' }
        $platforms += $platform
        $info.FileName = "$root\build\tests\$platform\sentence_measure_probe.exe"
        if (!$Dictionary) { $Dictionary = "$root\build\real-sentence-measure\user\schemas\虎整句\tiger-v2.tcd" }
        if (!$Model) { $Model = 'C:\Users\yc\Desktop\bime_codex_src_20260513\release_arm64\Models\sentence-ngram-v2.bin' }
        $info.Arguments = '"' + $dictionary + '" "' + $model + '" --memory'
        $info.UseShellExecute = $false
        $info.RedirectStandardInput = $true
        $info.RedirectStandardOutput = $true
        $info.CreateNoWindow = $true
        $process = [Diagnostics.Process]::Start($info)
        $processes += $process
        $ready = $process.StandardOutput.ReadLineAsync()
        if (!$ready.Wait(30000) -or $ready.Result -ne 'ready') { throw 'Reader did not become ready' }
    }
    $results = @()
    $index = 0
    foreach ($process in $processes) {
        $process.StandardInput.WriteLine(''); $process.StandardInput.Flush()
        $line = $process.StandardOutput.ReadLineAsync()
        if (!$line.Wait(30000)) { throw 'Reader measurement timed out' }
        $result = $line.Result | ConvertFrom-Json
        foreach ($mapping in @($result.model,$result.lexicon)) {
            if ($mapping.type -ne 262144 -or $mapping.protect -ne 2) { throw 'Expected read-only file mapping' }
            if ($mapping.multiple -lt $mapping.pages * 0.9) { throw 'Less than 90 percent multiply shared pages' }
        }
        $result | Add-Member -NotePropertyName architecture -NotePropertyValue $platforms[$index]
        $index++
        $results += $result
    }
    $name = if ($Mixed) { 'memory-mixed.json' } else { 'memory-ARM64.json' }
    $results | ConvertTo-Json -Depth 5 | Set-Content "$root\build\real-sentence-measure\$name" -Encoding UTF8
    $results | ConvertTo-Json -Depth 5
} finally {
    foreach ($process in $processes) {
        if (!$process.HasExited) {
            $process.StandardInput.WriteLine(''); $process.StandardInput.Flush()
            if (!$process.WaitForExit(5000)) { $process.Kill() }
        }
        $process.Dispose()
    }
}
