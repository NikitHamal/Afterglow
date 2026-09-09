// Afterglow headless verify: dependency-free CDP client (Node 22+ global
// WebSocket/fetch). Launches headless Chrome, loads a 3d.html URL, waits for
// the GLB + damping to settle, optionally runs page JS, captures PNGs and
// evaluates diagnostics. Usage:
//   node scratch/verify.mjs --go "pose=0&char=goatchan&play=1&solo=1&clean=1"
//     --shots "idle:0:top.png:4000,rub:G.spaceHeld=true;G.rub=1:6000:rub.png"
// Shot spec: name:preJS:preWaitMs:out  (preJS '-' = none)
'use strict';
import { spawn } from 'child_process';
import fs from 'fs';

const CHROME = 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe';
const PORT = 9333;
const BASE = 'http://127.0.0.1:5500/3d.html';

function arg(name, def) {
  const i = process.argv.indexOf('--' + name);
  return i >= 0 && process.argv[i + 1] ? process.argv[i + 1] : def;
}

let msgId = 0;
const pending = new Map();
const consoleLog = [];
function send(ws, method, params) {
  return new Promise((res, rej) => {
    const id = ++msgId;
    pending.set(id, { res, rej });
    ws.send(JSON.stringify({ id, method, params }));
  });
}
function connect(url) {
  return new Promise((res, rej) => {
    const ws = new WebSocket(url);
    ws.onopen = () => res(ws);
    ws.onerror = rej;
    ws.onmessage = ev => {
      let m; try { m = JSON.parse(ev.data); } catch { return; }
      if (m.id && pending.has(m.id)) {
        const p = pending.get(m.id); pending.delete(m.id);
        if (m.error) p.rej(new Error(m.error.message));
        else p.res(m.result);
      }
    };
  });
}
const sleep = ms => new Promise(r => setTimeout(r, ms));
async function evaluate(ws, expr, awaitPromise) {
  const r = await send(ws, 'Runtime.evaluate', {
    expression: expr, returnByValue: true, awaitPromise: awaitPromise !== false
  });
  if (r.exceptionDetails) {
    const d = r.exceptionDetails;
    const desc = (d.exception && d.exception.description) || d.text || JSON.stringify(d);
    throw new Error('page JS: ' + desc.split('\n')[0]);
  }
  return r.result && r.result.value;
}

(async () => {
  const go = arg('go', 'char=goatchan&play=1&solo=1&clean=1');
  const shotSpecs = arg('shots', 'idle:-:9000:idle.png').split(',');
  const diag = arg('diag', '');
  const width = parseInt(arg('w', '960'), 10), height = parseInt(arg('h', '720'), 10);

  const chrome = spawn(CHROME, [
    '--headless=new', '--no-sandbox', '--hide-scrollbars',
    '--enable-unsafe-swiftshader', '--use-angle=swiftshader',
    '--window-size=' + width + ',' + height,
    '--remote-debugging-port=' + PORT, '--user-data-dir=' + process.env.TEMP + '\\ag-verify',
    'about:blank'
  ], { stdio: 'ignore' });
  await sleep(2500);

  let ws;
  try {
    const list = await (await fetch('http://127.0.0.1:' + PORT + '/json/list')).json();
    const page = list.find(t => t.type === 'page');
    ws = await connect(page.webSocketDebuggerUrl);
    await send(ws, 'Page.enable', {});
    await send(ws, 'Runtime.enable', {});
    const url = BASE + '?' + go;
    console.log('loading', url);
    await send(ws, 'Page.navigate', { url });
    // wait for BOTH the page's READY flag and the GLB girl (headless
    // SwiftShader renders slowly — the 20MB model can land after frame 160)
    for (let i = 0; i < 240; i++) {
      await sleep(500);
      try {
        const st = await evaluate(ws, '(function(){try{return document.title+"|"+(window.GLB_MODEL&&GLB_MODEL.loaded?1:0)+":"+(window.G&&G.state||"?")}catch(e){return "err"}})()');
        const s = String(st);
        if (s.startsWith('SHOT-READY') && s.split('|')[1].startsWith('1')) { console.log('ready:', s); break; }
        if (i % 20 === 19) console.log('waiting...', s);
      } catch (e) { console.log('probe:', e.message); }
    }
    await sleep(2500); // damping settle
    const errs = await evaluate(ws, '(window.__agErrors||[]).slice(-8)');
    console.log('page errors:', JSON.stringify(errs));

    for (const spec of shotSpecs) {
      const parts = spec.split(':');
      let name = parts[0], preJS = parts[1], wait = parseInt(parts[2] || '3000', 10), out = parts[3] || (name + '.png');
      if (preJS && preJS.charAt(0) === '@') preJS = fs.readFileSync(preJS.slice(1), 'utf8');
      if (preJS && preJS !== '-') { await evaluate(ws, preJS); console.log(name, 'ran:', preJS); }
      await sleep(wait);
      const shot = await send(ws, 'Page.captureScreenshot', { format: 'png' });
      fs.writeFileSync('.shots/' + out, Buffer.from(shot.data, 'base64'));
      console.log('saved .shots/' + out);
    }
    let diagSrc = diag;
    const diagFile = arg('diagfile', '');
    if (diagFile) diagSrc = fs.readFileSync(diagFile, 'utf8');
    if (diagSrc) {
      const val = await evaluate(ws, diagSrc);
      const s = typeof val === 'string' ? val : JSON.stringify(val, null, 1);
      console.log('DIAG>>>'); console.log(s); console.log('<<<DIAG');
      fs.writeFileSync('.shots/diag.json', s);
    }
  } finally {
    try { ws && ws.close(); } catch {}
    chrome.kill();
  }
})().catch(e => { console.error('VERIFY FAIL:', e.message); process.exit(1); });
