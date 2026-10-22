// Afterglow — module: glbHim (loaded by 3d.html after glbModel.js)
'use strict';
/* ============================================================
   GLB MALE ("Marcus") — a realistic skinned man that takes over the
   procedural him3's looks. him3's Groups stay the single motion source
   (poses, IK hands, thrust, shaft aim all still run on them); every frame
   the GLB root is pinned to his hips and each MMD bone copies its
   procedural joint's rotation *delta from rest*, corrected for the
   different rest pose (MMD A-stance vs the procedural arms/legs hanging
   straight down). The procedural meshes are hidden, never removed, so a
   failed load simply leaves the old male in place.
   ============================================================ */
const GLB_HIM = {
  path: 'assets/marcus/marcus.glb',
  tones: {
    honey:   ['assets/marcus/skin_honey.jpg',   'assets/marcus/genital_honey.jpg'],
    caramel: ['assets/marcus/skin_caramel.jpg', 'assets/marcus/genital_caramel.jpg'],
    deep:    ['assets/marcus/skin_deep.jpg',    'assets/marcus/genital_deep.jpg']
  },
  tone: 'caramel', toneApplied: 'caramel',
  // customisation morphs (0..1); UI may write these or G.char.him {muscle, weight, belly, length, girth}
  shape: { body_muscle: 0, body_weight: 0, body_belly: 0, penis_length: 0, penis_girth: 0 },
  loading: false, loaded: false, failed: false, active: false,
  model: null, order: [], meshes: [], morph: [], proc: [], headParts: [],
  anchor: null, tipBone: null, tipLocal: null, headBone: null,
  face: { blinkT: 2.5, blink: 0, effort: 0, pleasure: 0, moan: 0, kiss: 0 },
  oralShift: null
};

const _ghQ = (typeof THREE !== 'undefined') ? new THREE.Quaternion() : null;
const _ghQ2 = (typeof THREE !== 'undefined') ? new THREE.Quaternion() : null;
const _ghQ3 = (typeof THREE !== 'undefined') ? new THREE.Quaternion() : null;
const _ghV = (typeof THREE !== 'undefined') ? new THREE.Vector3() : null;
const _ghV2 = (typeof THREE !== 'undefined') ? new THREE.Vector3() : null;
const _ghS = (typeof THREE !== 'undefined') ? new THREE.Vector3() : null;
const _ghX = (typeof THREE !== 'undefined') ? new THREE.Vector3(1, 0, 0) : null;
const _ghDown = (typeof THREE !== 'undefined') ? new THREE.Vector3(0, -1, 0) : null;

/* procedural joint -> MMD bone. Procedural "L" lives on -X = his anatomical
   right, so sides cross (same convention as GLB_RIG_MAP). rest: the joint's
   rest direction in hip space (null = keep the model's own rest curve). */
const GLB_HIM_MAP = [
  { from: 'torso', to: '上半身', rest: null },
  { from: 'chest', to: '上半身2', rest: null },
  { from: 'neck', to: '首', rest: null },
  { from: 'head', to: '頭', rest: null },
  { from: 'armL.shoulder', to: '腕R', rest: 'down', child: 'ひじR' },
  { from: 'armL.elbow', to: 'ひじR', rest: 'down', child: '手首R' },
  { from: 'armL.hand', to: '手首R', rest: 'down', child: '中指０R' },
  { from: 'armR.shoulder', to: '腕L', rest: 'down', child: 'ひじL' },
  { from: 'armR.elbow', to: 'ひじL', rest: 'down', child: '手首L' },
  { from: 'armR.hand', to: '手首L', rest: 'down', child: '中指０L' },
  { from: 'legL.hip', to: '足R', rest: 'down', child: 'ひざR' },
  { from: 'legL.knee', to: 'ひざR', rest: 'down', child: '足首R' },
  { from: 'legL.foot', to: '足首R', rest: null },
  { from: 'legR.hip', to: '足L', rest: 'down', child: 'ひざL' },
  { from: 'legR.knee', to: 'ひざL', rest: 'down', child: '足首L' },
  { from: 'legR.foot', to: '足首L', rest: null },
  { from: 'shaftRoot', to: '竿根', rest: 'down', child: '竿中' }
];
const GLB_HIM_CURL = { '人指': 0, '中指': 1, '薬指': 2, '小指': 3, '親指': -1 };

