$ErrorActionPreference = 'Stop'
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\codex\windows\single_exe'))
$gate = [IO.File]::ReadAllText((Join-Path $source 'SourceValidation.cs'))
$rom = [IO.File]::ReadAllText((Join-Path $source '..\launcher.py'))
$rocket = [IO.File]::ReadAllText((Join-Path $source '..\..\rocketleague\tools\export_octane.py'))
foreach ($pair in @(@('Sm64Sha1','SM64_SHA1',$rom),@('BodySha256','BODY_SHA',$rocket),@('WheelSha256','WHEEL_SHA',$rocket))) {
    $actual = [regex]::Match($gate,('const string ' + $pair[0] + ' = "([a-f0-9]+)"')).Groups[1].Value
    $expected = [regex]::Match($pair[2],($pair[1] + '\s*=\s*["'']([a-f0-9]+)["'']')).Groups[1].Value
    if (!$actual -or !$expected -or $actual -ne $expected) { throw ('Source validation contract drift: ' + $pair[0]) }
}
Write-Output 'PASS 3 source fingerprints match the packaged extraction validators'
