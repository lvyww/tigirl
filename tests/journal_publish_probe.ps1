param([Parameter(Mandatory=$true)][string]$Directory,
      [ValidateSet('before','after')][string]$CrashAt)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
if (-not (Test-Path -LiteralPath (Join-Path $Directory '.publish-test'))) { throw 'Requires isolated marked fixture directory' }
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Text;
using System.Threading;
using System.Collections.Generic;
public static class JournalPublishProbe {
    public static int OverlappingReplacements;
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern bool ReplaceFile(string target, string replacement, string backup, uint flags, IntPtr exclude, IntPtr reserved);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern bool MoveFileEx(string source, string target, uint flags);
    public static int MoveWithReadSharingError;
    public static void MoveCheck(string directory) {
        string live=Path.Combine(directory,"move-live"), stage=Path.Combine(directory,"move-stage");
        Require(!File.Exists(live) && !File.Exists(stage),"Refuse existing move fixture");
        File.WriteAllText(live,"original");File.WriteAllText(stage,"replacement");
        using(var guard=Open(live,FileShare.Read|FileShare.Delete)) {
            bool moved=MoveFileEx(stage,live,9);
            MoveWithReadSharingError=moved?0:Marshal.GetLastWin32Error();
        }
        Require(File.ReadAllText(live)==(MoveWithReadSharingError==0?"replacement":"original"),"Move changed live data unexpectedly");
        if(MoveWithReadSharingError!=0)Require(File.ReadAllText(stage)=="replacement","Failed move lost staging data");
    }
    static FileStream Open(string path, FileShare share) {
        return new FileStream(path, FileMode.Open, FileAccess.ReadWrite, share);
    }
    static void Require(bool condition, string message) {
        if (!condition) throw new Exception(message);
    }
    static bool SharingDenied(Action action) {
        try { action(); return false; }
        catch(IOException error) { return (error.HResult & 65535)==32; }
    }
    public static void CrashWorker(string directory, string boundary) {
        string live=Path.Combine(directory,"crash.tcu"), stage=Path.Combine(directory,"crash.new");
        string backup=Path.Combine(directory,"crash.bak");
        Require(!File.Exists(live) && !File.Exists(stage) && !File.Exists(backup),"Refuse existing crash fixture");
        File.WriteAllText(live,"old-complete");
        using(var guard=Open(live,FileShare.Read|FileShare.Delete)) {
            guard.Flush(true);
            using(var output=new FileStream(stage,FileMode.CreateNew,FileAccess.Write,FileShare.None)) {
                byte[] data=Encoding.UTF8.GetBytes("new-complete");
                output.Write(data,0,data.Length);output.Flush(true);
            }
            if(boundary=="after")
                Require(ReplaceFile(live,stage,backup,0,IntPtr.Zero,IntPtr.Zero),"Crash fixture replace failed");
            File.WriteAllText(Path.Combine(directory,"ready"),boundary);
            // Parent terminates this owned process while guard is still held.
            Thread.Sleep(Timeout.Infinite);
        }
    }
    public static void CrashChecks(string directory, string scriptPath) {
        foreach(string boundary in new[]{"before","after"}) {
            string fixture=Path.Combine(directory,"crash-"+boundary);
            Directory.CreateDirectory(fixture);File.WriteAllText(Path.Combine(fixture,".publish-test"),"");
            string script="& '"+scriptPath.Replace("'","''")+"' -Directory '"+
                fixture.Replace("'","''")+"' -CrashAt '"+boundary+"'";
            var start=new ProcessStartInfo(Path.Combine(Environment.GetEnvironmentVariable("SystemRoot"),
                @"System32\WindowsPowerShell\v1.0\powershell.exe"),
                "-NoProfile -ExecutionPolicy Bypass -EncodedCommand "+Convert.ToBase64String(Encoding.Unicode.GetBytes(script)));
            start.UseShellExecute=false;start.CreateNoWindow=true;start.RedirectStandardError=true;
            using(var child=Process.Start(start)) {
                var errors=child.StandardError.ReadToEndAsync();var timer=Stopwatch.StartNew();
                try {
                    while(!File.Exists(Path.Combine(fixture,"ready"))) {
                        Require(!child.HasExited,"Crash worker exited before boundary");
                        Require(timer.ElapsedMilliseconds<10000,"Crash worker did not reach boundary");
                        Thread.Sleep(10);
                    }
                    child.Kill();Require(child.WaitForExit(10000),"Terminated worker did not exit");
                    // EncodedCommand may emit only its serialization header
                    // before termination. Do not mistake that header for an
                    // error; any actual stderr payload still fails the check.
                    string stderr=errors.Result.Trim();
                    Require(stderr.Length==0 || stderr=="#< CLIXML","Crash worker stderr: "+stderr);
                } finally { if(!child.HasExited) {child.Kill();child.WaitForExit();} }
            }
            string live=Path.Combine(fixture,"crash.tcu"), stage=Path.Combine(fixture,"crash.new");
            string backup=Path.Combine(fixture,"crash.bak");
            Require(File.ReadAllText(live)==(boundary=="before"?"old-complete":"new-complete"),"Termination lost complete live file");
            if(boundary=="before")Require(File.ReadAllText(stage)=="new-complete" && !File.Exists(backup),"Unpublished staging state changed");
            else Require(!File.Exists(stage) && File.ReadAllText(backup)=="old-complete","Published backup state changed");
            using(var reopened=Open(live,FileShare.ReadWrite)) {
                reopened.Seek(0,SeekOrigin.End);reopened.WriteByte(33);reopened.Flush(true);
            }
            Require(File.ReadAllText(live).EndsWith("!"),"Termination left a blocking guard or lost next write");
        }
    }
    public static void Race(string directory) {
        string live=Path.Combine(directory,"race.tcu");
        Require(!File.Exists(live),"Refuse existing race fixture");
        File.WriteAllText(live,"");
        string script="$ErrorActionPreference='Stop'; $p='"+live.Replace("'","''")+"'; "+
            "for($i=0;$i -lt 100;){ $f=$null; try { "+
            "$f=[IO.File]::Open($p,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite); "+
            "$null=$f.Seek(0,[IO.SeekOrigin]::End); $b=[Text.Encoding]::UTF8.GetBytes(('W'+$i+\"`n\")); "+
            "$f.Write($b,0,$b.Length); $f.Flush($true); $i++ "+
            "} catch [IO.IOException] { if(($_.Exception.HResult -band 65535) -ne 32){throw} } "+
            "finally {if($f){$f.Dispose()}}; Start-Sleep -Milliseconds 5 }";
        var start=new ProcessStartInfo(Path.Combine(Environment.GetEnvironmentVariable("SystemRoot"),
            @"System32\WindowsPowerShell\v1.0\powershell.exe"),
            "-NoProfile -EncodedCommand "+Convert.ToBase64String(Encoding.Unicode.GetBytes(script)));
        start.UseShellExecute=false;start.CreateNoWindow=true;start.RedirectStandardError=true;
        using(var writer=Process.Start(start)) {
            var errors=writer.StandardError.ReadToEndAsync();
            var timer=Stopwatch.StartNew();
            try {
                for(int index=0;index<50;) {
                    Require(timer.ElapsedMilliseconds<15000,"Publication race timed out");
                    try {
                        using(var guard=Open(live,FileShare.Read|FileShare.Delete)) {
                            byte[] previous=new byte[guard.Length];int done=0;
                            while(done<previous.Length) {
                                int read=guard.Read(previous,done,previous.Length-done);
                                Require(read>0,"Unexpected guarded EOF");done+=read;
                            }
                            string content=Encoding.UTF8.GetString(previous);
                            if(!content.Contains("W0\n")) { Thread.Sleep(5);continue; }
                            if(!content.Contains("W99\n"))++OverlappingReplacements;
                            string stage=Path.Combine(directory,"race-stage-"+index);
                            string backup=Path.Combine(directory,"race-backup-"+index);
                            using(var output=new FileStream(stage,FileMode.CreateNew,FileAccess.Write,FileShare.None)) {
                                output.Write(previous,0,previous.Length);
                                byte[] marker=Encoding.UTF8.GetBytes("P"+index+"\n");
                                output.Write(marker,0,marker.Length);output.Flush(true);
                            }
                            Require(ReplaceFile(live,stage,backup,0,IntPtr.Zero,IntPtr.Zero),
                                "Race ReplaceFile failed: "+Marshal.GetLastWin32Error());
                            ++index;
                        }
                    }catch(IOException error) { if((error.HResult&65535)!=32)throw; }
                    Thread.Sleep(10);
                }
                Require(writer.WaitForExit(10000),"Writer failed to finish");
                Require(writer.ExitCode==0,"Writer failed: "+errors.Result);
                var lines=File.ReadAllLines(live);
                var unique=new HashSet<string>(lines);
                Require(lines.Length==150 && unique.Count==150,"Lost or duplicated race records");
                for(int i=0;i<100;++i)Require(unique.Contains("W"+i),"Missing writer record");
                for(int i=0;i<50;++i)Require(unique.Contains("P"+i),"Missing publication record");
                Require(OverlappingReplacements>0,"No publication overlapped the writer sequence");
            } finally { if(!writer.HasExited) {writer.Kill();writer.WaitForExit();} }
        }
    }
    public static void Run(string directory) {
        string live=Path.Combine(directory,"live.tcu"), stage=Path.Combine(directory,"staged.tcu");
        Require(!File.Exists(live) && !File.Exists(stage),"Refuse existing fixture files");
        File.WriteAllText(live,"old-complete");
        using(var old=Open(live,FileShare.ReadWrite)) {
            Require(SharingDenied(()=> { using(var guard=Open(live,FileShare.Read|FileShare.Delete)) {} }),
                "Publication guard admitted an existing legacy handle");
        }
        using(var guard=Open(live,FileShare.Read|FileShare.Delete)) {
            Require(SharingDenied(()=> { using(var old=Open(live,FileShare.ReadWrite)) {} }),
                "Legacy handle opened during publication guard");
            using(var output=new FileStream(stage,FileMode.CreateNew,FileAccess.Write,FileShare.None)) {
                byte[] bytes=System.Text.Encoding.UTF8.GetBytes("new-complete");
                output.Write(bytes,0,bytes.Length);output.Flush(true);
            }
            Require(ReplaceFile(live,stage,null,0,IntPtr.Zero,IntPtr.Zero),"ReplaceFile failed: "+Marshal.GetLastWin32Error());
            // The guard still owns the old file object. A new legacy client
            // must reach the complete replacement, never the guarded object.
            using(var next=Open(live,FileShare.ReadWrite)) {
                byte[] bytes=new byte[next.Length];
                Require(next.Read(bytes,0,bytes.Length)==bytes.Length &&
                    System.Text.Encoding.UTF8.GetString(bytes)=="new-complete","Replacement contents wrong");
                next.Seek(0,SeekOrigin.End);next.WriteByte(33);next.Flush(true);
            }
            guard.Seek(0,SeekOrigin.Begin);
            byte[] previous=new byte[guard.Length];guard.Read(previous,0,previous.Length);
            Require(System.Text.Encoding.UTF8.GetString(previous)=="old-complete","Old guard changed file identity");
        }
        Require(File.ReadAllText(live)=="new-complete!" && !File.Exists(stage),"Post-publication write lost");
    }
}
'@
if ($CrashAt) { [JournalPublishProbe]::CrashWorker($Directory,$CrashAt); exit }
[JournalPublishProbe]::Run($Directory)
[JournalPublishProbe]::MoveCheck($Directory)
[JournalPublishProbe]::Race($Directory)
[JournalPublishProbe]::CrashChecks($Directory,$PSCommandPath)
@{status='passed'; existing_legacy_handle_blocks=$true; guard_blocks_new_legacy_handle=$true;
  replace_while_guard_held=$true; post_replace_legacy_write=$true; process_crash_tested=$true;
  termination_boundaries=@('staging_flushed_before_replace','replace_returned_with_guard_held');
  termination_inside_replace_tested=$false;
  independent_writer_records=100; replacements=50; retained_race_records=150;
  overlapping_replacements=[JournalPublishProbe]::OverlappingReplacements;
  move_with_read_sharing_error=[JournalPublishProbe]::MoveWithReadSharingError;
  move_alternative_supported=([JournalPublishProbe]::MoveWithReadSharingError -eq 0);
  physical_power_loss_tested=$false; production_integration=$false} | ConvertTo-Json -Compress
