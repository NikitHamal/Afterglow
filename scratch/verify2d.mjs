// Afterglow 2D verify: fast screenshot + perf harness for the canvas game.
// Drives .shot/shot.html (HUD hidden, state injected via query) with one
// headless Chrome, captures multiple named shots, and measures render cost.
//
// Usage:
//   node scratch/verify2d.mjs --shots 'side:@scratch/c2d-side.js,face:@scratch/c2d-face.js'
//   node scratch/verify2d.mjs --go 'view=fpv&focus=face&pleasure=62&depth=.55' --perf
//   node scratch/verify2d.mjs --eval @scratch/diag2d.js
//
// Shot spec: name:camfile:wait  (camfile optional, @file or literal JS)
// Flag --perf runs a frame-time probe (draw() N times) and prints stats.
'use strict';
import { spawn } from 'child_process';
import fs from 'fs';

const CHROME = 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe';
const PORT = 9336;
const BASE = 'http://127.0.0.1:5500/.shot/shot.html';
const sleep = ms => new Promise(r => setTimeout(r, ms));

function arg(name, def) {
  const i = process.argv.indexOf('--' + name);
  return i >= 0 && process.argv[i + 1] ? process.argv[i + 1] : def;
}
function loadFile(spec) {
  if (!spec || spec === '-') return '';
  return spec.charAt(0) === '@' ? fs.readFileSync(spec.slice(1), 'utf8') : spec;
}

let msgId = 0;
const pending = new Map();
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
async function evaluate(ws, expr) {
  const r = await send(ws, 'Runtime.evaluate', { expression: expr, returnByValue: true, awaitPromise: true });
  if (r.exceptionDetails) {
    const d = r.exceptionDetails;
    throw new Error('page JS: ' + ((d.exception && d.exception.description) || d.text || 'err').split('\n')[0]);
  }
  return r.result && r.result.value;
}

(async () => {
  const go = arg('go', 'state=play&pleasure=62&depth=.55&t=25&view=side');
  const shotSpecs = arg('shots', '').split(',').filter(Boolean);
  const width = parseInt(arg('w', '1280'), 10), height = parseInt(arg('h', '720'), 10);
  const t0 = Date.now();

  const chrome = spawn(CHROME, [
    '--headless=new', '--no-sandbox', '--hide-scrollbars',
    '--enable-unsafe-swiftshader', '--use-angle=swiftshader',
    '--disable-lcd-text', '--disable-extensions',
    '--window-size=' + width + ',' + height,
    '--remote-debugging-port=' + PORT, '--user-data-dir=' + process.env.TEMP + '\\ag-verify2d',
    'about:blank'
  ], { stdio: 'ignore' });
  await sleep(1600);

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
    await sleep(1500);
    const errs = await evaluate(ws, '(window.__agErrors||[])');
    console.log('page errors:', JSON.stringify(errs));

    for (const spec of shotSpecs) {
      const parts = spec.split(':');
      const name = parts[0], cam = loadFile(parts[1]), wait = parseInt(parts[2] || '500', 10);
      if (cam) await evaluate(ws, cam);
      await sleep(wait);
      const shot = await send(ws, 'Page.captureScreenshot', { format: 'png' });
      fs.writeFileSync('.shots/' + name + '.png', Buffer.from(shot.data, 'base64'));
      console.log('saved .shots/' + name + '.png', ((Date.now() - t0) / 1000).toFixed(1) + 's');
    }

    const perf = process.argv.includes('--perf');
    if (perf) {
      const res = await evaluate(ws, `(function(){
        var N=200, times=[];
        for(var i=0;i<N;i++){ var a=performance.now(); draw(); times.push(performance.now()-a); }
        times.sort(function(x,y){return x-y;});
        var s=0; for(var i=0;i<N;i++) s+=times[i];
        return {avg:+(s/N).toFixed(3), p50:+times[(N/2)|0].toFixed(3), p95:+times[(N*0.95)|0].toFixed(3), max:+times[N-1].toFixed(3)};
      })()`);
      console.log('PERF draw() ms:', JSON.stringify(res));
    }

    const ev = arg('eval', '');
    if (ev) {
      const v = await evaluate(ws, loadFile(ev));
      console.log('EVAL>>>', typeof v === 'string' ? v : JSON.stringify(v, null, 1));
    }
    console.log('TOTAL', ((Date.now() - t0) / 1000).toFixed(1) + 's');
  } finally {
    try { ws && ws.close(); } catch {}
    chrome.kill();
  }
})().catch(e => { console.error('VERIFY2D FAIL:', e.message); process.exit(1); });