function glbHimKey(s) { return (s || '').replace(/[._]/g, ''); }
function glbHimJoint(path) {
  if (typeof him3 === 'undefined' || !him3) return null;
  let o = him3;
  const bits = path.split('.');
  for (let i = 0; i < bits.length; i++) { o = o[bits[i]]; if (!o) return null; }
  return o;
}

function glbHimLoad() {
  if (GLB_HIM.loading || GLB_HIM.loaded || GLB_HIM.failed) return;
  if (typeof THREE === 'undefined' || !THREE.GLTFLoader || typeof scene3d === 'undefined' || !scene3d) return;
  GLB_HIM.loading = true;
  new THREE.GLTFLoader().load(GLB_HIM.path, gltf => {
    try { glbHimSetup(gltf); }
    catch (err) { GLB_HIM.failed = true; console.error('[glbHim] setup failed, keeping the procedural male', err); }
    GLB_HIM.loading = false;
  }, undefined, err => {
    GLB_HIM.loading = false; GLB_HIM.failed = true;
    console.warn('[glbHim] load failed, keeping the procedural male', err);
  });
}

function glbHimSetup(gltf) {
  const model = gltf.scene;
  model.updateMatrixWorld(true);
  const bones = {};
  model.traverse(o => {
    if (o.isBone) bones[glbHimKey(o.name)] = o;
    if (o.isMesh) {
      o.castShadow = true; o.receiveShadow = true; o.frustumCulled = false;
      GLB_HIM.meshes.push(o);
      if (o.morphTargetDictionary) GLB_HIM.morph.push(o);
      if (/hair|eyes|brows|lashes|teeth|tongue/i.test(o.name)) GLB_HIM.headParts.push(o);
    }
  });
  if (!bones['上半身'] || !bones['竿根'] || !bones['足L']) throw new Error('unexpected skeleton');

  // bind data in MODEL space (model root is identity right now)
  const order = [];
  const visit = b => { order.push(b); b.children.forEach(c => { if (c.isBone) visit(c); }); };
  Object.values(bones).forEach(b => { if (!b.parent || !b.parent.isBone) visit(b); });
  const info = new Map();
  order.forEach(b => {
    const q = new THREE.Quaternion(); b.getWorldQuaternion(q);
    const p = new THREE.Vector3(); b.getWorldPosition(p);
    const pq = new THREE.Quaternion();
    if (b.parent && !b.parent.isBone) b.parent.getWorldQuaternion(pq);
    info.set(b, { bind: q, pos: p, local: b.quaternion.clone(), staticParent: pq, map: null, C: null, curl: null });
  });
  GLB_HIM_MAP.forEach(m => {
    const b = bones[glbHimKey(m.to)];
    if (!b) return;
    const I = info.get(b);
    I.map = m;
    I.C = new THREE.Quaternion();
    if (m.rest === 'down' && m.child && bones[glbHimKey(m.child)]) {
      const dir = info.get(bones[glbHimKey(m.child)]).pos.clone().sub(I.pos).normalize();
      I.C.setFromUnitVectors(dir, _ghDown);
    }
  });
  // relaxed finger curl on the MMD chains (local X flexes toward the palm on this rig)
  order.forEach(b => {
    const m = /^(人指|中指|薬指|小指|親指)([０１２３])[LR]$/.exec(glbHimKey(b.name));
    if (!m) return;
    const seg = '０１２３'.indexOf(m[2]);
    const k = [0.95, 0.75, 0.55, 0.38][seg] * (m[1] === '親指' ? 0.55 : 1.0);
    info.get(b).curl = new THREE.Quaternion().setFromAxisAngle(_ghX, 0.42 * k);
  });
  GLB_HIM.order = order; GLB_HIM.info = info; GLB_HIM.bones = bones;

  // pelvis anchor: him3.hips sits 0.042 above the hip joints (buildHim3 makeLeg)
  const hl = info.get(bones['足L']).pos, hr = info.get(bones['足R']).pos;
  GLB_HIM.anchor = hl.clone().add(hr).multiplyScalar(0.5).add(new THREE.Vector3(0, 0.042, 0));
  // shaft tip in 竿先's local frame: the chain sits at 0 / 0.42 / 0.78 of the length
  const tipB = bones['竿先'], midB = bones['竿中'];
  GLB_HIM.tipBone = tipB;
  GLB_HIM.tipLocal = new THREE.Vector3(0, info.get(tipB).pos.distanceTo(info.get(midB).pos) * (0.22 / 0.36) / (tipB.getWorldScale(new THREE.Vector3()).y || 1), 0);
  GLB_HIM.headBone = bones['頭'];
  GLB_HIM.oralShift = new THREE.Vector3();

  model.visible = false;
  scene3d.add(model);
  GLB_HIM.model = model;
  GLB_HIM.loaded = true;
  glbHimInstallTipHook();
  try { const m = /himtex=(\d+)/.exec(location.search); if (m) glbHimCapTextures(+m[1]); } catch (_) {}
  console.log('[glbHim] Marcus loaded:', order.length, 'bones,', GLB_HIM.meshes.length, 'meshes');
}

