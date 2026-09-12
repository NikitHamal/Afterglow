/* force-bind-arms.js — put the GLB arms + hands into a TRUE bind pose so we can
   see what the mesh looks like undeformed.
   gain=0 only FREEZES a part (the drive loop `continue`s), so we must also
   neutralise the arm IK (which is not gain-gated) and write bind eulers. */
(function () {
  GLB_PARTS.arms.gain = 0;
  GLB_PARTS.hands.gain = 0;
  // kill arm IK for this session (global function → hot-patchable)
  window.__origAimArmAt3 = window.__origAimArmAt3 || aimArmAt3;
  aimArmAt3 = function () { };
  var e = GLB_STORE[GLB_MODEL.key];
  var r = e && e.rig;
  if (!r) return 'no rig';
  var n = 0;
  r.bones.forEach(function (b) {
    if (b.part === 'arms' || b.part === 'hands') { b.dst.rotation.set(b.bx, b.by, b.bz); n++; }
  });
  r.fingers.forEach(function (f) {
    [f.base].concat(f.segs).forEach(function (s) {
      if (s) { s.dst.rotation.set(s.bx, s.by, s.bz); n++; }
    });
  });
  return JSON.stringify({ reset: n, armsGain: GLB_PARTS.arms.gain, handsGain: GLB_PARTS.hands.gain });
})()
