// Pure-Node simulation harness for Afterglow 3D rig math  (FAST, browser-free verify).
// Runs the REAL js3d/glbModel.js + anim3d.js + poses3d.js against the actual
// goatchan.glb skeleton (loaded via three GLTFLoader, no GPU/browser) and
// measures quantitative deformation metrics: ankle/foot direction, shin bend,
// hand bounding-box thickness, finger curl, and IK wrist-gap. This is the
// "real physics / simulation" verify loop — runs every pose in <1s total,
// versus ~15-120s for the Chrome screenshot verify (verify-fast.mjs).
//
// Setup (one time):  npm i three        (in this scratch/ dir, or repo root)
// Usage:  node verify-sim.mjs            (run all scenarios + PASS/FAIL verdict)
//         node verify-sim.mjs sweep      (sweep GLB_FOOT_TUNE / hands.scale)
'use strict';
import * as THREE from 'three';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';
import fs from 'fs';
import vm from 'vm';
import path from 'path';

const REPO = path.resolve(path.dirname(new URL(import.meta.url).pathname), "..");
const GLB_PATH = path.join(REPO, 'assets/goatchan/goatchan.glb');

// ---- global stubs the app scripts expect (browser -> node) ----
globalThis.THREE = THREE;
globalThis.GLTFLoader = GLTFLoader;
globalThis.fs = fs;
globalThis.window = { CAM3: null };
globalThis.self = globalThis;
globalThis.location = { search: '' };
globalThis.G = {
  t: 0, solo: false, play: 1, state: 'play', pos: 0, oral: 0, rub: 0,
  rubZone: 3, spaceHeld: false, pleasure: 0, ar: 0, depth: 0, impact: 0,
  vel: 0, shaftPulse: 0, char: { preset: 'goatchan', isGLB: true },
  // pose-follow state
  _aimSm3: null
};
globalThis.TAU = Math.PI * 2;
globalThis.clamp = (v, a, b) => Math.max(a, Math.min(b, v));
// fake DOM for GLTFLoader texture decode (we don't need real pixels)
function makeFakeImg() {
  const el = { width: 4, height: 4, complete: true, naturalWidth: 4, naturalHeight: 4,
    onload: null, onerror: null, _src: '', decode() { return Promise.resolve(); } };
  Object.defineProperty(el, 'src', {
    get() { return this._src; },
    set(v) { this._src = v; if (this.onload) setTimeout(() => this.onload(), 0); }
  });
  el.addEventListener = () => {}; el.removeEventListener = () => {};
  return el;
}
globalThis.document = {
  createElementNS: () => makeFakeImg(),
  createElement: () => makeFakeImg()
};
globalThis.createImageBitmap = async () => ({ width: 4, height: 4 });

// ---- load the real app modules into the shared realm ----
const files = ['glbModel.js', 'anim3d.js', 'poses3d.js'].map(f =>
  fs.readFileSync(path.join(REPO, 'js3d', f), 'utf8'));

