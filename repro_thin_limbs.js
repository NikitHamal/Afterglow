// THROWAWAY REPRODUCTION (delete before finishing).
// Symptom reported: in Goat-chan solo masturbation her HANDS read very thin and
// the SHINS below the knees deform ("after knees are fked up").
// This measures the real deformed skin (linear-blend skinning on the loaded GLB)
// and asserts physical tolerances. Run via:
//   node scratch/verify.mjs --go "char=goatchan&play=1&solo=1&clean=1&shot=1&nosanitize=1" \
//        --diagfile repro_thin_limbs.js
// Fails on the unpatched tree, must pass after the fix.
(function () {
  var FAIL = [], M = {};
  function clean(s) { return (s || '').replace(/[._]/g, ''); }
  function deg(r) { return +(r * 180 / Math.PI).toFixed(1); }

  var E = (window.GLB_STORE && GLB_STORE.goatchan) || null;
  if (!E || !E.loaded || !E.model) throw new Error('REPRO BLOCKED: goatchan GLB not loaded');
  if (!(window.G && G.solo)) throw new Error('REPRO BLOCKED: not in solo mode (G.solo=false)');

  // ---- drive the exact symptom: solo rub, hands on her body -------------
  G.spaceHeld = true; G.rub = 1;

  var meshes = [];
  E.model.traverse(function (o) { if (o.isSkinnedMesh) meshes.push(o); });
  if (!meshes.length) throw new Error('REPRO BLOCKED: no skinned mesh');
  var boneBy = {};
  meshes.forEach(function (m) {
    m.skeleton.bones.forEach(function (b) { boneBy[clean(b.name)] = b; });
  });

  var _m4 = new THREE.Matrix4(), _v = new THREE.Vector3(), _p = new THREE.Vector3(), _acc = new THREE.Vector3();
  var _w1 = new THREE.Vector3(), _w2 = new THREE.Vector3();

  function deformWorld(m, i, out) {
    m.updateWorldMatrix(true, false);
    var pos = m.geometry.attributes.position, si = m.geometry.attributes.skinIndex, sw = m.geometry.attributes.skinWeight;
    _p.fromBufferAttribute(pos, i).applyMatrix4(m.bindMatrix);
    _acc.set(0, 0, 0);
    for (var k = 0; k < 4; k++) {
      var w = k === 0 ? sw.getX(i) : k === 1 ? sw.getY(i) : k === 2 ? sw.getZ(i) : sw.getW(i);
      if (!w) continue;
      var bi = k === 0 ? si.getX(i) : k === 1 ? si.getY(i) : k === 2 ? si.getZ(i) : si.getW(i);
      _m4.multiplyMatrices(m.skeleton.bones[bi].matrixWorld, m.skeleton.boneInverses[bi]);
      _acc.addScaledVector(_v.copy(_p).applyMatrix4(_m4), w);
    }
    return out.copy(_acc).applyMatrix4(m.bindMatrixInverse).applyMatrix4(m.matrixWorld);
  }
  function bindWorld(m, i, out) {
    m.updateWorldMatrix(true, false);
    return out.fromBufferAttribute(m.geometry.attributes.position, i).applyMatrix4(m.matrixWorld);
  }

  // vertices whose dominant skin weight is `key` (cleaned bone name)
  function vertsFor(key) {
    var bone = boneBy[key];
    if (!bone) return null;
    var list = [];
    meshes.forEach(function (m) {
      var idx = m.skeleton.bones.indexOf(bone);
      if (idx < 0) return;
      var si = m.geometry.attributes.skinIndex, sw = m.geometry.attributes.skinWeight, n = si.count;
      for (var i = 0; i < n; i++) {
        var best = -1, bw = 0;
        for (var k = 0; k < 4; k++) {
          var w = k === 0 ? sw.getX(i) : k === 1 ? sw.getY(i) : k === 2 ? sw.getZ(i) : sw.getW(i);
          if (w > bw) { bw = w; best = k === 0 ? si.getX(i) : k === 1 ? si.getY(i) : k === 2 ? si.getZ(i) : si.getW(i); }
        }
        if (best === idx && bw >= 0.35) list.push({ m: m, i: i });
      }
    });
    return list.length >= 8 ? list : null;
  }

  // principal cross-section of a vertex set around its centroid, with `axis`
  // as the section normal -> sorted [minExtent, maxExtent] (metres)
  function section(list, axis, deformed) {
    var c = new THREE.Vector3(), v = new THREE.Vector3();
    for (var n = 0; n < list.length; n++) {
      (deformed ? deformWorld(list[n].m, list[n].i, v) : bindWorld(list[n].m, list[n].i, v));
      c.add(v);
    }
    c.multiplyScalar(1 / list.length);
    var u = Math.abs(axis.x) < 0.9 ? new THREE.Vector3(1, 0, 0) : new THREE.Vector3(0, 1, 0);
    u.crossVectors(axis, u).normalize();
    var w = new THREE.Vector3().crossVectors(axis, u).normalize();
    var suu = 0, sww = 0, suw = 0;
    for (n = 0; n < list.length; n++) {
      (deformed ? deformWorld(list[n].m, list[n].i, v) : bindWorld(list[n].m, list[n].i, v));
      v.sub(c);
      var a = v.dot(u), b = v.dot(w);
      suu += a * a; sww += b * b; suw += a * b;
    }
    suu /= list.length; sww /= list.length; suw /= list.length;
    var tr = suu + sww, det = suu * sww - suw * suw;
    var l1 = tr / 2 + Math.sqrt(Math.max(0, tr * tr / 4 - det));
    var l2 = tr / 2 - Math.sqrt(Math.max(0, tr * tr / 4 - det));
    var e = [2 * Math.sqrt(Math.max(0, l2)), 2 * Math.sqrt(Math.max(0, l1))];
    e.sort(function (a, b) { return a - b; });
    // principal axis of the largest extent, in world space
    var ang = 0.5 * Math.atan2(2 * suw, suu - sww);
    var wide = new THREE.Vector3().copy(u).multiplyScalar(Math.cos(ang)).addScaledVector(w, Math.sin(ang)).normalize();
    return { min: e[0], max: e[1], wide: wide };
  }

  function bonePos(key, out) {
    var b = boneBy[key];
    if (!b) return null;
    b.updateWorldMatrix(true, false);
    return out.setFromMatrixPosition(b.matrixWorld);
  }
  // world direction of a bone towards a child bone (or explicit child key)
  function dirTo(parentKey, childKey, out) {
    var p = bonePos(parentKey, _w1), q = bonePos(childKey, _w2);
    if (!p || !q) return null;
    return out.copy(q).sub(p).normalize();
  }
  function childKeyOf(key, prefer) {
    if (prefer && boneBy[prefer]) return prefer;
    var b = boneBy[key];
    for (var i = 0; i < b.children.length; i++) if (b.children[i].isBone) return clean(b.children[i].name);
    return null;
  }
  function angErr(a, b) { return deg(Math.acos(Math.max(-1, Math.min(1, a.dot(b))))); }

  // ---- her3 reference frames (what the retarget is asked to reproduce) ---
  function her3World(joint, out) {
    if (!joint) return null;
    joint.updateWorldMatrix(true, false);
    return out.setFromMatrixPosition(joint.matrixWorld);
  }
  var hL = {}, hR = {};
  hL.hip = her3World(her3.legL.hip, new THREE.Vector3());
  hL.knee = her3World(her3.legL.knee, new THREE.Vector3());
  hL.foot = her3World(her3.legL.foot, new THREE.Vector3());
  hR.hip = her3World(her3.legR.hip, new THREE.Vector3());
  hR.knee = her3World(her3.legR.knee, new THREE.Vector3());
  hR.foot = her3World(her3.legR.foot, new THREE.Vector3());
  var h3handL = new THREE.Vector3(), h3handR = new THREE.Vector3();
  her3.armL.hand.updateWorldMatrix(true, false);
  her3.armR.hand.updateWorldMatrix(true, false);
  var h3handQ_L = new THREE.Quaternion().setFromRotationMatrix(her3.armL.hand.matrixWorld).normalize();
  var h3handQ_R = new THREE.Quaternion().setFromRotationMatrix(her3.armR.hand.matrixWorld).normalize();
  h3handL.setFromMatrixPosition(her3.armL.hand.matrixWorld);
  h3handR.setFromMatrixPosition(her3.armR.hand.matrixWorld);
  // her3 hand wide axis = hand local X (fingers fan along X, palm normal = Z)
  var wideL3 = new THREE.Vector3(1, 0, 0).applyQuaternion(h3handQ_L).normalize();
  var wideR3 = new THREE.Vector3(1, 0, 0).applyQuaternion(h3handQ_R).normalize();

  // ---- hands: thickness collapse + roll vs her3 -----------------
  [['L', '手首L', wideL3], ['R', '手首R', wideR3]].forEach(function (t) {
    var side = t[0], wristKey = t[1], wide3 = t[2];
    var list = vertsFor(wristKey);
    M['hand' + side + '_verts'] = list ? list.length : 0;
    if (!list) { FAIL.push('hand' + side + ': no wrist-weighted vertices'); return; }
    var mid = childKeyOf(wristKey, '中指０' + side) || childKeyOf(wristKey);
    var along = dirTo(wristKey, mid, new THREE.Vector3());
    if (!along) { FAIL.push('hand' + side + ': no child bone for along-axis'); return; }
    var d = section(list, along, true), b = section(list, along, false);
    var thin = d.min / Math.max(1e-6, b.min), wide = d.max / Math.max(1e-6, b.max);
    var roll = angErr(d.wide, wide3);
    M['hand' + side] = { thin: +thin.toFixed(3), wide: +wide.toFixed(3), rollDeg: roll, thinMM: +(d.min * 1000).toFixed(1), bindThinMM: +(b.min * 1000).toFixed(1) };
    if (thin < 0.85) FAIL.push('hand' + side + ' is squashed: thickness ' + (thin * 100).toFixed(0) + '% of bind');
    if (wide < 0.85) FAIL.push('hand' + side + ' is squashed: width ' + (wide * 100).toFixed(0) + '% of bind');
    if (roll > 25) FAIL.push('hand' + side + ' rolled ' + roll + 'deg vs her3 palm');
  });

  // ---- shins: direction + candy-wrap below the knee --------------------
  [['L', hL, 'ひざL', '足首L', 'つま先L', '足L'], ['R', hR, 'ひざR', '足首R', 'つま先R', '足R']].forEach(function (t) {
    var side = t[0], h3 = t[1], kneeKey = t[2], ankKey = t[3], toeKey = t[4], thighKey = t[5];
    var wantShin = new THREE.Vector3().copy(h3.foot).sub(h3.knee).normalize();
    var gotShin = dirTo(kneeKey, ankKey, new THREE.Vector3());
    if (!gotShin) { FAIL.push('shin' + side + ': bones missing'); return; }
    var err = angErr(gotShin, wantShin);
    // lateral sign: the shin must not angle inward relative to her3
    var h3lat = wantShin.dot(new THREE.Vector3(1, 0, 0)), glat = gotShin.dot(new THREE.Vector3(1, 0, 0));
    // knee hinge plane: MMD shin hinges around the thigh's local X
    var thighX = new THREE.Vector3(1, 0, 0);
    boneBy[thighKey].updateWorldMatrix(true, false);
    thighX.setFromMatrixColumn(boneBy[thighKey].matrixWorld, 0).normalize();
    var hinge = 90 - angErr(thighX, gotShin);
    var list = vertsFor(kneeKey);
    var thin = null;
    if (list) {
      var along = dirTo(kneeKey, ankKey, new THREE.Vector3());
      var d = section(list, along, true), b = section(list, along, false);
      thin = d.min / Math.max(1e-6, b.min);
      M['shin' + side + '_verts'] = list.length;
    }
    // foot forward
    var gotFoot = dirTo(ankKey, toeKey, new THREE.Vector3());
    var wantFoot = new THREE.Vector3(0, 0, 1).applyQuaternion(new THREE.Quaternion().setFromRotationMatrix(her3['leg' + side].foot.matrixWorld)).normalize();
    var footErr = gotFoot ? angErr(gotFoot, wantFoot) : -1;
    M['shin' + side] = {
      dirErrDeg: err, hingeErrDeg: +hinge.toFixed(1), latGot: +glat.toFixed(3), latWant: +h3lat.toFixed(3),
      thin: thin === null ? null : +thin.toFixed(3), footErrDeg: footErr
    };
    if (err > 12) FAIL.push('shin' + side + ' points ' + err + 'deg off her3');
    if (Math.abs(hinge) > 12) FAIL.push('shin' + side + ' knee hinge off-axis by ' + hinge.toFixed(1) + 'deg (knock-knee twist)');
    if (thin !== null && thin < 0.85) FAIL.push('shin' + side + ' candy-wrapped: thickness ' + (thin * 100).toFixed(0) + '% of bind');
    if (footErr > 20) FAIL.push('foot' + side + ' rolled ' + footErr + 'deg vs her3');
  });

  var msg = 'REPRO ' + (FAIL.length ? 'FAIL' : 'PASS') + ' | ' + JSON.stringify(M) +
    (FAIL.length ? ' | ' + FAIL.join(' ; ') : '');
  if (FAIL.length) throw new Error(msg);
  return msg;
})()
