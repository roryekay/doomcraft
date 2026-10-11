"""Install the built bridge into an isolated official Minecraft Launcher profile."""
from pathlib import Path
import argparse, datetime, json, shutil

ROOT=Path(__file__).resolve().parent
INSTANCE=ROOT/'minecraft-instance'
LOCAL=ROOT/'local'
LAUNCHER=Path.home()/'AppData/Roaming/.minecraft'

def install(api):
    jars=[p for p in (ROOT/'mc/build/libs').glob('*.jar') if not p.name.endswith('-sources.jar')]
    if len(jars)!=1: raise RuntimeError('Build mc first with mc/gradlew.bat -p mc build.')
    version='fabric-loader-0.19.5-26.3'
    if not (LAUNCHER/'versions'/version/(version+'.json')).exists():
        raise RuntimeError('Install Minecraft 26.3 with Fabric Loader 0.19.5 using the official Fabric installer first.')
    mods=INSTANCE/'mods';mods.mkdir(parents=True,exist_ok=True)
    shutil.copy2(jars[0],mods/jars[0].name)
    if not api.is_file(): raise RuntimeError('Provide the official Fabric API 0.161.0+26.3 jar with --fabric-api.')
    shutil.copy2(api,mods/api.name)
    LOCAL.mkdir(exist_ok=True)
    options=INSTANCE/'options.txt'
    if not options.exists():
        primary=LAUNCHER/'options.txt'
        keys=[s for s in primary.read_text(encoding='utf8').splitlines() if s.startswith(('key_','mouseSensitivity:'))] if primary.exists() else []
        keys+=['renderDistance:6','simulationDistance:6','maxFps:60','enableVsync:false','pauseOnLostFocus:false','fullscreen:false','overrideWidth:1280','overrideHeight:720','guiScale:3']
        options.write_text('\n'.join(keys)+'\n',encoding='utf8')
    profile=LAUNCHER/'launcher_profiles.json'
    data=json.loads(profile.read_text(encoding='utf8'))
    stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    shutil.copy2(profile,LOCAL/f'launcher_profiles.backup-{stamp}.json')
    data.setdefault('profiles',{})['doomcraft-isolated']={
        'name':'Minecraft x Doom','type':'custom','icon':'TNT','lastVersionId':version,
        'gameDir':str(INSTANCE),'resolution':{'width':1280,'height':720},
        'javaArgs':'-Xmx4G -Xms1G --enable-native-access=ALL-UNNAMED -Dpassthrough.port=25601'}
    temporary=profile.with_suffix('.doomcraft.tmp')
    temporary.write_text(json.dumps(data,indent=2)+'\n',encoding='utf8');temporary.replace(profile)
    print('Installed Minecraft x Doom. Start that profile in Minecraft Launcher, then launch.ps1.')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--fabric-api',type=Path,required=True)
    args=parser.parse_args();install(args.fabric_api)
