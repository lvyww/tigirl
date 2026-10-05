param([string]$Exe,[string]$UserRoot,[string]$Capture)
$ErrorActionPreference='Stop'
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class SettingsHover {
 [StructLayout(LayoutKind.Sequential)] public struct ComboInfo {public int size;public Rect item,button;public uint state;public IntPtr combo,edit,list;}
 [DllImport("user32.dll")] public static extern bool GetComboBoxInfo(IntPtr hwnd,ref ComboInfo info);
 [DllImport("user32.dll")] public static extern int GetWindowRgn(IntPtr hwnd,IntPtr region);
 [DllImport("gdi32.dll")] public static extern IntPtr CreateRectRgn(int l,int t,int r,int b);
 [DllImport("gdi32.dll")] public static extern bool DeleteObject(IntPtr obj);

 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left,top,right,bottom; }
 [StructLayout(LayoutKind.Sequential)] public struct Point { public int x,y; }
 public delegate bool EnumProc(IntPtr hwnd,IntPtr arg);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd,int id);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd,out Rect rect);
 [DllImport("user32.dll")] public static extern bool GetCursorPos(out Point point);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd,IntPtr after,int x,int y,int w,int h,uint flags);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
 [DllImport("user32.dll")] public static extern bool EnumThreadWindows(uint tid,EnumProc callback,IntPtr arg);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd,StringBuilder text,int count);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd,StringBuilder text,int count);
 [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="SendMessageW")] public static extern IntPtr ReadText(IntPtr hwnd,uint message,IntPtr w,StringBuilder text);
 [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(Point point);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd,uint message,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd,uint message,IntPtr w,IntPtr l);
 public static IntPtr VisibleTipWindow(IntPtr window) {
  uint pid;uint tid=GetWindowThreadProcessId(window,out pid);IntPtr found=IntPtr.Zero;
  EnumThreadWindows(tid,(hwnd,arg)=>{var name=new StringBuilder(80);GetClassName(hwnd,name,80);
   if(name.ToString()=="tooltips_class32" && IsWindowVisible(hwnd))found=hwnd;return true;},IntPtr.Zero);return found;
 }
 public static string TipDiagnostic(IntPtr window) {
  uint pid;uint tid=GetWindowThreadProcessId(window,out pid);string found="";
  EnumThreadWindows(tid,(hwnd,arg)=>{var name=new StringBuilder(80);GetClassName(hwnd,name,80);
   if(name.ToString()=="tooltips_class32"){Rect r;GetWindowRect(hwnd,out r);found+="tip="+hwnd+" visible="+IsWindowVisible(hwnd)+" bounds="+r.left+","+r.top+","+r.right+","+r.bottom+"; ";}return true;},IntPtr.Zero);
  return found;
 }
 public static string VisibleTip(IntPtr window) {
  uint pid;uint tid=GetWindowThreadProcessId(window,out pid);string found=null;
  EnumThreadWindows(tid,(hwnd,arg)=>{var name=new StringBuilder(80);GetClassName(hwnd,name,80);
   if(name.ToString()=="tooltips_class32" && IsWindowVisible(hwnd)){var text=new StringBuilder(1000);ReadText(hwnd,0x000D,(IntPtr)1000,text);found="visible:"+text.ToString();}return true;},IntPtr.Zero);
  return found;
 }
}
'@
[void][SettingsHover]::SetThreadDpiAwarenessContext([IntPtr](-4))
$before=(Get-FileHash "$UserRoot\config.txt").Hash
$oldRoot=$env:NATIVE_TIGER_USER_ROOT;$env:NATIVE_TIGER_USER_ROOT=$UserRoot
$p=$null
try {
 $p=Start-Process $Exe -ArgumentList '--settings' -PassThru
 for($i=0;$i -lt 50;$i++){Start-Sleep -Milliseconds 100;$p.Refresh();if($p.MainWindowHandle -ne [IntPtr]::Zero){break}}
 $window=$p.MainWindowHandle
 if($window -eq [IntPtr]::Zero){throw 'No settings window'}
 [void][SettingsHover]::SetWindowPos($window,[IntPtr](-1),60,60,0,0,0x41)
 [void][SettingsHover]::SetForegroundWindow($window)
 [void][SettingsHover]::SendMessage($window,0x111,[IntPtr]207,[IntPtr]::Zero)
 $content=[SettingsHover]::GetDlgItem($window,222);$combo=[SettingsHover]::GetDlgItem($content,208)
 [void][SettingsHover]::SendMessage($combo,0x14F,[IntPtr]1,[IntPtr]::Zero)
 Start-Sleep -Milliseconds 300
 $info=New-Object SettingsHover+ComboInfo;$info.size=[Runtime.InteropServices.Marshal]::SizeOf($info)
 if(![SettingsHover]::GetComboBoxInfo($combo,[ref]$info)){throw 'Cannot get combo popup'}
 if(![SettingsHover]::IsWindowVisible($info.list)){throw 'Popup not visible'}
 $region=[SettingsHover]::CreateRectRgn(0,0,0,0)
 try {if([SettingsHover]::GetWindowRgn($info.list,$region) -ne 0){throw 'Popup still has clipped custom corner region'}} finally{[void][SettingsHover]::DeleteObject($region)}
 $first=[SettingsHover]::SendMessage($info.list,0x18E,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32()
 for($i=0;$i -lt 12;$i++){[void][SettingsHover]::SendMessage($info.list,0x115,[IntPtr]1,[IntPtr]::Zero)}
 Start-Sleep -Milliseconds 200
 $last=[SettingsHover]::SendMessage($info.list,0x18E,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32()
 if($first -eq $last){throw 'Dropdown did not scroll'}
 if($Capture){
  Add-Type -AssemblyName System.Drawing
  $rect=New-Object SettingsHover+Rect;[void][SettingsHover]::GetWindowRect($info.list,[ref]$rect)
  $bitmap=New-Object Drawing.Bitmap ($rect.right-$rect.left),($rect.bottom-$rect.top)
  $graphics=[Drawing.Graphics]::FromImage($bitmap)
  try{$graphics.CopyFromScreen($rect.left,$rect.top,0,0,$bitmap.Size);$bitmap.Save($Capture,[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
 }
 [void][SettingsHover]::SendMessage($combo,0x14F,[IntPtr]::Zero,[IntPtr]::Zero)
 [void][SettingsHover]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
 $p.WaitForExit(5000)|Out-Null
 if((Get-FileHash "$UserRoot\config.txt").Hash -ne $before){throw 'Configuration changed'}
 @{status='passed';rectangular_popup=$true;scroll_top_before=$first;scroll_top_after=$last;config_unchanged=$true;manager_sha256=(Get-FileHash $Exe).Hash}|ConvertTo-Json
}finally{
 if($p -and !$p.HasExited){[void][SettingsHover]::PostMessage($p.MainWindowHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero)}
 $env:NATIVE_TIGER_USER_ROOT=$oldRoot
}
