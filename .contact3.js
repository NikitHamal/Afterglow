// Contact verifier: snaps every pose and measures genital contact + grounding.
// Run: node .contact3.js   (expects CWD = F:\Afterglow)
const fs = require('fs');
const THREE = require('F:\\Afterglow\\vendor\\three.min.js');
global.THREE = THREE;

/* ---- minimal DOM stub (same approach as .smoke3d.js) ---- */
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

/* ---- shared game globals ---- */
global.clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
global.lerp = (a, b, t) => a + (b - a) * t;
global.sm = (a, b, v) => { v = clamp((v - a) / (b - a), 0, 1); return v * v * (3 - 2 * v); };
global.TAU = Math.PI * 2;
global.G = {
  t: 1.7, depth: 1, pleasure: 40, ar: 80, state: 'play', pos: 0, oral: 0,
  rub: 0, rubZone: 0, vel: 0, impact: 0, shaftPulse: 0, breast: { p: 0, v: 0 },
  nod: 0, view: 'side', fpvZoom: 1,
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

/* ---- load the 3D modules like classic <script> tags ---- */
const vm = require('vm');
function load(f) {
  const code = fs.readFileSync('F:\\Afterglow\\js3d\\' + f, 'utf8');
  vm.runInThisContext(code, { filename: f });
}
load('engine3d.js');
load('chars3d.js');
load('poses3d.js');
load('anim3d.js');

const her = buildHer3(), him = buildHim3();
function wp(o) { const v = new THREE.Vector3(); o.getWorldPosition(v); return v; }

function snap(p) {
  applyRig3(her, p.her, 0.016, 1);
  applyRig3(him, p.him, 0.016, 1);
  const ar = clamp((G.ar || 0) / 100, 0, 1);
  him.shaftRoot.rotation.x = (p.him.shaft != null) ? p.him.shaft : (-Math.PI / 2 + 0.12 - ar * 0.10);
  her.root.updateWorldMatrix(true, true);
  him.root.updateWorldMatrix(true, true);
}

const MATTRESS_TOP = 0.11;
let worst = 0, unreach = 0;

/* distance from point P to segment AB (penetration-aware contact) */
const _w = new THREE.Vector3();
function segDist(P, A, B) {
  _w.copy(B).sub(A);
  const len2 = _w.lengthSq() || 1e-9;
  const t = clamp(_w.clone().copy(P).sub(A).dot(_w) / len2, 0, 1);
  return { d: A.clone().addScaledVector(_w, t).distanceTo(P), t };
}

/* hand reachability: is the IK target inside arm range? */
function reach(label, rig, armKey, cfg) {
  const arm = rig[armKey];
  const S = wp(arm.shoulder);
  const T = handTarget3(her, him, cfg, new THREE.Vector3());
  const maxR = rig.R.upperArm + rig.R.foreArm + 0.05;
  const pct = Math.round(S.distanceTo(T) / maxR * 100);
  const flag = pct > 100 ? ' UNREACHABLE' : '';
  if (pct > 100) unreach++;
  return ` ${label}=${pct}%${flag}`;
}

function rep(label, p, oral) {
  snap(p);
  const V = vulvaWorld3(her);
  let d, tag;
  if (oral) {
    him.head.updateWorldMatrix(true, false);
    const M = him.head.localToWorld(new THREE.Vector3(0, -0.05, him.R.headR));
    d = M.distanceTo(V);
    tag = `mouth=(${M.x.toFixed(2)},${M.y.toFixed(2)},${M.z.toFixed(2)})`;
  } else {
    const S = shaftTipWorld3(him);
    him.shaftRoot.updateWorldMatrix(true, false);
    const B = him.shaftRoot.localToWorld(new THREE.Vector3(0, 0, 0));
    const sd = segDist(V, B, S);
    d = sd.d;   // distance vulva -> shaft SEGMENT (0 = penetration)
    tag = `tip=(${S.x.toFixed(2)},${S.y.toFixed(2)},${S.z.toFixed(2)}) pen=${(sd.t * 0.34).toFixed(2)}m`;
  }
  worst = Math.max(worst, d);
  const flag = d > 0.06 ? '  <-- FLOATING' : '  ok';
  let hands = '';
  const H = p.hands;
  if (H) {
    if (H.himL) hands += reach('himL', him, 'armL', H.himL);
    if (H.himR) hands += reach('himR', him, 'armR', H.himR);
    if (H.herL) hands += reach('herL', her, 'armL', H.herL);
    if (H.herR) hands += reach('herR', her, 'armR', H.herR);
  }
  console.log(
    `${label}: contact=${(d * 100).toFixed(1)}cm${flag}\n` +
    `   vulva=(${V.x.toFixed(2)},${V.y.toFixed(2)},${V.z.toFixed(2)}) ${tag}\n` +
    `   herKneeL=${wp(her.legL.knee).y.toFixed(2)} herKneeR=${wp(her.legR.knee).y.toFixed(2)}` +
    ` himKnee=${wp(him.legL.knee).y.toFixed(2)} himHand=${wp(him.armR.hand).y.toFixed(2)}` +
    ` (mattress top ${MATTRESS_TOP})${hands ? '\n   reach:' + hands : ''}`
  );
}

POSES3.forEach((p, i) => rep('pose' + i + ' ' + p.name, p, false));
rep('oral   ORAL', ORAL3, true);
console.log(`\nWORST CONTACT GAP: ${(worst * 100).toFixed(1)}cm (target < 6cm every pose) | UNREACHABLE HANDS: ${unreach}`);
process.exit(worst > 0.06 || unreach > 0 ? 2 : 0);