/* fx (jets) and anything else asking for the shaft tip get the GLB tip */
function glbHimInstallTipHook() {
  if (typeof shaftTipWorld3 !== 'function' || shaftTipWorld3.__glbHim) return;
  const orig = shaftTipWorld3;
  const hooked = function (him) {
    if (GLB_HIM.active && GLB_HIM.tipBone && him === him3) return glbHimTip(new THREE.Vector3());
    return orig(him);
  };
  hooked.__glbHim = true; hooked.__orig = orig;
  shaftTipWorld3 = hooked;               // eslint-disable-line no-global-assign
}
function glbHimTip(out) {
  GLB_HIM.tipBone.updateWorldMatrix(true, false);
  return out.copy(GLB_HIM.tipLocal).applyMatrix4(GLB_HIM.tipBone.matrixWorld);
}

function glbHimSetTone(tone) {
  if (!GLB_HIM.tones[tone]) return false;
  GLB_HIM.tone = tone;
  return true;
}
function glbHimApplyTone() {
  if (!GLB_HIM.loaded || GLB_HIM.toneApplied === GLB_HIM.tone) return;
  const want = GLB_HIM.tone; GLB_HIM.toneApplied = want;
  const files = GLB_HIM.tones[want];
  const loader = new THREE.TextureLoader();
  GLB_HIM.meshes.forEach(o => {
    const mats = Array.isArray(o.material) ? o.material : [o.material];
    mats.forEach(mt => {
      if (!mt || !mt.map) return;
      const idx = /marcus_skin/.test(mt.name) ? 0 : (/marcus_genital/.test(mt.name) ? 1 : -1);
      if (idx < 0) return;
      loader.load(files[idx], tex => {
        tex.flipY = false;
        if (THREE.sRGBEncoding != null) tex.encoding = THREE.sRGBEncoding;
        tex.anisotropy = mt.map.anisotropy || 4;
        mt.map = tex; mt.needsUpdate = true;
      });
    });
  });
}
function glbHimSetShape(name, v) {
  if (!(name in GLB_HIM.shape)) return false;
  GLB_HIM.shape[name] = clamp(+v || 0, 0, 1);
  return true;
}

function glbHimMorph(name, w) {
  for (let i = 0; i < GLB_HIM.morph.length; i++) {
    const o = GLB_HIM.morph[i], k = o.morphTargetDictionary[name];
    if (k !== undefined) o.morphTargetInfluences[k] = w;
  }
}

