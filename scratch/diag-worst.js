/* diag-worst.js — find the vertices that stretch the most in a limb segment and
   report their bind/deformed positions plus the bones actually moving them.
   This is the definitive "which bone is at fault" probe. Compact text. */
(function () {
  if (!window.GLB_MODEL || !GLB_MODEL.model) return 'no model';
  var meshes = [];
  GLB_MODEL.model.traverse(function (o) { if (o.isSkinnedMesh) meshes.push(o); });
  var bonesByName = {};
  GLB_MODEL.model.traverse(function (c) { if (c.isBone && c.name) bonesByName[c.name.replace(/[._]/g, '')] = c; });
  var sk = meshes[0].skeleton;
  for (var i = 0; i < meshes.length; i++) if (meshes[i].skeleton.bones.indexOf(bonesByName['ひざL']) >= 0) sk = meshes[i].skeleton;

  function bindPos(b) {
    var i = sk.bones.indexOf(b); if (i < 0) return null;
    return new THREE.Vector3().setFromMatrixPosition(new THREE.Matrix4().copy(sk.boneInverses[i]).invert());
  }
  function livePos(b) { b.updateWorldMatrix(true, false); return new THREE.Vector3().setFromMatrixPosition(b.matrixWorld); }

  var knee = bonesByName['ひざL'], ank = bonesByName['足首L'];
  var bK = bindPos(knee), bA = bindPos(ank);
  var lK = livePos(knee), lA = livePos(ank);

  var lines = [];
  lines.push('bindKnee=' + bK.toArray().map(function (x) { return x.toFixed(3); }) +
    ' liveKnee=' + lK.toArray().map(function (x) { return x.toFixed(3); }));
  lines.push('bindAnkle=' + bA.toArray().map(function (x) { return x.toFixed(3); }) +
    ' liveAnkle=' + lA.toArray().map(function (x) { return x.toFixed(3); }));
  lines.push('bindShinLen=' + bK.distanceTo(bA).toFixed(4) + ' liveShinLen=' + lK.distanceTo(lA).toFixed(4));

  // Report the live world position of every leg bone we DO and DON'T drive.
  var legBones = ['足L', 'ひざL', '足首L', 'つま先L', '足捩L', 'ひざALTL', '足ＩＫL', 'つま先ＩＫL'];
  legBones.forEach(function (n) {
    var b = bonesByName[n];
    if (!b) { lines.push('  ' + n + ': MISSING'); return; }
    var bp = bindPos(b), lp = livePos(b);
    lines.push('  ' + n + ' bind=' + (bp ? bp.toArray().map(function (x) { return x.toFixed(3); }) : '?') +
      ' live=' + lp.toArray().map(function (x) { return x.toFixed(3); }));
  });

  // worst-stretching verts on the sock mesh
  var target = null;
  meshes.forEach(function (m) { if (m.material && m.material.name === 'body_socks') target = m; });
  if (!target) return lines.join('\n') + '\n(no sock mesh)';
  var g = target.geometry, pos = g.attributes.position, si = g.attributes.skinIndex, sw = g.attributes.skinWeight;
  var bm = target.bindMatrix, bmInv = new THREE.Matrix4().copy(bm).invert();
  var bKl = bK.clone().applyMatrix4(bmInv), bAl = bA.clone().applyMatrix4(bmInv);
  var lKl = lK.clone().applyMatrix4(bmInv), lAl = lA.clone().applyMatrix4(bmInv);
  var bax = new THREE.Vector3().subVectors(bAl, bKl); var bL = bax.length(); bax.divideScalar(bL);
  var lax = new THREE.Vector3().subVectors(lAl, lKl); var lL = lax.length(); lax.divideScalar(lL);

  var rows = [];
  var p = new THREE.Vector3(), tmp = new THREE.Vector3(), d = new THREE.Vector3();
  for (var k = 0; k < pos.count; k++) {
    p.set(0, 0, 0).fromBufferAttribute(pos, k);
    tmp.subVectors(p, bKl);
    var t = tmp.dot(bax); if (t < 0 || t > bL) continue;
    var rb = tmp.clone().addScaledVector(bax, -t).length(); if (rb > 0.07 || rb < 1e-4) continue;
    var q = new THREE.Vector3();
    target.boneTransform(k, q.fromBufferAttribute(pos, k));
    d.subVectors(q, lKl); var td = d.dot(lax);
    var rd = d.addScaledVector(lax, -td).length();
    var ratio = rd / rb;
    var w = [];
    for (var c = 0; c < 4; c++) {
      var ww = sw.array[k * 4 + c], bi = si.array[k * 4 + c];
      if (ww > 0.001) w.push((target.skeleton.bones[bi] ? target.skeleton.bones[bi].name.replace(/[._]/g, '') : '#' + bi) + ':' + ww.toFixed(2));
    }
    rows.push({ ratio: ratio, k: k, bindR: rb, defR: rd, w: w.join(' '), t: (t / bL) });
  }
  rows.sort(function (a, b) { return b.ratio - a.ratio; });
  lines.push('sock shin verts=' + rows.length);
  rows.slice(0, 6).forEach(function (r) {
    lines.push('  v' + r.k + ' ratio=' + r.ratio.toFixed(2) + ' bindR=' + r.bindR.toFixed(4) + ' defR=' + r.defR.toFixed(4) +
      ' t=' + r.t.toFixed(2) + ' bones=[' + r.w + ']');
  });
  return lines.join('\n');
})()
