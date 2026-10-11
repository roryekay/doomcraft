param([ValidateSet('Doom','DoomII')][string]$Game='Doom')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$config=Join-Path $root 'local/install.json'
if(!(Test-Path -LiteralPath $config)) {& "$PSScriptRoot/setup.ps1" -ShowUI; exit}
$data=Get-Content -LiteralPath $config -Raw | ConvertFrom-Json
$wad=$data.$Game
if(!$wad -or !(Test-Path -LiteralPath $wad)) {& "$PSScriptRoot/setup.ps1" -ShowUI; exit}
& "$root/launch.ps1" -Game $Game -WadPath $wad -MinecraftProfileName 'DoomCraft'
