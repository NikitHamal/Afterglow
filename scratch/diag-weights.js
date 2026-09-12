/* diag-weights.js — which bones own the vertices in a given limb segment?
   For every skinned mesh, bucket vertices lying near the BIND shin / foot /
   hand segments and report the dominant bone weights. A vertex weighted to a
   bone we never drive (e.g. 足ＩＫ / 足捩 / 腕捩) will be dragged or left
   behind — that is the classic cause of a limb that stretches.

   Compact text. Read-only. */
(function () {
  if (!window.GLB_MODEL || !GLB_MODEL.model) return 'no model';

  var meshes = [];
  GLB_MODEL.model.traverse(function (o) { if (o.isSkinnedMesh) meshes.push(o); });
  if (!meshes.length) return 'no skinned mesh';

  var bonesByName = {};
  GLB_MODEL.model.traverse(function (c) { if (c.isBone && c.name) bonesByName[c.name.replace(/[._]/g, '')] = c; });

  var sk = null;
  for (var i = 0; i < meshes.length; i++) if (meshes[i].skeleton.bones.indexOf(bonesByName['ひざL']) >= 0) { sk = meshes[i].skeleton; break; }
  if (!sk) return 'no leg skeleton';

  function bindPos(b) {
    var i = sk.bones.indexOf(b);
    if (i < 0) return null;
    return new THREE.Vector3().setFromMatrixPosition(new THREE.Matrix4().copy(sk.boneInverses[i]).invert());
  }
  var kneeL = bindPos(bonesByName['ひざL']), ankL = bindPos(bonesByName['足首L']);
  var toeL = bindPos(bonesByName['つま先L']);
  var wrL = bindPos(bonesByName['手首L']), elL = bindPos(bonesByName['ひじL']);

  var SEGS = {
    shinL: [kneeL, ankL, 0.07],
    footL: [ankL, toeL, 0.07],
    handL: [wrL, null, 0.075]   // sphere around wrist
  };

  function near(p, seg) {
    if (!seg[0]) return false;
    if (!seg[1]) return p.distanceTo(seg[0]) <= seg[2];
    var ax = new THREE.Vector3().subVectors(seg[1], seg[0]);
    var L = ax.length(); if (L < 1e-6) return false; ax.divideScalar(L);
    var t = new THREE.Vector3().subVectors(p, seg[0]).dot(ax);
    if (t < -0.02 || t > L + 0.02) return false;
    return new THREE.Vector3().subVectors(p, seg[0]).addScaledVector(ax, -t).length() <= seg[2];
  }

  var lines = [];
  meshes.forEach(function (mesh) {
    var g = mesh.geometry;
    if (!g.attributes.position || !g.attributes.skinIndex) return;
    var pos = g.attributes.position, si = g.attributes.skinIndex, sw = g.attributes.skinWeight;
    var bm = mesh.bindMatrix;
    var per = {};
    var v = new THREE.Vector3();
    for (var k = 0; k < pos.count; k++) {
      v.set(0, 0, 0).fromBufferAttribute(pos, k).applyMatrix4(bm);
      Object.keys(SEGS).forEach(function (name) {
        if (!near(v, SEGS[name])) return;
        var acc = per[name] || (per[name] = {});
        for (var c = 0; c < 4; c++) {
          var w = sw.getComponent ? sw.getComponent(k, c) : sw.array[k * 4 + c];
          if (!w) continue;
          var bi = si.getComponent ? si.getComponent(k, c) : si.array[k * 4 + c];
          var bn = mesh.skeleton.bones[bi] ? mesh.skeleton.bones[bi].name.replace(/[._]/g, '') : ('#' + bi);
          acc[bn] = (acc[bn] || 0) + w;
        }
      });
    }
    Object.keys(per).forEach(function (name) {
      var arr = Object.keys(per[name]).map(function (b) { return [b, per[name][b]]; });
      arr.sort(function (a, b) { return b[1] - a[1]; });
      var tot = arr.reduce(function (s, x) { return s + x[1]; }, 0);
      lines.push(mesh.name + ' [' + name + '] bones=' + arr.slice(0, 7).map(function (x) {
        return x[0] + ':' + (100 * x[1] / tot).toFixed(0) + '%';
      }).join(' '));
    });
  });
  return lines.join('\n');
})()
