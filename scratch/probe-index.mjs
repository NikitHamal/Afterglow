// Minimal index.html boot probe: console/pageerror + pixel sample.
// Usage: node scratch/probe-index.mjs "view=side&pleasure=62" out.png
import { spawn } from 'child_process';
import fs from 'fs';

const CHROME = 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe';
const PORT = 9388;
const qs = process.argv[2] || 'view=side';
const out = process.argv[3] || '.shots/probe-index.png';
const sleep = ms => new Promise(r => setTimeout(r, ms));

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
      if (m.method === 'Runtime.consoleAPICalled') {
        const a = m.params.args.map(x => x.value != null ? x.value : (x.description || '')).join(' ');
        console.log('[console.' + m.params.type + ']', a.slice(0, 300));
      } else if (m.method === 'Runtime.exceptionThrown') {
        const d = m.params.exceptionDetails;
        console.log('[pageerror]', ((d.exception && d.exception.description) || d.text || '').split('\n')[0]);
      } else if (m.method === 'Log.entryAdded') {
        console.log('[log.' + m.params.entry.level + ']', (m.params.entry.text || '').slice(0, 300));
      }
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
  const chrome = spawn(CHROME, [
    '--headless=new', '--no-sandbox', '--hide-scrollbars',
    '--enable-unsafe-swiftshader', '--use-angle=swiftshader',
    '--disable-lcd-text', '--disable-extensions',
    '--window-size=1280,760',
    '--remote-debugging-port=' + PORT, '--user-data-dir=' + process.env.TEMP + '\\ag-probe',
    'about:blank'
  ], { stdio: 'ignore' });
  await sleep(1600);
  let ws;
  try {
    const list = await (await fetch('http://127.0.0.1:' + PORT + '/json/list')).json();
    const page = list.find(t => t.type === 'page');
    ws = await connect(page.webSocketDebuggerUrl);
    await send(ws, 'Runtime.enable', {});
    await send(ws, 'Log.enable', {});
    await send(ws, 'Page.enable', {});
    await send(ws, 'Emulation.setDeviceMetricsOverride', { width: 1280, height: 760, deviceScaleFactor: 1, mobile: false });
    const url = 'http://127.0.0.1:5500/index.html?play=1&char=goatchan&' + qs;
    console.log('loading', url);
    await send(ws, 'Page.navigate', { url });
    await sleep(3000);
    const info = await evaluate(ws, `(()=>{
      const cv=document.getElementById('scene'); if(!cv) return {err:'no #scene'};
      const c=cv.getContext('2d');
      const px=(x,y)=>{const d=c.getImageData(x,y,1,1).data;return [d[0],d[1],d[2]];};
      let errs=[]; try{errs=(window.__agErrors||[]);}catch(e){}
      return { cvW:cv.width, cvH:cv.height, state:G.state, view:G.view,
        center:px(640,360), mid:px(400,500), title:document.title, errs };
    })()`);
    console.log('INFO>>>', JSON.stringify(info));
    const shot = await send(ws, 'Page.captureScreenshot', { format: 'png' });
    fs.writeFileSync(out, Buffer.from(shot.data, 'base64'));
    console.log('saved', out);
  } finally {
    try { ws && ws.close(); } catch {}
    chrome.kill();
  }
})().catch(e => { console.error('PROBE FAIL:', e.message); process.exit(1); });
