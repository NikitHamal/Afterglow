// Full leg+hand audit: bind vs live eulers, BOTH ±Y world dirs (kills the
// axis-sign ambiguity), finger chain inventory, her3 reference. Sync, fast.
(function () {
  var out = { legs: {}, hands: {}, fingers: [] };
  function wdir(obj, x, y, z) {
    var v = new THREE.Vector3(x, y, z);
    obj.updateWorldMatrix(true, false);
    v.transformDirection(obj.matrixWorld);
    return [+v.x.toFixed(3), +v.y.toFixed(3), +v.z.toFixed(3)];
  }
  function eul(o) { return [+o.rotation.x.toFixed(3), +o.rotation.y.toFixed(3), +o.rotation.z.toFixed(3)]; }
  try {
    var E = GLB_STORE.goatchan, rig = E.rig;
    out.loaded = !!E.loaded;
    out.parts = JSON.parse(JSON.stringify(GLB_PARTS));
    var bind = {};
    (rig.bones || []).forEach(function (b) {
      bind[b.dst.name] = { path: b.path, part: b.part, bx: +b.bx.toFixed(3), by: +b.by.toFixed(3), bz: +b.bz.toFixed(3) };
    });
    var bones = {};
    E.model.traverse(function (c) {
      if (c.isBone && c.name) bones[c.name] = c;
    });
    function find(clean) {
      var keys = Object.keys(bones);
      for (var i = 0; i < keys.length; i++) {
        if (keys[i].replace(/[._]/g, '') === clean) return bones[keys[i]];
      }
      return null;
    }
    ['足L', '足R', 'ひざL', 'ひざR', '足首L', '足首R', 'つま先L', 'つま先R',
     '手首L', '手首R', '腕L', '腕R', 'ひじL', 'ひじR'].forEach(function (clean) {
      var b = find(clean);
      if (!b) { out.legs[clean] = null; return; }
      out.legs[clean] = {
        name: b.name,
        bind: bind[b.name] || null,
        live: eul(b),
        plusY: wdir(b, 0, 1, 0),
        minusY: wdir(b, 0, -1, 0),
        plusZ: wdir(b, 0, 0, 1)
      };
    });
    // finger inventory: name, parent, bind, live
    E.model.traverse(function (c) {
      if (!c.isBone || !c.name) return;
      if (/指/.test(c.name)) {
        out.fingers.push({
          name: c.name,
          parent: c.parent ? c.parent.name : null,
          live: eul(c)
        });
      }
    });
    // rig finger entries carry bind
    out.rigFingers = (rig.fingers || []).map(function (f) {
      return { name: f.dst.name, side: f.side, bx: +f.bx.toFixed(3), by: +f.by.toFixed(3), bz: +f.bz.toFixed(3), live: eul(f.dst) };
    });
    // her3 reference, both conventions
    out.her3 = {
      thighL: { m: wdir(her3.legL.hip, 0, -1, 0), e: eul(her3.legL.hip) },
      shinL: { m: wdir(her3.legL.knee, 0, -1, 0), e: eul(her3.legL.knee) },
      thighR: { m: wdir(her3.legR.hip, 0, -1, 0), e: eul(her3.legR.hip) },
      shinR: { m: wdir(her3.legR.knee, 0, -1, 0), e: eul(her3.legR.knee) },
      handL: { e: eul(her3.armL.hand) },
      handR: { e: eul(her3.armR.hand) },
      wristL: { e: eul(her3.armL.hand) }
    };
  } catch (err) { out.err = String((err && err.message) || err); }
  return out;
})()
