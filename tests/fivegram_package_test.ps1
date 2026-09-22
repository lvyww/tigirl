param([string]$Model = '')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. (Join-Path $root 'sentence_package.ps1')
if(!$Model){$Model=Join-Path $root 'data\Models\sentence-fivegram.klm'}
$stage=Join-Path $root ('build\fivegram-package-test-'+[Guid]::NewGuid().ToString('N'))
Copy-NativeTigerSentenceModel $Model $stage
$actual=@(Get-ChildItem -LiteralPath (Join-Path $stage 'Models') -File | ForEach-Object Name | Sort-Object)
$expected=@('provenance.json','sentence-fivegram.klm','sentence-lexical-v1.bin' | Sort-Object)
if(@(Compare-Object $actual $expected).Count){throw 'Unexpected packaged model files'}
foreach($file in @('LICENSE','COPYING','COPYING.3','COPYING.LESSER.3','double-conversion-LICENSE','TIGIRL_INTEGRATION.md')) {
    if(!(Test-Path -LiteralPath (Join-Path $stage "licenses\kenlm\$file"))){throw "Missing license: $file"}
}
$provenance=Get-Content -LiteralPath (Join-Path $stage 'Models\provenance.json') -Raw | ConvertFrom-Json
if($provenance.model -ne 'sentence-fivegram.klm' -or $provenance.bytes -ne 419929926){throw 'Invalid fivegram provenance'}
$bad=Join-Path $stage 'invalid.klm';[IO.File]::WriteAllText($bad,'not a model')
$rejected=$false;try{Assert-NativeTigerSentenceModel $bad}catch{$rejected=$true}
if(!$rejected){throw 'Invalid model accepted'}
[ordered]@{status='passed';stage=$stage;model=$provenance.model;bytes=$provenance.bytes;sha256=$provenance.sha256;no_trigram=$true;licenses=$true;invalid_model_rejected=$rejected}|ConvertTo-Json
