// Response-surface sweep: settle-aware probe of hip/ankle drive response.
// Sets GLB_PARTS tweaks live, awaits damping settle, records world dirs.
// (Tweaks are diagnosis-only; the chosen values get hard-coded after.)
(async function () {
  var out = { probes: [] };
  function wdir(obj, lx, ly, lz) {
    var v = new THREE.Vector3(lx, ly, lz);
    obj.updateWorldMatrix(true, false);
    v.transformDirection(obj.matrixWorld);
    return [+v.x.toFixed(3), +v.y.toFixed(3), +v.z.toFixed(3)];
  }
  function bones() {
    var b = {};
    GLB_STORE.goatchan.model.traverse(function (c) {
      if (!c.isBone || !c.name) return;
      var k = c.name.replace(/[._]/g, '');
      if (['足L', 'ひざL', '足首L'].indexOf(k) >= 0) b[k] = c;
    });
    return b;
  }
  var B = bones();
  function snap(tag) {
    // GLB leg +Y == anatomical down-leg (established vs her3 -Y)
    out.probes.push({
      tag: tag,
      tweakLegs: GLB_PARTS.legs.tweak.slice(),
      tweakFeet: GLB_PARTS.feet.tweak.slice(),
      thigh: wdir(B['足L'], 0, 1, 0),
      shin: wdir(B['ひざL'], 0, 1, 0),
      footFwd: wdir(B['足首L'], 0, 0, 1),
      hipE: [+B['足L'].rotation.x.toFixed(3), +B['足L'].rotation.y.toFixed(3), +B['足L'].rotation.z.toFixed(3)],
      ankE: [+B['足首L'].rotation.x.toFixed(3), +B['足首L'].rotation.y.toFixed(3), +B['足首L'].rotation.z.toFixed(3)]
    });
  }
  function wait(ms) { return new Promise(function (r) { setTimeout(r, ms); }); }
  snap('base');
  var legZ = [0.13, 0.26, -0.13, -0.26];
  for (var i = 0; i < legZ.length; i++) {
    glbSetTweak('legs', 0, 0, legZ[i]);
    await wait(1500);
    snap('legZ=' + legZ[i]);
  }
  glbSetTweak('legs', 0, 0, 0);
  var legX = [0.1, 0.2];
  for (var j = 0; j < legX.length; j++) {
    glbSetTweak('legs', legX[j], 0, 0);
    await wait(1500);
    snap('legX=' + legX[j]);
  }
  glbSetTweak('legs', 0, 0, 0);
  var feetX = [0.3, 0.6, 0.9];
  for (var k = 0; k < feetX.length; k++) {
    glbSetTweak('feet', feetX[k], 0, 0);
    await wait(1500);
    snap('feetX=' + feetX[k]);
  }
  glbSetTweak('feet', 0, 0, 0);
  await wait(800);
  return out;
})()
