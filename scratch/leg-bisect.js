/* leg-bisect.js — install a glbDriveRig wrapper that, after the normal drive,
   resets every leg/foot bone EXCEPT the ones named in window.__legKeep back to
   bind. Lets us bisect which single bone rotation is crushing the shin/foot.

   Set window.__legKeep = ['ひざL','ひざR']  (cleaned names, no dots)
   Use [] for all-bind, or null for normal drive.
   Read-only: only writes bone rotations. */
(function () {
  if (!window.__origDrive) window.__origDrive = glbDriveRig;
  window.__legKeep = window.__legKeep || [];
  glbDriveRig = function (e, dt) {
    window.__origDrive(e, dt);
    var keep = window.__legKeep;
    if (keep === null || keep === undefined) return;
    var r = e && e.rig;
    if (!r) return;
    var all = (keep.length === 1 && keep[0] === 'ALL');
    for (var i = 0; i < r.bones.length; i++) {
      var b = r.bones[i];
      if (b.part !== 'legs' && b.part !== 'feet') continue;
      var nm = (b.dst.name || '').replace(/[._]/g, '');
      if (!all && keep.indexOf(nm) < 0) b.dst.rotation.set(b.bx, b.by, b.bz);
    }
  };
  return 'leg-bisect installed; keep=' + JSON.stringify(window.__legKeep);
})()
