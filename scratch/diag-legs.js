// Dumps live her3 joint eulers vs Goat-chan MMD bone bind + current eulers.
(function () {
  var out = {};
  try { out.solo = !!G.solo; out.state = G.state; } catch (e) { out.gErr = String(e); }
  try {
    var E = GLB_STORE.goatchan;
    out.loaded = !!(E && E.loaded);
    out.rigBones = E && E.rig ? E.rig.bones.length : -1;
    out.fingers = E && E.rig ? E.rig.fingers.length : -1;
    if (E && E.rig) {
      out.bones = E.rig.bones.map(function (b) {
        var s = null;
        try {
          var o = her3, bits = b.path.split('.');
          for (var i = 0; i < bits.length; i++) o = o[bits[i]];
          s = [+o.rotation.x.toFixed(3), +o.rotation.y.toFixed(3), +o.rotation.z.toFixed(3)];
        } catch (_) { s = null; }
        return {
          path: b.path,
          src: s,
          bind: [+b.bx.toFixed(3), +b.by.toFixed(3), +b.bz.toFixed(3)],
          now: [+b.dst.rotation.x.toFixed(3), +b.dst.rotation.y.toFixed(3), +b.dst.rotation.z.toFixed(3)]
        };
      });
    }
  } catch (e) { out.rigErr = String((e && e.message) || e); }
  return out;
})()