// harness scenario/measurement code (sees all lexical consts/functions)
const harnessFn = `
async function runHarness() {
  const DEG = 180 / Math.PI;

  // ---- correct 2-bone IK (pole method), overrides aimArmAt3 for testing ----
  function ikFixed(arm, targetWorld, upperLen, foreLen, bendBias, rate, dt, elbRate, restDir) {
    const sh = arm.shoulder;
    const parent = sh.parent;
    if (!parent) return arm.elbow.rotation.x;
    parent.updateWorldMatrix(true, false);
    const rest = (restDir && restDir.isVector3) ? restDir.clone().normalize() : new THREE.Vector3(0, -1, 0);
    const S = sh.position.clone();
    const T = parent.worldToLocal(targetWorld.clone());
    let dir = T.clone().sub(S);
    const dRaw = dir.length();
    const d = clamp(dRaw, Math.abs(upperLen - foreLen) + 0.02, (upperLen + foreLen) * 0.999);
    dir.normalize();
    const q = new THREE.Quaternion().setFromUnitVectors(rest, dir);
    const alpha = Math.acos(clamp((upperLen * upperLen + d * d - foreLen * foreLen) / (2 * upperLen * d), -1, 1));
    const bendAxis = new THREE.Vector3(1, 0, 0).applyQuaternion(q); // parent-space, ⟂ dir
    const upperDir = dir.clone().applyAxisAngle(bendAxis, alpha);
    const elbowPos = S.clone().add(upperDir.clone().multiplyScalar(upperLen));
    let foreDir = T.clone().sub(elbowPos);
    if (foreDir.lengthSq() > 1e-12) foreDir.normalize();
    const qS = new THREE.Quaternion().setFromUnitVectors(rest, upperDir);
    const localFore = foreDir.clone().applyQuaternion(qS.clone().invert());
    const psi = Math.atan2(-localFore.x, localFore.z);
    const qTwist = new THREE.Quaternion().setFromAxisAngle(upperDir, psi);
    qS.premultiply(qTwist);
    const localFore2 = foreDir.clone().applyQuaternion(qS.clone().invert());
    const elbowAngle = clamp(Math.atan2(localFore2.z, localFore2.y) + (bendBias || 0), -2.9, 2.9);
    const track = dt > 0 ? (1 - Math.exp(-(rate || 14) * dt)) : 1;
    sh.quaternion.slerp(qS, track);
    const useE = (dt > 0 && elbRate) ? dampNum3(arm.elbow.rotation.x, elbowAngle, elbRate, dt) : elbowAngle;
    arm.elbow.rotation.set(useE, 0, 0);
    return useE;
  }
  // aimArmAt3 override removed: harness now exercises the REAL edited anim3d.js

  function wp(obj) { return obj.getWorldPosition(new THREE.Vector3()); }
  function findBone(model, name) {
    const cn = glbCleanKey(name);
    let r = null;
    model.traverse(o => { if (!r && o.name && glbCleanKey(o.name) === cn) r = o; });
    return r;
  }
  // ---- build a minimal procedural her3 rig mirroring chars3d joint paths ----
  function bone(name, parent, px, py, pz) {
    const o = new THREE.Object3D(); o.name = name;
    if (parent) parent.add(o); else scene3d.add(o);
    if (px !== undefined) o.position.set(px, py, pz);
    return o;
  }
  const scene3d = new THREE.Scene();
  const root = bone('root');
  const hips = bone('hips', root, 0, 0, 0);
  const torso = bone('torso', root, 0, 0.10, 0);
  const chest = bone('chest', torso, 0, 0.20, 0);
  const neck = bone('neck', chest, 0, 0.12, 0);
  const head = bone('head', neck, 0, 0.10, 0);
  const breastL = bone('breastL', chest, -0.13, 0.06, 0.10);
  const breastR = bone('breastR', chest, 0.13, 0.06, 0.10);
  const armL = { shoulder: bone('armL.shoulder', root, -0.18, 0.34, 0),
                 elbow: bone('armL.elbow', null), hand: bone('armL.hand', null) };
  armL.shoulder.add(armL.elbow); armL.elbow.add(armL.hand);
  const armR = { shoulder: bone('armR.shoulder', root, 0.18, 0.34, 0),
                 elbow: bone('armR.elbow', null), hand: bone('armR.hand', null) };
  armR.shoulder.add(armR.elbow); armR.elbow.add(armR.hand);
  const legL = { hip: bone('legL.hip', root, -0.09, 0, 0),
                 knee: bone('legL.knee', null), foot: bone('legL.foot', null) };
  legL.hip.add(legL.knee); legL.knee.add(legL.foot);
  const legR = { hip: bone('legR.hip', root, 0.09, 0, 0),
                 knee: bone('legR.knee', null), foot: bone('legR.foot', null) };
  legR.hip.add(legR.knee); legR.knee.add(legR.foot);
  const shaftRoot = bone('shaftRoot', hips, 0, -0.05, 0.05);
  const shaft = bone('shaft', shaftRoot);
  const glans = bone('glans', shaft);
  const face = { ctx: {}, tex: { needsUpdate: false } };
  const hair = bone('hair', head);
  her3 = { root, hips, torso, chest, neck, head, breastL, breastR, armL, armR, legL, legR, shaftRoot, shaft, glans, face, hair };
  him3 = { root: bone('himRoot'), hips: bone('himHips', her3.root), chest: bone('himChest'),
           armL: { shoulder: bone('hS'), elbow: bone('hE'), hand: bone('hH') },
           legL: { knee: bone('hk'), foot: bone('hf') }, legR: { knee: bone('hk2'), foot: bone('hf2') },
           head: bone('himHead'), shaftRoot: bone('himShaft'), shaft: bone('himShaft2'), glans: bone('himGlans'),
           fpvHide: [] };
  globalThis.her3 = her3; globalThis.him3 = him3;

  // ---- load the GLB ----
  const buf = fs.readFileSync(${JSON.stringify(GLB_PATH)});
  const ab = buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);
  const loader = new GLTFLoader();
  const gltf = await new Promise((res, rej) => loader.parse(ab, '', res, rej));
  const model = gltf.scene;
  const e = { model, rig: null, meshList: [], baseScale: 1, pelvisLocal: null,
              tailMeshes: [], earMeshes: [], jiggleY: 0, calm: 0.5, basePos: new THREE.Vector3() };
  glbBuildRig(e);
  scene3d.add(model);
  // place: compute pelvis-local + baseScale manually (glbPlaceEntry mutates
  // GLB_STORE, not our entry e)
  e.baseScale = 1.0;
  const pb = findBone(model, '下半身');
  if (pb) {
    pb.updateWorldMatrix(true, false);
    const pl = new THREE.Vector3().setFromMatrixPosition(pb.matrixWorld);
    model.worldToLocal(pl);
    e.pelvisLocal = pl;
  }

  // helper: drive one frame of the GLB from a her3 pose config
  function driveFrame(cfgHer, dt) {
    applyRig3(her3, cfgHer, dt, 1, null);
    _smPose3.her = JSON.parse(JSON.stringify(cfgHer));
    her3.root.updateMatrixWorld(true);
    glbDriveRig(e, dt);
    glbFollowHer3(e, dt, 'goatchan');
    model.updateMatrixWorld(true);
  }

  function seedHand(key, worldTarget) {
    _aimSm3[key].copy(worldTarget);
    _aimSm3.age[key] = (_aimSm3.age[key] || 0) + 1; // fresh -> IK-owned this frame
  }

  function measureFoot(side) {
    const knee = findBone(model, 'ひざ.' + side);
    const ankle = findBone(model, '足首.' + side);
    const toe = findBone(model, 'つま先.' + side);
    if (!knee || !ankle || !toe) return null;
    const kW = wp(knee), aW = wp(ankle), tW = wp(toe);
    const up = kW.clone().sub(aW);     // ankle -> knee (leg axis, points up)
    const foot = tW.clone().sub(aW);   // ankle -> toe (foot axis)
    const ankleBend = Math.acos(clamp(up.dot(foot) / ((up.length() * foot.length()) || 1), -1, 1)) * DEG;
    // knee interior: thigh(hip->knee) vs shin(knee->ankle)
    const hip = findBone(model, '足.' + side);
    let kneeBend = null;
    if (hip) {
      const thigh = kW.clone().sub(wp(hip));
      const shinD = aW.clone().sub(kW);
      kneeBend = Math.acos(clamp(thigh.dot(shinD) / ((thigh.length() * shinD.length()) || 1), -1, 1)) * DEG;
    }
    return { ankleBend, kneeBend, footLen: foot.length(), shinLen: up.length() };
  }

  // intrinsic hand metrics (model-local, orientation independent)
  function handBBox(side) {
    const wrist = findBone(model, '手首.' + side);
    const names = ['中指０.' + side, '中指３.' + side, '人指０.' + side, '人指３.' + side,
                   '小指０.' + side, '小指３.' + side, '親指０.' + side, '親指３.' + side];
    const box = new THREE.Box3();
    if (wrist) box.expandByPoint(wp(wrist));
    names.forEach(n => { const b = findBone(model, n); if (b) box.expandByPoint(wp(b)); });
    if (box.isEmpty()) return null;
    const s = new THREE.Vector3(); box.getSize(s);
    return { x: s.x, y: s.y, z: s.z, vol: s.x * s.y * s.z };
  }
  function handIntrinsic(side) {
    const wrist = findBone(model, '手首.' + side);
    const mid0 = findBone(model, '中指０.' + side), mid3 = findBone(model, '中指３.' + side);
    const ind0 = findBone(model, '人指０.' + side), pin0 = findBone(model, '小指０.' + side);
    if (!wrist || !mid0 || !mid3 || !ind0 || !pin0) return null;
    const len = wp(mid3).distanceTo(wp(wrist));          // hand length
    const width = wp(pin0).distanceTo(wp(ind0));          // palm spread width
    const midBase = wp(mid0).distanceTo(wp(wrist));
    return { len, width, midBase, ratio: width / (len || 1) };
  }
  function fingerCurl(side) {
    const seg = ['中指０.' + side, '中指１.' + side, '中指２.' + side, '中指３.' + side].map(n => findBone(model, n));
    if (seg.some(s => !s)) return null;
    const a = wp(seg[0]), b = wp(seg[1]), c = wp(seg[2]), d = wp(seg[3]);
    const v1 = b.clone().sub(a), v2 = c.clone().sub(b), v3 = d.clone().sub(c);
    const ang = (u, v) => Math.acos(clamp(u.dot(v) / ((u.length() * v.length()) || 1), -1, 1)) * DEG;
    return { j1: ang(v1, v2), j2: ang(v2, v3) };
  }

  const results = {};
  function computeAimTgt(hk, ms) {
    const tgt = _aimSm3[hk];
    const task = GLB_HAND3[ms];
    const land = task && task.land;
    if (land && e.rig.land && e.rig.land.pelvis) {
      let hb = null, mb = null, off = null;
      if (land === 'mons') { mb = e.rig.land.pelvis; off = GLB_LAND_OFF.mons; hb = clitorisWorld3(her3); }
      else if (land === 'breast') {
        const leftish = tgt.x < 0; mb = leftish ? e.rig.land.breastR : e.rig.land.breastL; off = GLB_LAND_OFF.breast;
        hb = breastWorld3(her3, leftish ? -1 : 1);
      }
      if (hb && mb) {
        mb.updateWorldMatrix(true, false);
        const out = new THREE.Vector3().setFromMatrixPosition(mb.matrixWorld);
        out.x += off[0]; out.y += off[1]; out.z += off[2];
        out.sub(hb).add(tgt);
        return out;
      }
    }
    return tgt.clone();
  }
  function wristGap(side, land) {
    const wrist = findBone(model, '手首.' + side);
    let hb = null, mb = null, off = null;
    if (land === 'mons') { mb = e.rig.land.pelvis; off = GLB_LAND_OFF.mons; hb = clitorisWorld3(her3); }
    else if (land === 'breast') { mb = e.rig.land.breastL; off = GLB_LAND_OFF.breast; hb = breastWorld3(her3, -1); }
    if (hb && mb) {
      mb.updateWorldMatrix(true, false);
      const aimTgt = new THREE.Vector3().setFromMatrixPosition(mb.matrixWorld);
      aimTgt.x += off[0]; aimTgt.y += off[1]; aimTgt.z += off[2];
      aimTgt.sub(hb).add(_aimSm3[side === 'L' ? 'herR' : 'herL']);
      return wrist.getWorldPosition(new THREE.Vector3()).distanceTo(aimTgt);
    }
    return null;
  }
  function scenario(name, cfgHer, opts) {
    opts = opts || {};
    G.solo = !!opts.solo; G.spaceHeld = !!opts.rub; G.rubZone = opts.zone || 3;
    G.rub = opts.rub ? 1 : 0;
    _glbArmAge.herL = -1; _glbArmAge.herR = -1;
    _aimSm3.init = {}; _aimSm3.age = {}; _aimSm3.drv = {};
    driveFrame(cfgHer, 0);
    for (let i = 0; i < 120; i++) driveFrame(cfgHer, 1 / 60);
    if (opts.rub) {
      // mimic updateAnim3's per-frame rub seeding: keep the IK target fresh
      // (age increments every frame so _glbArmIK stays owned) and slew it.
      for (let i = 0; i < 150; i++) {
        const v = clitorisWorld3(her3);
        _aimSm3.herR.copy(v); _aimSm3.age.herR = (_aimSm3.age.herR || 0) + 0.1;
        const b = breastWorld3(her3, -1);
        _aimSm3.herL.copy(b); _aimSm3.age.herL = (_aimSm3.age.herL || 0) + 0.1;
        driveFrame(cfgHer, 1 / 60);
      }
    }
    const footL = measureFoot('L'), footR = measureFoot('R');
    const hbL = handBBox('L'), hbR = handBBox('R');
    const hiL = handIntrinsic('L'), hiR = handIntrinsic('R');
    const fcL = fingerCurl('L'), fcR = fingerCurl('R');
    let gapR = null, gapL = null, reach = null, dist = null;
    if (opts.rub) {
      // herR -> MMD L (caress mons), herL -> MMD R (cup breast)
      gapR = wristGap('L', 'mons');
      gapL = wristGap('R', 'breast');
      const shL = findBone(model, '腕.L');
      const aT = computeAimTgt('herR', 'L');
      reach = e.rig.arms.L.upper + e.rig.arms.L.fore;
      dist = shL.getWorldPosition(new THREE.Vector3()).distanceTo(aT);
      if (name === 'SOLO-rub') {
        const wristL = findBone(model, '手首.L').getWorldPosition(new THREE.Vector3());
        const elbowL = findBone(model, 'ひじ.L').getWorldPosition(new THREE.Vector3());
        console.log('    DBG aimTgt=' + aT.toArray().map(v => v.toFixed(2)) +
          ' wrist=' + wristL.toArray().map(v => v.toFixed(2)) +
          ' elbow=' + elbowL.toArray().map(v => v.toFixed(2)) +
          ' sh=' + shL.getWorldPosition(new THREE.Vector3()).toArray().map(v => v.toFixed(2)));
        console.log('    DBG elbowBind=' + e.rig.arms.L.el.position.toArray().map(v => v.toFixed(3)) +
          ' wrBind=' + e.rig.arms.L.wr.position.toArray().map(v => v.toFixed(3)) +
          ' upper=' + e.rig.arms.L.upper.toFixed(3) + ' fore=' + e.rig.arms.L.fore.toFixed(3));
      }
    }
    results[name] = { footL, footR, hbL, hbR, hiL, hiR, fcL, fcR, gapR, gapL,
      hand3: JSON.parse(JSON.stringify(GLB_HAND3)),
      footTune: JSON.parse(JSON.stringify(GLB_FOOT_TUNE)) };
    const f = x => x == null ? '   -- ' : (x >= 0 ? ' ' : '') + x.toFixed(0);
    const fg = x => x == null ? '  --  ' : x.toFixed(2);
    console.log('  ' + name.padEnd(14) +
      ' aBendL=' + f(footL && footL.ankleBend) + ' aBendR=' + f(footR && footR.ankleBend) +
      ' kBendL=' + f(footL && footL.kneeBend) +
      ' HlenL=' + (hiL ? hiL.len.toFixed(2) : '--') + ' HwidL=' + (hiL ? hiL.width.toFixed(2) : '--') + ' HratL=' + (hiL ? hiL.ratio.toFixed(2) : '--') +
      ' Hbox.z=' + (hbL ? hbL.z.toFixed(2) : '--') +
      ' gapR=' + fg(gapR) + ' gapL=' + fg(gapL) +
      ' reach=' + (reach != null ? reach.toFixed(2) : '--') + ' dist=' + (dist != null ? dist.toFixed(2) : '--') +
      ' curl.j1=' + (fcL ? fcL.j1.toFixed(0) : '--'));
  }

  const DO_SCEN = [
    ['SOLO-idle', SOLO_POSE3.her, { solo: true }],
    ['SOLO-rub', SOLO_POSE3.her, { solo: true, rub: true, zone: 3 }],
    ['MISSIONARY', POSES3[0].her, {}],
    ['DOGGY', POSES3[2].her, {}],
    ['COWGIRL', POSES3[4].her, {}],
    ['LEGS-UP', POSES3[1].her, {}],
    ['PRONE', POSES3[3].her, {}],
    ['SPOON', POSES3[6].her, {}]
  ];

  const sweep = (process.argv && process.argv.indexOf('sweep') >= 0) || (globalThis.__sweep);
  if (sweep) {
    console.log('MODEL scale=' + e.baseScale.toFixed(3));
    console.log('RIG bones=' + e.rig.bones.length + ' fingers=' + e.rig.fingers.length + ' arms=' + (e.rig.arms ? Object.keys(e.rig.arms).length : 0));
    const ankle0s = [-2.4, -2.0, -1.6, -1.2, -0.9, -0.6, -0.3, 0.0];
    const scales = [1.0, 1.2, 1.35, 1.5];
    console.log('ankle0 sweep (SOLO idle + MISSIONARY), hands.scale sweep on SOLO:');
    for (const a0 of ankle0s) {
      GLB_FOOT_TUNE.ankle0 = a0;
      const row = [];
      for (const [nm, cfg, o] of DO_SCEN) {
        // solo poses matter most for the rest look
        if (nm !== 'SOLO-idle' && nm !== 'MISSIONARY') continue;
        G.solo = !!o.solo;
        _glbArmAge.herL = -1; _glbArmAge.herR = -1;
        _aimSm3.init = {}; _aimSm3.age = {}; _aimSm3.drv = {};
        driveFrame(cfg, 0); for (let i = 0; i < 60; i++) driveFrame(cfg, 1 / 60);
        const fl = measureFoot('L');
        row.push(nm + ':aB=' + (fl ? fl.ankleBend.toFixed(0) : '?'));
      }
      console.log('  ankle0=' + a0.toFixed(2).padStart(5) + '  ' + row.join('  '));
    }
    console.log('hands.scale sweep (SOLO idle):');
    GLB_FOOT_TUNE.ankle0 = -1.6;
    for (const sc of scales) {
      glbSetScale('hands', sc);
      _glbArmAge.herL = -1; _glbArmAge.herR = -1;
      _aimSm3.init = {}; _aimSm3.age = {}; _aimSm3.drv = {};
      driveFrame(SOLO_POSE3.her, 0); for (let i = 0; i < 60; i++) driveFrame(SOLO_POSE3.her, 1 / 60);
      const hi = handIntrinsic('L'), hb = handBBox('L');
      console.log('  scale=' + sc.toFixed(2) + '  Hlen=' + (hi ? hi.len.toFixed(2) : '?') + ' Hwid=' + (hi ? hi.width.toFixed(2) : '?') + ' Hrat=' + (hi ? hi.ratio.toFixed(2) : '?') + ' Hbox.z=' + (hb ? hb.z.toFixed(2) : '?'));
    }
    return;
  }

  console.log('MODEL scale baseScale=' + e.baseScale.toFixed(3) + ' pelvisLocal=' + (e.pelvisLocal ? e.pelvisLocal.toArray().map(v => v.toFixed(2)).join(',') : '?'));
  console.log('RIG bones=' + e.rig.bones.length + ' fingers=' + e.rig.fingers.length + ' arms=' + (e.rig.arms ? Object.keys(e.rig.arms).length : 0));
  for (const [nm, cfg, o] of DO_SCEN) scenario(nm, cfg, o);
  console.log('--- VERDICT (foot 70-140deg, handLen>=0.12, rub gapR<=0.12 gapL<=0.20) ---');
  let passAll = true;
  for (const [nm, cfg, o] of DO_SCEN) {
    const r = results[nm]; if (!r) continue;
    const aB = r.footL && r.footL.ankleBend;
    const okFoot = aB != null && aB >= 70 && aB <= 140;
    const hlen = r.hiL && r.hiL.len;
    const okHand = hlen != null && hlen >= 0.12;
    let okGap = true;
    if (o.rub) okGap = (r.gapR == null || r.gapR <= 0.12) && (r.gapL == null || r.gapL <= 0.20);
    const ok = okFoot && okHand && okGap; if (!ok) passAll = false;
    console.log('  ' + nm.padEnd(14) + (ok ? 'PASS' : 'FAIL') +
      ' foot=' + (aB != null ? aB.toFixed(0) : '?') +
      ' hand=' + (hlen != null ? hlen.toFixed(2) : '?') +
      (o.rub ? (' gapR=' + (r.gapR != null ? r.gapR.toFixed(2) : '?') + ' gapL=' + (r.gapL != null ? r.gapL.toFixed(2) : '?')) : ''));
  }
  console.log(passAll ? 'ALL PASS' : 'SOME FAIL');
  globalThis.__results = results;
}
`;

const src = files.join('\n') + '\n' + harnessFn;
vm.runInThisContext(src, { filename: 'harness-bundle.js' });
await runHarness();
console.log('DONE');
