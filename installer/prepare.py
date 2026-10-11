"""Stage explicit release files. Never copy a player's Minecraft instance or logs."""
import argparse, hashlib, json, shutil, urllib.request, zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def get(url):
    request=urllib.request.Request(url,headers={'User-Agent':'DoomCraft-release-builder'})
    return urllib.request.urlopen(request).read()

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--runtime',type=Path,required=True)
    parser.add_argument('--guest',type=Path,required=True)
    parser.add_argument('--engine-source',type=Path,required=True)
    args=parser.parse_args()
    payload=ROOT/'local/installer-payload'
    assert payload.resolve().parent == (ROOT/'local').resolve()
    if payload.exists():shutil.rmtree(payload)
    payload.mkdir(parents=True,exist_ok=True)
    runtime=payload/'local/runtime';runtime.mkdir(parents=True,exist_ok=True)
    for name in ['gzdoom.exe','gzdoom.pk3','doomcraft.pk3','brightmaps.pk3','lights.pk3','game_support.pk3','game_widescreen_gfx.pk3']:
        shutil.copy2(args.runtime/name,runtime/name)
    (payload/'guest').mkdir(exist_ok=True)
    shutil.copy2(args.guest,payload/'guest/passthrough-0.1.0-doom.jar')
    profile=json.loads(get('https://meta.fabricmc.net/v2/versions/loader/26.3/0.19.5/profile/json'))
    profile['releaseTime']=profile['time']='2026-10-10T00:00:00Z'
    (ROOT/'installer/fabric-profile.json').write_text(json.dumps(profile,indent=2)+'\n')
    artifacts=[]
    for lib in profile['libraries']:
        group,name,version=lib['name'].split(':')
        path=f'{group.replace(".","/")}/{name}/{version}/{name}-{version}.jar'
        url=lib['url']+path
        digest=lib.get('sha256') or get(url+'.sha256').decode().strip().split()[0]
        artifacts.append(dict(path='libraries/'+path,url=url,sha256=digest))
    api='fabric-api-0.161.0+26.3.jar'
    url='https://maven.fabricmc.net/net/fabricmc/fabric-api/fabric-api/0.161.0+26.3/'+api
    api_path='doomcraft-downloads/'+api
    artifacts.append(dict(path=api_path,url=url,sha256=get(url+'.sha256').decode().strip().split()[0]))
    manifest=dict(artifacts=artifacts,fabricApiPath=api_path,stock=dict(url='https://github.com/ZDoom/gzdoom/releases/download/g4.14.2/gzdoom-4-14-2-windows.zip',sha256='6d01c5bb48610cebfa9dfd0b7705a1e20bfd56032a74484ac256d8c4e3c51bee'))
    (ROOT/'installer/downloads.json').write_text(json.dumps(manifest,indent=2)+'\n')
    for name in ['launch.ps1','doomcraft.cfg','CONTROLS.md','THIRD_PARTY.md','LICENSE']:
        shutil.copy2(ROOT/name,payload/name)
    for name in ['licenses','installer']:
        shutil.copytree(ROOT/name,payload/name,dirs_exist_ok=True,ignore=shutil.ignore_patterns('prepare.py','test-setup.ps1','doomcraft.iss','__pycache__'))
    release=ROOT/'local/release';release.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(release/'DoomCraft-Engine-Source-0.1.0.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for file in args.engine_source.rglob('*'):
            if file.is_file() and '.git' not in file.relative_to(args.engine_source).parts:
                archive.write(file,'gzdoom/'+file.relative_to(args.engine_source).as_posix())
        for name in ['patch-engine.py','README.md','THIRD_PARTY.md','LICENSE']:
            archive.write(ROOT/name,'doomcraft/'+name)
        for file in (ROOT/'native').rglob('*'):
            if file.is_file():archive.write(file,'doomcraft/native/'+file.relative_to(ROOT/'native').as_posix())
    hashes={str(p.relative_to(payload)).replace('\\','/'):sha(p) for p in payload.rglob('*') if p.is_file()}
    (release/'payload-sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')
    print(f'Staged {len(hashes)} explicit files; proprietary games, accounts and saves excluded.')

if __name__=='__main__':main()
