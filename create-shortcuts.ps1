$ErrorActionPreference='Stop'
$desktop=[Environment]::GetFolderPath('Desktop')
$shell=New-Object -ComObject WScript.Shell
foreach($game in @('Doom','DoomII')) {
    $label=if($game -eq 'DoomII'){'Minecraft x Doom II'}else{'Minecraft x Doom'}
    $path=Join-Path $desktop "$label.lnk"
    $shortcut=$shell.CreateShortcut($path)
    $shortcut.TargetPath=Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
    $shortcut.Arguments='-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "'+(Join-Path $PSScriptRoot 'launch.ps1')+'" -Game '+$game+' -TestMap'
    $shortcut.WorkingDirectory=$PSScriptRoot
    $shortcut.IconLocation=(Join-Path $PSScriptRoot 'local/runtime/gzdoom.exe')+',0'
    $shortcut.Description='Real Minecraft inside classic Doom. Uses the isolated Minecraft x Doom profile and your Steam WAD.'
    $shortcut.Save()
    Write-Output $path
}
