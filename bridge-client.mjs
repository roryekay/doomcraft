// Local diagnostics using Node's built-in WebSocket; no third-party packages.
import fs from 'node:fs';
const request = process.argv[2] ? JSON.parse(fs.readFileSync(process.argv[2], 'utf8')) : null;
const socket = new WebSocket('ws://127.0.0.1:25601');
const stop = setTimeout(() => { socket.close(); }, 3500);
const latest = new Map();
socket.addEventListener('open', async () => {
  if (request) for (const m of Array.isArray(request) ? request : [request]) {
    const {delayMs=0,...wire}=m;
    if(delayMs)await new Promise(resolve=>setTimeout(resolve,Math.min(1500,delayMs)));
    socket.send(JSON.stringify(wire));
  }
});
socket.addEventListener('message', event => {
  try { const m=JSON.parse(String(event.data)); latest.set(m.t,m); } catch {}
});
socket.addEventListener('error', () => { console.error('Cannot connect to the Doom Minecraft guest.'); process.exitCode=1; });
socket.addEventListener('close', () => {
  clearTimeout(stop);
  for(const type of ['hello','gtastate','gtainfo','blocks','melee','screen']) if(latest.has(type)) console.log(JSON.stringify(latest.get(type)));
});
