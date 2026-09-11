param(
    [string]$Version='2026.9.10.4',
    [string]$DataSource="$PSScriptRoot\resources\DefaultData",
    [string]$SentenceModelPath="$PSScriptRoot\..\bime_codex_src_20260513\release_arm64\Models\sentence-ngram-v2.bin",
    [switch]$SkipBuild
)
$ErrorActionPreference='Stop'
if($Version -notmatch '^\d+\.\d+\.\d+\.\d+$'){throw 'Version must contain four numbers.'}
if(!$SkipBuild){& "$PSScriptRoot\build_x64.ps1" -SentenceModelPath $SentenceModelPath;if($LASTEXITCODE){throw 'Release build failed.'}}
$stage="$PSScriptRoot\build\packages\Tigirl-$Version-x64"
if(Test-Path -LiteralPath $stage){throw "Package already exists; use a new version or remove the old staging directory: $stage"}
New-Item -ItemType Directory -Force "$stage\x64","$stage\x86","$stage\DefaultData"|Out-Null
foreach($file in Get-ChildItem -LiteralPath "$PSScriptRoot\packaging" -File){Copy-Item -LiteralPath $file.FullName -Destination $stage}
New-Item -ItemType Directory -Force "$stage\setup"|Out-Null
foreach($file in @('deploy.ps1')){Copy-Item -LiteralPath "$PSScriptRoot\packaging\setup\$file" -Destination "$stage\setup"}
& "$env:windir\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:winexe /platform:x64 /optimize+ "/win32icon:$PSScriptRoot\assets\Tigirl.ico" "/win32manifest:$PSScriptRoot\packaging\setup\maintenance.manifest" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll "/out:$stage\setup\Tigirl.Maintenance.exe" "$PSScriptRoot\packaging\setup\Maintenance.cs"
if($LASTEXITCODE){throw 'Maintenance build failed.'}
Copy-Item -LiteralPath "$PSScriptRoot\assets\Tigirl.ico" -Destination $stage
Copy-Item -LiteralPath "$PSScriptRoot\appcontainer_data.ps1" -Destination $stage
foreach($file in @('Tigirl.dll','Tigirl.exe','Tigirl.Import.exe','Tigirl.Reminder.exe','tiger-v2.tcd','Models','字体')){Copy-Item -LiteralPath "$PSScriptRoot\build\x64\Release\$file" -Destination "$stage\x64" -Recurse}
Copy-Item -LiteralPath "$PSScriptRoot\build\Win32\Release\Tigirl.dll" -Destination "$stage\x86"
Copy-Item -LiteralPath "$PSScriptRoot\build\tests\x64\architecture_load_probe.exe" -Destination "$stage\verify_x64.exe"
Copy-Item -LiteralPath "$PSScriptRoot\build\tests\Win32\architecture_load_probe.exe" -Destination "$stage\verify_x86.exe"
New-Item -ItemType Directory -Force "$stage\DefaultData\码表"|Out-Null
foreach($scheme in @('虎码字词','虎整句')){Copy-Item -LiteralPath "$DataSource\码表\$scheme" -Destination "$stage\DefaultData\码表" -Recurse}
Copy-Item -LiteralPath "$DataSource\拼音反查码表" -Destination "$stage\DefaultData" -Recurse
# First-install settings only; existing personal configuration is never merged.
"Ctrl+空格切换中英文`t否`r`n当前码表`t虎码字词`r`n"|Set-Content -LiteralPath "$stage\default-config.txt" -Encoding UTF8
# Generate the bundled fallback and its sentence sidecars from clean sources.
& "$stage\x64\Tigirl.Import.exe" "$stage\DefaultData\码表\虎码字词" "$stage\DefaultData\拼音反查码表" "$stage\x64\fallback.tcd" 'zh-CN'
if($LASTEXITCODE){throw 'Default dictionary generation failed.'}
foreach($suffix in @('','.sentence.tcd','.supplement.tcd')){Move-Item -LiteralPath "$stage\x64\fallback.tcd$suffix" -Destination "$stage\x64\tiger-v2.tcd$suffix" -Force}
$files=@(Get-ChildItem -LiteralPath $stage -Recurse -File|Sort-Object FullName|ForEach-Object{
    [ordered]@{path=$_.FullName.Substring($stage.Length+1);bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})
[ordered]@{version=$Version;target='Windows x64, x64+x86 TSF';files=$files}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath "$stage\manifest.json" -Encoding UTF8
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip="$stage.zip"
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,[IO.Compression.CompressionLevel]::Optimal,$true)
(Get-FileHash -LiteralPath $zip).Hash+'  '+[IO.Path]::GetFileName($zip)|Set-Content -LiteralPath "$zip.sha256" -Encoding ASCII
Write-Output $zip
