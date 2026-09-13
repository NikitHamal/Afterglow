// Landmark audit: shoulder roots + body targets in BOTH rigs, to correct
// her3-anchored IK targets onto GLB anatomy.
(function () {
  var out = {};
  function wpos(obj) {
    obj.updateWorldMatrix(true, false);
    var v = new THREE.Vector3();
    obj.getWorldPosition(v);
    return [+v.x.toFixed(3), +v.y.toFixed(3), +v.z.toFixed(3)];
  }
  try {
    var E = GLB_STORE.goatchan, bones = {};
    E.model.traverse(function (c) { if (c.isBone && c.name) bones[c.name] = c; });
    function find(clean) {
      var ks = Object.keys(bones);
      for (var i = 0; i < ks.length; i++) if (ks[i].replace(/[._]/g, '') === clean) return bones[ks[i]];
      return null;
    }
    ['腕L', '腕R', '手首L', '手首R', '下半身', '腰', '上半身', '上半身2', '乳親L', '乳親R'].forEach(function (cl) {
      var b = find(cl);
      out['mmd_' + cl] = b ? wpos(b) : null;
    });
    out.her3_shL = wpos(her3.armL.shoulder);
    out.her3_shR = wpos(her3.armR.shoulder);
    out.her3_handL = wpos(her3.armL.hand);
    out.her3_handR = wpos(her3.armR.hand);
    out.her3_clit = clitorisWorld3(her3).toArray().map(function (v) { return +v.toFixed(3); });
    out.her3_breastL = breastWorld3(her3, -1).toArray().map(function (v) { return +v.toFixed(3); });
    out.her3_breastR = breastWorld3(her3, 1).toArray().map(function (v) { return +v.toFixed(3); });
    out.aimL = _aimSm3.herL.toArray().map(function (v) { return +v.toFixed(3); });
    out.aimR = _aimSm3.herR.toArray().map(function (v) { return +v.toFixed(3); });
    out.taskL = JSON.parse(JSON.stringify(GLB_HAND3.L));
    out.taskR = JSON.parse(JSON.stringify(GLB_HAND3.R));
  } catch (err) { out.err = String((err && err.message) || err); }
  return out;
})()
