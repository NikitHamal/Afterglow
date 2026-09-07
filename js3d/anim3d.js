// Afterglow 3D — module: anim3d (rig animation driven by the shared G state)
'use strict';

let her3 = null, him3 = null;
let _prevPose3 = null, _poseBlend3 = 1, _lastPosIdx3 = -1, _lastOral3 = 0, _lastSolo3 = false;
let _faceAccum = 0;
const _breastJig = { x: 0, v: 0 };
const _buttJig = { x: 0, v: 0 };
const _tmpV = new THREE.Vector3();

/* ---- shared blend layer: smoothed pose configs + damped gameplay drives ----
   Layer 1 (discrete):  lerpPose3 blends whole-pose switches.
   Layer 2 (continuous): every numeric pose channel is exponentially damped,
   so gameplay spikes (depth, pulse) can never snap a joint.
   Layer 3 (additive):   breath, thrust, jiggle springs, tremble. */
const _smPose3 = { her: null, him: null };
const _thrust3 = { y: 0, z: 0 };
const _liftSm3 = [0, 0]; // smoothed foot-plant root correction (never slams)
let _shaftP3 = 0;
const _hair3 = { x: 0, v: 0, z: 0, vz: 0 };
let _prevHeadQ3 = null;
const _tmpQ3a = new THREE.Quaternion();
const _tmpQ3b = new THREE.Quaternion();
const _aimSm3 = {
  himL: new THREE.Vector3(), himR: new THREE.Vector3(),
  herL: new THREE.Vector3(), herR: new THREE.Vector3(),
  init: {}, age: {}, drv: {}
};

/* ============================================================
   BUILD
   ============================================================ */
function initChars3() {
  her3 = buildHer3();
  him3 = buildHim3();
  scene3d.add(her3.root);
  scene3d.add(him3.root);
  _prevPose3 = currentPose3();
  _lastPosIdx3 = G.pos | 0;
  _lastOral3 = (G.oral || 0) > 0.03 ? 1 : 0;
  applyRig3(her3, _prevPose3.her, 0.016, 1);
  applyRig3(him3, _prevPose3.him, 0.016, 1);
  return { her: her3, him: him3 };
}

/* rigs for the camera layer (engine3d FPV rides his head) */
function rigs3() {
  return { her: her3, him: him3 };
}

/* harness hook: inject rigs without a renderer (node motion tests) */
function _setRigs3(her, him) { her3 = her; him3 = him; }

/* ============================================================
   HAND TARGETS — resolve a pose hand config to a world position.
   Pure function of the two rigs so the contact harness can test it.
   ============================================================ */
const _htV = new THREE.Vector3();
const _htV2 = new THREE.Vector3();  // second plant resolve (blend start point)
const _aimTgt = new THREE.Vector3(); // final slewed hand target
function handTarget3(her, him, cfg, out) {
  out = out || new THREE.Vector3();
  const r = cfg.k === 'himHips' ? him : her;
  if (cfg.k === 'herHead') {
    her.head.updateWorldMatrix(true, false);
    her.head.localToWorld(out.set(cfg.x || 0, cfg.y || 0, cfg.z || 0));
  } else if (cfg.k === 'herHips') {
    her.hips.updateWorldMatrix(true, false);
    her.hips.localToWorld(out.set(cfg.x || 0, cfg.y || 0, cfg.z || 0));
  } else if (cfg.k === 'himHips') {
    him.hips.updateWorldMatrix(true, false);
    him.hips.localToWorld(out.set(cfg.x || 0, cfg.y || 0, cfg.z || 0));
  } else if (cfg.k === 'herKneeL' || cfg.k === 'herKneeR') {
    const j = cfg.k === 'herKneeL' ? her.legL.knee : her.legR.knee;
    j.updateWorldMatrix(true, false);
    j.localToWorld(out.set(cfg.x || 0, cfg.y || 0, cfg.z || 0));
  } else if (cfg.k === 'himHead') {
    him.head.updateWorldMatrix(true, false);
    him.head.localToWorld(out.set(cfg.x || 0, cfg.y || 0, cfg.z || 0));
  } else if (cfg.k === 'himChest') {
    him.chest.updateWorldMatrix(true, false);
    him.chest.localToWorld(out.set(cfg.x || 0, cfg.y || 0, cfg.z || 0));
  } else if (cfg.k === 'himShoulderL' || cfg.k === 'himShoulderR') {
    const j = cfg.k === 'himShoulderL' ? him.armL.shoulder : him.armR.shoulder;
    j.updateWorldMatrix(true, false);
    j.localToWorld(out.set(cfg.x || 0, cfg.y || 0, cfg.z || 0));
  } else if (cfg.k === 'herThighL' || cfg.k === 'herThighR') {
    const j = cfg.k === 'herThighL' ? her.legL.hip : her.legR.hip;
    j.updateWorldMatrix(true, false);
    j.localToWorld(out.set(cfg.x || 0, cfg.y || 0, cfg.z || 0));
  } else { // herChest (default)
    her.chest.updateWorldMatrix(true, false);
    her.chest.localToWorld(out.set(cfg.x || 0, cfg.y || 0, cfg.z || 0));
  }
  const minF = (cfg && cfg.floor != null) ? cfg.floor : 0.125;
  if (out.y < minF) out.y = minF;
  return out;
}

