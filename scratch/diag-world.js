// World-space limb direction audit: where does the inward shin + rolled foot come from?
(function () {
  var out = {};
  function wdir(obj, lx, ly, lz) {
    var v = new THREE.Vector3(lx, ly, lz);
    obj.updateWorldMatrix(true, false);
    v.transformDirection(obj.matrixWorld);
    return [+v.x.toFixed(3), +v.y.toFixed(3), +v.z.toFixed(3)];
  }
  function euler(o) { return [+o.rotation.x.toFixed(3), +o.rotation.y.toFixed(3), +o.rotation.z.toFixed(3)]; }
  try {
    var E = GLB_STORE.goatchan;
    out.loaded = !!(E && E.loaded);
    // find MMD bones by cleaned name
    var bones = {};
    E.model.traverse(function (c) {
      if (!c.isBone || !c.name) return;
      var k = c.name.replace(/[._]/g, '');
      if (['足L', '足R', 'ひざL', 'ひざR', '足首L', '足首R'].indexOf(k) >= 0) bones[k] = c;
    });
    out.found = Object.keys(bones);
    ['L', 'R'].forEach(function (s) {
      var hip = bones['足' + s], knee = bones['ひざ' + s], ank = bones['足首' + s];
      // MMD leg bones point down local -Y; shin dir = knee's -Y in world; foot fwd = ankle +Z?
      out['thigh' + s] = { dirY: wdir(hip, 0, -1, 0), e: euler(hip) };
      out['shin' + s] = { dirY: wdir(knee, 0, -1, 0), e: euler(knee) };
      out['foot' + s] = { dirY: wdir(ank, 0, -1, 0), dirZ: wdir(ank, 0, 0, 1), e: euler(ank) };
    });
    // her3 reference: same -Y dirs
    out.her3 = {
      thighL: wdir(her3.legL.hip, 0, -1, 0),
      shinL: wdir(her3.legL.knee, 0, -1, 0),
      thighR: wdir(her3.legR.hip, 0, -1, 0),
      shinR: wdir(her3.legR.knee, 0, -1, 0),
      rootQ: her3.root.quaternion.toArray().map(function (v) { return +v.toFixed(3); })
    };
  } catch (e) { out.err = String((e && e.message) || e); }
  return out;
})()
