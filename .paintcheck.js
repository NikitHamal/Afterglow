// Validates blush/gloss/crease wiring on the built rigs.
const fs = require('fs');
const vm = require('vm');
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
const mc = () => ({ width: 512, height: 512, style: {}, getContext: () => ctxStub, addEventListener() {} });
global.document = {
  createElement: t => (t === 'canvas' ? mc() : { style: {}, classList: { add() {}, remove() {}, toggle() {} }, appendChild() {}, addEventListener() {} }),
  getElementById: () => null, addEventListener() {}, querySelector: () => null, querySelectorAll: () => []
};
global.window = { addEventListener() {}, devicePixelRatio: 1, innerWidth: 1280, innerHeight: 720 };
global.requestAnimationFrame = () => 0;
global.localStorage = { getItem: () => null, setItem() {} };
global.clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
global.lerp = (a, b, t) => a + (b - a) * t;
global.sm = (a, b, v) => { v = clamp((v - a) / (b - a), 0, 1); return v * v * (3 - 2 * v); };
global.TAU = Math.PI * 2;
global.G = {
  t: 0, depth: 0.3, pleasure: 40, ar: 50, state: 'play', pos: 0, oral: 0, rub: 0, rubZone: 0,
  vel: 0, impact: 0, shaftPulse: 0, breast: { p: 0, v: 0 }, nod: 0, view: 'side', fpvZoom: 1,
  char: {
    preset: 'yuki', skinTone: 0.18, hairColor: '#231318', hairStyle: 'long',
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

const load = f => vm.runInThisContext(fs.readFileSync('F:\\Afterglow\\js3d\\' + f, 'utf8'), { filename: f });
load('engine3d.js'); load('chars3d.js'); load('poses3d.js'); load('anim3d.js');
const her = buildHer3(), him = buildHim3();

let fail = 0;
const ok = (cond, msg) => { console.log((cond ? 'PASS ' : 'FAIL ') + msg); if (!cond) fail++; };

// 1. blushed meshes: cloned material + vertexColors + color attribute with real variation
let blushed = 0, flat = 0;
for (const rig of [her, him]) {
  rig.root.traverse(o => {
    if (o.isMesh && o.material && o.material.vertexColors && o.geometry.getAttribute('color')) {
      blushed++;
      const c = o.geometry.getAttribute('color');
      let mn = 9, mx = -9;
      for (let i = 0; i < c.count; i++) { mn = Math.min(mn, c.getY(i)); mx = Math.max(mx, c.getY(i)); }
      if (mx - mn < 0.01) { flat++; console.log('  flat blush on ' + (o.geometry.type || '?')); }
    }
  });
}
ok(blushed >= 7, `blushed meshes >= 7 (got ${blushed}: buttocks+breasts+thighs+areolas)`);
ok(flat === 0, 'no flat (dead) blush gradients');

// shared skin material must NOT have vertexColors (would blacken unpainted meshes)
ok(her.mat.vertexColors !== true, 'shared her skin material untouched by blush');
ok(him.mat.vertexColors !== true, 'shared him skin material untouched by blush');

// 2. gloss sprites exist and are additive
let gloss = 0, glossBad = 0;
for (const rig of [her, him]) {
  rig.root.traverse(o => {
    if (o.isSprite) {
      gloss++;
      if (!o.material.transparent || o.material.blending !== THREE.AdditiveBlending) glossBad++;
    }
  });
}
ok(gloss >= 10, `gloss decals >= 10 (got ${gloss}: butt/thigh/breast)`);
ok(glossBad === 0, 'all gloss decals additive + transparent');

// 3. crease/cleft meshes exist and are excluded from ink
let crease = 0, creaseInked = 0;
for (const rig of [her, him]) {
  rig.root.traverse(o => {
    if (o.isMesh && o.userData.noInk && o.material && o.material.isMeshToonMaterial) {
      crease++;
      // an outline shell sharing this geometry would be a bug
      o.children.forEach(ch => { if (ch.isMesh && ch.material && ch.material.isShaderMaterial && ch.material.uniforms && ch.material.uniforms.uThick) creaseInked++; });
    }
  });
}
ok(crease >= 6, `detail meshes present (got ${crease}: clefts/folds/areolas/nipples/pubes)`);
ok(creaseInked === 0, 'no ink shells on detail meshes');

// 4. ink shells present on the big masses
let ink = 0;
her.root.traverse(o => {
  if (o.isMesh && o.material && o.material.isShaderMaterial && o.material.uniforms && o.material.uniforms.uThick) ink++;
});
ok(ink >= 20, `ink outline shells on her (got ${ink})`);

// 5. outline ink color is warm brown, thin
const inkMat = OUTLINE_MAT;
const hex = '#' + inkMat.uniforms.uColor.value.getHexString();
ok(inkMat.uniforms.uThick.value <= 0.014, `outline base thickness sane (${inkMat.uniforms.uThick.value})`);
console.log(`  ink color ${hex}`);
ok(hex !== '#1b0a12', 'ink is not pure black anymore');

process.exit(fail ? 1 : 0);
