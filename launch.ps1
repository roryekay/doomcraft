param([ValidateSet('Doom','DoomII')][string]$Game='Doom',[switch]$TestMap,[string]$WadPath,[string]$MinecraftProfileName='Minecraft x Doom')
$ErrorActionPreference='Stop'
trap {
    $_.Exception.Message | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'local/launcher-error.txt')
    Add-Type -AssemblyName System.Windows.Forms
    [void][System.Windows.Forms.MessageBox]::Show($_.Exception.Message,'Minecraft x Doom could not start',[System.Windows.Forms.MessageBoxButtons]::OK,[System.Windows.Forms.MessageBoxIcon]::Error)
    exit 1
}
$root=$PSScriptRoot
$runtime=Join-Path $root 'local/runtime'
$exe=Join-Path $runtime 'gzdoom.exe'
if(!(Test-Path -LiteralPath $exe)) { throw 'DoomCraft engine build is not installed yet.' }
$wad=$WadPath
if(!$wad) {
    $wad=if($Game -eq 'DoomII'){$env:DOOMCRAFT_DOOM2_WAD}else{$env:DOOMCRAFT_DOOM_WAD}
}
if(!$wad) {
    $steamBase=Join-Path ${env:ProgramFiles(x86)} 'Steam/steamapps/common/Ultimate Doom/base'
    $wad=if($Game -eq 'DoomII'){Join-Path $steamBase 'doom2/DOOM2.WAD'}else{Join-Path $steamBase 'DOOM.WAD'}
}
if(!(Test-Path -LiteralPath $wad)) { throw 'Steam Doom download is not complete or the installation moved.' }
if(!(Get-NetTCPConnection -State Listen -LocalPort 25601 -ErrorAction SilentlyContinue)) {
    Start-Process explorer.exe -ArgumentList 'shell:AppsFolder\Microsoft.4297127D64EC6_8wekyb3d8bbwe!Minecraft'
    Add-Type -AssemblyName System.Windows.Forms
    [void][System.Windows.Forms.MessageBox]::Show("In Minecraft Launcher, select $MinecraftProfileName and click PLAY. Create or open a Minecraft world and leave that window running; Doom will connect when the guest loads. Click OK to open Doom.",'DoomCraft',[System.Windows.Forms.MessageBoxButtons]::OK,[System.Windows.Forms.MessageBoxIcon]::Information)
}
if(!(Get-Process gzdoom -ErrorAction SilentlyContinue)) {
    $arguments=@('-iwad',('"'+$wad+'"'),'-config',('"'+(Join-Path $root "local/$Game.ini")+'"'),'-savedir',('"'+(Join-Path $root "local/saves-$Game")+'"'),'-file',('"'+(Join-Path $runtime 'doomcraft.pk3')+'"'),'-width','1280','-height','720','-window','+exec',('"'+(Join-Path $root 'doomcraft.cfg')+'"'),'+logfile',('"'+(Join-Path $root 'local/doomcraft.log')+'"'))
    if($TestMap) { $arguments+=@('-skill','2','-warp','1'); if($Game -eq 'Doom') { $arguments+='1' } }
    Start-Process -FilePath $exe -WorkingDirectory $runtime -ArgumentList $arguments
}
