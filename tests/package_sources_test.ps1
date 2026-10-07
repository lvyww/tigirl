$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\package_sources.ps1"
$root=Join-Path $env:TEMP ('Tigirl-sources-'+[guid]::NewGuid().ToString('N'))
function Check($Value,$Message){if(!$Value){throw $Message}}
try{
 New-Item -ItemType Directory -Force "$root\data\码表\虎码字词","$root\data\码表\自选[甲]\nested","$root\data\拼音反查码表","$root\skins\ignored"|Out-Null
 Set-Content -LiteralPath "$root\data\码表\自选[甲]\nested\data.txt" -Value 'dictionary'
 Set-Content -LiteralPath "$root\skins\自选[甲].ssf" -Value 'skin'
 Set-Content -LiteralPath "$root\skins\second.SSF" -Value 'skin2'
 Set-Content -LiteralPath "$root\skins\notes.txt" -Value 'not a skin'
 Set-Content -LiteralPath "$root\skins\ignored\nested.ssf" -Value 'not top level'
 $plan=Get-NativeTigerPackageSources "$root\data" "$root\skins"
 Check ($plan.DefaultScheme -eq '虎码字词') 'Default preference failed'
 Copy-NativeTigerPackageSources $plan "$root\stage"
 Check (Test-Path -LiteralPath "$root\stage\码表\自选[甲]\nested\data.txt") 'Recursive dictionary copy failed'
 Check (Test-Path -LiteralPath "$root\stage\皮肤\自选[甲].ssf") 'Literal skin filename copy failed'
 Check (@(Get-ChildItem "$root\stage\皮肤" -File).Count -eq 2) 'Skin filtering failed'
 $plan=Get-NativeTigerPackageSources "$root\data" "$root\skins" '自选[甲]'
 Check ($plan.DefaultScheme -eq '自选[甲]') 'Explicit default failed'
 Remove-Item -LiteralPath "$root\data\码表\虎码字词" -Recurse -Force
 $plan=Get-NativeTigerPackageSources "$root\data" "$root\skins"
 Check ($plan.DefaultScheme -eq '自选[甲]') 'Removed default fallback failed'
 $failed=$false;try{Get-NativeTigerPackageSources "$root\data" "$root\skins" 'missing'|Out-Null}catch{$failed=$true}
 Check $failed 'Missing explicit default accepted'
 Remove-Item -LiteralPath "$root\data\码表\自选[甲]" -Recurse -Force
 $failed=$false;try{Get-NativeTigerPackageSources "$root\data" "$root\skins"|Out-Null}catch{$failed=$true}
 Check $failed 'Empty dictionaries accepted'
 'PASS: directory discovery, recursive copy, SSF filtering, literal filenames, default selection, empty-source validation.'
}finally{if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}}
