// Afterglow 3D — module: anim3d (rig animation driven by the shared G state)
'use strict';

let her3 = null, him3 = null;
let _prevPose3 = null, _poseBlend3 = 1, _lastPosIdx3 = -1, _lastOral3 = 0;
let _faceAccum = 0;
const _breastJig = { x: 0, v: 0 };
const _buttJig = { x: 0, v: 0 };
const _tmpV = new THREE.Vector3();

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

/* ============================================================
   HAND TARGETS — resolve a pose hand config to a world position.
   Pure function of the two rigs so the contact harness can test it.
   ============================================================ */
const _htV = new THREE.Vector3();
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
  } else { // herChest (default)
    her.chest.updateWorldMatrix(true, false);
    her.chest.localToWorld(out.set(cfg.x || 0, cfg.y || 0, cfg.z || 0));
  }
  if (cfg.floor != null && out.y < cfg.floor) out.y = cfg.floor;
  return out;
}

/* ============================================================
   CONTACT & BEDDING SOLVER
   Mattress safety: roots and knees never sink through bedding
   (mattress surface is y = 0.11). Poses are authored in contact.
   ============================================================ */
function solveContact3() {
  if (!her3 || !him3 || _poseBlend3 < 1) return;
  if (her3.root.position.y < 0.13) her3.root.position.y = 0.13;
  if (him3.root.position.y < 0.16) him3.root.position.y = 0.16;
}

/* pose hand-IK: plant the configured hands, unless that arm is busy rubbing */
function solveHands3(pose, rubArms) {
  const H = pose && pose.hands;
  if (!H || !her3 || !him3) return;
  const jobs = [
    [H.himL, him3.armL, 'himL'], [H.himR, him3.armR, 'himR'],
    [H.herL, her3.armL, 'herL'], [H.herR, her3.armR, 'herR']
  ];
  for (const [cfg, arm, key] of jobs) {
    if (!cfg || !arm) continue;
    if (rubArms && rubArms.has(key)) continue;
    const rig = (key === 'himL' || key === 'himR') ? him3 : her3;
    handTarget3(her3, him3, cfg, _htV);
    aimArmAt3(arm, _htV, rig.R.upperArm, rig.R.foreArm, -0.15);
  }
}
function springStep(s, target, k, c, dt) {
  s.v += (-k * (s.x - target) - c * s.v) * dt;
  s.x += s.v * dt;
  return s.x;
}

/* ============================================================
   2-BONE ARM AIMING (used for the rub hands)
   ============================================================ */
const _aimQ = new THREE.Quaternion();
const _downY = new THREE.Vector3(0, -1, 0);
function aimArmAt3(arm, targetWorld, upperLen, foreLen, bendBias) {
  const sh = arm.shoulder;
  const parent = sh.parent;
  if (!parent) return;
  parent.updateWorldMatrix(true, false);
  const local = parent.worldToLocal(_tmpV.copy(targetWorld));
  const dir = local.sub(sh.position);
  const dist = clamp(dir.length(), 0.02, (upperLen + foreLen) * 0.995);
  dir.normalize();

  _aimQ.setFromUnitVectors(_downY, dir);
  // blend with a bias so the elbow doesn't flip through the body
  sh.quaternion.slerp(_aimQ, 1);

  // cosine rule → interior angle at the elbow
  let cosI = (upperLen * upperLen + foreLen * foreLen - dist * dist) / (2 * upperLen * foreLen);
  cosI = clamp(cosI, -1, 1);
  const interior = Math.acos(cosI);
  arm.elbow.rotation.set(Math.PI - interior + (bendBias || 0), 0, 0);
}

/* ============================================================
   POSE APPLICATION
   ============================================================ */
function applyRig3(rig, cfg, dt, k) {
  const root = rig.root;
  // position: authored pose coordinates (smooth blending handled by lerpPose3)
  root.position.set(cfg.pos[0], cfg.pos[1], cfg.pos[2]);
  // orientation — snap toward target frame (cheap + stable when up/fwd are orthogonal)
  orientTo3(root, cfg.up, cfg.fwd);

  // legs
  const sp = cfg.spread;
  rig.legL.hip.rotation.x = cfg.hip[0];
  rig.legR.hip.rotation.x = cfg.hip[1];
  rig.legL.hip.rotation.z = -sp;
  rig.legR.hip.rotation.z = sp;
  rig.legL.knee.rotation.x = cfg.knee[0];
  rig.legR.knee.rotation.x = cfg.knee[1];

  // arms
  rig.armL.shoulder.rotation.x = cfg.arm[0];
  rig.armR.shoulder.rotation.x = cfg.arm[1];
  rig.armL.shoulder.rotation.z = cfg.armZ[0];
  rig.armR.shoulder.rotation.z = cfg.armZ[1];
  rig.armL.elbow.rotation.x = cfg.elbow[0];
  rig.armR.elbow.rotation.x = cfg.elbow[1];

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

  /* ---- base rigs ---- */
  applyRig3(her3, pose.her, dt, k);
  applyRig3(him3, pose.him, dt, k);

  /* ---- thrust: stroke along bed axis, anchoring knees on mattress ---- */
  const isCowgirl = (oral <= 0.03) && (posIdx === 4 || posIdx === 5);
  if (isCowgirl) {
    // Cowgirl: she rides vertically on his upright shaft; he rests flat on mattress
    her3.root.position.y += (1 - depth) * 0.11;
  } else if (oral <= 0.03) {
    // Horizontal poses (missionary, legs-up, doggy, prone, spooning):
    // He strokes back and forth along the mattress plane (Z).
    // Pelvis height (Y) stays anchored so his knees stay firmly planted on the bed.
    him3.root.position.z += (1 - depth) * 0.11;
  }

  /* ---- contact & bedding safety ---- */
  solveContact3();

  /* ---- shaft: per-pose erection angle, arousal swell, climax pulse ---- */
  const ar = clamp((G.ar || 0) / 100, 0, 1);
  const pulse = 1 + 0.34 * (G.shaftPulse || 0);
  him3.shaftRoot.rotation.x = (pose && pose.him && pose.him.shaft != null)
    ? pose.him.shaft : (-Math.PI / 2 + 0.12 - ar * 0.10);
  him3.shaft.scale.set(pulse, pulse, pulse);
  him3.glans.scale.setScalar(pulse * (1 + 0.12 * (G.shaftPulse || 0)));
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

  /* ---- hair sway: lags behind head motion ---- */
  if (her3.hair) {
    her3.hair.rotation.x = Math.sin(t * 2.2) * 0.02 - bs * 0.10;
    her3.hair.rotation.z = Math.sin(t * 1.7) * 0.018 + (G.nod || 0) * 0.06;
  }

  /* ---- rub hands: aim his arms at the selected zone ---- */
  const rub = clamp(G.rub || 0, 0, 1);
  const rubArms = new Set();
  if (rub > 0.03 && G.state !== 'climax' && G.state !== 'finish') {
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
      rubArms.add(key);
      aimArmAt3(arm, tv, him3.R.upperArm, him3.R.foreArm, -0.12);
    }
  }

  /* ---- pose hand-IK: plant free hands on bodies/bedding ---- */
  solveHands3(pose, rubArms);

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
   CLIMAX: shaft pulse hook (called from fx3d)
   ============================================================ */
function pulseShaft3(amount) {
  if (!him3) return;
  G.shaftPulse = Math.max(G.shaftPulse || 0, amount);
}