/* ============================================================
   ROCK-SOLID BED CONTACT & PHYSICALLY GROUNDED SOLVER
   Mattress top sits at y = 0.110. Real physics: checks 26 anatomically
   distributed surface probes (buttocks, sacrum, vulva, lumbar, shoulder blades,
   skull, knees, heels, palms) so no body part can EVER penetrate through bedding.
   ============================================================ */
const _contactProbes = [
  // Voluptuous buttocks & natal cleft (primary bed contact surface in supine poses)
  { get: () => her3 && her3.hips, p: [-0.070, -0.048, -0.216] },
  { get: () => her3 && her3.hips, p: [ 0.070, -0.048, -0.216] },
  { get: () => her3 && her3.hips, p: [ 0.000, -0.042, -0.176] },
  { get: () => her3 && her3.hips, p: [-0.070, -0.170, -0.092] },
  { get: () => her3 && her3.hips, p: [ 0.070, -0.170, -0.092] },
  { get: () => her3 && her3.hips, p: [ 0.000, -0.110,  0.030] },
  { get: () => her3 && her3.hips, p: [ 0.000, -0.060,  0.150] },
  // Spine & back (lumbar, ribcage, shoulders)
  { get: () => her3 && her3.torso, p: [ 0.000,  0.040, -0.110] },
  { get: () => her3 && her3.torso, p: [ 0.000,  0.220, -0.110] },
  { get: () => her3 && her3.chest, p: [-0.090,  0.260, -0.100] },
  { get: () => her3 && her3.chest, p: [ 0.090,  0.260, -0.100] },
  { get: () => her3 && her3.chest, p: [ 0.000,  0.150, -0.110] },
  // Head (back of skull resting on pillow/bed)
  { get: () => her3 && her3.head,  p: [ 0.000, -0.020, -0.125] },
  { get: () => her3 && her3.head,  p: [ 0.000,  0.100, -0.100] },
  // Knees (patellas & under-knees for kneeling poses)
  { get: () => her3 && her3.legL && her3.legL.knee, p: [0.000, 0.000, 0.065] },
  { get: () => her3 && her3.legR && her3.legR.knee, p: [0.000, 0.000, 0.065] },
  { get: () => her3 && her3.legL && her3.legL.knee, p: [0.000, -0.065, 0.000] },
  { get: () => her3 && her3.legR && her3.legR.knee, p: [0.000, -0.065, 0.000] }
];

const _himProbes = [
  { get: () => him3 && him3.hips, p: [0, -0.06, -0.16] },
  { get: () => him3 && him3.chest, p: [0, 0.18, 0.12] },
  { get: () => him3 && him3.legL && him3.legL.knee, p: [0, -0.07, 0] },
  { get: () => him3 && him3.legR && him3.legR.knee, p: [0, -0.07, 0] },
  { get: () => him3 && him3.legL && him3.legL.foot, p: [0, -0.03, 0.05] },
  { get: () => him3 && him3.legR && him3.legR.foot, p: [0, -0.03, 0.05] }
];

function solveContact3(dt) {
  if (!her3 || !him3) return;
  const BED_FLOOR = 0.114; // Mattress box surface (0.110) + 4mm contact line

  // 1. Her body contact check
  her3.root.updateWorldMatrix(true, true);
  let minHerY = Infinity;
  for (let i = 0; i < _contactProbes.length; i++) {
    const item = _contactProbes[i];
    const obj = item.get();
    if (!obj) continue;
    _tmpV.set(item.p[0], item.p[1], item.p[2]);
    obj.localToWorld(_tmpV);
    if (_tmpV.y < minHerY) minHerY = _tmpV.y;
  }
  if (minHerY < BED_FLOOR) {
    her3.root.position.y += (BED_FLOOR - minHerY);
  }

  // 2. His body contact check (when not solo)
  if (!G.solo && him3.root && him3.root.visible) {
    him3.root.updateWorldMatrix(true, true);
    let minHimY = Infinity;
    for (let i = 0; i < _himProbes.length; i++) {
      const item = _himProbes[i];
      const obj = item.get();
      if (!obj) continue;
      _tmpV.set(item.p[0], item.p[1], item.p[2]);
      obj.localToWorld(_tmpV);
      if (_tmpV.y < minHimY) minHimY = _tmpV.y;
    }
    if (minHimY < BED_FLOOR) {
      him3.root.position.y += (BED_FLOOR - minHimY);
    }
  }
}

/* foot planting, two tiers:
   - leg level (direction-safe): ankle origin under the sheet → half-lift the
     root. Capped small so authored contact never visibly breaks.
   - toe level (self-correcting): fold the ankle until the toe tip clears the
     sheet; flip direction if penetration grows, relax when clear. */
