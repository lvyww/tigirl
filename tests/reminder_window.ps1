Add-Type @'
using System.Runtime.InteropServices;
using System;
using System.Text;
public static class ReminderClock {
 [DllImport("kernel32.dll")] public static extern ulong GetTickCount64();
}
public static class ReminderWindow {
 delegate bool Visitor(IntPtr window, IntPtr parameter);
 [DllImport("user32.dll")] static extern bool EnumWindows(Visitor visit, IntPtr parameter);
 [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent, Visitor visit, IntPtr parameter);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr window, StringBuilder text, int count);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder text, int count);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr w, IntPtr l);
 static string Text(IntPtr window) {var text=new StringBuilder(128);GetWindowText(window,text,text.Capacity);return text.ToString();}
 public static IntPtr Find(uint process) {
  IntPtr found=IntPtr.Zero;
  EnumWindows(delegate(IntPtr window,IntPtr unused) {uint owner;GetWindowThreadProcessId(window,out owner);
   if(owner==process && Text(window)=="\u8BA1\u65F6\u5668")found=window;return true;},IntPtr.Zero);
  return found;
 }
 public static bool HasExpectedText(IntPtr parent) {
  bool found=false;
  EnumChildWindows(parent,delegate(IntPtr window,IntPtr unused) {if(Text(window)=="\u65F6\u95F4\u5DEE\u4E0D\u591A\u54AF\uFF01")found=true;return true;},IntPtr.Zero);
  return found;
 }
 public static IntPtr Button(IntPtr parent) {
  IntPtr found=IntPtr.Zero;
  EnumChildWindows(parent,delegate(IntPtr window,IntPtr unused) {var kind=new StringBuilder(64);GetClassName(window,kind,64);
   if(kind.ToString()=="Button")found=window;return true;},IntPtr.Zero);
  return found;
 }
}
'@

