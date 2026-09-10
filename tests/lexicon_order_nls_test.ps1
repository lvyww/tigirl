$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
& "$vs\MSBuild\Current\Bin\MSBuild.exe" "$PSScriptRoot\LexiconOrderProbe.vcxproj" /nr:false /p:Configuration=Release /p:Platform=x64 /v:minimal
if($LASTEXITCODE){throw 'Cannot build filename-order probe'}
# .NET Framework CompareInfo is an independent Windows NLS oracle.
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Linq;
using System.Globalization;
public static class NlsFileOrder {
 public static string[] Expected(string path,string culture) {
  string schema=new DirectoryInfo(path).Name;
  var all=Directory.GetFiles(path);
  return all.Where(p=>p.EndsWith(".txt",StringComparison.OrdinalIgnoreCase))
   .Concat(all.Where(p=>p.EndsWith(".dict.yaml",StringComparison.OrdinalIgnoreCase)))
   .OrderByDescending(p=>String.Equals(Path.GetFileName(p),schema+".txt",StringComparison.OrdinalIgnoreCase)||String.Equals(Path.GetFileName(p),schema+".dict.yaml",StringComparison.OrdinalIgnoreCase))
   .ThenBy(p=>Path.GetFileName(p),StringComparer.Create(CultureInfo.GetCultureInfo(culture),false))
   .Select(p=>String.Concat(Path.GetFileName(p).Select(c=>((int)c).ToString("x4")))).ToArray();
 }
}
'@ -ReferencedAssemblies System.Core
$fixture=Join-Path $env:TEMP ('Tigirl-order-'+[guid]::NewGuid().ToString('N'))
New-Item $fixture -ItemType Directory|Out-Null
try {
 foreach($name in @('a.txt','A.dict.yaml','á.txt','ä.txt','Å.txt','a-b.txt','ab.txt','a b.txt',"a'b.txt",'Σ.txt','ς.txt','İ.txt','ı.txt','方案.txt','中文.TXT','ignored.txtx',([IO.Path]::GetFileName($fixture)+'.TXT'),([IO.Path]::GetFileName($fixture)+'.dict.yaml'))) {
  [IO.File]::WriteAllText((Join-Path $fixture $name),'')
 }
 foreach($culture in @('en-US','zh-CN','sv-SE','tr-TR','de-DE','ja-JP')) {
  $actual=@(& "$PSScriptRoot\..\build\tests\x64\lexicon_order_probe.exe" $fixture $culture)
  if($LASTEXITCODE){throw "Sort failed: $culture"}
  $expected=[NlsFileOrder]::Expected($fixture,$culture)
  if(($actual -join '|') -cne ($expected -join '|')){throw "NLS filename order mismatch: $culture"}
 }
 'PASS: native filename order matches .NET Framework NLS in six cultures, with primary table precedence.'
}finally{Remove-Item $fixture -Recurse -Force}
