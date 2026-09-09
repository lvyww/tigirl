param([switch]$Arm64X)
$ErrorActionPreference='Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class TigirlResources {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr LoadLibraryExW(string file,IntPtr unused,uint flags);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern IntPtr FindResourceW(IntPtr module,IntPtr name,IntPtr type);
 [DllImport("kernel32.dll")] public static extern IntPtr LoadResource(IntPtr module,IntPtr resource);
 [DllImport("kernel32.dll")] public static extern IntPtr LockResource(IntPtr resource);
 [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr module);
}
'@
$root=Join-Path $PSScriptRoot '..\build'
$files=@("$root\x64\Release\Tigirl.dll","$root\Win32\Release\Tigirl.dll")
foreach($name in @('Tigirl.exe','Tigirl.Import.exe','Tigirl.Reminder.exe')){$files+="$root\x64\Release\$name"}
if($Arm64X){$files+="$root\ARM64X\ARM64EC\Release\Tigirl.dll";foreach($name in @('Tigirl.exe','Tigirl.Import.exe','Tigirl.Reminder.exe','Tigirl.SchemaSelect.exe')){$files+="$root\ARM64X\ARM64EC\Release\$name"}}
foreach($file in $files){
 $file=[IO.Path]::GetFullPath($file)
 $version=[Diagnostics.FileVersionInfo]::GetVersionInfo($file)
 if($version.ProductName -ne 'Tigirl' -or $version.OriginalFilename -ne [IO.Path]::GetFileName($file) -or $version.FileDescription -ne '虎娘 / Tigirl'){throw "Incorrect branding: $file"}
 $module=[TigirlResources]::LoadLibraryExW($file,[IntPtr]::Zero,0x22)
 if(!$module){throw "Cannot inspect resources: $file"}
 try{
  $resource=[TigirlResources]::FindResourceW($module,[IntPtr]12,[IntPtr]14)
  if(!$resource){throw "Missing brand icon: $file"}
  $bytes=[TigirlResources]::LockResource([TigirlResources]::LoadResource($module,$resource))
  if([Runtime.InteropServices.Marshal]::ReadInt16($bytes,4) -ne 10){throw "Missing icon sizes: $file"}
 }finally{[TigirlResources]::FreeLibrary($module)|Out-Null}
}
Add-Type -AssemblyName System.Drawing
$icon=[Drawing.Icon]::ExtractAssociatedIcon($files[2]);$bitmap=$icon.ToBitmap();$bitmap.Save("$root\tigirl-embedded-icon.png");$bitmap.Dispose();$icon.Dispose()
'PASS: DLL/tool names, product descriptions, original filenames, 10 embedded icon sizes.'
