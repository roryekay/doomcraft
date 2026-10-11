"""Install only reviewed engine resources and our built executable into a portable folder."""
from pathlib import Path
import shutil,zipfile,hashlib,json
ROOT=Path(__file__).resolve().parent
LOCAL=ROOT/'local'
runtime=LOCAL/'runtime'
runtime.mkdir(exist_ok=True)
for p in (LOCAL/'gzdoom-stock').iterdir():
    if p.name=='gzdoom.exe':continue
    if p.is_dir():shutil.copytree(p,runtime/p.name,dirs_exist_ok=True)
    else:shutil.copy2(p,runtime/p.name)
exe=LOCAL/'gzdoom-build/Release/gzdoom.exe'
if not exe.exists():raise RuntimeError('Custom GZDoom build missing')
shutil.copy2(exe,runtime/'gzdoom.exe')
for p in (LOCAL/'gzdoom-build').glob('*.pk3'):shutil.copy2(p,runtime/p.name)
with zipfile.ZipFile(runtime/'doomcraft.pk3','w',zipfile.ZIP_DEFLATED) as pack:
    for name in ('DECORATE','MENUDEF'):pack.write(ROOT/'native'/name,name)
for game in ('Doom','DoomII'):(LOCAL/f'saves-{game}').mkdir(exist_ok=True)
hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in runtime.iterdir() if p.is_file()}
(LOCAL/'artifact-hashes.json').write_text(json.dumps(hashes,indent=2)+'\n',encoding='utf8')
print('Portable DoomCraft engine installed; Steam WADs are referenced in place.')
