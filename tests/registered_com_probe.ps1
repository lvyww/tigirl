$ErrorActionPreference = 'Stop'
$instance = [Activator]::CreateInstance([type]::GetTypeFromCLSID([guid]'{D2291A80-84D8-4641-9AB2-BDD1472C846B}'))
try {
    @{bits=([IntPtr]::Size * 8); activated=($null -ne $instance); dll=(Get-Item 'Registry::HKEY_CLASSES_ROOT\CLSID\{D2291A80-84D8-4641-9AB2-BDD1472C846B}\InprocServer32').GetValue('')} | ConvertTo-Json -Compress
} finally { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($instance) }
