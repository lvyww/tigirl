param([string]$Exe,[string]$UserRoot,[string]$Dictionary)
$ErrorActionPreference='Stop'
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class SettingsCaptureDesktop {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct Startup {
  public int cb; public string reserved,desktop,title; public int x,y,cx,cy,charsX,charsY,fill,flags;
  public short show,bytes; public IntPtr extra,input,output,error;
 }
 [StructLayout(LayoutKind.Sequential)] struct ProcessInfo { public IntPtr process,thread; public uint pid,tid; }
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateDesktop(string name,IntPtr device,IntPtr mode,uint flags,uint access,IntPtr security);
 [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr desktop);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcess(string app,StringBuilder command,IntPtr ps,IntPtr ts,bool inherit,uint flags,IntPtr env,string directory,ref Startup startup,out ProcessInfo process);
 [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr handle,uint ms);
 [DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr process,out uint code);
 [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr process,uint code);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 public static void Run(string exe,string root,string dictionary) {
  string name="NativeTigerSettingsCapture_"+Guid.NewGuid().ToString("N");
  IntPtr desktop=CreateDesktop(name,IntPtr.Zero,IntPtr.Zero,0,0x01ff,IntPtr.Zero);
  if(desktop==IntPtr.Zero)throw new System.ComponentModel.Win32Exception();
  ProcessInfo process=new ProcessInfo();
  try {
   Startup startup=new Startup();startup.cb=Marshal.SizeOf(typeof(Startup));startup.desktop=name;
   var command=new StringBuilder("\""+exe+"\" \""+root+"\" \""+dictionary+"\" --test-settings \u864e\u7801\u5b57\u8bcd");
   if(!CreateProcess(exe,command,IntPtr.Zero,IntPtr.Zero,false,0,IntPtr.Zero,System.IO.Path.GetDirectoryName(exe),ref startup,out process))throw new System.ComponentModel.Win32Exception();
   if(WaitForSingleObject(process.process,60000)!=0)throw new Exception("Settings capture timed out");
   uint code;if(!GetExitCodeProcess(process.process,out code) || code!=0)throw new Exception("Settings capture failed: "+code);
  } finally {
   if(process.process!=IntPtr.Zero){if(WaitForSingleObject(process.process,0)!=0)TerminateProcess(process.process,99);CloseHandle(process.process);}
   if(process.thread!=IntPtr.Zero)CloseHandle(process.thread);CloseDesktop(desktop);
  }
 }
}
'@
[SettingsCaptureDesktop]::Run($Exe,$UserRoot,$Dictionary)
