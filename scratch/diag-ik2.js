// IK bisection: force-snap arms to the IK solution, measure, wait, measure
// again. If the snap lands but the loop loses it -> loop dynamics bug.
// If the snap itself misses -> geometric bug (lengths/rest/parent).
(async function () {
  var out = { snap1: {}, snap2: {} };
  function wpos(o) { o.updateWorldMatrix(true, false); var v = new THREE.Vector3(); o.getWorldPosition(v); return v; }
  function wdir(o, x, y, z) { var v = new THREE.Vector3(x, y, z); o.updateWorldMatrix(true, false); v.transformDirection(o.matrixWorld); return v; }
  function bones() {
    var b = {};
    GLB_STORE.goatchan.model.traverse(function (c) { if (c.isBone && c.name) b[c.name.replace(/[._]/g, '')] = c; });
    return b;
  }
  var B = bones();
  var A = GLB_STORE.goatchan.rig.arms.R; // MMD-R <- herL (pose plant)
  function meas(tag) {
    var tgt = _aimSm3.herL;
    var sp = wpos(A.sh), wp = wpos(A.wr);
    var toT = tgt.clone().sub(sp).normalize();
    var upY = wdir(A.sh, 0, 1, 0);
    var cos = Math.max(-1, Math.min(1, upY.x * toT.x + upY.y * toT.y + upY.z * toT.z));
    var hp = wpos(her3.armL.hand);
    out[tag] = {
      tgt: tgt.toArray().map(function (v) { return +v.toFixed(3); }),
      mmdWristGap: +wp.distanceTo(tgt).toFixed(3),
      herWristGap: +hp.distanceTo(tgt).toFixed(3),
      aimErrDeg: +(Math.acos(cos) * 180 / Math.PI).toFixed(1),
      mmdElbowX: +A.el.rotation.x.toFixed(3),
      herElbowX: +her3.armL.elbow.rotation.x.toFixed(3)
    };
  }
  // force-snap both rigs (huge rates = track 1.0)
  aimArmAt3(her3.armL, _aimSm3.herL, her3.R.upperArm, her3.R.foreArm, -0.15, 1000, 0.016, 1000);
  aimArmAt3({ shoulder: A.sh, elbow: A.el }, _aimSm3.herL, A.upper, A.fore, -0.22, 1000, 0.016, 1000, new THREE.Vector3(0, 1, 0));
  meas('snap1');
  await new Promise(function (r) { setTimeout(r, 1500); });
  meas('snap2');
  return out;
})()
