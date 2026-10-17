function Get-NativeTigerPackageSources($DataSource,$SkinSource,[string]$DefaultScheme=''){
    $schemes=@(Get-ChildItem -LiteralPath "$DataSource\码表" -Directory -ErrorAction Stop|Sort-Object Name)
    if(!$schemes.Count){throw 'At least one bundled dictionary directory is required.'}
    if(!$DefaultScheme){
        if($schemes.Name -contains '虎码字词'){$DefaultScheme='虎码字词'}else{$DefaultScheme=$schemes[0].Name}
    }
    if($schemes.Name -notcontains $DefaultScheme){throw "Default dictionary is not bundled: $DefaultScheme"}
    # Archived names (including pre-rename aliases) must not re-enter a release
    # if an old build or manual copy leaves an SSF in the source directory.
    $catalog=Get-Content -LiteralPath "$PSScriptRoot\assets\skins\catalog.json" -Raw -Encoding UTF8 -ErrorAction Stop|ConvertFrom-Json
    $archived=@{}
    foreach($skin in $catalog.archived){
        foreach($name in @($skin.file)+@($skin.previous_files)){
            if($name){$archived[[string]$name]=$true}
        }
    }
    $skins=@(Get-ChildItem -LiteralPath $SkinSource -File -ErrorAction Stop|Where-Object {$_.Extension -eq '.ssf' -and !$archived.ContainsKey($_.Name)}|Sort-Object Name)
    $reverse=Get-Item -LiteralPath "$DataSource\拼音反查码表" -ErrorAction Stop
    if(!$reverse.PSIsContainer){throw 'Reverse lookup data must be a directory.'}
    [pscustomobject]@{Schemes=$schemes;Skins=$skins;Reverse=$reverse;DefaultScheme=$DefaultScheme}
}

function Copy-NativeTigerPackageSources($Sources,$Destination){
    New-Item -ItemType Directory -Force "$Destination\码表","$Destination\皮肤"|Out-Null
    foreach($scheme in $Sources.Schemes){Copy-Item -LiteralPath $scheme.FullName -Destination "$Destination\码表" -Recurse -ErrorAction Stop}
    foreach($skin in $Sources.Skins){Copy-Item -LiteralPath $skin.FullName -Destination "$Destination\皮肤" -ErrorAction Stop}
    Copy-Item -LiteralPath $Sources.Reverse.FullName -Destination $Destination -Recurse -ErrorAction Stop
}
