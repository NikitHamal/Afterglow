/* diag-overlap.js — how deep does the male body sink into hers?

   Builds a coarse XZ grid over the union of both bodies' footprints. For each
   cell it records her highest vertex and his lowest vertex (sampled with
   boneTransform so the deformed skin is measured, not the bind box). Where a
   cell has both, the penetration is herMaxY - hisMinY; the report gives the
   worst cell and the area-weighted mean, which is the lift he needs.

   Also reports the same along Z, which is the separation axis for the
   "he is behind her" poses. Compact text. */
(function () {
  if (!window.GLB_MODEL || !GLB_MODEL.model) return 'no model';
  if (typeof her3 === 'undefined' || typeof him3 === 'undefined') return 'no rigs';

  function sample(root, stride) {
    var pts = [];
    root.updateWorldMatrix(true, true);
    var v = new THREE.Vector3();
    root.traverse(function (o) {
      if (!o.isMesh || !o.geometry || !o.geometry.attributes.position) return;
      var p = o.geometry.attributes.position;
      var st = stride || Math.max(1, Math.floor(p.count / 2500));
      for (var i = 0; i < p.count; i += st) {
        if (o.isSkinnedMesh && o.skeleton) {
          v.set(0, 0, 0);
          try { o.boneTransform(i, v.fromBufferAttribute(p, i)); } catch (e) { continue; }
          v.applyMatrix4(o.matrixWorld);
        } else {
          v.set(0, 0, 0).fromBufferAttribute(p, i).applyMatrix4(o.matrixWorld);
        }
        pts.push(v.x, v.y, v.z);
      }
    });
    return pts;
  }

  var HER = sample(GLB_MODEL.model);
  var HIM = sample(him3.root);
  if (!HER.length || !HIM.length) return 'no samples';

  var CELL = 0.10;
  var g = {};   // key "ix,iz" -> {herMaxY, himMinY}
  function feed(pts, isHer) {
    for (var i = 0; i < pts.length; i += 3) {
      var ix = Math.floor(pts[i] / CELL), iz = Math.floor(pts[i + 2] / CELL);
      var k = ix + ',' + iz;
      var c = g[k] || (g[k] = { herMaxY: -1e9, himMinY: 1e9, n: 0 });
      if (isHer) { if (pts[i + 1] > c.herMaxY) c.herMaxY = pts[i + 1]; }
      else { if (pts[i + 1] < c.himMinY) c.himMinY = pts[i + 1]; }
      c.n++;
    }
  }
  feed(HER, true); feed(HIM, false);

  var worst = -1e9, worstCell = null, sum = 0, cnt = 0, pen = [];
  Object.keys(g).forEach(function (k) {
    var c = g[k];
    if (c.herMaxY < -1e8 || c.himMinY > 1e8) return;
    var d = c.herMaxY - c.himMinY;      // >0 => he is below her top => overlap
    if (d > 0) { pen.push(d); sum += d; cnt++; }
    if (d > worst) { worst = d; worstCell = k; }
  });

  // Z-axis separation (for "behind her" poses): her nearest/farthest vs his
  var hz = [1e9, -1e9], mz = [1e9, -1e9];
  for (var i = 2; i < HER.length; i += 3) { if (HER[i] < hz[0]) hz[0] = HER[i]; if (HER[i] > hz[1]) hz[1] = HER[i]; }
  for (var j = 2; j < HIM.length; j += 3) { if (HIM[j] < mz[0]) mz[0] = HIM[j]; if (HIM[j] > mz[1]) mz[1] = HIM[j]; }

  var lines = [];
  lines.push('pose ' + (typeof G !== 'undefined' ? G.pos : '?') + '  herPts=' + (HER.length / 3) + ' himPts=' + (HIM.length / 3));
  lines.push('cells with both bodies = ' + cnt + '   cells penetrating = ' + pen.length);
  lines.push('worst penetration = ' + (worst > -1e8 ? worst.toFixed(4) : 'n/a') + ' m  at cell ' + worstCell);
  lines.push('mean penetration over penetrating cells = ' + (cnt ? (sum / cnt).toFixed(4) : 'n/a') + ' m');
  lines.push('her Z span [' + hz[0].toFixed(3) + ',' + hz[1].toFixed(3) + ']  his Z span [' + mz[0].toFixed(3) + ',' + mz[1].toFixed(3) + ']');
  return lines.join('\n');
})()
