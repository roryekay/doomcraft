param([string]$MinecraftHome=(Join-Path $env:APPDATA '.minecraft'),[switch]$ShowUI)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
$Root=Split-Path $PSScriptRoot -Parent

function Find-DoomWads([string[]]$SteamRoots) {
    if(!$SteamRoots) {
        $SteamRoots=@((Join-Path ${env:ProgramFiles(x86)} 'Steam'))
        foreach($key in @('HKCU:\Software\Valve\Steam','HKLM:\SOFTWARE\WOW6432Node\Valve\Steam')) {
            $item=Get-ItemProperty $key -ErrorAction SilentlyContinue
            if($item.SteamPath){$SteamRoots+=$item.SteamPath}
            if($item.InstallPath){$SteamRoots+=$item.InstallPath}
        }
    }
    $libraries=@($SteamRoots)
    foreach($steam in $SteamRoots) {
        $vdf=Join-Path $steam 'steamapps/libraryfolders.vdf'
        if(Test-Path -LiteralPath $vdf) {
            foreach($m in [regex]::Matches([IO.File]::ReadAllText($vdf),'"path"\s+"([^"]+)"')) {
                $libraries+=$m.Groups[1].Value.Replace('\\','\')
            }
        }
    }
    $found=@{}
    foreach($library in ($libraries | Select-Object -Unique)) {
        $common=Join-Path $library 'steamapps/common'
        if(!(Test-Path -LiteralPath $common)){continue}
        foreach($folder in (Get-ChildItem -LiteralPath $common -Directory -ErrorAction SilentlyContinue | Where-Object Name -Match 'doom')) {
            foreach($file in (Get-ChildItem -LiteralPath $folder.FullName -Recurse -File -ErrorAction SilentlyContinue | Where-Object Name -Match '^(doom|doom2)\.wad$')) {
                if(Test-Iwad $file.FullName){$found[$file.Name.ToUpperInvariant()]=$file.FullName}
            }
        }
    }
    return $found
}

function Test-Iwad([string]$Path) {
    if(!(Test-Path -LiteralPath $Path -PathType Leaf)){return $false}
    $stream=[IO.File]::OpenRead($Path)
    try {
        $bytes=New-Object byte[] 4
        return ($stream.Length -gt 12 -and $stream.Read($bytes,0,4) -eq 4 -and [Text.Encoding]::ASCII.GetString($bytes) -eq 'IWAD')
    } finally {$stream.Dispose()}
}

function Install-Guest([string]$MinecraftFolder) {
    $defaultHome=[IO.Path]::GetFullPath((Join-Path $env:APPDATA '.minecraft'))
    if([IO.Path]::GetFullPath($MinecraftFolder) -eq $defaultHome -and (Get-Process MinecraftLauncher -ErrorAction SilentlyContinue)){throw 'Close Minecraft Launcher, then click Set up again. This prevents it overwriting the new profile.'}
    $manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'downloads.json') -Raw | ConvertFrom-Json
    foreach($artifact in $manifest.artifacts) {
        $target=Join-Path $MinecraftFolder $artifact.path
        if(!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $artifact.sha256) {
            New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
            $temp=$target+'.doomcraft-download'
            try {
                Invoke-WebRequest -Uri $artifact.url -OutFile $temp -UseBasicParsing
                if((Get-FileHash -LiteralPath $temp -Algorithm SHA256).Hash -ne $artifact.sha256){throw "Download verification failed: $($artifact.path)"}
                Move-Item -LiteralPath $temp -Destination $target -Force
            } finally {if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp}}
        }
    }
    $version='fabric-loader-0.19.5-26.3'
    $versions=Join-Path $MinecraftFolder "versions/$version"
    New-Item -ItemType Directory -Force -Path $versions | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'fabric-profile.json') -Destination (Join-Path $versions "$version.json") -Force
    $instance=Join-Path $Root 'minecraft-instance'
    $mods=Join-Path $instance 'mods'
    New-Item -ItemType Directory -Force -Path $mods | Out-Null
    Copy-Item -LiteralPath (Join-Path $Root 'guest/passthrough-0.1.0-doom.jar') -Destination $mods -Force
    Copy-Item -LiteralPath (Join-Path $MinecraftFolder $manifest.fabricApiPath) -Destination $mods -Force
    $options=Join-Path $instance 'options.txt'
    if(!(Test-Path -LiteralPath $options)) {
        $keys=@()
        $original=Join-Path $MinecraftFolder 'options.txt'
        if(Test-Path -LiteralPath $original){$keys=@(Get-Content -LiteralPath $original | Where-Object {$_ -match '^(key_|mouseSensitivity:)'})}
        $keys+=@('renderDistance:6','simulationDistance:6','maxFps:60','enableVsync:false','pauseOnLostFocus:false','fullscreen:false','overrideWidth:1280','overrideHeight:720','guiScale:3')
        [IO.File]::WriteAllLines($options,$keys,(New-Object Text.UTF8Encoding $false))
    }
    $profiles=@(Get-ChildItem -LiteralPath $MinecraftFolder -Filter 'launcher_profiles*.json' -File | Where-Object Name -In @('launcher_profiles.json','launcher_profiles_microsoft_store.json'))
    if(!$profiles){throw 'Run Minecraft Launcher and sign in once, close it, then click Set up again.'}
    foreach($file in $profiles) {
        $data=Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        if(!$data.profiles){$data | Add-Member profiles ([pscustomobject]@{}) -Force}
        $profile=[pscustomobject]@{name='DoomCraft';type='custom';icon='TNT';lastVersionId=$version;gameDir=$instance;resolution=@{width=1280;height=720};javaArgs='-Xmx4G -Xms1G --enable-native-access=ALL-UNNAMED -Dpassthrough.port=25601'}
        $data.profiles | Add-Member 'doomcraft-installer' $profile -Force
        $backup=Join-Path $Root ('local/'+$file.BaseName+'.backup-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssffff')+'.json')
        Copy-Item -LiteralPath $file.FullName -Destination $backup
        $temp=$file.FullName+'.doomcraft-tmp'
        [IO.File]::WriteAllText($temp,($data | ConvertTo-Json -Depth 100),(New-Object Text.UTF8Encoding $false))
        Move-Item -LiteralPath $temp -Destination $file.FullName -Force
    }
}

