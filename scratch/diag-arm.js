// Arm IK feasibility: bone offsets (lengths) + elbow hinge-axis probe.
(function () {
  var out = { arms: {}, probe: null };
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
    ['L', 'R'].forEach(function (s) {
      var sh = find('腕' + s), el = find('ひじ' + s), wr = find('手首' + s);
      out.arms[s] = {
        elbowOffset: [el.position.x, el.position.y, el.position.z].map(function (v) { return +v.toFixed(4); }),
        wristOffset: [wr.position.x, wr.position.y, wr.position.z].map(function (v) { return +v.toFixed(4); }),
        elbowE: [+el.rotation.x.toFixed(3), +el.rotation.y.toFixed(3), +el.rotation.z.toFixed(3)]
      };
    });
    // hinge probe: rotate elbow per-axis, watch wrist displacement
    var el = find('ひじL'), wr = find('手首L');
    var p0 = wpos(wr), res = {};
    ['x', 'y', 'z'].forEach(function (ax) {
      var save = el.rotation[ax];
      el.rotation[ax] = save + 0.4;
      el.updateWorldMatrix(true, true);
      var p = wpos(wr);
      res['plus_' + ax] = [+(p[0] - p0[0]).toFixed(3), +(p[1] - p0[1]).toFixed(3), +(p[2] - p0[2]).toFixed(3)];
      el.rotation[ax] = save;
    });
    el.updateWorldMatrix(true, true);
    out.probe = { disp: res, restored: wpos(wr), p0: p0 };
  } catch (err) { out.err = String((err && err.message) || err); }
  return out;
})()
