$ErrorActionPreference = 'Stop'
$source = $PSScriptRoot
$output = Join-Path ([System.IO.Path]::GetTempPath()) ('SuperRocket64-Launcher-UI-QA-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output -Force | Out-Null
$exe = Join-Path $output 'LauncherDesktopTests.exe'
& 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe' /nologo /warn:4 /warnaserror+ /target:exe /platform:x64 /optimize+ /main:SuperRocket64.LauncherDesktopTests "/out:$exe" /r:System.dll /r:System.Core.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll /r:System.Web.Extensions.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll (Join-Path $source 'Bootstrap.cs') (Join-Path $source 'PayloadInfo.Test.cs') (Join-Path $PSScriptRoot 'LauncherDesktopTests.cs')
if ($LASTEXITCODE) { throw 'Desktop test compilation failed' }
$process = Start-Process -FilePath $exe -ArgumentList ('"' + $output + '"') -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $output 'stdout.txt') -RedirectStandardError (Join-Path $output 'stderr.txt')
Get-Content (Join-Path $output 'stdout.txt')
if ($process.ExitCode) { Get-Content (Join-Path $output 'ui-error.txt') -ErrorAction SilentlyContinue; Get-Content (Join-Path $output 'stderr.txt'); throw 'Separate-desktop test failed' }
Get-Content (Join-Path $output 'ui-result.json')
