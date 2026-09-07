// One-off probe round 3: FPV-face geometry — head pos, face normal, view angle.
// Run: node .probe3.js   (expects CWD = F:\Afterglow) — DELETE AFTER USE.
const fs = require('fs');
const THREE = require('F:\\Afterglow\\vendor\\three.min.js');
global.THREE = THREE;
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
  getElementById: () => null, addEventListener() {}, querySelector: () => null, querySelectorAll: () => []
};
global.window = { addEventListener() {}, devicePixelRatio: 1, innerWidth: 1280, innerHeight: 720 };
global.requestAnimationFrame = () => 0;
global.localStorage = { getItem: () => null, setItem() {} };
global.clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
global.lerp = (a, b, t) => a + (b - a) * t;
global.TAU = Math.PI * 2;
global.G = {
  t: 1.7, depth: 1, pleasure: 40, ar: 80, state: 'play', pos: 0, oral: 0,
  rub: 0, rubZone: 0, vel: 0, impact: 0, shaftPulse: 0, breast: { p: 0, v: 0 },
  nod: 0, view: 'side', fpvZoom: 1,
  char: { preset: 'yuki', skinTone: 0.18, hairColor: '#231318', hairStyle: 'long', breastSize: 0.45 }
};
global.getSkin = () => ({ her: '#f0c6b4', herSh: '#d9a08c', herDk: '#b97f6e' });
global.hexToRgb = h => {
  const s = (h || '#000').replace('#', '');
  return [parseInt(s.slice(0, 2), 16), parseInt(s.slice(2, 4), 16), parseInt(s.slice(4, 6), 16)];
};
global.herExpression = () => ({ eye: 0.6, rolled: 0.2, mouth: 0.4, blush: 0.5, brow: 0.3, tilt: 0.2 });
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
applyRig3(her, POSES3[0].her, 0.016, 1);
applyRig3(him, POSES3[0].him, 0.016, 1);
her.root.updateWorldMatrix(true, true);
const headPos = new THREE.Vector3();
her.head.getWorldPosition(headPos);
const headQ = new THREE.Quaternion();
her.head.getWorldQuaternion(headQ);
const faceN = new THREE.Vector3(0, 0, 1).applyQuaternion(headQ);
console.log('headPos=(' + headPos.toArray().map(v => +v.toFixed(3)).join(',') + ')');
console.log('faceNormal=(' + faceN.toArray().map(v => +v.toFixed(3)).join(',') + ')');
// FPV face-focus camera (engine3d hardcoords for pos 0/1)
const eye = new THREE.Vector3(0, 0.72, -0.75);
const tgt = new THREE.Vector3(0, 0.28, -1.18);
const viewDir = eye.clone().sub(headPos).normalize();
const ang = faceN.angleTo(viewDir) * 180 / Math.PI;
console.log('eye->head dist=' + eye.distanceTo(headPos).toFixed(3));
console.log('face-vs-view angle=' + ang.toFixed(1) + 'deg (<70 = face visible)');
// face decal world orientation: sample decal mesh +Z in world
her.faceMesh.updateWorldMatrix(true, false);
const decalN = new THREE.Vector3(0, 0, 1).applyQuaternion(her.faceMesh.getWorldQuaternion(new THREE.Quaternion()));
console.log('decalNormal=(' + decalN.toArray().map(v => +v.toFixed(3)).join(',') + ')');
// where is her vulva / his head (sanity)
console.log('vulva=(' + vulvaWorld3(her).toArray().map(v => +v.toFixed(3)).join(',') + ')');
