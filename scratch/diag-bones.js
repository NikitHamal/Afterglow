/* diag-bones.js — bind vs live position + live local euler for the leg/arm/hand
   chain, in SKELETON-WORLD space, plus the IK targets the rig is aiming at.
   Fast read-only probe. */
(function () {
  if (!window.GLB_MODEL || !GLB_MODEL.model) return JSON.stringify({ ok: false, why: 'no model' });
  var body = null, best = -1;
  GLB_MODEL.model.traverse(function (o) {
    if (o.isSkinnedMesh && o.geometry && o.geometry.attributes.position) {
      var c = o.geometry.attributes.position.count;
      if (c > best) { best = c; body = o; }
    }
  });
  if (!body) return JSON.stringify({ ok: false, why: 'no skinned mesh' });
  body.updateWorldMatrix(true, false);
  var sk = body.skeleton;

  var bones = {};
  GLB_MODEL.model.traverse(function (c) { if (c.isBone && c.name) bones[c.name.replace(/[._]/g, '')] = c; });

  var names = ['下半身', '腰', '上半身', '上半身2', '首', '頭',
    '腕L', 'ひじL', '手首L', '腕R', 'ひじR', '手首R',
    '足L', 'ひざL', '足首L', 'つま先L', '足R', 'ひざR', '足首R', 'つま先R',
    '乳親L', '乳親R'];

  function r(v) { return [+v.x.toFixed(4), +v.y.toFixed(4), +v.z.toFixed(4)]; }
  function deg(rad) { return +(rad * 180 / Math.PI).toFixed(1); }

  var out = { ok: true, mesh: body.name, verts: body.geometry.attributes.position.count, bones: {} };
  for (var i = 0; i < names.length; i++) {
    var b = bones[names[i]];
    if (!b) { out.bones[names[i]] = null; continue; }
    var bi = sk.bones.indexOf(b);
    var bind = null;
    if (bi >= 0) {
      var m = new THREE.Matrix4().copy(sk.boneInverses[bi]).invert();
      bind = r(new THREE.Vector3().setFromMatrixPosition(m));
    }
    b.updateWorldMatrix(true, false);
    var live = new THREE.Vector3().setFromMatrixPosition(b.matrixWorld);
    var e = b.rotation;
    out.bones[names[i]] = {
      bind: bind,
      live: r(live),
      local: [deg(e.x), deg(e.y), deg(e.z)],
      pos: r(b.position),
      scale: [+b.scale.x.toFixed(3), +b.scale.y.toFixed(3), +b.scale.z.toFixed(3)]
    };
  }

  // IK targets (procedural her3 hands) + the GLB landmark anchors
  try {
    if (typeof _aimSm3 !== 'undefined' && _aimSm3) {
      out.aim = {};
      ['herL', 'herR'].forEach(function (k) {
        var t = _aimSm3[k];
        out.aim[k] = (t && t.isVector3) ? r(t) : null;
        out.aim[k + '_age'] = (_aimSm3.age && _aimSm3.age[k]);
      });
    }
  } catch (e) { out.aim = 'err ' + e.message; }
  try {
    if (typeof GLB_HAND3 !== 'undefined') out.hand3 = JSON.parse(JSON.stringify(GLB_HAND3));
  } catch (e) {}
  try {
    if (typeof her3 !== 'undefined' && her3) {
      out.her3 = {};
      ['legL', 'legR'].forEach(function (lg) {
        if (!her3[lg]) return;
        out.her3[lg] = {};
        ['hip', 'knee', 'foot'].forEach(function (j) {
          var o = her3[lg][j];
          if (o && o.rotation) out.her3[lg][j] = [deg(o.rotation.x), deg(o.rotation.y), deg(o.rotation.z)];
        });
      });
    }
  } catch (e) {}
  return JSON.stringify(out);
})()
