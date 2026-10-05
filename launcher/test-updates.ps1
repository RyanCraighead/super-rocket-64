$ErrorActionPreference = 'Stop'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\codex\windows\single_exe'))
$sources = @('Bootstrap.cs','Updates.cs','UpdateUi.cs','Shortcuts.cs','UpdateBuild.cs') | ForEach-Object { Join-Path $source $_ }
$sources += Join-Path $PSScriptRoot 'PayloadInfo.Test.cs'
$references = @('/r:System.dll','/r:System.Core.dll','/r:System.Drawing.dll','/r:System.Windows.Forms.dll','/r:System.Web.Extensions.dll','/r:System.IO.Compression.dll','/r:System.IO.Compression.FileSystem.dll')
$output = Join-Path ([IO.Path]::GetTempPath()) ('srqa-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $output | Out-Null
$fixture = Join-Path $output 'UpdateFixture.exe'
& $compiler /nologo /warn:4 /warnaserror+ /target:exe /platform:x64 /optimize+ "/out:$fixture" /r:System.Web.Extensions.dll (Join-Path $PSScriptRoot 'UpdateFixture.cs')
if ($LASTEXITCODE) { throw 'Fixture compilation failed' }
$exe = Join-Path $output 'UpdateTests.exe'
& $compiler /nologo /warn:4 /warnaserror+ /target:exe /platform:x64 /optimize+ /main:SuperRocket64.UpdateTests "/out:$exe" $references $sources (Join-Path $PSScriptRoot 'UpdateTests.cs')
if ($LASTEXITCODE) { throw 'Updater test compilation failed' }
$run = Join-Path $output 'tests'
$process = Start-Process -FilePath $exe -ArgumentList @($run,$fixture) -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $output 'stdout.txt') -RedirectStandardError (Join-Path $output 'stderr.txt')
Get-Content -LiteralPath (Join-Path $output 'stdout.txt')
if ($process.ExitCode) { Get-Content -LiteralPath (Join-Path $output 'stderr.txt'); throw 'Updater tests failed' }
Write-Output "Evidence: $output"
