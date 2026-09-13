/* diag-fingers.js — measure the actual finger geometry: how far apart adjacent
   fingers point (the "fan"), and how much each finger is curled. Reports the
   live GLB_HAND3 task too. Compact text. */
(function () {
  if (!window.GLB_MODEL || !GLB_MODEL.model) return 'no model';
  var bones = {};
  GLB_MODEL.model.traverse(function (c) { if (c.isBone && c.name) bones[c.name.replace(/[._]/g, '')] = c; });
  function wp(b) { if (!b) return null; b.updateWorldMatrix(true, false); return new THREE.Vector3().setFromMatrixPosition(b.matrixWorld); }
  function deg(r) { return +(r * 180 / Math.PI).toFixed(1); }

  var FINGERS = ['人指', '中指', '薬指', '小指'];
  var lines = [];
  lines.push('pose=' + (typeof G !== 'undefined' ? G.pos : '?') + ' solo=' + (typeof G !== 'undefined' ? !!G.solo : '?'));
  lines.push('HAND3=' + JSON.stringify(typeof GLB_HAND3 !== 'undefined' ? GLB_HAND3 : null));
  lines.push('PH.gain=' + (typeof GLB_PARTS !== 'undefined' ? GLB_PARTS.hands.gain : '?'));

  ['L', 'R'].forEach(function (side) {
    var dirs = [], curls = [];
    FINGERS.forEach(function (f) {
      var b0 = bones[f + '０' + side], b3 = bones[f + '３' + side] || bones[f + '２' + side];
      if (!b0 || !b3) { dirs.push(null); curls.push(null); return; }
      var p0 = wp(b0), p3 = wp(b3);
      var d = new THREE.Vector3().subVectors(p3, p0);
      var len = d.length();
      dirs.push(len > 1e-6 ? d.divideScalar(len) : null);
      // total flexion down the chain = sum of rotation.x over the segments
      var sum = 0, n = 0;
      for (var k = 0; k <= 3; k++) {
        var bb = bones[f + '０１２３'[k] + side];
        if (bb) { sum += bb.rotation.x; n++; }
      }
      curls.push(n ? deg(sum) : null);
    });
    var gaps = [];
    for (var i = 0; i < dirs.length - 1; i++) {
      if (!dirs[i] || !dirs[i + 1]) { gaps.push(null); continue; }
      gaps.push(deg(Math.acos(Math.max(-1, Math.min(1, dirs[i].dot(dirs[i + 1]))))));
    }
    lines.push(side + '  adjacent-finger angles (deg) 人-中/中-薬/薬-小 = ' + JSON.stringify(gaps));
    lines.push('   summed flexion per finger (deg) = ' + JSON.stringify(curls));
  });

  // thumb-vs-index separation, and the hand's overall "flatness"
  ['L', 'R'].forEach(function (side) {
    var th = bones['親指０' + side], ix = bones['人指０' + side];
    var tt = bones['親指２' + side] || bones['親指１' + side];
    if (!th || !ix) return;
    var a = new THREE.Vector3().subVectors(wp(ix), wp(th)).normalize();
    var b = new THREE.Vector3().subVectors(wp(tt), wp(th)).normalize();
    lines.push(side + ' thumb-index angle = ' + deg(Math.acos(Math.max(-1, Math.min(1, a.dot(b))))) + ' deg');
  });
  return lines.join('\n');
})()
