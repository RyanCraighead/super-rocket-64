$ErrorActionPreference = 'Stop'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\codex\windows\single_exe'))
$sources = @('Bootstrap.cs','Presentation.cs','PresentationPages.cs','SourceValidation.cs','Wizard.cs','Updates.cs','UpdateUi.cs','Shortcuts.cs','UpdateBuild.cs') | ForEach-Object { Join-Path $source $_ }
$sources += Join-Path $PSScriptRoot 'PayloadInfo.Test.cs'
$sources += Join-Path $PSScriptRoot 'SourceValidationTests.cs'
$references = @('/r:System.dll','/r:System.Core.dll','/r:System.Drawing.dll','/r:System.Windows.Forms.dll','/r:System.Web.Extensions.dll','/r:System.IO.Compression.dll','/r:System.IO.Compression.FileSystem.dll')
$output = Join-Path ([IO.Path]::GetTempPath()) ('srqa-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $output | Out-Null
$exe = Join-Path $output 'LauncherDesktopTests.exe'
$conceptResources = Get-ChildItem -LiteralPath (Join-Path $source 'Art') -Filter '*.png' | ForEach-Object { '/resource:' + $_.FullName + ',Concept.' + $_.Name }
& $compiler /nologo "/win32manifest:$(Join-Path $source 'Launcher.manifest')" /warn:4 /warnaserror+ /target:exe /platform:x64 /optimize+ /main:SuperRocket64.LauncherDesktopTests "/out:$exe" $references $conceptResources $sources (Join-Path $PSScriptRoot 'UpdateTests.cs') (Join-Path $PSScriptRoot 'LauncherDesktopTests.cs')
if ($LASTEXITCODE) { throw 'UI test compilation failed' }
$process = Start-Process -FilePath $exe -ArgumentList @($output) -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $output 'stdout.txt') -RedirectStandardError (Join-Path $output 'stderr.txt')
Get-Content -LiteralPath (Join-Path $output 'stdout.txt')
if ($process.ExitCode) { Get-Content -LiteralPath (Join-Path $output 'ui-error.txt') -ErrorAction SilentlyContinue; Get-Content -LiteralPath (Join-Path $output 'stderr.txt'); throw 'UI tests failed' }
Get-Content -LiteralPath (Join-Path $output 'ui-result.json')
Get-Content -LiteralPath (Join-Path $output 'desktop-isolation.json')
Write-Output "Evidence: $output"
