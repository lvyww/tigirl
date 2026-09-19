# User-scoped data merge. Imported by initialize.ps1 and fixture tests.
$ErrorActionPreference='Stop'
function Assert-PlainTree([string]$Path) {
    if (!(Test-Path -LiteralPath $Path)) { return }
    $current=Get-Item -LiteralPath $Path -Force
    while ($current) {
        if ($current.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing redirected path: $($current.FullName)" }
        $current=$current.Parent
    }
    foreach ($item in Get-ChildItem -LiteralPath $Path -Recurse -Force) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing redirected data: $($item.FullName)" }
    }
}
# A per-user baseline describes bytes actually installed, never merely offered.
function Read-DataBaseline([string]$Path) {
    $result=@{}
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return $result }
    try {
        $record=Get-Content -LiteralPath $Path -Raw|ConvertFrom-Json
        if ($record.schemaVersion -ne 1) { throw 'Unknown baseline format.' }
        foreach ($entry in $record.files) {
            if (!$entry.path -or $entry.path -match '(^[\\/]|:|(^|[\\/])\.\.?([\\/]|$))' -or $entry.sha256 -notmatch '^[a-fA-F0-9]{64}$' -or $result.ContainsKey($entry.path)) { throw 'Invalid baseline entry.' }
            $result[$entry.path]=$entry.sha256
        }
    } catch {
        Write-Warning 'Installation baseline is unreadable; existing user files will be preserved.'
        return @{}
    }
    return $result
}
function Get-DataMerge([string]$Source,[string]$Destination) {
    Assert-PlainTree $Source
    Assert-PlainTree $Destination
    if (!(Test-Path -LiteralPath $Source -PathType Container)) { return }
    $baseline=Read-DataBaseline (Join-Path $Destination 'installed-data-baseline.json')
    $prefix=[IO.Path]::GetFullPath($Source).TrimEnd('\')+'\'
    foreach ($file in Get-ChildItem -LiteralPath $Source -Recurse -File) {
        $relative=$file.FullName.Substring($prefix.Length)
        $target=Join-Path $Destination $relative
        $exists=Test-Path -LiteralPath $target
        if ($exists -and !(Test-Path -LiteralPath $target -PathType Leaf)) { throw "A directory occupies a file path: $target" }
        $hash=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        $old=if ($exists) { (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash } else { '' }
        $managed=$exists -and $baseline.ContainsKey($relative) -and $old -eq $baseline[$relative]
        $reason=if(!$exists){'Missing'}elseif($managed){'Unmodified'}elseif($baseline.ContainsKey($relative)){'Modified'}else{'Unknown'}
        # Even identical official bytes may be copied. Unknown/modified files are not adopted.
        [pscustomobject]@{Source=$file.FullName;Target=$target;RelativePath=$relative;Hash=$hash;OldHash=$old;BaselineHash=$baseline[$relative];Reason=$reason;Conflict=($exists -and !$managed -and $hash -ne $old);Choice=$(if(!$exists -or $managed){'Copy'}else{'Skip'})}
    }
}
function Save-DataBaseline($Plan,[string]$Path) {
    $baseline=Read-DataBaseline $Path
    foreach($item in $Plan) {
        if($item.Choice -eq 'Copy') { $baseline[$item.RelativePath]=$item.Hash }
    }
    $files=@($baseline.Keys|Sort-Object|ForEach-Object {@{path=$_;sha256=$baseline[$_]}})
    $tmp=$Path+'.tmp'
    @{schemaVersion=1;files=$files}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $tmp -Encoding UTF8
    if(Test-Path -LiteralPath $Path){[IO.File]::Replace($tmp,$Path,[NullString]::Value)}else{[IO.File]::Move($tmp,$Path)}
}
function Select-DataConflicts($Plan) {
    $conflicts=@($Plan | Where-Object Conflict)
    if (!$conflicts.Count) { return }
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    $form=New-Object Windows.Forms.Form
    $icon=Join-Path $PSScriptRoot 'Tigirl.ico';if(Test-Path -LiteralPath $icon){$form.Icon=[Drawing.Icon]::new($icon)}
    $form.AutoScaleMode='Dpi';$form.TopMost=$true
    $form.Text='虎娘：选择同名文件的处理方式';$form.Width=900;$form.Height=520;$form.StartPosition='CenterScreen'
    $label=New-Object Windows.Forms.Label
    $label.Text='以下文件内容不同。默认保留现有文件；覆盖前会自动备份。';$label.Dock='Top';$label.Height=35;$form.Controls.Add($label)
    $grid=New-Object Windows.Forms.DataGridView
    $grid.Dock='Fill';$grid.AllowUserToAddRows=$false;$grid.AllowUserToDeleteRows=$false;$grid.RowHeadersVisible=$false
    $column=New-Object Windows.Forms.DataGridViewTextBoxColumn;$column.HeaderText='现有文件';$column.ReadOnly=$true;$column.AutoSizeMode='Fill';[void]$grid.Columns.Add($column)
    $choice=New-Object Windows.Forms.DataGridViewComboBoxColumn;$choice.HeaderText='处理方式';$choice.Width=100;[void]$choice.Items.Add('跳过');[void]$choice.Items.Add('覆盖');[void]$grid.Columns.Add($choice)
    foreach($item in $conflicts){[void]$grid.Rows.Add($item.Target,'跳过')}
    $form.Controls.Add($grid);$grid.BringToFront()
    $panel=New-Object Windows.Forms.FlowLayoutPanel;$panel.Dock='Bottom';$panel.Height=45;$form.Controls.Add($panel)
    foreach($entry in @(@('全部跳过','跳过'),@('全部覆盖','覆盖'))) {
        $button=New-Object Windows.Forms.Button;$button.Text=$entry[0];$button.Tag=$entry[1];$button.Width=110
        $button.Add_Click({param($sender,$eventArgs)foreach($row in $grid.Rows){$row.Cells[1].Value=$sender.Tag}});$panel.Controls.Add($button)
    }
    $ok=New-Object Windows.Forms.Button;$ok.Text='继续';$ok.DialogResult='OK';$panel.Controls.Add($ok)
    $cancel=New-Object Windows.Forms.Button;$cancel.Text='取消';$cancel.DialogResult='Cancel';$panel.Controls.Add($cancel);$form.CancelButton=$cancel
    if($form.ShowDialog() -ne 'OK'){$form.Dispose();throw [OperationCanceledException]::new('已取消，未复制文件。')}
    $grid.EndEdit()|Out-Null
    for($i=0;$i -lt $conflicts.Count;$i++){if($grid.Rows[$i].Cells[1].Value -eq '覆盖'){$conflicts[$i].Choice='Copy'}}
    $form.Dispose()
}
function Invoke-DataMerge($Plan,[string]$Backup) {
    $done=[Collections.Generic.List[object]]::new()
    try {
        foreach($item in $Plan) {
            if($item.Choice -ne 'Copy'){continue}
            $current=if(Test-Path -LiteralPath $item.Target){(Get-FileHash -LiteralPath $item.Target).Hash}else{''}
            if($current -ne $item.OldHash -or (Get-FileHash -LiteralPath $item.Source).Hash -ne $item.Hash){throw 'Data changed while awaiting your selection; please retry.'}
            Assert-PlainTree (Split-Path $item.Target -Parent)
            New-Item -ItemType Directory -Force (Split-Path $item.Target -Parent)|Out-Null
            $saved=''
            if($current){New-Item -ItemType Directory -Force $Backup|Out-Null;$saved=Join-Path $Backup ($done.Count.ToString()+'.bak');Copy-Item -LiteralPath $item.Target -Destination $saved}
            $done.Add([pscustomobject]@{Target=$item.Target;Backup=$saved;Hash=$item.Hash})
            # Persist rollback information before modifying a target, including first installs.
            New-Item -ItemType Directory -Force $Backup|Out-Null
            $index=Join-Path $Backup 'files.json';$indexTmp=$index+'.tmp'
            ConvertTo-Json -InputObject @($done.ToArray()) -Depth 4|Set-Content $indexTmp -Encoding UTF8
            if(Test-Path $index){[IO.File]::Replace($indexTmp,$index,[NullString]::Value)}else{[IO.File]::Move($indexTmp,$index)}
            $temp=$item.Target+'.install-'+[guid]::NewGuid().ToString('N')
            try {Copy-Item -LiteralPath $item.Source -Destination $temp;if($current){[IO.File]::Replace($temp,$item.Target,[NullString]::Value)}else{[IO.File]::Move($temp,$item.Target)}}finally{if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp}}
        }
        if($done.Count){New-Item -ItemType Directory -Force $Backup|Out-Null;ConvertTo-Json -InputObject @($done.ToArray()) -Depth 4|Set-Content -LiteralPath (Join-Path $Backup 'files.json') -Encoding UTF8}
        return $done.ToArray()
    }catch{Undo-DataMerge $done.ToArray();throw}
}
function Undo-DataMerge($Changes) {
    for($i=$Changes.Count-1;$i -ge 0;$i--){$item=$Changes[$i];if((Test-Path -LiteralPath $item.Target) -and (Get-FileHash -LiteralPath $item.Target).Hash -ne $item.Hash){Write-Warning "A later edit was retained; restore manually from $($item.Backup)";continue};if($item.Backup){Copy-Item -LiteralPath $item.Backup -Destination $item.Target -Force}elseif(Test-Path -LiteralPath $item.Target){Remove-Item -LiteralPath $item.Target}}
}
