// Afterglow 2D capture tool (agent harness).
// Drives .shot/shot.html with one headless Chrome, prints page errors, and
// captures PNGs with optional zoom-crop clips so skin shading can be reviewed
// at pixel scale.
//
// Usage:
//   node scratch/cap2d.mjs --go 'state=play&view=fpv&pleasure=62' --shot cur
//   node scratch/cap2d.mjs --go '...' --shot torso:420,180,440,420,2
//   node scratch/cap2d.mjs --go '...' --eval @scratch/sample.js
//
// Clip spec: name:x,y,w,h[,scale]   (all optional; default = full frame)
'use strict';
import { spawn } from 'child_process';
import fs from 'fs';

const CHROME = 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe';
const PORT = parseInt(arg('port', '9336'), 10);
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
  const go = arg('go', 'state=play&view=fpv&pleasure=62&depth=.55&t=25&char=goatchan');
  const shot = arg('shot', '');
  const width = parseInt(arg('w', '1280'), 10), height = parseInt(arg('h', '720'), 10);

  const chrome = spawn(CHROME, [
    '--headless=new', '--no-sandbox', '--hide-scrollbars',
    '--enable-unsafe-swiftshader', '--use-angle=swiftshader',
    '--disable-lcd-text', '--disable-extensions',
    '--window-size=' + width + ',' + height,
    '--remote-debugging-port=' + PORT, '--user-data-dir=' + process.env.TEMP + '\\ag-cap2d-' + PORT,
    'about:blank'
  ], { stdio: 'ignore' });
  await sleep(2600);

  let ws;
  try {
    const list = await (await fetch('http://127.0.0.1:' + PORT + '/json/list')).json();
    const page = list.find(t => t.type === 'page');
    ws = await connect(page.webSocketDebuggerUrl);
    await send(ws, 'Page.enable', {});
    await send(ws, 'Runtime.enable', {});
    // Never let Chrome serve a stale js/*.js — every capture must be live code.
    await send(ws, 'Network.enable', {});
    await send(ws, 'Network.setCacheDisabled', { cacheDisabled: true });
    await send(ws, 'Emulation.setDeviceMetricsOverride', { width, height, deviceScaleFactor: 1, mobile: false });
    // The python dev server occasionally drops a script under concurrent load,
    // which leaves the page half-initialised (G/skArc/draw undefined). Retry the
    // whole load until it comes up clean and has actually painted pixels.
    let errs = [], painted = false;
    for (let attempt = 1; attempt <= 5; attempt++) {
      await send(ws, 'Page.navigate', { url: BASE + '?' + go + '&cb=' + Date.now() });
      painted = false;
      for (let i = 0; i < 24; i++) {
        await sleep(500);
        try {
          painted = await evaluate(ws, `(function(){
            try{
              if (typeof X === 'undefined' || typeof W === 'undefined') return false;
              const d = X.getImageData(0, 0, W, H).data;
              let s = 0;
              for (let k = 0; k < d.length; k += 4004) s += d[k] + d[k+1] + d[k+2];
              return s > 6000;
            }catch(e){ return false; }
          })()`);
        } catch { painted = false; }
        if (painted) break;
      }
      errs = await evaluate(ws, '(window.__agErrors||[]).concat(window.__agFatal||[])');
      if (painted && errs.length === 0) break;
      console.log('attempt ' + attempt + ' dirty (painted=' + painted + ', errors=' + errs.length + '), reloading');
    }
    console.log('painted:', painted);
    console.log('page errors:', JSON.stringify(errs.slice(0, 3)) + (errs.length > 3 ? ' (+' + (errs.length - 3) + ')' : ''));

    // wait for the deterministic frame driver to finish
    const ready = await evaluate(ws, '(function(){ return !!(window.__shot&&window.__shot.ready); })()');
    console.log('ready:', ready);

    const evPre = arg('eval', '');
    if (evPre) {
      const v = await evaluate(ws, loadFile(evPre));
      console.log('EVAL>>>', typeof v === 'string' ? v : JSON.stringify(v, null, 1));
    }

    if (shot) {
      const parts = shot.split(':');
      const name = parts[0];
      let clip;
      if (parts[1]) {
        const n = parts[1].split(',').map(Number);
        clip = { x: n[0], y: n[1], width: n[2], height: n[3], scale: n[4] || 1 };
      }
      const p = { format: arg('fmt', 'png') };
      if (p.format === 'jpeg') p.quality = parseInt(arg('q', '82'), 10);
      if (clip) p.clip = clip;
      const res = await send(ws, 'Page.captureScreenshot', p);
      const ext = p.format === 'jpeg' ? '.jpg' : '.png';
      fs.writeFileSync('.shots/' + name + ext, Buffer.from(res.data, 'base64'));
      console.log('saved .shots/' + name + ext);
    }
  } finally {
    try { ws && ws.close(); } catch {}
    chrome.kill();
  }
})().catch(e => { console.error('CAP2D FAIL:', e.message); process.exit(1); });
