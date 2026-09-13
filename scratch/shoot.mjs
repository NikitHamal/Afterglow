// Afterglow fast shooter: talks to verify-daemon (localhost:9340).
// Keeps .shots/ to latest run only (wipes *.png first, unless --wipe 0).
// Usage:
//   node scratch/shoot.mjs --shots 'mons:@scratch/cam-mons.js:800,foot:@scratch/cam-feet.js:600'
//   node scratch/shoot.mjs --go 'shot=1&char=goatchan&play=1&solo=1&clean=1&readyframes=90' --prejs @scratch/rub-zone3.js --shots '...' --diagfile scratch/diag-ik.js
'use strict';
import fs from 'fs';
import { spawn } from 'child_process';

const API = 'http://127.0.0.1:9340';
function arg(name, def) {
  const i = process.argv.indexOf('--' + name);
  return i >= 0 && process.argv[i + 1] ? process.argv[i + 1] : def;
}
const sleep = ms => new Promise(r => setTimeout(r, ms));
async function post(path, obj) {
  const r = await fetch(API + path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(obj) });
  const j = await r.json();
  if (!j.ok) throw new Error(path + ': ' + (j.error || 'fail'));
  return j;
}
async function ping() {
  try { const r = await fetch(API + '/ping'); return await r.json(); } catch { return null; }
}
function loadFile(spec) {
  if (!spec || spec === '-') return '';
  return spec.charAt(0) === '@' ? fs.readFileSync(spec.slice(1), 'utf8') : spec;
}

(async () => {
  const t0 = Date.now();
  let p = await ping();
  if (!p || !p.ok) {
    console.log('starting daemon...');
    spawn('node', ['scratch/verify-daemon.mjs'], { stdio: 'ignore', detached: true, cwd: process.cwd() });
    for (let i = 0; i < 100; i++) {
      await sleep(1000);
      p = await ping();
      if (p && p.ok && p.ready) break;
      if (i % 10 === 9) console.log('waiting daemon...', JSON.stringify(p));
    }
    p = await ping();
    if (!p || !p.ready) throw new Error('daemon did not become ready');
  }
  console.log('daemon:', JSON.stringify(p));

  const go = arg('go', '');
  if (go) {
    console.log('loading', go.slice(0, 80));
    const r = await post('/go', { q: go, settle: 1200 });
    console.log('page errors:', JSON.stringify(r.errors));
  }
  const prejs = loadFile(arg('prejs', ''));
  if (prejs) { await post('/eval', { src: prejs }); console.log('prejs ran'); await sleep(900); }

  if (arg('wipe', '1') === '1') {
    for (const f of fs.readdirSync('.shots')) {
      if (f.endsWith('.png')) fs.unlinkSync('.shots/' + f);
    }
    console.log('wiped old shots');
  }
  const errOnly = arg('erronly', '0') === '1';
  if (!errOnly) {
    const errs = await post('/eval', { src: '(window.__agErrors||[]).slice(-8)' });
    console.log('page errors:', JSON.stringify(errs.value));
  }

  for (const spec of arg('shots', 'idle:-:600').split(',')) {
    const parts = spec.split(':');
    const name = parts[0], cam = loadFile(parts[1]), wait = parseInt(parts[2] || '600', 10);
    const r = await post('/shot', { name: name + '.png', cam, wait });
    console.log('saved', r.file, ((Date.now() - t0) / 1000).toFixed(1) + 's');
  }
  const diagFile = arg('diagfile', '');
  const diagSrc = diagFile ? fs.readFileSync(diagFile, 'utf8') : arg('diag', '');
  if (diagSrc) {
    const v = await post('/eval', { src: diagSrc });
    const s = typeof v.value === 'string' ? v.value : JSON.stringify(v.value, null, 1);
    console.log('DIAG>>>'); console.log(s); console.log('<<<DIAG');
    fs.writeFileSync('.shots/diag.json', s);
  }
  console.log('TOTAL', ((Date.now() - t0) / 1000).toFixed(1) + 's');
})().catch(e => { console.error('SHOOT FAIL:', e.message); process.exit(1); });