/* ---- face: blink timer, effort with stroke speed, pleasure with arousal ---- */
function glbHimFace(dt) {
  const F = GLB_HIM.face, g = (typeof G !== 'undefined') ? G : {};
  const ar = clamp((g.ar || 0) / 100, 0, 1);
  const climax = (g.state === 'climax' || g.climax) ? 1 : 0;
  F.blinkT -= dt;
  if (F.blinkT <= 0) { F.blinkT = 2.4 + Math.random() * 3.6; F.blinkPh = 0; }
  let blink = 0;
  if (F.blinkPh != null) {
    F.blinkPh += dt;
    const p = F.blinkPh / 0.17;
    blink = p < 0.4 ? p / 0.4 : Math.max(0, 1 - (p - 0.4) / 0.6);
    if (p >= 1) F.blinkPh = null;
  }
  const speed = Math.abs(g.vel || 0) * 0.08 + (g.spaceHeld || g.dragOn ? 0.25 : 0);
  const ease = (cur, tgt, r) => cur + (tgt - cur) * Math.min(1, dt * r);
  F.effort = ease(F.effort, clamp(speed * 0.9 + climax * 0.35, 0, 0.75), 4);
  F.pleasure = ease(F.pleasure, clamp(ar * 0.55 + climax * 0.45, 0, 0.9), 2.5);
  F.moan = ease(F.moan, clamp(climax * 0.75 + (g.impact || 0) * 0.25, 0, 0.8), 6);
  F.kiss = ease(F.kiss, clamp((g.kiss || 0) * 0.7, 0, 0.7), 5);
  glbHimMorph('blink', Math.max(blink, climax * 0.7));
  glbHimMorph('effort', F.effort * (1 - F.pleasure * 0.5));
  glbHimMorph('pleasure', F.pleasure);
  glbHimMorph('moan', F.moan);
  glbHimMorph('kiss', F.kiss);
  const ch = g.char && g.char.him;
  const S = GLB_HIM.shape;
  glbHimMorph('body_muscle', ch && ch.muscle != null ? ch.muscle : S.body_muscle);
  glbHimMorph('body_weight', ch && ch.weight != null ? ch.weight : S.body_weight);
  glbHimMorph('body_belly', ch && ch.belly != null ? ch.belly : S.body_belly);
  glbHimMorph('penis_length', ch && ch.length != null ? ch.length : S.penis_length);
  glbHimMorph('penis_girth', ch && ch.girth != null ? ch.girth : S.penis_girth);
}

/* ---- correction pass: the GLB skeleton has its own proportions, so after the
   rotation retarget his head, hands and feet are swung onto the procedural
   joints the gameplay aims (head: one spine swing; limbs: two-bone IK with the
   procedural elbow/knee as the bend pole). ---- */
