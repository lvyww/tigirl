$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
public static class TestDesktopState {
 [StructLayout(LayoutKind.Sequential)] struct LastInput { public uint size, time; }
 [DllImport("user32.dll", SetLastError=true)] static extern bool GetLastInputInfo(ref LastInput value);
 [DllImport("kernel32.dll")] static extern uint GetTickCount();
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll", SetLastError=true)] static extern IntPtr OpenInputDesktop(uint flags, bool inherit, uint access);
 [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool GetUserObjectInformation(IntPtr desktop, int index, StringBuilder value, int length, out int needed);
 [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr desktop);
 public static uint IdleMilliseconds() {
  var value = new LastInput { size=(uint)Marshal.SizeOf(typeof(LastInput)) };
  if(!GetLastInputInfo(ref value)) throw new Win32Exception();
  return unchecked(GetTickCount()-value.time);
 }
 public static string DesktopName() {
  var handle=OpenInputDesktop(0,false,1);
  if(handle==IntPtr.Zero) throw new Win32Exception();
  try {
   var name=new StringBuilder(256);int needed;
   if(!GetUserObjectInformation(handle,2,name,512,out needed))throw new Win32Exception();
   return name.ToString();
  } finally {CloseDesktop(handle);}
 }
}
'@
@{ foreground=[TestDesktopState]::GetForegroundWindow().ToInt64();
   desktop=[TestDesktopState]::DesktopName();
   idle_ms=[TestDesktopState]::IdleMilliseconds();
   timestamp_utc=[DateTime]::UtcNow.ToString('o') } | ConvertTo-Json
