// dev-up.mjs — start (or restart) the two long-lived services the fast verify
// loop needs, detached so they survive the shell that launched them.
//
//   1. static file server  http://127.0.0.1:5500   (scratch/serve.py, threaded)
//   2. warm-Chrome verifier http://127.0.0.1:9340  (scratch/verify-daemon.mjs)
//
// Usage: node scratch/dev-up.mjs [--restart] [--port 5500]
'use strict';
import { spawn, execSync } from 'child_process';
import fs from 'fs';

const PY = 'C:\\Python314\\python.exe';
const NODE = process.execPath;
const PORT = (() => {
  const i = process.argv.indexOf('--port');
  return i >= 0 ? process.argv[i + 1] : '5500';
})();
const RESTART = process.argv.includes('--restart');
const ROOT = process.cwd();

const sleep = ms => new Promise(r => setTimeout(r, ms));

function killStale() {
  // python holding our port, and any previous verify daemon
  try {
    const out = execSync('netstat -ano', { encoding: 'utf8' });
    const pids = new Set();
    for (const line of out.split('\n')) {
      if (!/LISTENING/.test(line)) continue;
      if (line.includes(':' + PORT) || line.includes(':9340') || line.includes(':9335')) {
        const pid = line.trim().split(/\s+/).pop();
        if (pid && pid !== '0') pids.add(pid);
      }
    }
    for (const pid of pids) {
      try { execSync(`taskkill /F /PID ${pid}`, { stdio: 'ignore' }); } catch {}
    }
    if (pids.size) console.log('killed stale pids:', [...pids].join(','));
  } catch {}
}

async function waitHttp(url, tries, label) {
  for (let i = 0; i < tries; i++) {
    try {
      const r = await fetch(url, { signal: AbortSignal.timeout(3000) });
      if (r.ok) return true;
    } catch {}
    await sleep(500);
  }
  console.log('  (still not up after ' + tries + ' tries: ' + label + ')');
  return false;
}

(async () => {
  if (RESTART) killStale();

  // --- static server
  const srvUp = await (async () => {
    try { const r = await fetch(`http://127.0.0.1:${PORT}/3d.html`, { signal: AbortSignal.timeout(2000) }); return r.ok; } catch { return false; }
  })();
  if (!srvUp) {
    console.log('starting static server on', PORT);
    spawn(PY, ['scratch/serve.py', PORT], { cwd: ROOT, detached: true, stdio: 'ignore', windowsHide: true }).unref();
    await waitHttp(`http://127.0.0.1:${PORT}/3d.html`, 20, 'static server');
  }
  const srvOk = await (async () => {
    try { const r = await fetch(`http://127.0.0.1:${PORT}/3d.html`, { signal: AbortSignal.timeout(3000) }); return r.ok; } catch { return false; }
  })();
  console.log('static server :', srvOk ? 'OK' : 'FAIL', `http://127.0.0.1:${PORT}/3d.html`);

  // --- verify daemon
  let dOk = false;
  try { const r = await fetch('http://127.0.0.1:9340/ping', { signal: AbortSignal.timeout(2000) }); dOk = r.ok; } catch {}
  if (!dOk) {
    console.log('starting verify daemon...');
    spawn(NODE, ['scratch/verify-daemon.mjs'], { cwd: ROOT, detached: true, stdio: 'ignore', windowsHide: true }).unref();
    await waitHttp('http://127.0.0.1:9340/ping', 20, 'daemon http');
  }
  console.log('verify daemon :', 'http://127.0.0.1:9340');

  // daemon becomes "ready" only after its warm page finishes loading
  for (let i = 0; i < 120; i++) {
    try {
      const j = await (await fetch('http://127.0.0.1:9340/ping', { signal: AbortSignal.timeout(3000) })).json();
      if (j.ready) { console.log('daemon ready  :', j.url); process.exit(0); }
      if (i % 12 === 11) console.log('  warming page...', ((i + 1) * 1.5).toFixed(0) + 's');
    } catch {}
    await sleep(1500);
  }
  console.log('daemon did not reach ready; use `node scratch/eval.mjs --go ...` to load manually');
  process.exit(0);
})();
