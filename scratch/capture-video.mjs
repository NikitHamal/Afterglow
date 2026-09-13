// Afterglow: capture a real-time video of the 3D scene.
// Uses CDP Page.startScreencast (streams frames) instead of one
// Page.captureScreenshot per frame — ~10x less round-trip overhead, and the
// frames arrive already paced to the page's rAF loop so motion is smooth.
//
// Usage:
//   node scratch/capture-video.mjs --secs 10 --fps 30 --out out/solo10.mp4
//        [--go "char=goatchan&play=1&solo=1&clean=1"]
//        [--prejs scratch/rub-zone3.js] [--cam scratch/cam-mons.js]
//        [--w 960] [--h 600]
'use strict';
import { spawn } from 'child_process';
import fs from 'fs';
import path from 'path';

const CHROME = 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe';
const FFMPEG = 'F:\\Programs\\namida\\bin\\ffmpeg.exe';
const PORT = 9335;
const BASE = 'http://127.0.0.1:5500/3d.html';

function arg(name, def) {
  const i = process.argv.indexOf('--' + name);
  return i >= 0 && process.argv[i + 1] ? process.argv[i + 1] : def;
}

const secs = parseFloat(arg('secs', '10'));
const fps = parseInt(arg('fps', '30'), 10);
const outFile = arg('out', '.shots/solo10.mp4');
const go = arg('go', 'char=goatchan&play=1&solo=1&clean=1&readyframes=90');
const prejsFile = arg('prejs', '');
const camFile = arg('cam', '');
const width = parseInt(arg('w', '960'), 10);
const height = parseInt(arg('h', '600'), 10);

const TMPDIR = path.join(process.cwd(), '.shots', '_frames');
fs.rmSync(TMPDIR, { recursive: true, force: true });
fs.mkdirSync(TMPDIR, { recursive: true });

let msgId = 0;
const pending = new Map();
const sleep = ms => new Promise(r => setTimeout(r, ms));

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
        if (m.error) p.rej(new Error(m.error.message)); else p.res(m.result);
      }
      // Screencast frames arrive as events, not responses.
      if (m.method === 'Page.screencastFrame' && frameSink) frameSink(m.params);
    };
  });
}

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

let frameSink = null;