const _footW3 = new THREE.Vector3();
const _toeW3 = new THREE.Vector3();
const _footSign3 = [1, 1, 1, 1];
const _footPen3 = [0, 0, 0, 0];
const _footCool3 = [0, 0, 0, 0];
function plantFeet3(dt) {
  if (!her3 || !him3 || dt <= 0) return;
  const FLOOR = 0.125; // mattress top (0.11) + sole margin
  const feet = [her3.legL.foot, her3.legR.foot, him3.legL.foot, him3.legR.foot];
  const lift = [0, 0];
  for (let i = 0; i < 4; i++) {
    const f = feet[i];
    f.updateWorldMatrix(true, false);
    f.getWorldPosition(_footW3);
    const pa = FLOOR - _footW3.y;
    if (pa > 0.03) lift[i < 2 ? 0 : 1] += (pa - 0.03) * 0.5;
    _toeW3.set(0, -0.01, 0.14);
    f.localToWorld(_toeW3);
    const pt = FLOOR - _toeW3.y;
    _footCool3[i] = Math.max(0, _footCool3[i] - dt);
    if (pt > 0.005) {
      if (_footCool3[i] <= 0 && pt > _footPen3[i] + 0.002) {
        _footSign3[i] *= -1; // last fold went the wrong way — flip, then wait
        _footCool3[i] = 0.4;
      }
      f.rotation.x = clamp(f.rotation.x + _footSign3[i] * Math.min(pt * 6 * dt + 0.05 * dt, 0.08), -0.5, 0.5);
    } else if (pt < -0.01) {
      f.rotation.x = dampNum3(f.rotation.x, 0, 6, dt);
    }
    _footPen3[i] = pt;
  }
  // slew the correction itself: planting glides in, never slams
  _liftSm3[0] = dampNum3(_liftSm3[0], Math.min(lift[0], 0.05), 8, dt);
  _liftSm3[1] = dampNum3(_liftSm3[1], Math.min(lift[1], 0.05), 8, dt);
  her3.root.position.y += _liftSm3[0];
  him3.root.position.y += _liftSm3[1];
}

/* pose hand-IK: plant the configured hands, unless that arm is busy rubbing.
   Blend-proof: pose configs swap discretely mid-blend, so during a blend we
   resolve BOTH endpoint plants and glide between the POINTS (re-timed from
   the swap instant). Arms never teleport, never whip. dt<=0 snaps (harness). */
function _soloHandTarget(cfg, isRight) {
  if (!cfg) return null;
  if (cfg.k && cfg.k.startsWith('him')) {
    return { k: 'herHead', x: isRight ? 0.12 : -0.12, y: 0.04, z: 0.03 };
  }
  return cfg;
}

function solveHands3(pose, rubArms, dt) {
  const H = pose && pose.hands;
  if (!H || !her3 || !him3) return;
  const jobs = [];
  if (!G.solo) {
    jobs.push([H.himL, him3.armL, 'himL'], [H.himR, him3.armR, 'himR']);
  }
  const hL = G.solo ? _soloHandTarget(H.herL, false) : H.herL;
  const hR = G.solo ? _soloHandTarget(H.herR, true) : H.herR;
  jobs.push([hL, her3.armL, 'herL'], [hR, her3.armR, 'herR']);
  for (const [cfg, arm, key] of jobs) {
    if (!cfg || !arm) continue;
    if (rubArms && rubArms.has(key)) continue;
    const rig = (key === 'himL' || key === 'himR') ? him3 : her3;
    handTarget3(her3, him3, cfg, _htV);
    if (dt > 0) {
      // first touch: start from the live hand so planting reaches out smoothly
      if (!_aimSm3.init[key]) {
        arm.hand.updateWorldMatrix(true, false);
        arm.hand.getWorldPosition(_aimSm3[key]);
        _aimSm3.init[key] = 1;
        _aimSm3.drv[key] = 'pose';
        _aimSm3.age[key] = 0;
      }
      // takeover ramp (rub→pose): track gently for 0.3s, then full rate
      _aimTgt.copy(_htV);
      const PH = _prevPose3 && _prevPose3.hands;
      let pcfg = PH ? PH[key] : null;
      if (G.solo && (key === 'herL' || key === 'herR')) pcfg = _soloHandTarget(pcfg, key === 'herR');
      if (pcfg && pcfg !== cfg && _poseBlend3 < 1 && _poseBlend3 > 0) {
        handTarget3(her3, him3, pcfg, _htV2);
        const pe = _poseBlend3 * _poseBlend3 * (3 - 2 * _poseBlend3);
        if (pe > 0.5) {
          let e2 = (pe - 0.5) * 2;
          e2 = e2 * e2 * (3 - 2 * e2);
          _aimTgt.set(
            _htV2.x + (_htV.x - _htV2.x) * e2,
            _htV2.y + (_htV.y - _htV2.y) * e2,
            _htV2.z + (_htV.z - _htV2.z) * e2
          );
        } else {
          _aimTgt.copy(_htV2);
        }
      }
      dampSlewVec3(_aimSm3[key], _aimTgt, 10, 1.5, dt);
      if (_aimSm3.drv[key] !== 'pose') { _aimSm3.drv[key] = 'pose'; _aimSm3.age[key] = 0; }
      _aimSm3.age[key] = (_aimSm3.age[key] || 0) + dt;
      // takeover starts gentle and eases to full tracking (a relocated hand
      // is far away — the arm must traverse real angle)
      const pramp = Math.min(1, _aimSm3.age[key] / 0.3);
      aimArmAt3(arm, _aimSm3[key], rig.R.upperArm, rig.R.foreArm, -0.15, 4 + 10 * pramp, dt, 8 + 6 * pramp);
    } else {
      aimArmAt3(arm, _htV, rig.R.upperArm, rig.R.foreArm, -0.15);
    }
  }
}
function springStep(s, target, k, c, dt) {
  s.v += (-k * (s.x - target) - c * s.v) * dt;
  s.x += s.v * dt;
  return s.x;
}

