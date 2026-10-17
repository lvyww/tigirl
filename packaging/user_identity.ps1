# Identity, not elevation alone, decides whether this is the interactive user's data.
function Get-TigirlSessionUserSid {
    if(!('TigirlSessionIdentity' -as [type])){
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Security.Principal;
public static class TigirlSessionIdentity {
 [DllImport("wtsapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 static extern bool WTSQuerySessionInformationW(IntPtr server,int session,int kind,out IntPtr text,out int bytes);
 [DllImport("wtsapi32.dll")] static extern void WTSFreeMemory(IntPtr value);
 static string Query(int kind) {
  IntPtr text;int bytes;
  if(!WTSQuerySessionInformationW(IntPtr.Zero,-1,kind,out text,out bytes))throw new System.ComponentModel.Win32Exception();
  try {return Marshal.PtrToStringUni(text);}finally {WTSFreeMemory(text);}
 }
 public static string Sid() {
  string name=Query(5),domain=Query(7);
  if(String.IsNullOrEmpty(name))throw new InvalidOperationException("No interactive session user");
  return new NTAccount(domain,name).Translate(typeof(SecurityIdentifier)).Value;
 }
}
'@
    }
    return [TigirlSessionIdentity]::Sid()
}
function Test-TigirlInitializationIdentity([bool]$Elevated,[string]$CurrentSid,[string]$SessionSid) {
    if(!$Elevated){return $true}
    return ![string]::IsNullOrEmpty($CurrentSid) -and $CurrentSid -eq $SessionSid
}
