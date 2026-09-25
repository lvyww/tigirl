param([string]$Model = '')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $root 'sentence_package.ps1')
if(!$Model){$Model=Join-Path $root 'data\Models\sentence-fivegram-mobile.bin'}
$stage=Join-Path $root ('build\fivegram-package-test-'+[Guid]::NewGuid().ToString('N'))
Copy-NativeTigerSentenceModel $Model $stage
$actual=@(Get-ChildItem -LiteralPath (Join-Path $stage 'Models') -File | ForEach-Object Name | Sort-Object)
$expected=@('provenance.json','sentence-fivegram-mobile.bin','sentence-lexical-v1.bin' | Sort-Object)
if(@(Compare-Object $actual $expected).Count){throw 'Unexpected packaged model files'}
$provenance=Get-Content -LiteralPath (Join-Path $stage 'Models\provenance.json') -Raw | ConvertFrom-Json
if($provenance.model -ne 'sentence-fivegram-mobile.bin' -or $provenance.bytes -ne 405663171 -or $provenance.format_version -ne 2 -or $provenance.quantization_bits -ne 8){throw 'Invalid fivegram provenance'}
$bad=Join-Path $stage 'invalid.bin';[IO.File]::WriteAllText($bad,'not a model')
$rejected=$false;try{Assert-NativeTigerSentenceModel $bad}catch{$rejected=$true}
if(!$rejected){throw 'Invalid model accepted'}
[ordered]@{status='passed';stage=$stage;model=$provenance.model;bytes=$provenance.bytes;sha256=$provenance.sha256;no_trigram=$true;invalid_model_rejected=$rejected}|ConvertTo-Json
