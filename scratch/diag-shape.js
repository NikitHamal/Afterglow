/* diag-shape.js — measure REAL deformed geometry (not just bones) so we can
   tell "the rig is crushing the mesh" from "the mesh just looks odd".
   Reports, per side, the cross-section radius profile of the shin and thigh
   in BIND vs DEFORMED state, plus a collapse ratio. Also hand bbox volume.

   Returns a JSON string. Read-only: touches nothing but Math. */
(function () {
  var res = { ok: false };
  if (!window.GLB_MODEL || !GLB_MODEL.model) return JSON.stringify({ ok: false, why: 'no model' });

  // --- pick the skinned body mesh (most verts)
  var body = null, best = -1;
  GLB_MODEL.model.traverse(function (o) {
    if (o.isSkinnedMesh && o.geometry && o.geometry.attributes.position) {
      var c = o.geometry.attributes.position.count;
      if (c > best) { best = c; body = o; }
    }
  });
  if (!body) return JSON.stringify({ ok: false, why: 'no skinned mesh' });
  body.updateWorldMatrix(true, false);

  function boneByName(n) {
    var f = null;
    GLB_MODEL.model.traverse(function (c) {
      if (c.isBone && c.name.replace(/[._]/g, '') === n) f = f || c;
    });
    return f;
  }
  function bindPos(bone) {
    var i = body.skeleton.bones.indexOf(bone);
    if (i < 0) return null;
    var m = new THREE.Matrix4().copy(body.skeleton.boneInverses[i]).invert();
    return new THREE.Vector3().setFromMatrixPosition(m);
  }
  function livePos(bone) {
    bone.updateWorldMatrix(true, false);
    return new THREE.Vector3().setFromMatrixPosition(bone.matrixWorld);
  }

  var pos = body.geometry.attributes.position;
  var N = pos.count;
  var bindM = body.bindMatrix;

  // Pre-compute bind positions in skeleton-world space
  var bindV = new Float32Array(N * 3);
  var _v = new THREE.Vector3();
  for (var i = 0; i < N; i++) {
    _v.fromBufferAttribute(pos, i).applyMatrix4(bindM);
    bindV[i * 3] = _v.x; bindV[i * 3 + 1] = _v.y; bindV[i * 3 + 2] = _v.z;
  }

  // radius profile of verts whose BIND position lies in a cylinder around a->b
  function profile(a, b, rMax, deformed) {
    var ax = new THREE.Vector3().subVectors(b, a);
    var L = ax.length();
    if (L < 1e-6) return null;
    ax.divideScalar(L);
    var bands = [[], [], [], []];
    var tmp = new THREE.Vector3();
    for (var i = 0; i < N; i++) {
      var bx = bindV[i * 3], by = bindV[i * 3 + 1], bz = bindV[i * 3 + 2];
      tmp.set(bx - a.x, by - a.y, bz - a.z);
      var t = tmp.dot(ax);
      if (t < -0.02 || t > L + 0.02) continue;
      var perpBind = tmp.clone().addScaledVector(ax, -t).length();
      if (perpBind > rMax) continue;
      var p;
      if (deformed) {
        p = new THREE.Vector3();
        body.boneTransform(i, p.fromBufferAttribute(pos, i));
        p.applyMatrix4(bindM);
      } else {
        p = tmp.clone().add(a);
      }
      var d = new THREE.Vector3().subVectors(p, a);
      var td = d.dot(ax);
      var perp = d.addScaledVector(ax, -td).length();
      var bi = Math.min(3, Math.max(0, Math.floor(t / L * 4)));
      bands[bi].push(perp);
    }
    var out = [];
    for (var k = 0; k < 4; k++) {
      var arr = bands[k];
      if (!arr.length) { out.push(null); continue; }
      var s = 0; for (var j = 0; j < arr.length; j++) s += arr[j];
      out.push(+(s / arr.length).toFixed(4));
    }
    var cnt = 0; for (var k2 = 0; k2 < 4; k2++) cnt += bands[k2].length;
    return { r: out, n: cnt };
  }

  res.ok = true;
  res.mesh = body.name;
  res.verts = N;

  ['L', 'R'].forEach(function (s) {
    var thigh = boneByName('足' + s), knee = boneByName('ひざ' + s), ank = boneByName('足首' + s);
    if (!thigh || !knee || !ank) return;
    var bThigh = bindPos(thigh), bKnee = bindPos(knee), bAnk = bindPos(ank);
    var lThigh = livePos(thigh), lKnee = livePos(knee), lAnk = livePos(ank);

    var thighBind = profile(bThigh, bKnee, 0.20, false);
    var thighDef = profile(bThigh, bKnee, 0.20, true);
    var shinBind = profile(bKnee, bAnk, 0.20, false);
    var shinDef = profile(bKnee, bAnk, 0.20, true);

    function ratio(b, d) {
      if (!b || !d) return null;
      var o = [];
      for (var k = 0; k < 4; k++) {
        if (b.r[k] == null || d.r[k] == null) { o.push(null); continue; }
        o.push(+(d.r[k] / b.r[k]).toFixed(3));
      }
      return o;
    }

    res[s] = {
      boneLen: { thigh: +lThigh.distanceTo(lKnee).toFixed(4), shin: +lKnee.distanceTo(lAnk).toFixed(4) },
      thigh: { bind: thighBind.r, def: thighDef.r, ratio: ratio(thighBind, thighDef), n: thighBind.n },
      shin: { bind: shinBind.r, def: shinDef.r, ratio: ratio(shinBind, shinDef), n: shinBind.n },
      // world-space axis of the shin bone (direction knee->ankle), normalized
      shinDir: (function () {
        var d = new THREE.Vector3().subVectors(lAnk, lKnee).normalize();
        return [+d.x.toFixed(3), +d.y.toFixed(3), +d.z.toFixed(3)];
      })(),
      // twist proxy: shin bone's local X axis in world, vs bind
      shinTwistDeg: (function () {
        var i = body.skeleton.bones.indexOf(knee);
        if (i < 0) return null;
        var bindMx = new THREE.Matrix4().copy(body.skeleton.boneInverses[i]).invert();
        var bX = new THREE.Vector3().setFromMatrixColumn(bindMx, 0).normalize();
        var lX = new THREE.Vector3().setFromMatrixColumn(knee.matrixWorld, 0).normalize();
        var dot = Math.max(-1, Math.min(1, bX.dot(lX)));
        return +(Math.acos(dot) * 180 / Math.PI).toFixed(1);
      })()
    };
  });

  // --- hand: deformed bbox + fingertip spread (index vs pinky)
  (function () {
    var handV = [];
    for (var i = 0; i < N; i++) {
      var x = bindV[i * 3], y = bindV[i * 3 + 1], z = bindV[i * 3 + 2];
      if (Math.abs(x) > 0.33 && y > 0.62 && y < 0.82) handV.push(i);
    }
    if (!handV.length) return;
    var mn = [1e9, 1e9, 1e9], mx = [-1e9, -1e9, -1e9];
    var p = new THREE.Vector3();
    for (var k = 0; k < handV.length; k++) {
      p.set(0, 0, 0);
      body.boneTransform(handV[k], p.fromBufferAttribute(pos, handV[k]));
      p.applyMatrix4(bindM);
      var c = [p.x, p.y, p.z];
      for (var a = 0; a < 3; a++) { if (c[a] < mn[a]) mn[a] = c[a]; if (c[a] > mx[a]) mx[a] = c[a]; }
    }
    res.hand = { n: handV.length, dim: [+(mx[0] - mn[0]).toFixed(4), +(mx[1] - mn[1]).toFixed(4), +(mx[2] - mn[2]).toFixed(4)] };
    // fingertip spread: distance between 人指2L tip and 小指2L tip
    var idxTip = boneByName('人指３L') || boneByName('人指２L');
    var pkTip = boneByName('小指３L') || boneByName('小指２L');
    if (idxTip && pkTip) {
      res.hand.indexToPinky = +livePos(idxTip).distanceTo(livePos(pkTip)).toFixed(4);
    }
    res.hand.bones = {
      curl: window.GLB_HAND3 ? JSON.parse(JSON.stringify(GLB_HAND3)) : null
    };
  })();

  return JSON.stringify(res);
})()
