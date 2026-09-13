/* diag-collapse.js v3 — WORLD-SPACE limb cross-section comparison.

   All quantities are computed in the CURRENT world space so rest and deformed
   values are directly comparable:
     rest  vertex = geometry position * mesh.matrixWorld
     def   vertex = mesh.boneTransform(i, p) * mesh.matrixWorld
     rest  bone   = mesh.matrixWorld * bindMatrixInverse * inverse(boneInverse)
     live  bone   = bone.matrixWorld

   For each skinned mesh we take vertices whose REST position lies inside a
   cylinder around the REST limb segment, then compare their perpendicular
   distance from the REST axis with their distance from the LIVE axis.
   ratio < 1 => crushed, ratio > 1 => stretched. Compact text out. */
(function () {
  if (!window.GLB_MODEL || !GLB_MODEL.model) return 'no model';
  var meshes = [];
  GLB_MODEL.model.traverse(function (o) { if (o.isSkinnedMesh && o.geometry.attributes.position) meshes.push(o); });
  if (!meshes.length) return 'no skinned mesh';

  var bonesByName = {};
  GLB_MODEL.model.traverse(function (c) { if (c.isBone && c.name) bonesByName[c.name.replace(/[._]/g, '')] = c; });

  var sk = null;
  for (var i = 0; i < meshes.length; i++) if (meshes[i].skeleton.bones.indexOf(bonesByName['ひざL']) >= 0) sk = meshes[i].skeleton;
  if (!sk) return 'no leg skeleton';

  function idx(b) { return sk.bones.indexOf(b); }
  function liveWorld(b) { b.updateWorldMatrix(true, false); return new THREE.Vector3().setFromMatrixPosition(b.matrixWorld); }

  var lines = [];
  meshes.forEach(function (mesh) {
    if (mesh.geometry.attributes.position.count < 150) return;
    mesh.updateWorldMatrix(true, false);
    var mw = mesh.matrixWorld;
    var bmInv = new THREE.Matrix4().copy(mesh.bindMatrix).invert();
    // rest pose placed at the current model transform
    var restXf = new THREE.Matrix4().multiplyMatrices(mw, bmInv);

    function restBoneWorld(b) {
      var i = idx(b); if (i < 0) return null;
      var m = new THREE.Matrix4().copy(sk.boneInverses[i]).invert();
      return new THREE.Vector3().setFromMatrixPosition(m).applyMatrix4(restXf);
    }

    var segs = {};
    ['L', 'R'].forEach(function (s) {
      var t = bonesByName['足' + s], k = bonesByName['ひざ' + s], a = bonesByName['足首' + s], to = bonesByName['つま先' + s];
      if (!t || !k || !a) return;
      segs['thigh' + s] = [restBoneWorld(t), restBoneWorld(k), liveWorld(t), liveWorld(k)];
      segs['shin' + s] = [restBoneWorld(k), restBoneWorld(a), liveWorld(k), liveWorld(a)];
      if (to) segs['foot' + s] = [restBoneWorld(a), restBoneWorld(to), liveWorld(a), liveWorld(to)];
    });

    var pos = mesh.geometry.attributes.position;
    var N = pos.count;
    var rMax = window.__rMax || 0.07;
    var out = [];
    Object.keys(segs).forEach(function (key) {
      var A = segs[key];
      var bax = new THREE.Vector3().subVectors(A[1], A[0]); var bL = bax.length(); if (bL < 1e-6) return; bax.divideScalar(bL);
      var lax = new THREE.Vector3().subVectors(A[3], A[2]); var lL = lax.length(); if (lL < 1e-6) return; lax.divideScalar(lL);
      var bands = [[], [], [], []];
      var v = new THREE.Vector3(), q = new THREE.Vector3(), d = new THREE.Vector3(), tmp = new THREE.Vector3();
      for (var k2 = 0; k2 < N; k2++) {
        v.set(0, 0, 0).fromBufferAttribute(pos, k2).applyMatrix4(mw);   // rest, world
        tmp.subVectors(v, A[0]);
        var t = tmp.dot(bax); if (t < 0 || t > bL) continue;
        var rb = tmp.clone().addScaledVector(bax, -t).length();
        if (rb > rMax || rb < 1e-6) continue;
        q.set(0, 0, 0); mesh.boneTransform(k2, q.fromBufferAttribute(pos, k2));
        q.applyMatrix4(mw);                                            // deformed, world
        d.subVectors(q, A[2]);
        var td = d.dot(lax);
        var rd = d.addScaledVector(lax, -td).length();
        var bi = Math.min(3, Math.max(0, Math.floor(t / bL * 4)));
        bands[bi].push([rb, rd]);
      }
      var row = { n: 0, ratio: [] };
      for (var b2 = 0; b2 < 4; b2++) {
        var arr = bands[b2];
        if (!arr.length) { row.ratio.push(null); continue; }
        var sb = 0, sd = 0;
        for (var j = 0; j < arr.length; j++) { sb += arr[j][0]; sd += arr[j][1]; }
        row.n += arr.length; sb /= arr.length; sd /= arr.length;
        row.ratio.push(+(sd / sb).toFixed(3));
      }
      if (row.n >= 15) out.push('   ' + key + ' n=' + row.n + ' ratio=' + JSON.stringify(row.ratio));
    });
    if (out.length) {
      lines.push('MESH ' + mesh.name + ' (' + (mesh.material && mesh.material.name) + ')');
      lines.push.apply(lines, out);
    }
  });
  return lines.join('\n');
})()