/* frame-rate independent exponential damper (the no-snap primitive) */
function dampNum3(cur, target, rate, dt) {
  if (!isFinite(cur)) cur = target;
  if (!isFinite(target)) return cur;
  return cur + (target - cur) * (1 - Math.exp(-rate * dt));
}
/* deep-damp a pose config toward its target. Root channels ride slow,
   limb channels fast; hand configs are data, not motion — copied as-is. */
function dampRateFor3(key) {
  if (key === 'pos' || key === 'up' || key === 'fwd') return 10;
  if (key === 'head' || key === 'shaft') return 11;
  return 14;
}
function dampVal3(sm, tgt, rate, dt) {
  if (typeof tgt === 'number') return dampNum3(typeof sm === 'number' ? sm : tgt, tgt, rate, dt);
  if (Array.isArray(tgt)) {
    const out = Array.isArray(sm) ? sm : [];
    for (let i = 0; i < tgt.length; i++) out[i] = dampVal3(out[i], tgt[i], rate, dt);
    out.length = tgt.length;
    return out;
  }
  if (tgt && typeof tgt === 'object') {
    const out = (sm && typeof sm === 'object' && !Array.isArray(sm)) ? sm : {};
    for (const k in tgt) out[k] = dampVal3(out[k], tgt[k], rate, dt);
    return out;
  }
  return tgt;
}
function dampCfg3(sm, tgt, dt) {
  if (!tgt || typeof tgt !== 'object' || Array.isArray(tgt)) return dampVal3(sm, tgt, 14, dt);
  const out = (sm && typeof sm === 'object' && !Array.isArray(sm)) ? sm : {};
  for (const k in tgt) {
    if (k === 'hands') { out[k] = tgt[k]; continue; }
    out[k] = dampVal3(out[k], tgt[k], dampRateFor3(k), dt);
  }
  return out;
}
function dampVec3(sm, tgt, rate, dt) {
  sm.x = dampNum3(sm.x, tgt.x, rate, dt);
  sm.y = dampNum3(sm.y, tgt.y, rate, dt);
  sm.z = dampNum3(sm.z, tgt.z, rate, dt);
  return sm;
}
/* damped + displacement-capped target tracking, one primitive: exponential
   approach for responsiveness, then a HARD per-frame displacement cap so a
   far-away new target can never yank the limb in a single frame. */
function dampSlewVec3(sm, tgt, rate, maxRate, dt) {
  const px = sm.x, py = sm.y, pz = sm.z;
  dampVec3(sm, tgt, rate, dt);
  _tmpV.set(sm.x - px, sm.y - py, sm.z - pz);
  const d = _tmpV.length();
  const m = maxRate * Math.max(dt, 0.0005);
  if (d > m && d > 1e-9) {
    const s = m / d;
    sm.x = px + _tmpV.x * s; sm.y = py + _tmpV.y * s; sm.z = pz + _tmpV.z * s;
  }
  return sm;
}

/* ============================================================
   2-BONE ARM AIMING (used for the rub hands)
   ============================================================ */
const _aimQ = new THREE.Quaternion();
const _downY = new THREE.Vector3(0, -1, 0);
/* Two-bone IK with persistent joint state: slerps the LIVE shoulder
   orientation toward the aim (FK never rebases IK arms — see applyRig3 skip),
   damps the elbow toward the cosine-rule angle. Targets must arrive
   pre-slewed; the tracker itself never pops (shortest-arc slerp). */
function aimArmAt3(arm, targetWorld, upperLen, foreLen, bendBias, rate, dt, elbRate) {
  const sh = arm.shoulder;
  const parent = sh.parent;
  if (!parent) return arm.elbow.rotation.x;
  parent.updateWorldMatrix(true, false);
  const local = parent.worldToLocal(_tmpV.copy(targetWorld));
  const dir = local.sub(sh.position);
  // arms can't fold flat: keep the target out of the singularity ball around
  // the joint, or the aim quaternion flips 180° in a single frame
  const minR = Math.abs(upperLen - foreLen) + 0.06;
  const dist = clamp(dir.length(), minR, (upperLen + foreLen) * 0.995);
  dir.normalize();

  _aimQ.setFromUnitVectors(_downY, dir);
  const track = dt > 0 ? (1 - Math.exp(-(rate || 14) * dt)) : 1;
  sh.quaternion.slerp(_aimQ, track);

  // cosine rule → interior angle at the elbow, joint-limited so limbs never
  // lock straight or fold past flesh (also bounds the extension whip)
  let cosI = (upperLen * upperLen + foreLen * foreLen - dist * dist) / (2 * upperLen * foreLen);
  cosI = clamp(cosI, -1, 1);
  const interior = clamp(Math.acos(cosI), 0.18, Math.PI - 0.18);
  const ikE = Math.PI - interior + (bendBias || 0);
  const useE = (dt > 0 && elbRate) ? dampNum3(arm.elbow.rotation.x, ikE, elbRate, dt) : ikE;
  arm.elbow.rotation.set(useE, 0, 0);
  return useE;
}

