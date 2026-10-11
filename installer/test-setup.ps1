$ErrorActionPreference='Stop'
. "$PSScriptRoot/setup.ps1"
$source=Join-Path (Split-Path $PSScriptRoot -Parent) 'local/installer-payload'
$Root=Join-Path $env:TEMP ('dc-test-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
Copy-Item -LiteralPath $source -Destination $Root -Recurse
function Assert($condition,$message){if(!$condition){throw $message}}
$fixture=Join-Path $Root 'local/fixture'
$MinecraftFolder=Join-Path $fixture 'minecraft';New-Item -ItemType Directory -Force -Path $MinecraftFolder | Out-Null
$original='{"profiles":{"existing":{"name":"Keep my profile"}},"settings":{"test":"preserved"}}'
[IO.File]::WriteAllText((Join-Path $MinecraftFolder 'launcher_profiles.json'),$original)
[IO.File]::WriteAllText((Join-Path $MinecraftFolder 'options.txt'),"key_key.inventory:key.keyboard.r`nmouseSensitivity:0.37`n")
$steam=Join-Path $fixture 'steam';$library=Join-Path $fixture 'another drive'
New-Item -ItemType Directory -Force -Path "$steam/steamapps","$library/steamapps/common/DOOM + DOOM II/base" | Out-Null
[IO.File]::WriteAllText("$steam/steamapps/libraryfolders.vdf",('"libraryfolders" { "1" { "path" "'+$library.Replace('\','\\')+'" } }'))
[IO.File]::WriteAllBytes("$library/steamapps/common/DOOM + DOOM II/base/DOOM.WAD",[Text.Encoding]::ASCII.GetBytes('IWAD'+('x'*16)))
[IO.File]::WriteAllBytes("$library/steamapps/common/DOOM + DOOM II/base/DOOM2.WAD",[Text.Encoding]::ASCII.GetBytes('PWAD'+('x'*16)))
$found=Find-DoomWads @($steam)
Assert ($found['DOOM.WAD'] -and !$found['DOOM2.WAD']) 'Steam library or IWAD validation failed'
Install-Guest $MinecraftFolder
$data=Get-Content "$MinecraftFolder/launcher_profiles.json" -Raw | ConvertFrom-Json
Assert ($data.profiles.existing.name -eq 'Keep my profile' -and $data.settings.test -eq 'preserved') 'Existing launcher metadata changed'
Assert ($data.profiles.'doomcraft-installer'.gameDir -eq (Join-Path $Root 'minecraft-instance')) 'Wrong isolated game directory'
Assert (Test-Path "$Root/minecraft-instance/mods/passthrough-0.1.0-doom.jar") 'Missing bridge'
Assert ((Get-Content "$Root/minecraft-instance/options.txt" -Raw) -match 'key_key.inventory:key.keyboard.r') 'Keybind import failed'
[IO.File]::WriteAllText("$Root/minecraft-instance/options.txt",'user modified options')
Install-Guest $MinecraftFolder
Assert ((Get-Content "$Root/minecraft-instance/options.txt" -Raw) -eq 'user modified options') 'Reinstall overwrote options'
Assert (@(Get-ChildItem "$Root/local/launcher_profiles.backup-*.json").Count -ge 2) 'Profile backups missing'
Install-Runtime
Assert (Test-Path "$Root/local/runtime/zmusic.dll") 'Missing sound dependency'
Write-Output 'PASS: secondary Steam library, invalid IWAD, verified downloads, profile preservation, backups, keybind import, reinstall options, runtime dependencies.'