function Install-Runtime {
    $manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'downloads.json') -Raw | ConvertFrom-Json
    $cache=Join-Path $Root 'local/downloads';New-Item -ItemType Directory -Force -Path $cache | Out-Null
    $zip=Join-Path $cache 'gzdoom-stock.zip'
    if(!(Test-Path -LiteralPath $zip) -or (Get-FileHash -LiteralPath $zip).Hash -ne $manifest.stock.sha256) {
        Invoke-WebRequest -Uri $manifest.stock.url -OutFile $zip -UseBasicParsing
        if((Get-FileHash -LiteralPath $zip).Hash -ne $manifest.stock.sha256){throw 'GZDoom runtime download verification failed'}
    }
    $unpack=Join-Path $cache 'stock';Expand-Archive -LiteralPath $zip -DestinationPath $unpack -Force
    foreach($name in @('openal32.dll','sndfile.dll','zmusic.dll','licenses.zip')) {
        $file=@(Get-ChildItem -LiteralPath $unpack -Recurse -File -Filter $name)
        if($file.Count -ne 1){throw "Missing runtime dependency: $name"}
        Copy-Item -LiteralPath $file[0].FullName -Destination (Join-Path $Root 'local/runtime') -Force
    }
    foreach($name in @('fm_banks','soundfonts')) {
        $folder=@(Get-ChildItem -LiteralPath $unpack -Recurse -Directory | Where-Object Name -EQ $name)
        if($folder.Count -eq 1){Copy-Item -LiteralPath $folder[0].FullName -Destination (Join-Path $Root 'local/runtime') -Recurse -Force}
    }
}

