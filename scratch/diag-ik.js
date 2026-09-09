// IK convergence audit: aim error per arm, elbow states, ownership flags.
(function () {
  var out = { arms: {} };
  function wpos(obj) {
    obj.updateWorldMatrix(true, false);
    var v = new THREE.Vector3();
    obj.getWorldPosition(v);
    return v;
  }
  function wdir(obj, x, y, z) {
    var v = new THREE.Vector3(x, y, z);
    obj.updateWorldMatrix(true, false);
    v.transformDirection(obj.matrixWorld);
    return v;
  }
  try {
    var E = GLB_STORE.goatchan, bones = {};
    E.model.traverse(function (c) { if (c.isBone && c.name) bones[c.name] = c; });
    function find(clean) {
      var ks = Object.keys(bones);
      for (var i = 0; i < ks.length; i++) if (ks[i].replace(/[._]/g, '') === clean) return bones[ks[i]];
      return null;
    }
    out.baseScale = +E.baseScale.toFixed(4);
    [['herL', 'R', 'armL'], ['herR', 'L', 'armR']].forEach(function (t) {
      var hk = t[0], ms = t[1], hp = t[2];
      var sh = find('腕' + ms), el = find('ひじ' + ms), wr = find('手首' + ms);
      var tgt = _aimSm3[hk];
      var sp = wpos(sh), wp = wpos(wr);
      var toT = tgt.clone().sub(sp); var dist = +toT.length().toFixed(3); toT.normalize();
      var upY = wdir(sh, 0, 1, 0);
      var cos = upY.x * toT.x + upY.y * toT.y + upY.z * toT.z;
      var herSh = her3[hp].shoulder, herEl = her3[hp].elbow;
      out.arms[hk] = {
        mmdSide: ms,
        age: +_aimSm3.age[hk].toFixed(3), drv: _aimSm3.drv[hk], init: _aimSm3.init[hk],
        dist: dist,
        reach: +((el.position.length() + wr.position.length()) * E.baseScale).toFixed(3),
        aimErrDeg: +(Math.acos(Math.max(-1, Math.min(1, cos))) * 180 / Math.PI).toFixed(1),
        mmdElbowX: +el.rotation.x.toFixed(3),
        herElbowX: +herEl.rotation.x.toFixed(3),
        wristGap: +wp.distanceTo(tgt).toFixed(3),
        task: JSON.parse(JSON.stringify(GLB_HAND3[ms]))
      };
    });
  } catch (err) { out.err = String((err && err.message) || err); }
  return out;
})()
