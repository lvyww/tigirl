$ErrorActionPreference='Stop'
$build=Join-Path $PSScriptRoot '..\build'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
foreach($arch in @('x64','Win32','ARM64')) {
 foreach($test in @('SentenceEmptyCodeProbe','SentenceTabLockProbe','SentenceContinuationProbe','SentenceSessionProbe')) {
  & "$vs\MSBuild\Current\Bin\MSBuild.exe" "$PSScriptRoot\..\tests\$test.vcxproj" /nr:false /p:Configuration=Release "/p:Platform=$arch" /v:minimal
  if($LASTEXITCODE){throw "Build failed: $test $arch"}
 }
 $empty=Join-Path $build ('empty-code-'+$arch+'-'+[guid]::NewGuid().ToString('N')+'.tcd')
 & "$build\tests\$arch\sentence_empty_code_probe.exe" $empty
 if($LASTEXITCODE){throw "Empty-code ambiguity failed: $arch"}
 $fixture=Join-Path $build ('tab-lock-'+$arch+'-'+[guid]::NewGuid().ToString('N')+'.tcd')
 & "$build\tests\$arch\sentence_tab_lock_probe.exe" $fixture
 if($LASTEXITCODE){throw "Tab lock failed: $arch"}
 $continuation=Join-Path $build ('continuation-'+$arch+'-'+[guid]::NewGuid().ToString('N')+'.tcd')
 & "$build\tests\$arch\sentence_continuation_probe.exe" $continuation
 if($LASTEXITCODE){throw "Continuation failed: $arch"}
 & "$build\tests\$arch\sentence_session_probe.exe"
 if($LASTEXITCODE){throw "Session failed: $arch"}
}
