/* diag-size.js — compare the GLB Goat-chan's body volume against the
   procedural girl and the male, in the CURRENT pose. If the GLB girl is
   smaller than the rig the couple poses were authored for, the male body
   (sized for the procedural girl) will engulf her.

   Skinned meshes are measured by sampling boneTransform, since
   Box3.setFromObject uses the undeformed geometry box. Compact text. */
(function () {
  if (!window.GLB_MODEL || !GLB_MODEL.model) return 'no model';

  function bboxOf(root, stride) {
    var mn = [1e9, 1e9, 1e9], mx = [-1e9, -1e9, -1e9], n = 0;
    root.updateWorldMatrix(true, true);
    var v = new THREE.Vector3(), q = new THREE.Vector3();
    root.traverse(function (o) {
      if (!o.isMesh || !o.geometry || !o.geometry.attributes.position) return;
      var pos = o.geometry.attributes.position;
      var st = stride || Math.max(1, Math.floor(pos.count / 1200));
      for (var i = 0; i < pos.count; i += st) {
        if (o.isSkinnedMesh && o.skeleton) {
          q.set(0, 0, 0);
          try { o.boneTransform(i, q.fromBufferAttribute(pos, i)); } catch (e) { continue; }
          q.applyMatrix4(o.matrixWorld);
        } else {
          q.set(0, 0, 0).fromBufferAttribute(pos, i).applyMatrix4(o.matrixWorld);
        }
        if (q.x < mn[0]) mn[0] = q.x; if (q.x > mx[0]) mx[0] = q.x;
        if (q.y < mn[1]) mn[1] = q.y; if (q.y > mx[1]) mx[1] = q.y;
        if (q.z < mn[2]) mn[2] = q.z; if (q.z > mx[2]) mx[2] = q.z;
        n++;
      }
    });
    return { n: n, min: mn, max: mx, size: [mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]] };
  }

  function f(a) { return a.map(function (x) { return +x.toFixed(3); }); }
  var lines = [];
  lines.push('pose G.pos=' + (typeof G !== 'undefined' ? G.pos : '?') + ' solo=' + (typeof G !== 'undefined' ? !!G.solo : '?'));

  var glb = bboxOf(GLB_MODEL.model);
  lines.push('GLB her   n=' + glb.n + ' min=' + JSON.stringify(f(glb.min)) + ' max=' + JSON.stringify(f(glb.max)) + ' size=' + JSON.stringify(f(glb.size)));

  if (typeof her3 !== 'undefined' && her3 && her3.root) {
    var h = bboxOf(her3.root);
    lines.push('proc her  n=' + h.n + ' min=' + JSON.stringify(f(h.min)) + ' max=' + JSON.stringify(f(h.max)) + ' size=' + JSON.stringify(f(h.size)));
  }
  if (typeof him3 !== 'undefined' && him3 && him3.root) {
    var m = bboxOf(him3.root);
    lines.push('him       n=' + m.n + ' min=' + JSON.stringify(f(m.min)) + ' max=' + JSON.stringify(f(m.max)) + ' size=' + JSON.stringify(f(m.size)));
    lines.push('him.root.pos=' + JSON.stringify(f([him3.root.position.x, him3.root.position.y, him3.root.position.z])) +
      ' scale=' + him3.root.scale.x.toFixed(3));
  }
  if (typeof her3 !== 'undefined' && her3 && her3.root) {
    lines.push('her3.root.pos=' + JSON.stringify(f([her3.root.position.x, her3.root.position.y, her3.root.position.z])) +
      ' scale=' + her3.root.scale.x.toFixed(3));
  }
  lines.push('GLB baseScale=' + GLB_MODEL.baseScale.toFixed(4) +
    ' model.scale=' + GLB_MODEL.model.scale.x.toFixed(4));
  return lines.join('\n');
})()