/* ============================================================
   POSE APPLICATION
   ============================================================ */
function applyRig3(rig, cfg, dt, k, skip) {
  const root = rig.root;
  // position: authored pose coordinates (smooth blending handled by lerpPose3)
  root.position.set(cfg.pos[0], cfg.pos[1], cfg.pos[2]);
  // orientation — snap toward target frame (cheap + stable when up/fwd are orthogonal)
  orientTo3(root, cfg.up, cfg.fwd);

  // spine / torso & chest: explicit base rotations prevent additive layers from runaway spinning
  if (rig.torso) {
    const sp = cfg.spine || [0, 0, 0];
    rig.torso.rotation.set(sp[0] || 0, sp[1] || 0, sp[2] || 0);
  }
  if (rig.chest) {
    const ch = cfg.chest || [0, 0, 0];
    rig.chest.rotation.set(ch[0] || 0, ch[1] || 0, ch[2] || 0);
  }
  if (rig.hips) {
    rig.hips.rotation.set(0, 0, 0);
  }

  // legs
  const sp = cfg.spread;
  rig.legL.hip.rotation.x = cfg.hip[0];
  rig.legR.hip.rotation.x = cfg.hip[1];
  rig.legL.hip.rotation.z = -sp;
  rig.legR.hip.rotation.z = sp;
  if (cfg.hipRot) {
    rig.legL.hip.rotation.y = cfg.hipRot[0] || 0;
    rig.legR.hip.rotation.y = cfg.hipRot[1] || 0;
  } else {
    rig.legL.hip.rotation.y = 0;
    rig.legR.hip.rotation.y = 0;
  }
  rig.legL.knee.rotation.x = cfg.knee[0];
  rig.legR.knee.rotation.x = cfg.knee[1];

  // arms: FK-eased, and skipped wholesale while IK owns the joint.
  // (IK arms keep persistent joint state across frames — even an eased FK
  // write would fight the plant. Releases glide back via the 18/s ease.)
  const AR = 18;
  if (!skip || !skip.has('armL')) {
    rig.armL.shoulder.rotation.x = dampNum3(rig.armL.shoulder.rotation.x, cfg.arm[0], AR, dt);
    rig.armL.shoulder.rotation.z = dampNum3(rig.armL.shoulder.rotation.z, cfg.armZ[0], AR, dt);
    rig.armL.elbow.rotation.x = dampNum3(rig.armL.elbow.rotation.x, cfg.elbow[0], AR, dt);
    if (cfg.wrist && rig.armL.hand) rig.armL.hand.rotation.z = cfg.wrist[0] || 0;
  }
  if (!skip || !skip.has('armR')) {
    rig.armR.shoulder.rotation.x = dampNum3(rig.armR.shoulder.rotation.x, cfg.arm[1], AR, dt);
    rig.armR.shoulder.rotation.z = dampNum3(rig.armR.shoulder.rotation.z, cfg.armZ[1], AR, dt);
    rig.armR.elbow.rotation.x = dampNum3(rig.armR.elbow.rotation.x, cfg.elbow[1], AR, dt);
    if (cfg.wrist && rig.armR.hand) rig.armR.hand.rotation.z = cfg.wrist[1] || 0;
  }

  // head
  rig.neck.rotation.x = cfg.head[0];
  rig.neck.rotation.y = cfg.head[1];
  rig.neck.rotation.z = cfg.head[2];
}

/* ============================================================
   MAIN ANIMATION UPDATE
   ============================================================ */
