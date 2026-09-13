param()
$ErrorActionPreference='Stop'
$fixture=Join-Path $env:TEMP ('Tigirl-process-wait-'+[guid]::NewGuid().ToString('N'))
New-Item $fixture -ItemType Directory|Out-Null
try {
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot\..\packaging\initialize.ps1",[ref]$tokens,[ref]$errors)
    if($errors.Count){throw ($errors|Out-String)}
    $fn=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Invoke-InitializerCompiler'},$true)
    . ([scriptblock]::Create($fn.Extent.Text))
    function Write-InitializeLog([string]$Message){Write-Host $Message}
    $exe=Join-Path $fixture 'worker.exe'
    Add-Type -OutputAssembly $exe -OutputType ConsoleApplication -TypeDefinition @'
using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
public class Worker {
 public static int Main(string[] args) {
  string self=typeof(Worker).Assembly.Location;
  if(args.Length>0 && args[0]=="child") { Thread.Sleep(6000); return 0; }
  if(File.Exists(self+".fail")) return 7;
  var child=Process.Start(new ProcessStartInfo(self,"child") {UseShellExecute=false,CreateNoWindow=true});
  File.WriteAllText(self+".pid",child.Id.ToString());
  return 0;
 }
}
'@
    $timer=[Diagnostics.Stopwatch]::StartNew()
    $old=Start-Process $exe -ArgumentList '--initialize' -Wait -PassThru
    $timer.Stop()
    if($timer.ElapsedMilliseconds -lt 5000){throw 'Baseline did not wait for descendant.'}
    Write-Host "Old process-tree wait: $($timer.ElapsedMilliseconds) ms"
    $timer.Restart()
    $code=Invoke-InitializerCompiler $exe
    $timer.Stop()
    if($code -ne 0 -or $timer.ElapsedMilliseconds -ge 5000){throw 'Compiler-only wait failed.'}
    $child=Get-Process -Id ([int](Get-Content ($exe+'.pid'))) -ErrorAction Stop
    Write-Host "Compiler-only wait: $($timer.ElapsedMilliseconds) ms; descendant still running"
    $child.WaitForExit();$child.Dispose()
    New-Item ($exe+'.fail') -ItemType File|Out-Null
    if((Invoke-InitializerCompiler $exe) -ne 7){throw 'Compiler failure exit code lost.'}
    'PASS: descendant lifetime does not block completion; compiler exit status preserved.'
}finally{Remove-Item $fixture -Recurse -Force}
