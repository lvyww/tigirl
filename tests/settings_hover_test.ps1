param([string]$Exe,[string]$UserRoot,[string]$Capture)
$ErrorActionPreference='Stop'
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class SettingsHover {
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
$cursor=New-Object SettingsHover+Point
[void][SettingsHover]::GetCursorPos([ref]$cursor)
$before=(Get-FileHash "$UserRoot\config.txt").Hash
$previousRoot=$env:NATIVE_TIGER_USER_ROOT
$env:NATIVE_TIGER_USER_ROOT=$UserRoot
$process=$null
try {
 $process=Start-Process $Exe -ArgumentList '--settings' -PassThru
 for($i=0;$i -lt 40;$i++){Start-Sleep -Milliseconds 100;$process.Refresh();if($process.MainWindowHandle -ne [IntPtr]::Zero){break}}
 $window=$process.MainWindowHandle
 if($window -eq [IntPtr]::Zero){throw 'Settings window did not open'}
 [void][SettingsHover]::SetWindowPos($window,[IntPtr](-1),40,40,0,0,0x41)
 [void][SettingsHover]::SetForegroundWindow($window)
 $content=[SettingsHover]::GetDlgItem($window,222)
 $target=[SettingsHover]::GetDlgItem($content,105)
 $windowClass=New-Object Text.StringBuilder 80
 [void][SettingsHover]::GetClassName($window,$windowClass,80)
 if($target -eq [IntPtr]::Zero){throw "Target missing; main class $windowClass"}
 Start-Sleep -Milliseconds 300
 $bounds=New-Object SettingsHover+Rect
 [void][SettingsHover]::GetWindowRect($target,[ref]$bounds)
 [void][SettingsHover]::SetCursorPos(($bounds.left+25),($bounds.top+10))
 [void][SettingsHover]::SetWindowPos($window,[IntPtr](-1),0,0,0,0,0x13)
 [void][SettingsHover]::SendMessage($target,0x0200,[IntPtr]::Zero,[IntPtr]((10 -shl 16) -bor 25))
 Start-Sleep -Milliseconds 1400
 $actualCursor=New-Object SettingsHover+Point;[void][SettingsHover]::GetCursorPos([ref]$actualCursor)
 $rootBounds=New-Object SettingsHover+Rect;[void][SettingsHover]::GetWindowRect($window,[ref]$rootBounds)
 $diagnostic="cursor $($actualCursor.x),$($actualCursor.y); window visible $([SettingsHover]::IsWindowVisible($window)); bounds $($rootBounds.left),$($rootBounds.top),$($rootBounds.right),$($rootBounds.bottom)"
 $point=New-Object SettingsHover+Point;$point.x=$bounds.left+25;$point.y=$bounds.top+10
 if([SettingsHover]::WindowFromPoint($point) -ne $target){$hit=[SettingsHover]::WindowFromPoint($point);$hitClass=New-Object Text.StringBuilder 80;[void][SettingsHover]::GetClassName($hit,$hitClass,80);throw "Hover target is obscured by $hitClass; $diagnostic; root $windowClass; point $($point.x),$($point.y); target $target; hit $hit; $([SettingsHover]::TipDiagnostic($window))"}
 $text=[SettingsHover]::VisibleTip($window)
 if(!$text -or !$text.Contains('清屏')){throw "Hover tip did not appear: $text $diagnostic; $([SettingsHover]::TipDiagnostic($window))"}
 if($Capture){
  Add-Type -AssemblyName System.Drawing
  [void][SettingsHover]::GetWindowRect($window,[ref]$bounds)
  $bitmap=New-Object Drawing.Bitmap ($bounds.right-$bounds.left),($bounds.bottom-$bounds.top)
  $graphics=[Drawing.Graphics]::FromImage($bitmap)
  try{$graphics.CopyFromScreen($bounds.left,$bounds.top,0,0,$bitmap.Size);$bitmap.Save($Capture,[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
  [void][SettingsHover]::GetWindowRect([SettingsHover]::VisibleTipWindow($window),[ref]$bounds)
  $bitmap=New-Object Drawing.Bitmap ($bounds.right-$bounds.left),($bounds.bottom-$bounds.top)
  $graphics=[Drawing.Graphics]::FromImage($bitmap)
  $tipCapture=[IO.Path]::Combine([IO.Path]::GetDirectoryName($Capture),[IO.Path]::GetFileNameWithoutExtension($Capture)+'-tip.png')
  try{$graphics.CopyFromScreen($bounds.left,$bounds.top,0,0,$bitmap.Size);$bitmap.Save($tipCapture,[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
 }
 [void][SettingsHover]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
 $process.WaitForExit(5000)|Out-Null
 if((Get-FileHash "$UserRoot\config.txt").Hash -ne $before){throw 'Hover changed config'}
 $report=@{status='passed';actual_mouse_hover=$true;mouse_move_relayed=$true;tip=$text;config_unchanged=$true;manager_sha256=(Get-FileHash $Exe).Hash}
 $report|ConvertTo-Json|Set-Content "$UserRoot\hover-report.json" -Encoding utf8
 $report|ConvertTo-Json -Compress
} finally {
 if($process -and !$process.HasExited){[void][SettingsHover]::PostMessage($process.MainWindowHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero)}
 [void][SettingsHover]::SetCursorPos($cursor.x,$cursor.y)
 $env:NATIVE_TIGER_USER_ROOT=$previousRoot
}
