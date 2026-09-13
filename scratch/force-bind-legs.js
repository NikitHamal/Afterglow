/* force-bind-legs.js — freeze legs+feet at TRUE bind (gain=0 only freezes, so
   write bind eulers explicitly) to see the undeformed shin/foot. */
(function () {
  GLB_PARTS.legs.gain = 0;
  GLB_PARTS.feet.gain = 0;
  var e = GLB_STORE[GLB_MODEL.key];
  var r = e && e.rig;
  if (!r) return 'no rig';
  var n = 0;
  r.bones.forEach(function (b) {
    if (b.part === 'legs' || b.part === 'feet') { b.dst.rotation.set(b.bx, b.by, b.bz); n++; }
  });
  return JSON.stringify({ reset: n, legsGain: GLB_PARTS.legs.gain, feetGain: GLB_PARTS.feet.gain });
})()
