"""Read boss encounter metadata from the user's original Steam IWADs; copy no game lumps."""
from pathlib import Path
import struct,re,json,collections
ROOT=Path(__file__).resolve().parent
BASE=Path('C:/Program Files (x86)/Steam/steamapps/common/Ultimate Doom/base')
classes={16:('Cyberdemon','Cyberdemon'),7:('SpiderMastermind','Spider mastermind'),88:('BossBrain','Icon of Sin')}
rows=[]
for game,wad in [('Doom',BASE/'DOOM.WAD'),('DoomII',BASE/'doom2/DOOM2.WAD')]:
    data=wad.read_bytes();magic,count,directory=struct.unpack_from('<4sii',data)
    assert magic==b'IWAD'
    current=None;indices=collections.Counter()
    for i in range(count):
        offset,size,name=struct.unpack_from('<ii8s',data,directory+i*16);name=name.rstrip(b'\0').decode('ascii')
        if re.fullmatch(r'E[1-4]M[1-9]|MAP\d\d',name):current=name;indices.clear()
        if name!='THINGS' or current is None:continue
        for x,y,angle,kind,flags in struct.iter_unpack('<hhhhh',data[offset:offset+size]):
            if flags&16 or not flags&7:continue
            if kind==3003 and current=='E1M8':cls,label='BaronOfHell','Bruiser brother'
            elif kind in classes:cls,label=classes[kind]
            else:continue
            # Use the first difficulty group in which this exact encounter exists.
            skill=2 if flags&2 else 3 if flags&4 else 1
            # Index counts only bosses of this class present on the selected skill.
            index=indices[(cls,skill)];indices[(cls,skill)]+=1
            rows.append(dict(game=game,map=current,actor=cls,index=index,skill=skill,x=x,y=y,label=f'{current} - {label} {index+1}'+(' (hard)' if skill==3 and not flags&2 else '')))
    # Multiple peers with different flags need their index from the complete skill-filtered list.
    # The shipped boss things use the same skill flags within each encounter; runtime reports unavailable entries explicitly.
(ROOT/'local/bosses.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf8')
lines=['OptionMenu "DoomCraftDoomBosses" {','Title "Doom boss fights"']
for game in ('Doom','DoomII'):
    if game=='DoomII':lines+=['}','OptionMenu "DoomCraftDoomIIBosses" {','Title "Doom II boss fights"']
    lines.append('Command "Refill TNT + firework crossbow", "doomcraft_resupply"')
    for r in rows:
        if r['game']==game:lines.append(f'Command "{r["label"]}", "doomcraft_boss {r["map"]} {r["actor"]} {r["index"]} {r["skill"]}"')
lines+=['}']
(ROOT/'native/MENUDEF').write_text('\n'.join(lines)+'\n',encoding='utf8')
print(f'Created {len(rows)} boss destinations from owned Doom and Doom II files.')
for r in rows:print(r['game'],r['label'])
