// headless smoke test for the 3D character builder (no WebGL required)
const fs = require('fs');
const path = require('path');
const THREE = require('./vendor/three.min.js');
global.THREE = THREE;

/* ---- minimal DOM stub ---- */
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

/* ---- shared game globals that the 3D modules read ---- */
global.clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
global.lerp = (a, b, t) => a + (b - a) * t;
global.sm = (a, b, v) => { v = clamp((v - a) / (b - a), 0, 1); return v * v * (3 - 2 * v); };
global.TAU = Math.PI * 2;
global.G = {
  t: 0, depth: 0.3, pleasure: 40, ar: 50, state: 'play', pos: 0, oral: 0, rub: 0, rubZone: 0,
  vel: 0, impact: 0, shaftPulse: 0, breast: { p: 0, v: 0 }, nod: 0, view: 'side', fpvZoom: 1,
  char: {
    preset: 'yuki', name: 'Yuki', skinTone: 0.18, hairColor: '#231318', hairStyle: 'long',
    bodyScale: 0.45, breastSize: 0.45, nippleColor: '#c25f63', blushColor: '#e86070',
    lipColor: '#b3555f', eyeColor: '#4a2c33', pubicHair: 'trim'
  }
};
global.getSkin = () => ({ her: '#f0c6b4', herSh: '#d9a08c', herDk: '#b97f6e' });
global.hexToRgb = h => {
  const s = (h || '#000').replace('#', '');
  return [parseInt(s.slice(0, 2), 16), parseInt(s.slice(2, 4), 16), parseInt(s.slice(4, 6), 16)];
};
global.herExpression = () => ({ eye: 0.6, rolled: 0.2, mouth: 0.4, blush: 0.5, brow: 0.3, tilt: 0.2 });

/* ---- load the 3D modules that don't need a GL context ---- */
const vm = require('vm');
function load(f) {
  const code = fs.readFileSync(path.join('js3d', f), 'utf8');
  // runInThisContext shares the global scope, so top-level declarations
  // become visible across modules exactly like classic <script> tags.
  vm.runInThisContext(code, { filename: f });
}
let ok = true;
try {
  load('engine3d.js');
  console.log('engine3d.js  loaded');
} catch (e) { ok = false; console.log('engine3d.js  FAIL: ' + e.message); }

try {
  load('chars3d.js');
  console.log('chars3d.js   loaded');
} catch (e) { ok = false; console.log('chars3d.js   FAIL: ' + e.message); }

try {
  load('poses3d.js');
  console.log('poses3d.js   loaded');
} catch (e) { ok = false; console.log('poses3d.js   FAIL: ' + e.message); }

if (!ok) process.exit(1);

/* ---- build the characters ---- */
let her, him;
try {
  her = buildHer3();
  console.log('buildHer3()  ok');
} catch (e) { console.log('buildHer3()  FAIL: ' + e.message); process.exit(1); }
try {
  him = buildHim3();
  console.log('buildHim3()  ok');
} catch (e) { console.log('buildHim3()  FAIL: ' + e.message); process.exit(1); }

function countMeshes(o) {
  let n = 0;
  o.traverse(c => { if (c.isMesh) n++; });
  return n;
}
console.log('  her meshes: ' + countMeshes(her.root) + '   him meshes: ' + countMeshes(him.root));

/* ---- rig completeness ---- */
const herBones = ['hips', 'torso', 'chest', 'neck', 'head', 'breastL', 'breastR', 'armL', 'armR', 'legL', 'legR', 'face', 'hair'];
const himBones = ['hips', 'torso', 'chest', 'neck', 'head', 'shaftRoot', 'shaft', 'glans', 'armL', 'armR', 'legL', 'legR'];
const missHer = herBones.filter(b => !her[b]);
const missHim = himBones.filter(b => !him[b]);
console.log('  her missing bones: ' + (missHer.length ? missHer.join(',') : 'none'));
console.log('  him missing bones: ' + (missHim.length ? missHim.join(',') : 'none'));

/* ---- every pose applies cleanly to both rigs ---- */
for (let i = 0; i < POSES3.length; i++) {
  G.pos = i; G.oral = 0;
  const p = currentPose3();
  try {
    applyRig3Test(her, p.her);
    applyRig3Test(him, p.him);
  } catch (e) { console.log('  pose ' + i + ' FAIL: ' + e.message); process.exit(1); }
}
G.oral = 1;
try {
  const o = currentPose3();
  applyRig3Test(her, o.her); applyRig3Test(him, o.him);
  console.log('  all 7 poses + oral applied cleanly');
} catch (e) { console.log('  oral FAIL: ' + e.message); process.exit(1); }

/* minimal applyRig3 stand-in (mirrors anim3d.js) */
function applyRig3Test(rig, cfg) {
  const root = rig.root;
  root.position.set(cfg.pos[0], cfg.pos[1], cfg.pos[2]);
  orientTo3(root, cfg.up, cfg.fwd);
  rig.legL.hip.rotation.x = cfg.hip[0];
  rig.legR.hip.rotation.x = cfg.hip[1];
  rig.legL.knee.rotation.x = cfg.knee[0];
  rig.legR.knee.rotation.x = cfg.knee[1];
  rig.armL.shoulder.rotation.x = cfg.arm[0];
  rig.armR.shoulder.rotation.x = cfg.arm[1];
  rig.armL.elbow.rotation.x = cfg.elbow[0];
  rig.armR.elbow.rotation.x = cfg.elbow[1];
  rig.neck.rotation.set(cfg.head[0], cfg.head[1], cfg.head[2]);
  root.updateWorldMatrix(true, true);
  // NaN check across the whole rig
  let bad = 0;
  root.traverse(o => {
    if (!isFinite(o.position.x + o.position.y + o.position.z)) bad++;
    if (!isFinite(o.quaternion.x + o.quaternion.w)) bad++;
  });
  if (bad) throw new Error(bad + ' nodes have NaN transforms');
}

/* ---- face painting ---- */
try {
  paintFace3(her.face.ctx, herExpression(), G.char);
  console.log('paintFace3() ok');
} catch (e) { console.log('paintFace3() FAIL: ' + e.message); process.exit(1); }

/* ---- world anchors ---- */
try {
  const v = vulvaWorld3(her), s = shaftTipWorld3(him), b = breastWorld3(her, -1);
  const finite = [v, s, b].every(p => isFinite(p.x + p.y + p.z));
  console.log('world anchors ok (finite=' + finite + ')');
  if (!finite) process.exit(1);
} catch (e) { console.log('anchors FAIL: ' + e.message); process.exit(1); }

console.log('\nSMOKE TEST PASSED');
