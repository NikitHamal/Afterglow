// Afterglow — module: glbHair (loaded by 3d.html after glbModel.js)
'use strict';
/* ============================================================
   GLB HAIR CHAINS — strand-group physics for realistic models.
   Any GLB that ships bones named 髪<chain><seg> (seg a, b, c… plus an
   unweighted tip leaf z) gets a verlet simulation per chain:
     • the root rides the head bone; every joint has a "rest" target that
       moves rigidly with the head, and a stiffness pulls toward it (style
       memory, stronger near the root), so the hair keeps its shape but
       swings with real inertia when the head or body moves;
     • gravity acts in world space, so lying down drapes the hair;
     • joints are pushed out of body colliders (spheres the model ships in
       its glTF extras, bound to bones: head, neck, chest, back, shoulders),
       so hair never passes through her;
     • after the sim each bone is rotated to aim at its simulated child.
   Big jumps (pose switch, character swap, paused tab) reset to rest.
   ============================================================ */
const GLB_HAIR_TUNE = {
  gravity: 7.5,               // m/s² in game units (the 1.35-unit women)
  damping: 3.2,               // 1/s velocity loss
  stiffness: [14, 8, 5, 3.4, 2.6, 2.2],   // 1/s pull toward the rest pose, root → tip
  margin: 0.012,              // extra collider radius (cards have volume around the guide)
  iterations: 2
};
const _hrA = new THREE.Vector3(), _hrB = new THREE.Vector3(), _hrC = new THREE.Vector3();
const _hrQa = new THREE.Quaternion(), _hrQb = new THREE.Quaternion(), _hrQp = new THREE.Quaternion();
const _hrM = new THREE.Matrix4();

function glbHairInit(e) {
  if (!e || !e.model) return null;
  const chains = {};
  e.model.updateMatrixWorld(true);
  e.model.traverse(c => {
    if (!c.isBone) return;
    const m = /^髪(\d+)([a-z])$/.exec(glbCleanKey(c.name));
    if (m) (chains[m[1]] = chains[m[1]] || []).push({ bone: c, seg: m[2] });
  });
  const keys = Object.keys(chains);
  if (!keys.length) return null;
  const H = { chains: [], colliders: [], fresh: true };
  keys.forEach(k => {
    const segs = chains[k].sort((a, b) => (a.seg < b.seg ? -1 : 1)).map(x => x.bone);
    const n = segs.length;
    const ch = { bones: segs, restQ: segs.map(b => b.quaternion.clone()), len: [], P: [], Pp: [], R: [] };
    for (let i = 0; i < n; i++) {
      ch.P.push(new THREE.Vector3()); ch.Pp.push(new THREE.Vector3()); ch.R.push(new THREE.Vector3());
      if (i < n - 1) ch.len.push(segs[i + 1].position.length());
    }
    H.chains.push(ch);
  });
  // colliders: model-space centres + radii from the GLB extras, re-expressed in their bone's space
  let spec = null;
  e.model.traverse(c => { if (!spec && c.userData && c.userData.afterglow) spec = c.userData.afterglow; });
  try { if (typeof spec === 'string') spec = JSON.parse(spec); } catch (_) { spec = null; }
  const byName = {};
  e.model.traverse(c => { if (c.isBone) byName[glbCleanKey(c.name)] = c; });
  if (spec && spec.hairColliders) {
    spec.hairColliders.forEach(col => {
      const b = byName[glbCleanKey(col.bone)];
      if (!b) return;
      const w = e.model.localToWorld(new THREE.Vector3(col.c[0], col.c[1], col.c[2]));
      H.colliders.push({ bone: b, local: b.worldToLocal(w.clone()), r: col.r, w: new THREE.Vector3() });
    });
  }
  return H;
}

function glbHairReset(H) {
  for (const ch of H.chains) for (let i = 0; i < ch.P.length; i++) { ch.P[i].copy(ch.R[i]); ch.Pp[i].copy(ch.R[i]); }
}

function glbHairUpdate(e, dt) {
  const H = e && e.hair;
  if (!H || !e.model || !e.model.visible) return;
  const T = GLB_HAIR_TUNE;
  const s = e.model.scale.x || 1;
  // rest pose of every chain for this frame (rigid with the head)
  for (const ch of H.chains) for (let i = 0; i < ch.bones.length; i++) ch.bones[i].quaternion.copy(ch.restQ[i]);
  e.model.updateMatrixWorld(true);
  let jump = false;
  for (const ch of H.chains) {
    for (let i = 0; i < ch.bones.length; i++) {
      ch.bones[i].getWorldPosition(_hrA);
      if (!H.fresh && i === 0 && _hrA.distanceToSquared(ch.R[0]) > 0.09) jump = true;
      ch.R[i].copy(_hrA);
    }
  }
  if (H.fresh || jump || !(dt > 0) || dt > 0.1) { glbHairReset(H); H.fresh = false; if (!(dt > 0)) return; }
  for (const c of H.colliders) c.w.copy(c.local).applyMatrix4(c.bone.matrixWorld);
  const damp = Math.exp(-T.damping * dt), g = -T.gravity * dt * dt;
  for (const ch of H.chains) {
    const n = ch.P.length;
    ch.P[0].copy(ch.R[0]); ch.Pp[0].copy(ch.R[0]);
    for (let i = 1; i < n; i++) {
      const P = ch.P[i], Pp = ch.Pp[i];
      _hrA.subVectors(P, Pp).multiplyScalar(damp);
      Pp.copy(P);
      P.add(_hrA); P.y += g;
      const k = 1 - Math.exp(-T.stiffness[Math.min(i, T.stiffness.length - 1)] * dt);
      P.lerp(ch.R[i], k);
    }
    for (let it = 0; it < T.iterations; it++) {
      for (let i = 1; i < n; i++) {
        const P = ch.P[i], Q = ch.P[i - 1];
        _hrA.subVectors(P, Q); const d = _hrA.length() || 1e-6;
        P.copy(Q).addScaledVector(_hrA, (ch.len[i - 1] * s) / d);
        for (const c of H.colliders) {
          const r = (c.r + T.margin) * s;
          _hrB.subVectors(P, c.w); const dd = _hrB.length();
          if (dd < r) P.copy(c.w).addScaledVector(_hrB, r / (dd || 1e-6));
        }
      }
    }
    // aim each bone at its simulated child, root → tip
    for (let i = 0; i < n - 1; i++) {
      const b = ch.bones[i];
      b.updateMatrixWorld(true);
      b.getWorldPosition(_hrA);
      ch.bones[i + 1].getWorldPosition(_hrB);
      _hrB.sub(_hrA).normalize();
      _hrC.subVectors(ch.P[i + 1], ch.P[i]).normalize();
      _hrQa.setFromUnitVectors(_hrB, _hrC);
      b.getWorldQuaternion(_hrQb);
      _hrQb.premultiply(_hrQa);
      b.parent.getWorldQuaternion(_hrQp);
      b.quaternion.copy(_hrQp.invert().multiply(_hrQb));
    }
    ch.bones[0].updateMatrixWorld(true);
  }
}

if (typeof window !== 'undefined') { window.GLB_HAIR_TUNE = GLB_HAIR_TUNE; }
