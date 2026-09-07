// Motion-layer verifier: drives updateAnim3 through pose switches, depth
// spikes and rub, asserting finite transforms, bounded hair, and no per-frame
// snapping (max joint/root delta per frame must stay small).
// Run: node .blendtest3.js   (expects CWD = F:\Afterglow)
const fs = require('fs');
const THREE = require('F:\\Afterglow\\vendor\\three.min.js');
global.THREE = THREE;

/* ---- minimal DOM stub (same approach as .contact3.js) ---- */
const ctxStub = new Proxy({}, {
  get: (t, k) => {
    if (k === 'createRadialGradient' || k === 'createLinearGradient')
      return () => ({ addColorStop() {} });
    if (k === 'canvas') return { width: 512, height: 512 };
    if (k === 'measureText') return () => ({ width: 10 });
    return () => {};
  },
  set: () => true
});
function makeCanvas() {
  return { width: 512, height: 512, style: {}, getContext: () => ctxStub, addEventListener() {} };
}
global.document = {
  createElement: t => (t === 'canvas' ? makeCanvas() : { style: {}, classList: { add() {}, remove() {}, toggle() {} }, appendChild() {}, addEventListener() {} }),
  getElementById: () => null,
  addEventListener() {},
  querySelector: () => null,
  querySelectorAll: () => []
};
global.window = { addEventListener() {}, devicePixelRatio: 1, innerWidth: 1280, innerHeight: 720 };
global.requestAnimationFrame = () => 0;
global.localStorage = { getItem: () => null, setItem() {} };
global.clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
global.lerp = (a, b, t) => a + (b - a) * t;
global.TAU = Math.PI * 2;
global.G = {
  t: 0, depth: 0, pleasure: 40, ar: 80, state: 'play', pos: 0, oral: 0,
  rub: 0, rubZone: 0, vel: 0, impact: 0, shaftPulse: 0, nod: 0,
  oralDepth: 0, oralGag: 0, orgT: 0, after: 0,
  char: { preset: 'yuki', skinTone: 0.18, hairColor: '#231318', hairStyle: 'long', breastSize: 0.45 }
};
global.getSkin = () => ({ her: '#f0c6b4', herSh: '#d9a08c', herDk: '#b97f6e' });
global.hexToRgb = h => {
  const s = (h || '#000').replace('#', '');
  return [parseInt(s.slice(0, 2), 16), parseInt(s.slice(2, 4), 16), parseInt(s.slice(4, 6), 16)];
};
global.herExpression = () => ({ eye: 0.6, rolled: 0.2, mouth: 0.4, blush: 0.5, brow: 0.3, tilt: 0.2 });
global.applyPoseCam3 = () => {};

const vm = require('vm');
function load(f) {
  const code = fs.readFileSync('F:\\Afterglow\\js3d\\' + f, 'utf8');
  vm.runInThisContext(code, { filename: f });
}
load('engine3d.js');
load('chars3d.js');
load('poses3d.js');
load('anim3d.js');

_setRigs3(buildHer3(), buildHim3());

let bad = 0, maxStep = 0, maxQ = 0;
// Orientation is compared as LOCAL QUATERNION angle — euler numbers alias
// (π-flips with identical pose) whenever aimArmAt3 writes quaternions.
function snapJoints() {
  const r = rigs3();
  return {
    pHer: r.her.root.position.clone(), pHim: r.him.root.position.clone(),
    q: [
      r.her.legL.hip.quaternion.clone(), r.her.legR.knee.quaternion.clone(),
      r.her.armL.shoulder.quaternion.clone(), r.her.armR.shoulder.quaternion.clone(),
      r.him.armL.shoulder.quaternion.clone(), r.him.armR.elbow.quaternion.clone(),
      r.her.neck.quaternion.clone(), r.her.hair.quaternion.clone(),
      r.him.shaftRoot.quaternion.clone(),
      r.her.legL.foot.quaternion.clone(), r.her.legR.foot.quaternion.clone()
    ],
    shaftS: r.him.shaft.scale.x,
    hairX: r.her.hair.rotation.x, hairZ: r.her.hair.rotation.z
  };
}
// warmup: absorb the initial pose application (frame-0 snap is by design)
for (let f = 0; f < 40; f++) { G.t = f * 0.016; G.depth = 1; updateAnim3(0.016); }
let prev = snapJoints();
for (let f = 40; f < 190; f++) {
  G.t = f * 0.016;
  // scripted abuse: depth spikes, pose switch (settles well before rub),
  // rub zones incl. mid-rub switches, climax pulse
  G.depth = (f % 30 < 15) ? 1 : 0;
  if (f === 50) G.pos = 2;
  if (f === 140) { G.pos = 0; G.oral = 1; }
  if (f === 160) { G.oral = 0; G.shaftPulse = 6.5; }
  G.impact = (f % 10 === 0) ? 1 : 0;
  G.vel = (f % 2) ? 3 : -3;
  G.rub = f > 100 && f < 130 ? 1 : 0;
  G.rubZone = (f / 10 | 0) % 4;
  updateAnim3(0.016);
  const cur = snapJoints();
  const dp = cur.pHer.distanceTo(prev.pHer) + cur.pHim.distanceTo(prev.pHim);
  if (dp > 0.08) { bad++; console.log(`ROOT SNAP Δ=${dp.toFixed(3)} @frame${f}`); }
  maxStep = Math.max(maxStep, dp);
  for (let i = 0; i < cur.q.length; i++) {
    const a = cur.q[i].angleTo(prev.q[i]);
    maxQ = Math.max(maxQ, a);
    if (a > 0.25) { bad++; console.log(`JOINT SNAP q${i} Δ=${a.toFixed(3)} @frame${f}`); }
  }
  const ds = Math.abs(cur.shaftS - prev.shaftS);
  if (ds > 0.35) { bad++; console.log(`SHAFT POP Δ=${ds.toFixed(3)} @frame${f}`); }
  for (const h of [cur.hairX, cur.hairZ]) {
    if (!Number.isFinite(h)) { bad++; console.log(`HAIR NON-FINITE @frame${f}`); }
  }
  prev = cur;
}
console.log(`max root step/frame: ${maxStep.toFixed(4)} | max joint angle/frame: ${maxQ.toFixed(4)}`);
console.log(`hair final: x=${prev.hairX.toFixed(3)} z=${prev.hairZ.toFixed(3)} (bounded ±0.6)`);
if (Math.abs(prev.hairX) > 0.6 || Math.abs(prev.hairZ) > 0.6) { bad++; console.log('HAIR OUT OF BOUNDS'); }
console.log(bad === 0 ? 'BLEND TEST PASSED' : `BLEND TEST FAILED (${bad} violations)`);
process.exit(bad === 0 ? 0 : 1);
