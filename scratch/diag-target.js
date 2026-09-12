/* diag-target.js — measure the solo hand IK end to end.

   For each procedural hand we report:
     raw     = the her3-anchored target anim3d produced (_aimSm3)
     anchored= the same target after the GLB-landmark re-anchor in glbDriveRig
     wrist   = the MMD wrist bone's actual world position
     gap     = |wrist - anchored|
     reach   = upper + fore (how far the arm can physically get)
     shoulderToTarget = |shoulder - anchored|  (> reach => unreachable)

   Also dumps the landmark world positions the re-anchor uses. Compact text. */
(function () {
  if (!window.GLB_MODEL || !GLB_MODEL.model) return 'no model';
  var e = (typeof GLB_STORE !== 'undefined') && GLB_STORE[GLB_MODEL.key];
  if (!e || !e.rig) return 'no rig';
  var rig = e.rig;

  var bones = {};
  GLB_MODEL.model.traverse(function (c) { if (c.isBone && c.name) bones[c.name.replace(/[._]/g, '')] = c; });

  function wp(b) { if (!b) return null; b.updateWorldMatrix(true, false); return new THREE.Vector3().setFromMatrixPosition(b.matrixWorld); }
  function f(v) { return v ? v.toArray().map(function (x) { return +x.toFixed(3); }) : null; }
  function d(a, b) { return (a && b) ? +a.distanceTo(b).toFixed(3) : null; }

  var lines = [];
  lines.push('HAND3=' + JSON.stringify(typeof GLB_HAND3 !== 'undefined' ? GLB_HAND3 : null));
  lines.push('aimAge=' + JSON.stringify(typeof _aimSm3 !== 'undefined' ? _aimSm3.age : null));

  // landmarks the re-anchor depends on
  lines.push('--- landmarks');
  lines.push('  下半身     ' + JSON.stringify(f(wp(bones['下半身']))));
  lines.push('  乳親L      ' + JSON.stringify(f(wp(bones['乳親L']))));
  lines.push('  乳親R      ' + JSON.stringify(f(wp(bones['乳親R']))));
  try { lines.push('  clitorisW3 ' + JSON.stringify(f(clitorisWorld3(her3)))); } catch (err) { lines.push('  clitorisW3 ERR ' + err.message); }
  try { lines.push('  breastW3(-1) ' + JSON.stringify(f(breastWorld3(her3, -1)))); } catch (err) { lines.push('  breastW3(-1) ERR ' + err.message); }
  try { lines.push('  breastW3(+1) ' + JSON.stringify(f(breastWorld3(her3, 1)))); } catch (err) { lines.push('  breastW3(+1) ERR ' + err.message); }

  var OFF = (typeof GLB_LAND_OFF !== 'undefined') ? GLB_LAND_OFF : { breast: [0, 0.11, 0], mons: [0, 0.09, 0.06] };
  lines.push('LAND_OFF=' + JSON.stringify(OFF));

  var pairs = [['herL', 'R'], ['herR', 'L']];
  pairs.forEach(function (p) {
    var hk = p[0], ms = p[1];
    var A = rig.arms && rig.arms[ms];
    var raw = (typeof _aimSm3 !== 'undefined') ? _aimSm3[hk] : null;
    var task = (typeof GLB_HAND3 !== 'undefined') ? GLB_HAND3[ms] : null;
    lines.push('--- ' + hk + ' -> MMD ' + ms + ' land=' + (task && task.land));
    if (!A) { lines.push('  no arm rig'); return; }
    var sh = wp(A.sh), el = wp(A.el), wr = wp(A.wr);
    lines.push('  shoulder ' + JSON.stringify(f(sh)) + '  elbow ' + JSON.stringify(f(el)) + '  wrist ' + JSON.stringify(f(wr)));
    lines.push('  upper=' + A.upper.toFixed(4) + ' fore=' + A.fore.toFixed(4) + ' reach=' + (A.upper + A.fore).toFixed(4));
    lines.push('  raw target ' + JSON.stringify(f(raw)));

    // replicate the re-anchor exactly as glbDriveRig does it
    var aimTgt = raw, note = 'none';
    if (task && task.land && rig.land && raw) {
      var mb = null, off = null, her3pt = null;
      if (task.land === 'mons') {
        mb = rig.land.pelvis; off = OFF.mons;
        try { her3pt = clitorisWorld3(her3); } catch (err) { }
      } else if (task.land === 'breast') {
        var leftish = raw.x < 0;
        mb = leftish ? rig.land.breastR : rig.land.breastL;
        off = OFF.breast;
        try { her3pt = breastWorld3(her3, leftish ? -1 : 1); } catch (err) { }
      }
      if (mb && her3pt) {
        var gw = wp(mb).clone();
        gw.x += off[0]; gw.y += off[1]; gw.z += off[2];
        aimTgt = gw.sub(her3pt).add(raw);
        note = 'anchored(' + task.land + ' side=' + (raw.x < 0 ? 'herLeft(-x)' : 'herRight(+x)') + ')';
      } else {
        note = 'anchor SKIPPED (mb=' + !!mb + ' her3pt=' + !!her3pt + ')';
      }
    }
    lines.push('  anchored target ' + JSON.stringify(f(aimTgt)) + '  [' + note + ']');
    lines.push('  |wrist - anchored| = ' + d(wr, aimTgt));
    lines.push('  |shoulder - anchored| = ' + d(sh, aimTgt) + '   (reach ' + (A.upper + A.fore).toFixed(3) + ')');
    if (raw && aimTgt) lines.push('  re-anchor moved the target by ' + d(raw, aimTgt));
  });
  return lines.join('\n');
})()
