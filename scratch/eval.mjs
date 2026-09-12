// eval.mjs — fastest possible probe: posts JS to the warm daemon and prints the
// result. No navigation, no screenshot. ~0.2-1s per call.
//
//   node scratch/eval.mjs @scratch/diag-shape.js
//   node scratch/eval.mjs "JSON.stringify({t:G.t, state:G.state})"
//   node scratch/eval.mjs --go 'shot=1&char=goatchan&play=1&solo=1&readyframes=90' @scratch/diag-shape.js
//   node scratch/eval.mjs --seq @scratch/sweep-a.js --seq @scratch/sweep-b.js
'use strict';
import fs from 'fs';

const API = 'http://127.0.0.1:9340';
const sleep = ms => new Promise(r => setTimeout(r, ms));

function argAll(name) {
  const out = [];
  for (let i = 0; i < process.argv.length; i++) {
    if (process.argv[i] === '--' + name && process.argv[i + 1]) out.push(process.argv[i + 1]);
  }
  return out;
}
function arg(name, def) {
  const i = process.argv.indexOf('--' + name);
  return i >= 0 && process.argv[i + 1] ? process.argv[i + 1] : def;
}
function loadFile(spec) {
  if (!spec || spec === '-') return '';
  return spec.charAt(0) === '@' ? fs.readFileSync(spec.slice(1), 'utf8') : spec;
}
async function post(path, obj) {
  const r = await fetch(API + path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(obj) });
  const j = await r.json();
  if (!j.ok) throw new Error(path + ': ' + (j.error || 'fail'));
  return j;
}

(async () => {
  const t0 = Date.now();
  const go = arg('go', '');
  if (go) {
    const r = await post('/go', { q: go, settle: parseInt(arg('settle', '1200'), 10) });
    console.log('loaded in', ((Date.now() - t0) / 1000).toFixed(1) + 's  errors=' + JSON.stringify(r.errors));
  }
  const settle = parseInt(arg('settle2', '0'), 10);
  if (settle) await sleep(settle);

  const seqs = argAll('seq');
  const inline = argAll('js');
  const FLAGS = new Set(['--go', '--settle', '--settle2', '--js', '--seq']);
  const positional = [];
  for (let i = 2; i < process.argv.length; i++) {
    const a = process.argv[i];
    if (FLAGS.has(a)) { i++; continue; }          // skip flag + its value
    if (a.charAt(0) === '-') continue;            // stray flag
    positional.push(a);
  }
  const jobs = seqs.length ? seqs : (inline.length ? inline : positional);

  for (const spec of jobs) {
    const src = loadFile(spec);
    const t = Date.now();
    const r = await post('/eval', { src });
    const v = r.value;
    const s = typeof v === 'string' ? v : JSON.stringify(v, null, 1);
    console.log('--- ' + (spec.length > 60 ? spec.slice(0, 60) + '…' : spec) + '  (' + (Date.now() - t) + 'ms)');
    console.log(s);
  }
  console.log('TOTAL', ((Date.now() - t0) / 1000).toFixed(1) + 's');
})().catch(e => { console.error('EVAL FAIL:', e.message); process.exit(1); });