function Show-Setup {
    Add-Type -AssemblyName System.Windows.Forms
    $form=New-Object Windows.Forms.Form
    $form.Text='DoomCraft game setup';$form.Width=650;$form.Height=440;$form.StartPosition='CenterScreen'
    $intro=New-Object Windows.Forms.Label
    $intro.SetBounds(20,15,590,55);$intro.Text='DoomCraft finds installed Steam Doom files and installs Fabric and the bridge. You need Minecraft Java Edition and your own Doom games. Close Minecraft Launcher before setup.';$form.Controls.Add($intro)
    $fields=@{}
    $wads=Find-DoomWads
    foreach($spec in @(@('Doom','DOOM.WAD',85),@('DoomII','DOOM2.WAD',135),@('Minecraft','Minecraft folder',185))) {
        $name=$spec[0];$y=[int]$spec[2]
        $label=New-Object Windows.Forms.Label;$label.SetBounds(20,$y,110,22);$label.Text=$spec[1];$form.Controls.Add($label)
        $box=New-Object Windows.Forms.TextBox;$box.SetBounds(135,$y,375,25)
        $box.Text=if($name -eq 'Minecraft'){$MinecraftHome}else{[string]$wads[$spec[1]]}
        $form.Controls.Add($box);$fields[$name]=$box
        $browse=New-Object Windows.Forms.Button;$browse.SetBounds(520,$y,90,25);$browse.Text='Browse';$browse.Tag=$name
        $browse.Add_Click({
            $box=$fields[$this.Tag]
            if($this.Tag -eq 'Minecraft'){$dialog=New-Object Windows.Forms.FolderBrowserDialog;if($dialog.ShowDialog() -eq 'OK'){$box.Text=$dialog.SelectedPath}}
            else {$dialog=New-Object Windows.Forms.OpenFileDialog;$dialog.Filter='Doom IWAD files|*.wad';if($dialog.ShowDialog() -eq 'OK'){$box.Text=$dialog.FileName}}
        });$form.Controls.Add($browse)
    }
    $status=New-Object Windows.Forms.Label;$status.SetBounds(20,230,590,80);$status.Text='Detected paths appear above. Browse if a game is installed elsewhere. Either Doom game is enough; both are recommended.';$form.Controls.Add($status)
    $install=New-Object Windows.Forms.Button;$install.SetBounds(20,325,160,35);$install.Text='Set up DoomCraft'
    $install.Add_Click({
        $this.Enabled=$false;$status.Text='Installing verified Fabric libraries and configuring Minecraft...';$form.Refresh()
        try {
            $config=@{MinecraftHome=$fields.Minecraft.Text}
            foreach($name in @('Doom','DoomII')){if($fields[$name].Text){if(!(Test-Iwad $fields[$name].Text)){throw "Invalid $name IWAD file"};$config[$name]=$fields[$name].Text}}
            if(!$config.Doom -and !$config.DoomII){throw 'Install Doom through Steam first, or browse to your original DOOM.WAD / DOOM2.WAD.'}
            if(!(Test-Path -LiteralPath $config.MinecraftHome)){throw 'Install and open Minecraft Launcher once, sign in, then retry setup.'}
            New-Item -ItemType Directory -Force -Path (Join-Path $Root 'local') | Out-Null
            Install-Runtime
            Install-Guest $config.MinecraftHome
            [IO.File]::WriteAllText((Join-Path $Root 'local/install.json'),($config | ConvertTo-Json),(New-Object Text.UTF8Encoding $false))
            $status.Text='Ready! Open Minecraft Launcher, select the DoomCraft installation and click Play. Create/open a Minecraft world, then use the DoomCraft desktop shortcut. Use this setup again if you move either game.'
        } catch {$status.Text=$_.Exception.Message} finally {$this.Enabled=$true}
    });$form.Controls.Add($install)
    $link=New-Object Windows.Forms.LinkLabel;$link.SetBounds(205,330,400,30);$link.Text='Get Minecraft Launcher';$link.Add_LinkClicked({Start-Process 'https://www.minecraft.net/en-us/download'});$form.Controls.Add($link)
    [void]$form.ShowDialog()
}
if($ShowUI){Show-Setup}
