Option Explicit
' Double-click: run the complete build in the background without console windows.
' Progress: build\setup-runs\latest-universal.json
' Successful installer: build\packages\latest-universal.json
Dim files, shell, root, powershell, command
Set files = CreateObject("Scripting.FileSystemObject")
Set shell = CreateObject("WScript.Shell")
root = files.GetParentFolderName(WScript.ScriptFullName)
powershell = shell.ExpandEnvironmentStrings("%SystemRoot%") & "\System32\WindowsPowerShell\v1.0\powershell.exe"
If files.FileExists(shell.ExpandEnvironmentStrings("%SystemRoot%") & "\Sysnative\WindowsPowerShell\v1.0\powershell.exe") Then
  powershell = shell.ExpandEnvironmentStrings("%SystemRoot%") & "\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
End If
shell.CurrentDirectory = root
command = Chr(34) & powershell & Chr(34) & " -NoLogo -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File " & Chr(34) & root & "\build_setup_universal.ps1" & Chr(34)
shell.Run command, 0, False