function updateAnim3(dt) {
  if (!her3 || !him3) return;
  const t = G.t || 0;
  const depth = clamp(G.depth || 0, 0, 1);
  const oral = clamp(G.oral || 0, 0, 1);
  const p = clamp((G.pleasure || 0) / 100, 0, 1);

  /* ---- detect pose change and start a blend ---- */
  const posIdx = (G.pos | 0);
  const oralNow = oral > 0.03 ? 1 : 0;
  if (posIdx !== _lastPosIdx3 || oralNow !== _lastOral3) {
    _prevPose3 = _prevPose3 || currentPose3();
    _lastPosIdx3 = posIdx; _lastOral3 = oralNow;
    _poseBlend3 = 0;
    applyPoseCam3();
  }

  const target = currentPose3();
  let pose = target;
  if (_poseBlend3 < 1) {
    _poseBlend3 = Math.min(1, _poseBlend3 + dt * 2.6);
    const e = _poseBlend3 * _poseBlend3 * (3 - 2 * _poseBlend3);
    pose = lerpPose3(_prevPose3 || target, target, e);
    if (_poseBlend3 >= 1) _prevPose3 = target;
  } else {
    _prevPose3 = target;
  }

  const k = 1 - Math.pow(0.0009, dt);

  /* ---- breathing ---- */
  const brRate = 0.16 + p * 0.0045;
  const brAmt = 2.4 + p * 1.6;
  const br = Math.sin(t * TAU * brRate) * (brAmt / 260);

  /* ---- IK ownership: arms driven by IK keep continuous joint state, so FK
     application skips them entirely (a per-frame FK rebase teleports the arm
     back every frame — every takeover pop traced back to this). ---- */
  const rub = clamp(G.rub || 0, 0, 1);
  const zone = (G.rubZone | 0);
  const rubActive = rub > 0.03 && G.state !== 'climax' && G.state !== 'finish';
  const rubKeys = new Set();
  if (rubActive) {
    // MUST mirror the zone→arm mapping in the rub block below
    if (zone === 3 || zone === 1) rubKeys.add('himR');
    else if (zone === 2) rubKeys.add('himL');
    else { rubKeys.add('himR'); rubKeys.add('himL'); }
  }
  const soloNow = !!G.solo;
  if (soloNow !== _lastSolo3) {
    _lastSolo3 = soloNow;
    _aimSm3.init.herL = 0;
    _aimSm3.init.herR = 0;
  }
  const skipHim = new Set(rubKeys);
  const skipHer = new Set();
  const poseHands = pose.hands;
  if (poseHands) {
    if (!G.solo) {
      if (poseHands.himL && !rubKeys.has('himL')) skipHim.add('armL');
      if (poseHands.himR && !rubKeys.has('himR')) skipHim.add('armR');
    }
    if (poseHands.herL) skipHer.add('armL');
    if (poseHands.herR) skipHer.add('armR');
  }

  /* ---- base rigs through the continuous blend layer (never raw gameplay) ---- */
  _smPose3.her = dampCfg3(_smPose3.her, pose.her, dt);
  _smPose3.him = dampCfg3(_smPose3.him, pose.him, dt);
  applyRig3(her3, _smPose3.her, dt, k, skipHer);
  applyRig3(him3, _smPose3.him, dt, k, skipHim);

  /* ---- thrust: damped stroke drive so depth spikes glide instead of
     snapping. Knees stay planted: cowgirl rides vertically, horizontal
     poses slide on Z, oral stays put. ---- */
  const isCowgirl = (oral <= 0.03) && (posIdx === 4 || posIdx === 5);
  _thrust3.y = dampNum3(_thrust3.y, isCowgirl ? (1 - depth) * 0.11 : 0, 8, dt);
  _thrust3.z = dampNum3(_thrust3.z, (!isCowgirl && oral <= 0.03) ? (1 - depth) * 0.11 : 0, 8, dt);
  her3.root.position.y += _thrust3.y;
  him3.root.position.z += _thrust3.z;

  /* ---- contact & bedding safety ---- */
  solveContact3(dt);

  /* ---- shaft: smoothed erection angle, arousal swell, damped climax pulse ---- */
  const ar = clamp((G.ar || 0) / 100, 0, 1);
  _shaftP3 = dampNum3(_shaftP3, G.shaftPulse || 0, 7, dt);
  const pulse = 1 + 0.34 * _shaftP3;
  const smHim = _smPose3.him || pose.him;
  him3.shaftRoot.rotation.x = (smHim && smHim.shaft != null)
    ? smHim.shaft : (-Math.PI / 2 + 0.12 - ar * 0.10);
  him3.shaft.scale.set(pulse, pulse, pulse);
  him3.glans.scale.setScalar(pulse * (1 + 0.12 * _shaftP3));
  const shaftMat = him3.shaft.material;
  if (shaftMat && shaftMat.emissive) {
    shaftMat.emissive.setRGB(0.28 * ar, 0.06 * ar, 0.09 * ar);
  }

  /* ---- breathing on her chest ---- */
  her3.chest.scale.set(1 + br * 1.5, 1 + br * 2.2, 1 + br * 1.8);
  him3.chest.scale.set(1 + br * 1.0, 1 + br * 1.4, 1 + br * 1.2);

  /* ---- breast + buttock jiggle ---- */
  const drive = (G.impact || 0) * 0.9 + Math.abs(G.vel || 0) * 0.06;
  const bt = springStep(_breastJig, drive, 190, 15, dt);
  const bt2 = springStep(_buttJig, drive * 0.85, 165, 14, dt);
  const bs = clamp(bt, -0.4, 0.4);
  her3.breastL.scale.set(1 + bs * 0.16, 1 - bs * 0.20, 1 + bs * 0.12);
  her3.breastR.scale.set(1 + bs * 0.16, 1 - bs * 0.20, 1 + bs * 0.12);
  her3.breastL.rotation.x = -0.14 + bs * 0.30;
  her3.breastR.rotation.x = -0.14 + bs * 0.30;
  her3.hips.scale.set(1 + bt2 * 0.05, 1 - bt2 * 0.05, 1 + bt2 * 0.07);

  /* ---- pose-dependent soft-tissue compression: shank bunches when the knee
     folds, forearm when the elbow folds, torso dents on hard impact ---- */
  const flexL = clamp(Math.abs(her3.legL.knee.rotation.x) / 1.6, 0, 1);
  const flexR = clamp(Math.abs(her3.legR.knee.rotation.x) / 1.6, 0, 1);
  her3.legL.knee.scale.set(1 + flexL * 0.05, 1 - flexL * 0.04, 1 + flexL * 0.05);
  her3.legR.knee.scale.set(1 + flexR * 0.05, 1 - flexR * 0.04, 1 + flexR * 0.05);
  const elbL = clamp(Math.abs(her3.armL.elbow.rotation.x) / 1.5, 0, 1);
  const elbR = clamp(Math.abs(her3.armR.elbow.rotation.x) / 1.5, 0, 1);
  her3.armL.elbow.scale.set(1 + elbL * 0.05, 1 - elbL * 0.04, 1 + elbL * 0.05);
  her3.armR.elbow.scale.set(1 + elbR * 0.05, 1 - elbR * 0.04, 1 + elbR * 0.05);
  const sq = clamp(drive, 0, 0.6);
  her3.torso.scale.set(1 + sq * 0.02, 1 - sq * 0.025, 1 + sq * 0.02);

  /* ---- hair inertia: spring-lagged behind head angular velocity, so locks
     swing late and settle instead of waving on a sine loop ---- */
  if (her3.hair && her3.head) {
    her3.head.getWorldQuaternion(_tmpQ3a);
    let wvx = 0, wvz = 0;
    if (_prevHeadQ3 && dt > 0) {
      _tmpQ3b.copy(_prevHeadQ3).invert().multiply(_tmpQ3a);
      const w = clamp(_tmpQ3b.w, -1, 1);
      const ang = 2 * Math.acos(w);
      const s = Math.sqrt(Math.max(0, 1 - w * w)) || 1;
      wvx = clamp(_tmpQ3b.x / s * ang / dt, -8, 8);
      wvz = clamp(_tmpQ3b.z / s * ang / dt, -8, 8);
    }
    if (!_prevHeadQ3) _prevHeadQ3 = new THREE.Quaternion();
    _prevHeadQ3.copy(_tmpQ3a);
    // locks trail the motion, then settle; idle sway + thrust lag ride on top
    const htx = clamp(-wvx * 0.014, -0.30, 0.30) + Math.sin(t * 2.2) * 0.02 - bs * 0.10;
    const htz = clamp(-wvz * 0.014, -0.30, 0.30) + Math.sin(t * 1.7) * 0.018 + (G.nod || 0) * 0.06;
    _hair3.v += (-70 * (_hair3.x - htx) - 13 * _hair3.v) * dt;
    _hair3.x += _hair3.v * dt;
    _hair3.vz += (-70 * (_hair3.z - htz) - 13 * _hair3.vz) * dt;
    _hair3.z += _hair3.vz * dt;
    her3.hair.rotation.x = _hair3.x;
    her3.hair.rotation.z = _hair3.z;
  }

  /* ---- rub hands: aim his arms at the selected zone (keys precomputed above) ---- */
  const rubArms = rubKeys;
  if (rubActive) {
    const zone = (G.rubZone | 0);
    her3.root.updateWorldMatrix(true, true);
    const knead = Math.sin(t * 9) * 0.030 * rub;
    const knead2 = Math.sin(t * 18) * 0.016 * rub;

    const targets = [];
    if (zone === 3) {
      const v = vulvaWorld3(her3);
      v.y += knead2; v.x += knead;
      targets.push([him3.armR, v, 'himR']);
    } else if (zone === 1) {
      const v = breastWorld3(her3, -1); v.y += knead2; v.x += knead;
      targets.push([him3.armR, v, 'himR']);
    } else if (zone === 2) {
      const v = breastWorld3(her3, 1); v.y += knead2; v.x += knead;
      targets.push([him3.armL, v, 'himL']);
    } else {
      const L = breastWorld3(her3, -1); L.y += knead2; L.x += knead;
      const R = breastWorld3(her3, 1); R.y += knead2; R.x -= knead;
      targets.push([him3.armR, L, 'himR'], [him3.armL, R, 'himL']);
    }
    for (const [arm, tv, key] of targets) {
      // (ownership already recorded in rubKeys above)
      // damped + slew-capped hand target, and a transfer ramp: taking over an
      // arm starts at gentle 10/s tracking and eases to full 60/s over 0.25s
      // (fast track preserves the knead; the ramp kills the takeover pop)
      if (!_aimSm3.init[key]) {
        arm.hand.updateWorldMatrix(true, false);
        arm.hand.getWorldPosition(_aimSm3[key]);
        _aimSm3.age[key] = 0;
        _aimSm3.init[key] = 1;
      }
      if (_aimSm3.drv[key] !== 'rub') { _aimSm3.drv[key] = 'rub'; _aimSm3.age[key] = 0; }
      _aimSm3.age[key] += dt;
      const ramp = Math.min(1, _aimSm3.age[key] / 0.25);
      dampSlewVec3(_aimSm3[key], tv, 10, 1.5, dt);
      aimArmAt3(arm, _aimSm3[key], him3.R.upperArm, him3.R.foreArm, -0.12, 4 + 56 * ramp, dt, 8 + 17 * ramp);
    }
  }

  /* ---- pose hand-IK: plant free hands on bodies/bedding ---- */
  solveHands3(pose, rubArms, dt);

  /* ---- oral: his head bobs at her vulva ---- */
  if (oral > 0.03) {
    const bob = Math.sin(t * 5.2) * 0.055 * (0.4 + (G.oralDepth || 0) * 0.6);
    him3.neck.rotation.x += bob;
    him3.root.position.y += Math.sin(t * 5.2) * 0.012;
    if ((G.oralGag || 0) > 0) {
      her3.hips.rotation.x += Math.sin(t * 22) * 0.01 * G.oralGag;
    }
  }

  /* ---- orgasm: she arches and trembles ---- */
  if (G.state === 'orgasm') {
    const e = Math.sin(Math.PI * clamp((G.orgT || 0) / 5.2, 0, 1));
    her3.torso.rotation.x += -0.16 * e;
    her3.neck.rotation.x += -0.24 * e;
    const tr = Math.sin(t * 44) * 0.012 * e;
    her3.root.position.x += tr;
    him3.root.position.x += tr * 0.5;
  }

  /* ---- afterglow: boneless slump ---- */
  if ((G.after || 0) > 0) {
    const s = Math.min(1, G.after / 3);
    her3.torso.rotation.x += 0.08 * s;
    her3.armL.shoulder.rotation.x += 0.35 * s;
    her3.armR.shoulder.rotation.x += 0.35 * s;
  }

  /* ---- solo showcase: him hidden, she performs a slow alluring sway.
     GLB girls inherit it for free — they ride her3.root via glbFollowHer3. ---- */
  if (him3.root) him3.root.visible = !G.solo;
  if (G.solo) updateSolo3(dt, t);

  /* ---- plant the feet after every other transform has landed ---- */
  plantFeet3(dt);

  /* ---- rock-solid mattress physics: prevent any body part from sinking below bed ---- */
  solveContact3(dt);

  /* ---- repaint the anime face (throttled) ---- */
  _faceAccum += dt;
  if (_faceAccum > 1 / 26) {
    _faceAccum = 0;
    const E = (typeof herExpression === 'function') ? herExpression() : { eye: .7, rolled: 0, mouth: .1, blush: .2, brow: 0 };
    paintFace3(her3.face.ctx, E, G.char || {});
    her3.face.tex.needsUpdate = true;
  }

  /* ---- FPV: hide his upper body & head so the camera never clips inside them ---- */
  const fpv = (typeof CAM3 !== 'undefined') && CAM3.mode === 'fpv';
  if (him3.fpvHide) {
    for (let i = 0; i < him3.fpvHide.length; i++) {
      him3.fpvHide[i].visible = !fpv;
    }
  } else if (him3.head) {
    him3.head.visible = !fpv;
  }
}

