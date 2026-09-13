/* isolate-fingers.js — freeze the whole arm at bind but keep the FINGER drive
   alive, so the hand stays put and we can watch exactly what curl/spread do
   to the fingers. Read-only for the model; only writes rig rotations. */
(function () {
  GLB_PARTS.arms.gain = 0;    // shoulders + elbows frozen
  GLB_PARTS.hands.gain = 1;   // wrist + fingers still driven
  if (!window.__origAimArmAt3) window.__origAimArmAt3 = aimArmAt3;
  aimArmAt3 = function () { };  // arm IK is not gain-gated; kill it
  var e = GLB_STORE[GLB_MODEL.key];
  var r = e && e.rig;
  if (!r) return 'no rig';
  var n = 0;
  r.bones.forEach(function (b) {
    if (b.part === 'arms') { b.dst.rotation.set(b.bx, b.by, b.bz); n++; }
  });
  return JSON.stringify({ frozenArms: n, handsGain: GLB_PARTS.hands.gain });
})()
