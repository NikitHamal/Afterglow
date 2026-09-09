// Afterglow fast verify: drop-in replacement for verify.mjs, tuned for
// iteration speed. Same CLI (--go/--shots/--diag/--diagfile/--w/--h).
// Speed wins: warm disk cache (persistent user-data-dir), ?readyframes=90,
// 1.2s settle, 0.5s between batched cam shots (camera snap is instant),
// small default viewport (640x400, ~2.5x fewer SwiftShader pixels).
// Typical single-load run (3 shots + diag): ~15s vs ~60-120s before.
//
// NEW: clears .shots/ at start so only the LATEST run's frames remain
// (pass --keep to preserve previous shots). Chrome path auto-detected
// across Windows / macOS / Linux (override with CHROME_PATH env).
'use strict';
import { spawn, execSync } from 'child_process';
import fs from 'fs';
import path from 'path';
import os from 'os';

const PORT = 9334;
const BASE = 'http://127.0.0.1:5500/3d.html';

// ---- Chrome auto-detect (Windows / macOS / Linux) ----
function findChrome() {
  if (process.env.CHROME_PATH && fs.existsSync(process.env.CHROME_PATH)) return process.env.CHROME_PATH;
  const cands = [
    'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
    '/usr/bin/google-chrome', '/usr/bin/google-chrome-stable',
    '/usr/bin/chromium', '/usr/bin/chromium-browser',
    '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
    '/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge'
  ];
  for (const c of cands) if (fs.existsSync(c)) return c;
  try {
    const p = execSync(
      'command -v google-chrome || command -v google-chrome-stable || command -v chromium || command -v chromium-browser || command -v chrome',
      { stdio: ['ignore', 'pipe', 'ignore'] }).toString().trim();
    if (p) return p;
  } catch (_) {}
  return cands[0];
}
const CHROME = findChrome();

function arg(name, def) {
  const i = process.argv.indexOf('--' + name);
  return i >= 0 && process.argv[i + 1] ? process.argv[i + 1] : def;
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
const sleep = ms => new Promise(r => setTimeout(r, ms));
async function evaluate(ws, expr) {
  const r = await send(ws, 'Runtime.evaluate', {
    expression: expr, returnByValue: true, awaitPromise: true
  });
  if (r.exceptionDetails) {
    const d = r.exceptionDetails;
    const desc = (d.exception && d.exception.description) || d.text || JSON.stringify(d);
    throw new Error('page JS: ' + desc.split('\n')[0]);
  }
  return r.result && r.result.value;
}

(async () => {
  const go = arg('go', 'char=goatchan&play=1&solo=1&clean=1&readyframes=90');
  const shotSpecs = arg('shots', 'idle:-:600:idle.png').split(',');
  const diag = arg('diag', '');
  const prejs = arg('prejs', '');
  const width = parseInt(arg('w', '640'), 10), height = parseInt(arg('h', '400'), 10);
  const keep = process.argv.includes('--keep');
  const t0 = Date.now();

  // Clear previous shots so only the LATEST run remains (unless --keep).
  if (!keep) {
    try { fs.rmSync('.shots', { recursive: true, force: true }); } catch (_) {}
    fs.mkdirSync('.shots', { recursive: true });
    console.log('cleared .shots/ (latest run only)');
  }

  const cacheDir = (process.env.TEMP ? process.env.TEMP + '\\ag-verify-fast'
    : path.join(os.tmpdir(), 'ag-verify-fast'));
  const chrome = spawn(CHROME, [
    '--headless=new', '--no-sandbox', '--hide-scrollbars',
    '--enable-unsafe-swiftshader', '--use-angle=swiftshader',
    '--disable-lcd-text', '--disable-extensions',
    '--window-size=' + width + ',' + height,
    '--remote-debugging-port=' + PORT, '--user-data-dir=' + cacheDir,
    'about:blank'
  ], { stdio: 'ignore' });
  await sleep(1200);

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
    for (let i = 0; i < 120; i++) {
      await sleep(250);
      try {
        const st = await evaluate(ws, '(function(){try{return document.title+"|"+(window.GLB_MODEL&&GLB_MODEL.loaded?1:0)+":"+(window.G&&G.state||"?")}catch(e){return "err"}})()');
        const s = String(st);
        if (s.startsWith('SHOT-READY') && s.split('|')[1].startsWith('1')) { console.log('ready:', s, ((Date.now() - t0) / 1000).toFixed(1) + 's'); break; }
        if (i % 24 === 23) console.log('waiting...', s);
      } catch (e) { console.log('probe:', e.message); }
    }
    await sleep(1200); // damping settle (GLB_DRIVE_RATE 18/s settles <0.5s)
    if (prejs && prejs !== '-') {
      const src = prejs.charAt(0) === '@' ? fs.readFileSync(prejs.slice(1), 'utf8') : prejs;
      await evaluate(ws, src);
      console.log('prejs ran');
      await sleep(900);
    }
    const errs = await evaluate(ws, '(window.__agErrors||[]).slice(-8)');
    console.log('page errors:', JSON.stringify(errs));

    for (const spec of shotSpecs) {
      const parts = spec.split(':');
      let name = parts[0], preJS = parts[1], wait = parseInt(parts[2] || '500', 10), out = parts[3] || (name + '.png');
      if (preJS && preJS.charAt(0) === '@') preJS = fs.readFileSync(preJS.slice(1), 'utf8');
      if (preJS && preJS !== '-') { await evaluate(ws, preJS); }
      await sleep(wait);
      const shot = await send(ws, 'Page.captureScreenshot', { format: 'png' });
      fs.writeFileSync('.shots/' + out, Buffer.from(shot.data, 'base64'));
      console.log('saved .shots/' + out, ((Date.now() - t0) / 1000).toFixed(1) + 's');
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
    console.log('TOTAL', ((Date.now() - t0) / 1000).toFixed(1) + 's');
  } finally {
    try { ws && ws.close(); } catch { }
    chrome.kill();
  }
})().catch(e => { console.error('VERIFY FAIL:', e.message); process.exit(1); });
