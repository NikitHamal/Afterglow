/* diag-hand.js — is the finger curl actually reaching the mesh?
   For each finger, compare the bind direction (base->tip) with the live
   direction: a real curl rotates it by tens of degrees. Also measure palm
   thickness (extent along the hand's own normal axis) in bind vs deformed.

   Compact text output. Read-only. */
(function () {
  if (!window.GLB_MODEL || !GLB_MODEL.model) return 'no model';

  var meshes = [];
  GLB_MODEL.model.traverse(function (o) { if (o.isSkinnedMesh) meshes.push(o); });
  if (!meshes.length) return 'no skinned mesh';

  var bonesByName = {};
  GLB_MODEL.model.traverse(function (c) { if (c.isBone && c.name) bonesByName[c.name.replace(/[._]/g, '')] = c; });

  var sk = meshes[0].skeleton;
  var bindInv = new THREE.Matrix4().copy(meshes[0].bindMatrix).invert();

  function bindPos(b) {
    var i = sk.bones.indexOf(b);
    if (i < 0) return null;
    return new THREE.Vector3().setFromMatrixPosition(new THREE.Matrix4().copy(sk.boneInverses[i]).invert());
  }
  function livePos(b) {
    b.updateWorldMatrix(true, false);
    return new THREE.Vector3().setFromMatrixPosition(b.matrixWorld);
  }
  function angleBetween(a, b) {
    var d = Math.max(-1, Math.min(1, a.dot(b)));
    return +(Math.acos(d) * 180 / Math.PI).toFixed(1);
  }

  var lines = [];
  lines.push('GLB_HAND3=' + JSON.stringify(typeof GLB_HAND3 !== 'undefined' ? GLB_HAND3 : null));
  lines.push('PH_gain=' + JSON.stringify(typeof GLB_PARTS !== 'undefined' ? GLB_PARTS.hands : null));

  var FINGERS = ['親指', '人指', '中指', '薬指', '小指'];
  ['L', 'R'].forEach(function (side) {
    lines.push('--- side ' + side);
    FINGERS.forEach(function (f) {
      var segs = [];
      for (var d = 0; d <= 3; d++) {
        var b = bonesByName[f + '０１２３'[d] + side];
        if (b) segs.push(b);
      }
      if (segs.length < 2) { lines.push('  ' + f + ' missing'); return; }
      var b0 = bindPos(segs[0]), b1 = bindPos(segs[segs.length - 1]);
      var l0 = livePos(segs[0]), l1 = livePos(segs[segs.length - 1]);
      var bd = new THREE.Vector3().subVectors(b1, b0).normalize();
      var ld = new THREE.Vector3().subVectors(l1, l0).normalize();
      // local X rotation actually present on each segment
      var eul = segs.map(function (b) {
        return [+(b.rotation.x * 180 / Math.PI).toFixed(1), +(b.rotation.y * 180 / Math.PI).toFixed(1), +(b.rotation.z * 180 / Math.PI).toFixed(1)];
      });
      lines.push('  ' + f + ' segs=' + segs.length +
        ' bendDeg=' + angleBetween(bd, ld) +
        ' lenBind=' + b0.distanceTo(b1).toFixed(4) + ' lenLive=' + l0.distanceTo(l1).toFixed(4) +
        ' localEuler=' + JSON.stringify(eul));
    });
  });

  // ---- palm thickness: use 手首L local axes. Local Y = along bone; the palm
  // normal should be roughly local Z or X. Measure vert spread along all 3.
  (function () {
    var wr = bonesByName['手首L'];
    if (!wr) return;
    var body = meshes.reduce(function (a, b) {
      return b.geometry.attributes.position.count > a.geometry.attributes.position.count ? b : a;
    });
    body.updateWorldMatrix(true, false);
    var pos = body.geometry.attributes.position;
    var bm = body.bindMatrix, bmInv = new THREE.Matrix4().copy(bm).invert();
    var b0 = bindPos(wr);
    var inLocal = new THREE.Vector3().copy(b0).applyMatrix4(bmInv);
    // gather verts within 8cm of the wrist bone origin in mesh-local bind space
    var sel = [];
    var v = new THREE.Vector3();
    for (var i = 0; i < pos.count; i++) {
      v.set(0, 0, 0).fromBufferAttribute(pos, i);
      if (v.distanceTo(inLocal) < 0.09) sel.push(i);
    }
    if (!sel.length) { lines.push('palm: no verts'); return; }
    // axes: use the LIVE wrist matrix columns, in mesh-local space
    wr.updateWorldMatrix(true, false);
    var M = new THREE.Matrix4().multiplyMatrices(bmInv, wr.matrixWorld);
    var axX = new THREE.Vector3().setFromMatrixColumn(M, 0).normalize();
    var axY = new THREE.Vector3().setFromMatrixColumn(M, 1).normalize();
    var axZ = new THREE.Vector3().setFromMatrixColumn(M, 2).normalize();
    var rng = { x: [1e9, -1e9], y: [1e9, -1e9], z: [1e9, -1e9] };
    var p = new THREE.Vector3(), d = new THREE.Vector3();
    for (var k = 0; k < sel.length; k++) {
      p.set(0, 0, 0);
      body.boneTransform(sel[k], p.fromBufferAttribute(pos, sel[k]));
      d.subVectors(p, inLocal);
      var cx = d.dot(axX), cy = d.dot(axY), cz = d.dot(axZ);
      if (cx < rng.x[0]) rng.x[0] = cx; if (cx > rng.x[1]) rng.x[1] = cx;
      if (cy < rng.y[0]) rng.y[0] = cy; if (cy > rng.y[1]) rng.y[1] = cy;
      if (cz < rng.z[0]) rng.z[0] = cz; if (cz > rng.z[1]) rng.z[1] = cz;
    }
    lines.push('palmL n=' + sel.length +
      ' spanX=' + (rng.x[1] - rng.x[0]).toFixed(4) +
      ' spanY=' + (rng.y[1] - rng.y[0]).toFixed(4) +
      ' spanZ=' + (rng.z[1] - rng.z[0]).toFixed(4));
  })();

  return lines.join('\n');
})()
