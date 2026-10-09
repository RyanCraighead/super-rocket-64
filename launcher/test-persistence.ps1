param([string]$SourceRoot = ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))))
$ErrorActionPreference = 'Stop'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$source = Join-Path $SourceRoot 'codex\windows\single_exe'
$sources = @(Get-ChildItem -LiteralPath $source -Filter '*.cs' | Select-Object -ExpandProperty FullName)
$sources += Join-Path $PSScriptRoot 'PayloadInfo.Test.cs'
$sources += Join-Path $SourceRoot 'launcher\UpdateTests.cs'
$sources += Join-Path $PSScriptRoot 'PersistenceTests.cs'
$refs = @('/r:System.dll','/r:System.Core.dll','/r:System.Drawing.dll','/r:System.Windows.Forms.dll','/r:System.Web.Extensions.dll','/r:System.IO.Compression.dll','/r:System.IO.Compression.FileSystem.dll')
$output = Join-Path ([IO.Path]::GetTempPath()) ('sr-persistence-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $output | Out-Null
$fixture = Join-Path $output 'Fixture.exe'
& $compiler /nologo /warn:4 /warnaserror+ /target:exe /platform:x64 /optimize+ "/out:$fixture" /r:System.Web.Extensions.dll (Join-Path $PSScriptRoot 'UpdateFixture.cs')
if ($LASTEXITCODE) { throw 'Fixture compilation failed' }
$exe = Join-Path $output 'Persistence.exe'
& $compiler /nologo /warn:4 /warnaserror+ /target:exe /platform:x64 /optimize+ /main:SuperRocket64.PersistenceTests "/out:$exe" $refs $sources
if ($LASTEXITCODE) { throw 'Persistence test compilation failed' }
$process = Start-Process -FilePath $exe -ArgumentList @((Join-Path $output 'data-test'),$fixture) -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $output 'stdout.txt') -RedirectStandardError (Join-Path $output 'stderr.txt')
Get-Content -LiteralPath (Join-Path $output 'stdout.txt')
if ($process.ExitCode) { Get-Content -LiteralPath (Join-Path $output 'stderr.txt'); throw 'Persistence tests failed' }
Write-Output "Evidence: $output"