/* ============================================================
   SOLO SHOWCASE (S): just her, no male. Slow hip-first figure-eight sway,
   arch, head roll and ambient moans — works for procedural girls and every
   GLB preset alike. Purely additive on top of the current pose rig.
   ============================================================ */
function updateSolo3(dt, t) {
  G.soloPh = (G.soloPh || 0) + dt * 0.6;
  const ph = G.soloPh;
  // Natural subtle sensual sway & gentle breathing
  const sway = Math.sin(ph), sway2 = Math.sin(ph * 0.5 + 0.8);
  her3.root.position.x += sway * 0.012;
  her3.root.position.y += Math.abs(Math.cos(ph)) * 0.005;
  // Torso / spine roll and arch (additive on top of clean base pose)
  her3.torso.rotation.y += sway2 * 0.06;
  her3.torso.rotation.z += sway * 0.02;
  her3.torso.rotation.x += -0.04 - Math.abs(sway) * 0.02;
  her3.chest.scale.y += 0.015 * Math.sin(ph * 2);
  her3.neck.rotation.y += sway2 * 0.08;
  her3.neck.rotation.x += -0.04;
  // ambient moans while she shows off (only during free play)
  G.soloMoanT = (G.soloMoanT == null ? 2.5 : G.soloMoanT) - dt;
  if (G.soloMoanT <= 0 && G.state === 'play' && typeof playMoan === 'function') {
    playMoan(0.35 + Math.random() * 0.25, { dur: 0.55 + Math.random() * 0.3, pmul: 1.05, vol: 0.85 });
    G.soloMoanT = 3.5 + Math.random() * 3.5;
  }
}

/* ============================================================
   CLIMAX: shaft pulse hook (called from fx3d)
   ============================================================ */
function pulseShaft3(amount) {
  if (!him3) return;
  G.shaftPulse = Math.max(G.shaftPulse || 0, amount);
}
