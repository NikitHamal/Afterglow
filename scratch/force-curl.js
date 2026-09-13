/* force-curl.js — after the normal drive, overwrite every finger's curl with
   window.__forceCurl (0..1) and zero the fan. Lets us verify the finger chain
   really reaches the mesh, independent of whatever task anim3d assigned. */
(function () {
  if (!window.__origDrive) window.__origDrive = glbDriveRig;
  window.__forceCurl = (window.__forceCurl == null) ? 0.6 : window.__forceCurl;
  glbDriveRig = function (e, dt) {
    window.__origDrive(e, dt);
    var c = window.__forceCurl;
    if (c == null) return;
    var r = e && e.rig;
    if (!r) return;
    var decay = [0.95, 0.75, 0.55, 0.38];
    r.fingers.forEach(function (f) {
      var chain = [f.base].concat(f.segs);
      var curlK = f.thumb ? 0.55 : 1.0;
      for (var i = 0; i < chain.length; i++) {
        var s = chain[i];
        if (!s || !s.dst) continue;
        var k = decay[Math.min(i, decay.length - 1)] * curlK;
        s.dst.rotation.x = s.bx + c * 0.85 * k;
        s.dst.rotation.z = s.bz;
      }
    });
  };
  return 'force-curl installed, curl=' + window.__forceCurl;
})()
