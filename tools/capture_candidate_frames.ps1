param(
 [Parameter(Mandatory)][string]$Application,
 [string[]]$ApplicationArguments=@(),
 [string]$OutputDirectory=(Join-Path ([Environment]::GetFolderPath('Desktop')) ('Tigirl-Frames-'+(Get-Date -Format 'yyyyMMdd-HHmmss')))
)
$ErrorActionPreference='Stop'
$exe=(Resolve-Path -LiteralPath $Application).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force|Out-Null
$directory=(Resolve-Path -LiteralPath $OutputDirectory).Path
$previous=$env:TIGIRL_FRAME_TRACE_DIR
try {
 $env:TIGIRL_FRAME_TRACE_DIR=$directory
 if($ApplicationArguments.Count){$process=Start-Process -FilePath $exe -ArgumentList $ApplicationArguments -PassThru}
 else {$process=Start-Process -FilePath $exe -PassThru}
 Write-Host "已启动 PID $($process.Id)。请用简短测试文字重现，再选词上屏或取消，使候选窗隐藏。"
 Write-Host "记录目录：$directory"
 Write-Host '仅启动的新进程继承记录开关；单实例应用需先完全退出旧进程。'
} finally {$env:TIGIRL_FRAME_TRACE_DIR=$previous}