const _ghA = (typeof THREE !== 'undefined') ? new THREE.Vector3() : null;
const _ghB = (typeof THREE !== 'undefined') ? new THREE.Vector3() : null;
const _ghC = (typeof THREE !== 'undefined') ? new THREE.Vector3() : null;
const _ghE = (typeof THREE !== 'undefined') ? new THREE.Vector3() : null;
const _ghP = (typeof THREE !== 'undefined') ? new THREE.Vector3() : null;
const _ghW = (typeof THREE !== 'undefined') ? new THREE.Quaternion() : null;
const _ghW2 = (typeof THREE !== 'undefined') ? new THREE.Quaternion() : null;
function glbHimSetWorldQ(bone, wq) {
  bone.parent.getWorldQuaternion(_ghW2).invert();
  bone.quaternion.copy(_ghW2.multiply(wq));
  bone.updateMatrixWorld(true);
}
function glbHimSwing(bone, pivot, cur, want, maxAng) {
  const a = _ghA.copy(cur).sub(pivot), b = _ghB.copy(want).sub(pivot);
  if (a.lengthSq() < 1e-8 || b.lengthSq() < 1e-8) return;
  a.normalize(); b.normalize();
  _ghW.setFromUnitVectors(a, b);
  const ang = 2 * Math.acos(clamp(_ghW.w, -1, 1));
  if (maxAng && ang > maxAng) _ghW.slerp(_ghW2.identity(), 1 - maxAng / ang);
  bone.getWorldQuaternion(_ghQ);
  glbHimSetWorldQ(bone, _ghW.multiply(_ghQ));
}
function glbHimTwoBone(upper, lower, end, target, pole) {
  const S = upper.getWorldPosition(_ghC), E = lower.getWorldPosition(_ghE), W = end.getWorldPosition(_ghP);
  const a = S.distanceTo(E), b = E.distanceTo(W);
  const T = _ghV.copy(target);
  const dir = _ghV2.copy(T).sub(S); let d = dir.length();
  if (d < 1e-5 || a < 1e-5 || b < 1e-5) return;
  dir.divideScalar(d);
  d = clamp(d, Math.abs(a - b) + 1e-3, a + b - 1e-3);
  // bend plane from the procedural pole (elbow/knee), falling back to the current bend
  const pl = _ghA.copy(pole).sub(S); pl.addScaledVector(dir, -pl.dot(dir));
  if (pl.lengthSq() < 1e-8) { pl.copy(E).sub(S); pl.addScaledVector(dir, -pl.dot(dir)); }
  if (pl.lengthSq() < 1e-8) return;
  pl.normalize();
  const cosA = clamp((a * a + d * d - b * b) / (2 * a * d), -1, 1), sinA = Math.sqrt(1 - cosA * cosA);
  const Enew = _ghB.copy(S).addScaledVector(dir, a * cosA).addScaledVector(pl, a * sinA);
  glbHimSwing(upper, S.clone(), E.clone(), Enew, 0);
  const E2 = lower.getWorldPosition(_ghE).clone(), W2 = end.getWorldPosition(_ghP).clone();
  glbHimSwing(lower, E2, W2, S.clone().addScaledVector(dir, d), 0);
}
function glbHimCorrect() {
  const B = GLB_HIM.bones;
  // head: one swing of the lower spine (capped) so the skull lands on his procedural head
  const hp = him3.head.getWorldPosition(new THREE.Vector3());
  const piv = B['上半身'].getWorldPosition(new THREE.Vector3());
  glbHimSwing(B['上半身'], piv, B['頭'].getWorldPosition(new THREE.Vector3()), hp, 0.45);
  // arms (procedural L = his right) and legs
  const limbs = [
    [B['腕R'], B['ひじR'], B['手首R'], him3.armL.hand, him3.armL.elbow],
    [B['腕L'], B['ひじL'], B['手首L'], him3.armR.hand, him3.armR.elbow],
    [B['足R'], B['ひざR'], B['足首R'], him3.legL.foot, him3.legL.knee],
    [B['足L'], B['ひざL'], B['足首L'], him3.legR.foot, him3.legR.knee]
  ];
  for (let i = 0; i < limbs.length; i++) {
    const L = limbs[i];
    if (!L[0] || !L[1] || !L[2] || !L[3] || !L[4]) continue;
    // the end joint keeps its retargeted world orientation through the IK
    const keep = L[2].getWorldQuaternion(new THREE.Quaternion());
    glbHimTwoBone(L[0], L[1], L[2], L[3].getWorldPosition(new THREE.Vector3()), L[4].getWorldPosition(new THREE.Vector3()));
    glbHimSetWorldQ(L[2], keep);
  }
}

/* dev only: ?himtex=1024 caps the male's texture size (headless SwiftShader
   runs out of texture memory with every GLB loaded) */
function glbHimCapTextures(cap) {
  GLB_HIM.meshes.forEach(o => {
    const mats = Array.isArray(o.material) ? o.material : [o.material];
    mats.forEach(mt => ['map', 'normalMap', 'roughnessMap', 'metalnessMap', 'aoMap'].forEach(k => {
      const t = mt && mt[k]; if (!t || !t.image || !t.image.width || t.image.width <= cap) return;
      const cv = document.createElement('canvas'); cv.width = cv.height = cap;
      cv.getContext('2d').drawImage(t.image, 0, 0, cap, cap);
      const n = new THREE.CanvasTexture(cv); n.flipY = t.flipY; n.encoding = t.encoding; n.wrapS = t.wrapS; n.wrapT = t.wrapT;
      mt[k] = n; mt.needsUpdate = true;
    }));
  });
}

