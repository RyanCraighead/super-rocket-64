$ErrorActionPreference = 'Stop'
$compiler = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$testExecutable = Join-Path ([System.IO.Path]::GetTempPath()) ('SuperRocket64-Headless-' + [Guid]::NewGuid().ToString('N') + '.exe')
& $compiler /nologo /warn:4 /warnaserror+ /target:exe /platform:x64 /optimize+ /main:SuperRocket64.BootstrapTests "/out:$testExecutable" /r:System.dll /r:System.Core.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll /r:System.Web.Extensions.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll (Join-Path $PSScriptRoot '..\codex\windows\single_exe\Bootstrap.cs') (Join-Path $PSScriptRoot 'BootstrapTests.cs') (Join-Path $PSScriptRoot 'PayloadInfo.Test.cs')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $testExecutable
exit $LASTEXITCODE
