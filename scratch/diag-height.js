/* diag-height.js — pose-independent size comparison between the procedural
   girl and the loaded GLB girl, so a couple-fit scale can be derived.
   Reports each rig's LOCAL-space vertical span (bind), plus a few limb
   lengths that are stable across poses. Compact text. */
(function () {
  var lines = [];

  // --- GLB: bind-space bbox from geometry positions (mesh-local == bind)
  if (window.GLB_MODEL && GLB_MODEL.model) {
    var mn = [1e9, 1e9, 1e9], mx = [-1e9, -1e9, -1e9];
    GLB_MODEL.model.traverse(function (o) {
      if (!o.isMesh || !o.geometry || !o.geometry.attributes.position) return;
      var p = o.geometry.attributes.position;
      var m = o.matrixWorld;
      for (var i = 0; i < p.count; i += 3) {
        var x = p.getX(i), y = p.getY(i), z = p.getZ(i);
        if (x < mn[0]) mn[0] = x; if (x > mx[0]) mx[0] = x;
        if (y < mn[1]) mn[1] = y; if (y > mx[1]) mx[1] = y;
        if (z < mn[2]) mn[2] = z; if (z > mx[2]) mx[2] = z;
      }
    });
    var bs = GLB_MODEL.baseScale;
    lines.push('GLB bind bbox size (model units) = ' +
      [mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]].map(function (v) { return +v.toFixed(3); }));
    lines.push('GLB baseScale=' + bs.toFixed(4) + '  -> world height=' + ((mx[1] - mn[1]) * bs).toFixed(3) +
      '  width=' + ((mx[0] - mn[0]) * bs).toFixed(3));
  }

  // --- procedural rigs: local-space bone span (pose-independent)
  function rigSpan(rig, label) {
    if (!rig || !rig.root) { lines.push(label + ': none'); return; }
    rig.root.updateWorldMatrix(true, true);
    var inv = new THREE.Matrix4().copy(rig.root.matrixWorld).invert();
    var mn = [1e9, 1e9, 1e9], mx = [-1e9, -1e9, -1e9], n = 0;
    var v = new THREE.Vector3();
    rig.root.traverse(function (o) {
      if (!o.isObject3D) return;
      o.getWorldPosition(v);
      v.applyMatrix4(inv);
      if (v.y < mn[1]) mn[1] = v.y; if (v.y > mx[1]) mx[1] = v.y;
      if (v.x < mn[0]) mn[0] = v.x; if (v.x > mx[0]) mx[0] = v.x;
      if (v.z < mn[2]) mn[2] = v.z; if (v.z > mx[2]) mx[2] = v.z;
      n++;
    });
    lines.push(label + ' nodes=' + n + ' local span size=' +
      [mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]].map(function (x) { return +x.toFixed(3); }) +
      '  scale=' + rig.root.scale.x.toFixed(3));
  }
  try { rigSpan(her3, 'proc her'); } catch (e) { lines.push('proc her ERR ' + e.message); }
  try { rigSpan(him3, 'him     '); } catch (e) { lines.push('him ERR ' + e.message); }

  // a couple of stable limb lengths
  function dist(a, b) { if (!a || !b) return null; return +a.distanceTo(b).toFixed(4); }
  try {
    if (her3 && her3.neck) {
      lines.push('proc her neck.local.y=' + her3.neck.position.y.toFixed(4) +
        ' torso.local.y=' + (her3.torso ? her3.torso.position.y.toFixed(4) : '?'));
    }
  } catch (e) { }
  return lines.join('\n');
})()
