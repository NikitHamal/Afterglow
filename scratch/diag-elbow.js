/* diag-elbow.js — is `arm.elbow.rotation.x = ikE` actually producing the
   intended elbow angle? Compares the requested interior angle with the one
   the bones actually form. Compact text. */
(function () {
  if (!window.GLB_MODEL || !GLB_MODEL.model) return 'no model';
  var e = GLB_STORE[GLB_MODEL.key];
  if (!e || !e.rig) return 'no rig';
  var rig = e.rig;

  var bones = {};
  GLB_MODEL.model.traverse(function (c) { if (c.isBone && c.name) bones[c.name.replace(/[._]/g, '')] = c; });
  function wp(b) { if (!b) return null; b.updateWorldMatrix(true, false); return new THREE.Vector3().setFromMatrixPosition(b.matrixWorld); }
  function deg(r) { return +(r * 180 / Math.PI).toFixed(1); }

  var lines = [];
  // bind rotations recorded by glbBuildRig for the arm bones
  rig.bones.forEach(function (b) {
    var nm = b.dst.name.replace(/[._]/g, '');
    if (!/^(腕|ひじ|手首)[LR]$/.test(nm)) return;
    lines.push(nm + ' bind=(' + deg(b.bx) + ',' + deg(b.by) + ',' + deg(b.bz) + ')' +
      ' now=(' + deg(b.dst.rotation.x) + ',' + deg(b.dst.rotation.y) + ',' + deg(b.dst.rotation.z) + ')');
  });

  ['L', 'R'].forEach(function (ms) {
    var A = rig.arms[ms];
    if (!A) return;
    var sh = wp(A.sh), el = wp(A.el), wr = wp(A.wr);
    if (!sh || !el || !wr) return;
    var u = new THREE.Vector3().subVectors(el, sh), f = new THREE.Vector3().subVectors(wr, el);
    var interior = Math.acos(Math.max(-1, Math.min(1, u.clone().normalize().dot(f.clone().normalize()))));
    var dist = sh.distanceTo(wr);
    lines.push('   sh=' + sh.toArray().map(function (x) { return +x.toFixed(3); }) +
      ' el=' + el.toArray().map(function (x) { return +x.toFixed(3); }) +
      ' wr=' + wr.toArray().map(function (x) { return +x.toFixed(3); }));
    lines.push('   |u_vec|=' + u.length().toFixed(4) + ' (A.upper=' + A.upper.toFixed(4) + ')' +
      '  |f_vec|=' + f.length().toFixed(4) + ' (A.fore=' + A.fore.toFixed(4) + ')');
    var cosI = (A.upper * A.upper + A.fore * A.fore - dist * dist) / (2 * A.upper * A.fore);
    var wanted = Math.acos(Math.max(-1, Math.min(1, cosI)));
    var cosA = (A.upper * A.upper + dist * dist - A.fore * A.fore) / (2 * A.upper * dist);
    lines.push('MMD ' + ms + '  upper=' + A.upper.toFixed(4) + ' fore=' + A.fore.toFixed(4) +
      ' |sh-wr|=' + dist.toFixed(4));
    lines.push('   interior achieved=' + deg(interior) + '  wanted=' + deg(wanted) +
      '  diff=' + deg(interior - wanted));
    lines.push('   shoulderAngle alpha=' + deg(Math.acos(Math.max(-1, Math.min(1, cosA)))) +
      '  elbow.rotation.x=' + deg(A.el.rotation.x));
    // what flexion would the current code request?
    var ikE = Math.PI - wanted;
    lines.push('   ikE(PI-interior)=' + deg(ikE) + '   achievedFlexion(PI-interiorAchieved)=' + deg(Math.PI - interior));
  });
  return lines.join('\n');
})()
