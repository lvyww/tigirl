$ErrorActionPreference = 'Stop'
$instance = [Activator]::CreateInstance([type]::GetTypeFromCLSID([guid]'{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}'))
try {
    @{bits=([IntPtr]::Size * 8); activated=($null -ne $instance); dll=(Get-Item 'Registry::HKEY_CLASSES_ROOT\CLSID\{69CB1B2F-CDE7-43A2-96C9-EBF642E780EB}\InprocServer32').GetValue('')} | ConvertTo-Json -Compress
} finally { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($instance) }
