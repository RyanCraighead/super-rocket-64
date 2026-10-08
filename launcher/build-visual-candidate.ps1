param(
    [Parameter(Mandatory=$true)][string]$BaseExe,
    [Parameter(Mandatory=$true)][string]$BaseManifest,
    [Parameter(Mandatory=$true)][string]$Output
)
# Build a private presentation candidate around an already verified release payload.
# Never regenerate the engine, change the game data contract, or publish a release.
$ErrorActionPreference = 'Stop'
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\codex\windows\single_exe'))
$basePath = [IO.Path]::GetFullPath($BaseExe)
$manifest = Get-Content -LiteralPath $BaseManifest -Raw | ConvertFrom-Json
if ($manifest.edition -ne 'super-rocket-64' -or $manifest.platform -ne 'windows-x64') { throw 'Wrong base edition/platform' }
if ((Get-FileHash -LiteralPath $basePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $manifest.sha256 -or (Get-Item -LiteralPath $basePath).Length -ne $manifest.size) { throw 'Base executable verification failed' }
$versionSource = Get-Content -LiteralPath (Join-Path $source 'UpdateBuild.cs') -Raw
if ($versionSource -notmatch 'const string Version = "(\d+\.\d+\.\d+)"' -or $Matches[1] -ne $manifest.version) { throw 'Source and payload release versions differ' }
$outputRoot = [IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$payload = Join-Path $outputRoot 'Payload.zip'
$assembly = [Reflection.Assembly]::LoadFile($basePath)
$stream = $assembly.GetManifestResourceStream('Payload')
if ($null -eq $stream) { throw 'Base has no embedded payload' }
$file = [IO.File]::Create($payload)
try { $stream.CopyTo($file) } finally { $file.Dispose(); $stream.Dispose() }
$payloadHash = (Get-FileHash -LiteralPath $payload -Algorithm SHA256).Hash.ToLowerInvariant()
if ($payloadHash -ne $manifest.payload_sha256) { throw 'Embedded payload verification failed' }
$payloadSize = (Get-Item -LiteralPath $payload).Length
$payloadInfo = 'namespace SuperRocket64 { internal static class PayloadInfo { internal const string ZipSha256 = "' + $payloadHash + '"; internal const long ZipSize = ' + $payloadSize + 'L; } }'
[IO.File]::WriteAllText((Join-Path $outputRoot 'PayloadInfo.cs'),$payloadInfo,[Text.UTF8Encoding]::new($false))
$sources = @('Bootstrap.cs','Presentation.cs','PresentationPages.cs','Wizard.cs','Updates.cs','UpdateUi.cs','Shortcuts.cs','UpdateBuild.cs') | ForEach-Object { Join-Path $source $_ }
$resources = Get-ChildItem -LiteralPath (Join-Path $source 'Art') -Filter '*.png' | ForEach-Object { '/resource:' + $_.FullName + ',Concept.' + $_.Name }
$exe = Join-Path $outputRoot ('Super-Rocket-64-v' + $manifest.version + '-Visual-Candidate.exe')
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $compiler /nologo /target:winexe /platform:x64 /optimize+ /warn:4 /warnaserror+ "/win32manifest:$(Join-Path $source 'Launcher.manifest')" "/out:$exe" "/resource:$payload,Payload" /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:System.Web.Extensions.dll $resources $sources (Join-Path $outputRoot 'PayloadInfo.cs')
if ($LASTEXITCODE) { throw 'Candidate compilation failed' }
$evidence = [ordered]@{private_candidate=$true;version=$manifest.version;artifact=$exe;size=(Get-Item -LiteralPath $exe).Length;sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant();base_sha256=$manifest.sha256;payload_sha256=$payloadHash;payload_size=$payloadSize;unchanged_release_payload=$true;published=$false}
[IO.File]::WriteAllText((Join-Path $outputRoot 'candidate-build.json'),($evidence | ConvertTo-Json),[Text.UTF8Encoding]::new($false))
$evidence | ConvertTo-Json
