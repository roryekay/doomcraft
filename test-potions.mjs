import fs from 'node:fs';
const s=new WebSocket('ws://127.0.0.1:25601'),events=[],results=[];
s.addEventListener('message',e=>{try{events.push({...JSON.parse(String(e.data)),at:Date.now()});}catch{}});
await new Promise((r,j)=>{s.addEventListener('open',r,{once:true});s.addEventListener('error',j,{once:true});});
const send=m=>s.send(JSON.stringify(m)),wait=ms=>new Promise(r=>setTimeout(r,ms)),cmd=c=>send({t:'cmd',c});
try {
 send({t:'director',op:'freeze',on:true});cmd('gamemode creative @a');send({t:'slot',n:1});
 for(const kind of ['splash_potion','lingering_potion']) {
  cmd(`item replace entity @p hotbar.1 with minecraft:${kind}[potion_contents={potion:"minecraft:water"}]`);
  send({t:'director',op:'view',yaw:270,pitch:0});await wait(600);const logBefore=fs.readFileSync('local/doomcraft.log','utf8').length;const start=Date.now();
  send({t:'key',k:'use',down:true});await wait(120);send({t:'key',k:'use',down:false});await wait(2000);
  const samples=events.filter(e=>e.at>=start&&e.t==='proj').flatMap(e=>e.p.filter(p=>p[1].startsWith('potion:')));
  const id=samples[0]?.[0],hit=events.find(e=>e.at>=start&&e.t==='projhit'&&e.id===id);
  results.push({kind,collision:'Doom wall',id,hit,maxZ:Math.max(...samples.map(p=>p[4]))});
  if(!fs.readFileSync('local/doomcraft.log','utf8').slice(logBefore).includes('potion:water hit world')||samples.some(p=>p[4]>115.4))throw Error(`${kind} missed native wall`);
 }
 // Known empty air in the spawn room; remove exactly this temporary block afterward.
 cmd('setblock 33 65 114 minecraft:glass');cmd('item replace entity @p hotbar.1 with minecraft:splash_potion[potion_contents={potion:"minecraft:water"}]');await wait(500);const logBefore=fs.readFileSync('local/doomcraft.log','utf8').length;const start=Date.now();
 send({t:'key',k:'use',down:true});await wait(120);send({t:'key',k:'use',down:false});await wait(1800);
 const samples=events.filter(e=>e.at>=start&&e.t==='proj').flatMap(e=>e.p.filter(p=>p[1].startsWith('potion:')));
 results.push({kind:'splash_potion',collision:'Minecraft glass',samples:samples.length,maxZ:Math.max(...samples.map(p=>p[4]))});
 if(samples.some(p=>p[4]>114.2))throw Error('Potion passed through Minecraft glass');
} finally {
 send({t:'key',k:'use',down:false});cmd('setblock 33 65 114 minecraft:air');cmd('item replace entity @p hotbar.1 with minecraft:air');cmd('gamemode survival @a');send({t:'slot',n:3});send({t:'director',op:'view',yaw:90,pitch:0});send({t:'director',op:'freeze',on:false});await wait(500);s.close();
 fs.writeFileSync('local/potion-test.json',JSON.stringify({results,events},null,2));console.log(JSON.stringify(results,null,2));
}