(async () => {
  const t0 = Date.now();
  const chrome = spawn(CHROME, [
    '--headless=new', '--no-sandbox', '--hide-scrollbars',
    '--enable-unsafe-swiftshader', '--use-angle=swiftshader',
    '--disable-lcd-text', '--disable-extensions',
    '--window-size=' + width + ',' + height,
    '--remote-debugging-port=' + PORT,
    '--user-data-dir=' + process.env.TEMP + '\\ag-video',
    'about:blank'
  ], { stdio: 'ignore' });
  await sleep(1800);

  let ws;
  try {
    const list = await (await fetch('http://127.0.0.1:' + PORT + '/json/list')).json();
    const page = list.find(t => t.type === 'page');
    ws = await connect(page.webSocketDebuggerUrl);
    await send(ws, 'Page.enable', {});
    await send(ws, 'Runtime.enable', {});

    const url = BASE + '?' + go;
    console.log('[cap] loading', url);
    await send(ws, 'Page.navigate', { url });

    for (let i = 0; i < 160; i++) {
      await sleep(250);
      try {
        const st = await evaluate(ws,
          '(function(){try{return document.title+"|"+(window.GLB_MODEL&&GLB_MODEL.loaded?1:0)}catch(e){return "err"}})()');
        const s = String(st);
        if (s.startsWith('SHOT-READY') && s.split('|')[1].startsWith('1')) {
          console.log('[cap] ready', ((Date.now() - t0) / 1000).toFixed(1) + 's');
          break;
        }
      } catch (e) { /* keep polling */ }
    }
    await sleep(1200); // damping settle

    // Camera preset
    if (camFile) {
      await evaluate(ws, fs.readFileSync(camFile, 'utf8'));
      console.log('[cap] camera set');
      await sleep(600);
    }
    // Pre-JS (e.g. start the rub)
    if (prejsFile) {
      await evaluate(ws, fs.readFileSync(prejsFile, 'utf8'));
      console.log('[cap] prejs ran');
      await sleep(800);
    }

    const errs = await evaluate(ws, '(window.__agErrors||[]).slice(-5)');
    console.log('[cap] page errors:', JSON.stringify(errs));

    // --- Screencast capture ---
    const targetFrames = Math.round(secs * fps);
    const collected = [];
    let ackPending = 0;

    frameSink = async (params) => {
      collected.push({
        data: params.data,
        sessionId: params.sessionId,
        ts: params.metadata && params.metadata.timestamp
      });
      // Must ack promptly or Chrome throttles the stream.
      try { await send(ws, 'Page.screencastFrameAck', { sessionId: params.sessionId }); } catch {}
    };

    await send(ws, 'Page.startScreencast', {
      format: 'jpeg', quality: 85,
      maxWidth: width, maxHeight: height, everyNthFrame: 1
    });
    console.log('[cap] screencast started, target', targetFrames, 'frames /', secs + 's');

    const deadline = Date.now() + secs * 1000;
    while (Date.now() < deadline) {
      await sleep(200);
      if (collected.length % 30 === 0 && collected.length) {
        process.stdout.write('\r[cap] frames: ' + collected.length);
      }
    }
    await send(ws, 'Page.stopScreencast', {});
    await sleep(300);
    console.log('\n[cap] captured', collected.length, 'raw frames');

    // Trim/pad to exactly the frame count we want.
    let frames = collected;
    if (frames.length > targetFrames) {
      // Evenly sample down to targetFrames so duration stays exact.
      const step = frames.length / targetFrames;
      frames = Array.from({ length: targetFrames }, (_, i) => frames[Math.floor(i * step)]);
    }
    console.log('[cap] writing', frames.length, 'frames @', fps, 'fps');

    for (let i = 0; i < frames.length; i++) {
      const name = 'f_' + String(i).padStart(5, '0') + '.jpg';
      fs.writeFileSync(path.join(TMPDIR, name), Buffer.from(frames[i].data, 'base64'));
    }

    // --- Encode ---
    // SwiftShader rasterises the scene slowly, so the number of frames we
    // actually get in `secs` is usually well below the target. Encode at the
    // REAL captured rate so playback duration stays exactly `secs` — motion is
    // a bit steppy but the timing/content is right. Pad the last frame if we
    // came up short so the clip never ends early.
    const outAbs = path.resolve(outFile);
    fs.mkdirSync(path.dirname(outAbs), { recursive: true });
    const realFps = Math.max(1, frames.length / secs);
    console.log('[cap] encoding ->', outAbs, 'at', realFps.toFixed(1), 'fps');

    // This ffmpeg build (namida, --enable-small) has no libx264: only the
    // MediaFoundation hw encoder. Try h264_mf, fall back to mpeg4.
    async function runFfmpeg(codec, extra) {
      const ff = spawn(FFMPEG, [
        '-y', '-framerate', String(realFps),
        '-i', path.join(TMPDIR, 'f_%05d.jpg'),
        '-c:v', codec, ...extra,
        '-pix_fmt', 'yuv420p', '-movflags', '+faststart',
        '-vf', 'scale=trunc(iw/2)*2:trunc(ih/2)*2',
        outAbs
      ], { stdio: ['ignore', 'pipe', 'pipe'] });
      let err = '';
      ff.stderr.on('data', d => { err += d.toString(); });
      return new Promise((res, rej) => {
        ff.on('close', c => c === 0 ? res() : rej(new Error(codec + ' exit ' + c + '\n' + err.slice(-600))));
        ff.on('error', rej);
      });
    }
    try {
      await runFfmpeg('h264_mf', ['-b:v', '6M']);
    } catch (e) {
      console.log('[cap] h264_mf failed, trying mpeg4:', e.message.split('\n')[0]);
      await runFfmpeg('mpeg4', ['-q:v', '3']);
    }

    const sz = fs.statSync(outAbs).size;
    console.log('[cap] DONE', outAbs, (sz / 1048576).toFixed(1) + 'MB',
      ((Date.now() - t0) / 1000).toFixed(1) + 's');
  } finally {
    try { ws && ws.close(); } catch {}
    chrome.kill();
  }
})().catch(e => {
  console.error('CAPTURE FAIL:', e.message);
  process.exit(1);
});
