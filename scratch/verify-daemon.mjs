// Afterglow verify daemon: one warm headless Chrome + one loaded page kept
// alive across runs. Shots then cost ~1-3s each instead of ~35s reloads.
// Endpoints (localhost:9340):
//   GET  /ping            -> {ok, ready, url}
//   POST /go   {q}        -> navigate to 3d.html?<q>, wait SHOT-READY+settle
//   POST /eval {src}      -> Runtime.evaluate, returns {value}
//   POST /shot {name}     -> captureScreenshot -> .shots/<name>
'use strict';
import { spawn } from 'child_process';
import fs from 'fs';
import http from 'http';

const CHROME = 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe';
const PORT = 9335;
const HTTP_PORT = 9340;
const BASE = 'http://127.0.0.1:5500/3d.html';
const W = 640, H = 400;

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
  const r = await send(ws, 'Runtime.evaluate', { expression: expr, returnByValue: true, awaitPromise: true });
  if (r.exceptionDetails) {
    const d = r.exceptionDetails;
    throw new Error('page JS: ' + ((d.exception && d.exception.description) || d.text || 'err').split('\n')[0]);
  }
  return r.result && r.result.value;
}

let ws = null, ready = false, curUrl = '';
async function ensurePage() {
  if (ws) return ws;
  const list = await (await fetch('http://127.0.0.1:' + PORT + '/json/list')).json();
  const page = list.find(t => t.type === 'page');
  ws = await connect(page.webSocketDebuggerUrl);
  try { ws.onclose = () => { ws = null; ready = false; }; } catch {}
  await send(ws, 'Page.enable', {});
  await send(ws, 'Runtime.enable', {});
  return ws;
}
async function waitReady(ws, settleMs) {
  for (let i = 0; i < 160; i++) {
    await sleep(250);
    try {
      const st = String(await evaluate(ws, '(function(){try{return document.title+"|"+(window.GLB_MODEL&&GLB_MODEL.loaded?1:0)+":"+(window.G&&G.state||"?")}catch(e){return "err"}})()'));
      if (st.startsWith('SHOT-READY') && st.split('|')[1].startsWith('1')) { ready = true; break; }
    } catch {}
  }
  await sleep(settleMs || 1200);
}

async function boot() {
  spawn(CHROME, [
    '--headless=new', '--no-sandbox', '--hide-scrollbars',
    '--enable-unsafe-swiftshader', '--use-angle=swiftshader',
    '--disable-lcd-text', '--disable-extensions',
    '--window-size=' + W + ',' + H,
    '--remote-debugging-port=' + PORT, '--user-data-dir=' + process.env.TEMP + '\\ag-daemon',
    'about:blank'
  ], { stdio: 'ignore', detached: true });
  await sleep(1800);
  const w = await ensurePage();
  curUrl = BASE + '?char=goatchan&play=1&solo=1&clean=1&readyframes=90';
  console.log('daemon loading', curUrl);
  await send(w, 'Page.navigate', { url: curUrl });
  await waitReady(w, 1200);
  console.log('daemon READY');
}

function body(req) {
  return new Promise(res => {
    let s = '';
    req.on('data', c => { s += c; });
    req.on('end', () => { try { res(JSON.parse(s || '{}')); } catch { res({}); } });
  });
}

const server = http.createServer(async (req, res) => {
  res.setHeader('Content-Type', 'application/json');
  try {
    if (req.method === 'GET' && req.url === '/ping') {
      res.end(JSON.stringify({ ok: true, ready, url: curUrl }));
    } else if (req.method === 'POST' && req.url === '/go') {
      const b = await body(req);
      const w = await ensurePage();
      ready = false;
      curUrl = BASE + '?' + (b.q || 'char=goatchan&play=1&solo=1&clean=1&readyframes=90');
      await send(w, 'Page.navigate', { url: curUrl });
      await waitReady(w, b.settle || 1200);
      const errs = await evaluate(w, '(window.__agErrors||[]).slice(-8)');
      res.end(JSON.stringify({ ok: true, url: curUrl, errors: errs }));
    } else if (req.method === 'POST' && req.url === '/eval') {
      const b = await body(req);
      const w = await ensurePage();
      const v = await evaluate(w, b.src || '1');
      res.end(JSON.stringify({ ok: true, value: typeof v === 'string' ? v.slice(0, 4000) : v }));
    } else if (req.method === 'POST' && req.url === '/shot') {
      const b = await body(req);
      const w = await ensurePage();
      if (b.cam) await evaluate(w, b.cam);
      await sleep(b.wait || 600);
      const shot = await send(w, 'Page.captureScreenshot', { format: 'png' });
      fs.writeFileSync('.shots/' + b.name, Buffer.from(shot.data, 'base64'));
      res.end(JSON.stringify({ ok: true, file: '.shots/' + b.name }));
    } else { res.statusCode = 404; res.end('{"ok":false}'); }
  } catch (e) { res.statusCode = 500; res.end(JSON.stringify({ ok: false, error: e.message })); }
});

server.listen(HTTP_PORT, '127.0.0.1', () => { console.log('daemon http on ' + HTTP_PORT); boot(); });
