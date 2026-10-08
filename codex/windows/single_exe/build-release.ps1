param(
    [Parameter(Mandatory=$true)][string]$Engine,
    [Parameter(Mandatory=$true)][string]$Dependencies,
    [Parameter(Mandatory=$true)][string]$Output,
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$outputRoot = [IO.Path]::GetFullPath($Output)
$versionSource = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'UpdateBuild.cs') -Raw
if ($versionSource -notmatch 'const string Version = "(\d+\.\d+\.\d+)"') { throw 'Missing launcher version' }
$version = $Matches[1]
& $Python -X utf8 (Join-Path $root 'tools\build_release_payload.py') --engine $Engine --dependencies $Dependencies --output $outputRoot
if ($LASTEXITCODE) { throw 'Release payload generation failed' }
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$exe = Join-Path $outputRoot 'Super-Rocket-64-Windows-x64.exe'
$payload = Join-Path $outputRoot 'Payload.zip'
$sources = @('Bootstrap.cs','Presentation.cs','PresentationPages.cs','Wizard.cs','Updates.cs','UpdateUi.cs','Shortcuts.cs','UpdateBuild.cs') | ForEach-Object { Join-Path $PSScriptRoot $_ }
$conceptResources = Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'Art') -Filter '*.png' | ForEach-Object { '/resource:' + $_.FullName + ',Concept.' + $_.Name }
& $compiler /nologo "/win32manifest:$(Join-Path $PSScriptRoot 'Launcher.manifest')" /target:winexe /platform:x64 /optimize+ /warn:4 /warnaserror+ "/out:$exe" "/resource:$payload,Payload" /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:System.Web.Extensions.dll $conceptResources $sources (Join-Path $outputRoot 'PayloadInfo.cs')
if ($LASTEXITCODE) { throw 'Release launcher compilation failed' }
$manifest = [ordered]@{schema=1;edition='super-rocket-64';platform='windows-x64';update_protocol=1;data_contract=1;version=$version;asset=[IO.Path]::GetFileName($exe);size=(Get-Item -LiteralPath $exe).Length;sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant();payload_sha256=(Get-FileHash -LiteralPath $payload -Algorithm SHA256).Hash.ToLowerInvariant()}
[IO.File]::WriteAllText((Join-Path $outputRoot 'Super-Rocket-64-update.json'), (($manifest | ConvertTo-Json) + "`n"), [Text.UTF8Encoding]::new($false))
$manifest | ConvertTo-Json
