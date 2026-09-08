$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Diagnostics;
public static class ManagerLaunchProbe {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] public struct Startup {
  public int cb; public string reserved,desktop,title; public int x,y,cx,cy,charsX,charsY,fill,flags;
  public short show,bytes;public IntPtr extra,input,output,error;
 }
 [StructLayout(LayoutKind.Sequential)] public struct ProcessInfo {public IntPtr process,thread;public uint pid,tid;}
 public delegate bool EnumWindow(IntPtr window,IntPtr data);
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateDesktop(string name,IntPtr device,IntPtr mode,uint flags,uint access,IntPtr security);
 [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr desktop);
 [DllImport("user32.dll",SetLastError=true)] static extern bool EnumDesktopWindows(IntPtr desktop,EnumWindow callback,IntPtr data);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr window,StringBuilder value,int count);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window,StringBuilder value,int count);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
 [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr window);
 [DllImport("user32.dll",SetLastError=true)] static extern bool PostMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcess(string app,StringBuilder command,IntPtr processSecurity,IntPtr threadSecurity,bool inherit,uint flags,IntPtr environment,string directory,ref Startup startup,out ProcessInfo process);
 [DllImport("user32.dll")] static extern uint WaitForInputIdle(IntPtr process,uint milliseconds);
 [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr handle,uint milliseconds);
 [DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr process,out uint code);
 [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr process,uint code);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 static void Error(string operation) {throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(),operation);}
 public static uint Run(string exe,string arguments,string expectedClass,uint expectedCode) {
  string name="NativeTigerLaunchTest_"+Guid.NewGuid().ToString("N");
  IntPtr desktop=CreateDesktop(name,IntPtr.Zero,IntPtr.Zero,0,0x01ff,IntPtr.Zero);
  if(desktop==IntPtr.Zero)Error("Create isolated test desktop");
  ProcessInfo process=new ProcessInfo();
  try {
   Startup startup=new Startup();startup.cb=Marshal.SizeOf(typeof(Startup));startup.desktop=name;
   if(!CreateProcess(exe,new StringBuilder("\""+exe+"\" "+arguments),IntPtr.Zero,IntPtr.Zero,false,0,IntPtr.Zero,System.IO.Path.GetDirectoryName(exe),ref startup,out process))Error("Launch native manager");
   IntPtr found=IntPtr.Zero;var timer=Stopwatch.StartNew();
   while(timer.ElapsedMilliseconds<15000 && found==IntPtr.Zero) {
    if(WaitForSingleObject(process.process,0)==0)throw new Exception("Manager exited before creating expected window");
    EnumWindow callback=delegate(IntPtr window,IntPtr data) {
     uint pid;GetWindowThreadProcessId(window,out pid);var type=new StringBuilder(256);GetClassName(window,type,256);
     if(pid==process.pid && type.ToString()==expectedClass && IsWindowVisible(window)){found=window;return false;}return true;
    };
    EnumDesktopWindows(desktop,callback,IntPtr.Zero);
    if(found==IntPtr.Zero)System.Threading.Thread.Sleep(25);
   }
   if(found==IntPtr.Zero)throw new Exception("Expected manager window not found on isolated desktop");
   if(expectedCode!=0) {
    var title=new StringBuilder(256);GetWindowText(found,title,256);
    if(title.ToString()!="原生虎码 · 无法启动")throw new Exception("Unexpected startup error dialog");
   }
   if(WaitForInputIdle(process.process,5000)!=0)throw new Exception("Manager did not become input-idle");
   if(!PostMessage(found,0x10,IntPtr.Zero,IntPtr.Zero))Error("Close test window");
   if(WaitForSingleObject(process.process,10000)!=0)throw new Exception("Manager failed to exit after closing window");
   uint code;if(!GetExitCodeProcess(process.process,out code))Error("Read manager exit code");
   if(code!=expectedCode)throw new Exception("Manager returned failure "+code);return code;
  } finally {
   if(process.process!=IntPtr.Zero){if(WaitForSingleObject(process.process,0)!=0){TerminateProcess(process.process,99);WaitForSingleObject(process.process,5000);}CloseHandle(process.process);}
   if(process.thread!=IntPtr.Zero)CloseHandle(process.thread);CloseDesktop(desktop);
  }
 }
}
'@
$temporary = Join-Path $root ('build\manager-launch-' + [Guid]::NewGuid().ToString('N'))
$previous = $env:NATIVE_TIGER_USER_ROOT
try {
    New-Item -ItemType Directory -Path $temporary | Out-Null
    $config = Join-Path $temporary 'config.txt'
    [IO.File]::WriteAllText($config,"# isolated launch fixture`r`n",(New-Object Text.UTF8Encoding($true)))
    $before = (Get-FileHash -LiteralPath $config).Hash
    $env:NATIVE_TIGER_USER_ROOT = $temporary
    $exe = Join-Path $root 'build\ARM64\Release\schema_manager.exe'
    [void][ManagerLaunchProbe]::Run($exe,'--settings','NativeTigerInputSettings',0)
    [void][ManagerLaunchProbe]::Run($exe,'','NativeTigerSchemaManager',0)
    if ((Get-FileHash -LiteralPath $config).Hash -ne $before) { throw 'Closing launched windows changed configuration' }
    $broken = Join-Path $temporary 'unreadable-config'
    New-Item -ItemType Directory -Path (Join-Path $broken 'config.txt') -Force | Out-Null
    $env:NATIVE_TIGER_USER_ROOT = $broken
    [void][ManagerLaunchProbe]::Run($exe,'--settings','#32770',1)
    if (!(Test-Path -LiteralPath (Join-Path $broken 'config.txt') -PathType Container)) { throw 'Failed launch changed bad configuration fixture' }
    $report = [ordered]@{status='passed';startup_failure_dialog=$true;failed_launch_exit_code=1;direct_settings_launch=$true;manager_launch=$true;visible_window_style=$true;close_exited=$true;cancel_preserved_config=$true;isolated_desktop=$true;input_desktop_switched=$false;physical_visual_validation=$false;capture_validation='unavailable: inactive-desktop capture was incomplete or blank';exe_sha256=(Get-FileHash -LiteralPath $exe).Hash}
    $report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build\manager-launch-arm64.json') -Encoding UTF8
    $report | ConvertTo-Json
} finally {
    $env:NATIVE_TIGER_USER_ROOT = $previous
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
