// Leg positions (crossing check) + finger curl-axis probe + foot frame.
// All sync math except nothing — no settle waits needed.
(function () {
  var out = { joints: {}, fingers: null, foot: {} };
  function wpos(obj) {
    obj.updateWorldMatrix(true, false);
    var v = new THREE.Vector3();
    obj.getWorldPosition(v);
    return [+v.x.toFixed(3), +v.y.toFixed(3), +v.z.toFixed(3)];
  }
  function wdir(obj, x, y, z) {
    var v = new THREE.Vector3(x, y, z);
    obj.updateWorldMatrix(true, false);
    v.transformDirection(obj.matrixWorld);
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
    ['足L', '足R', 'ひざL', 'ひざR', '足首L', '足首R', 'つま先L', 'つま先R'].forEach(function (cl) {
      var b = find(cl);
      out.joints[cl] = b ? wpos(b) : null;
    });
    // Arm tracking: does each MMD wrist sit where its her3 hand is?
    // (MMD L = anatomical L = her3 "R" chain after the side swap.)
    [['手首L', her3.armR.hand], ['手首R', her3.armL.hand]].forEach(function (pair) {
      var b = find(pair[0]);
      var pw = wpos(b), ph = wpos(pair[1]);
      var d = Math.hypot(pw[0] - ph[0], pw[1] - ph[1], pw[2] - ph[2]);
      out.joints[pair[0]] = { wrist: pw, herHand: ph, gap: +d.toFixed(3) };
    });
    out.joints.her3 = {
      hipL: wpos(her3.legL.hip), kneeL: wpos(her3.legL.knee), footL: wpos(her3.legL.foot),
      hipR: wpos(her3.legR.hip), kneeR: wpos(her3.legR.knee), footR: wpos(her3.legR.foot)
    };
    // foot frame: which local axis lifts the toes?
    var ank = find('足首L');
    out.foot.frameL = { X: wdir(ank, 1, 0, 0), Y: wdir(ank, 0, 1, 0), Z: wdir(ank, 0, 0, 1) };
    // finger probe: rotate 中指０L per-axis, watch fingertip (中指３L origin)
    var base = find('中指０L'), tip = find('中指３L'), wrist = find('手首L');
    var tip0 = wpos(tip);
    out.fingers = {
      tip0: tip0,
      wristFrame: { X: wdir(wrist, 1, 0, 0), Y: wdir(wrist, 0, 1, 0), Z: wdir(wrist, 0, 0, 1) },
      wristE: [+wrist.rotation.x.toFixed(3), +wrist.rotation.y.toFixed(3), +wrist.rotation.z.toFixed(3)],
      baseE: [+base.rotation.x.toFixed(3), +base.rotation.y.toFixed(3), +base.rotation.z.toFixed(3)],
      delta: {}
    };
    ['x', 'y', 'z'].forEach(function (ax) {
      var save = base.rotation[ax];
      base.rotation[ax] = save + 0.5;
      base.updateWorldMatrix(true, true);
      var p = wpos(tip);
      out.fingers.delta['plus_' + ax] = [
        +(p[0] - tip0[0]).toFixed(3),
        +(p[1] - tip0[1]).toFixed(3),
        +(p[2] - tip0[2]).toFixed(3)
      ];
      base.rotation[ax] = save;
    });
    base.updateWorldMatrix(true, true);
    out.fingers.restored = wpos(tip);
  } catch (err) { out.err = String((err && err.message) || err); }
  return out;
})()
