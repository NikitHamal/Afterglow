// Afterglow 3D — module: perf3d (loaded after post3d.js / glbHim.js, before app3d.js)
'use strict';
/* ============================================================
   FRAME BUDGET
   - Adaptive resolution: the renderer's pixel ratio follows the measured
     frame time (steps of 0.25 between 0.75 and min(2, devicePixelRatio));
     post3d's targets follow through resize3(). Off in dev screenshot modes.
   - Shadow map refreshed every other frame: characters move smoothly at
     30 Hz shadow rate, the room never needs more.
   - Tiny or hidden GLB parts (teeth, tongue, eyes, lashes, brows) stop
     casting shadows — they only cost shadow-pass triangles.
   ============================================================ */
const PERF3 = {
  maxPR: Math.min(2, (typeof window !== 'undefined' && window.devicePixelRatio) || 1),
  minPR: 0.75, pr: 0, acc: 0, n: 0, slow: 0, fast: 0, frame: 0, sweepT: 0.5, shadowEvery: 2,
  on: (typeof location === 'undefined') || !/[?&](adapt=0|shot=|pose=|clean=1)/.test(location.search)
};
const _PERF3_NOCAST = /teeth|tongue|eyes|lashes|brows/i;

function perf3Init() {
  if (typeof renderer3d === 'undefined' || !renderer3d) return;
  PERF3.pr = renderer3d.getPixelRatio();
  renderer3d.shadowMap.autoUpdate = false;
  renderer3d.shadowMap.needsUpdate = true;
}

function perf3SetPR(pr) {
  pr = clamp(Math.round(pr * 4) / 4, PERF3.minPR, Math.max(PERF3.minPR, PERF3.maxPR));
  if (pr === PERF3.pr) return;
  PERF3.pr = pr;
  renderer3d.setPixelRatio(pr);
  if (typeof resize3 === 'function') resize3();
}

function perf3Sweep() {
  if (typeof scene3d === 'undefined' || !scene3d) return;
  scene3d.traverse(o => { if (o.isMesh && o.castShadow && _PERF3_NOCAST.test(o.name || '')) o.castShadow = false; });
}

// raw = unclamped seconds since the previous frame
function perf3Tick(raw) {
  if (typeof renderer3d === 'undefined' || !renderer3d) return;
  PERF3.frame++;
  if (PERF3.frame % PERF3.shadowEvery === 0) renderer3d.shadowMap.needsUpdate = true;
  PERF3.sweepT -= Math.max(0, Math.min(raw, 0.5));
  if (PERF3.sweepT <= 0) { PERF3.sweepT = 2; perf3Sweep(); }
  if (!PERF3.on || document.hidden || raw <= 0 || raw > 0.5) return;
  PERF3.acc += raw; PERF3.n++;
  if (PERF3.acc < 1) return;
  const avg = PERF3.acc / PERF3.n; PERF3.acc = 0; PERF3.n = 0;
  if (avg > 1 / 40) { PERF3.fast = 0; if (++PERF3.slow >= 2) { PERF3.slow = 0; perf3SetPR(PERF3.pr - 0.25); } }
  else if (avg < 1 / 55) { PERF3.slow = 0; if (++PERF3.fast >= 5) { PERF3.fast = 0; perf3SetPR(PERF3.pr + 0.25); } }
  else PERF3.slow = PERF3.fast = 0;
}

if (typeof window !== 'undefined') window.PERF3 = PERF3;