/* ---- per frame: runs after updateGLBModel in tick3 ---- */
function updateGLBHim(dt) {
  if (!GLB_HIM.loaded) { glbHimLoad(); return; }
  if (typeof him3 === 'undefined' || !him3 || !him3.hips) return;
  const model = GLB_HIM.model;
  if (GLB_HIM.procOwner !== him3) {                 // him3 is rebuilt on character switches
    GLB_HIM.procOwner = him3; GLB_HIM.proc.length = 0;
    if (him3.root) him3.root.traverse(o => { if (o.isMesh) GLB_HIM.proc.push(o); });
  }
  const show = !!(him3.root && him3.root.visible);
  GLB_HIM.active = true;
  model.visible = show;
  for (let i = 0; i < GLB_HIM.proc.length; i++) GLB_HIM.proc[i].visible = false;
  if (!show) return;
  if (typeof G !== 'undefined' && G.char && G.char.him && G.char.him.tone) glbHimSetTone(G.char.him.tone);
  glbHimApplyTone();

  // root: pinned to his hips (orientation, scale, position of the pelvis anchor)
  const hips = him3.hips;
  hips.updateWorldMatrix(true, false);
  hips.getWorldQuaternion(model.quaternion);
  hips.getWorldScale(_ghS); model.scale.copy(_ghS);
  hips.getWorldPosition(_ghV);
  _ghV2.copy(GLB_HIM.anchor).multiply(_ghS).applyQuaternion(model.quaternion);
  model.position.copy(_ghV).sub(_ghV2).add(GLB_HIM.oralShift);

  // bones: model-space target = delta(rest -> now, in hip space) * C * bind
  _ghQ3.copy(model.quaternion).invert();                       // hips^-1 (model space == hip space)
  const cur = GLB_HIM._cur || (GLB_HIM._cur = new Map());
  const order = GLB_HIM.order, info = GLB_HIM.info;
  for (let i = 0; i < order.length; i++) {
    const b = order[i], I = info.get(b);
    const parentQ = (b.parent && b.parent.isBone) ? cur.get(b.parent) : I.staticParent;
    let R = cur.get(b);
    if (!R) { R = new THREE.Quaternion(); cur.set(b, R); }
    const src = I.map ? glbHimJoint(I.map.from) : null;
    if (src) {
      src.getWorldQuaternion(_ghQ);
      R.copy(_ghQ3).multiply(_ghQ).multiply(I.C).multiply(I.bind);
      b.quaternion.copy(_ghQ2.copy(parentQ).invert().multiply(R));
    } else {
      b.quaternion.copy(I.local);
      if (I.curl) b.quaternion.multiply(I.curl);
      R.copy(parentQ).multiply(b.quaternion);
    }
  }
  model.updateMatrixWorld(true);
  glbHimCorrect();

  // shaft swell + FPV head collapse
  const sb = GLB_HIM.bones['竿根'];
  const pulse = (him3.shaft && him3.shaft.scale) ? him3.shaft.scale.x : 1;
  sb.scale.setScalar(pulse);
  const fpv = (typeof CAM3 !== 'undefined') && CAM3.mode === 'fpv';
  GLB_HIM.headBone.scale.setScalar(fpv ? 0.001 : 1);
  for (let i = 0; i < GLB_HIM.headParts.length; i++) GLB_HIM.headParts[i].visible = !fpv;

  // tip alignment: the poses were authored around the procedural 0.34-long shaft (its tip
  // barely enters at full depth), so the man slides along until his realistic tip sits
  // where the procedural one does. Hands/feet stay pinned by the IK pass, so his limbs
  // absorb the move. Off for lick-oral and solo (his tip plays no part there).
  const g = (typeof G !== 'undefined') ? G : {};
  const lick = (g.oral || 0) > 0.03 && !(g.oralMode === 'blow' || g.oralT === 2);
  const want = GLB_HIM._want || (GLB_HIM._want = new THREE.Vector3());
  if (!lick && shaftTipWorld3.__orig) {
    const pt = shaftTipWorld3.__orig(him3), ht = glbHimTip(_ghV);
    want.copy(GLB_HIM.oralShift).add(pt).sub(ht);
    if (want.length() > 0.22) want.setLength(0.22);
  } else want.set(0, 0, 0);
  GLB_HIM.oralShift.lerp(want, Math.min(1, dt * 8));

  glbHimFace(dt);
}

if (typeof window !== 'undefined') {
  window.GLB_HIM = GLB_HIM;
  window.glbHimSetTone = glbHimSetTone;
  window.glbHimSetShape = glbHimSetShape;
}
