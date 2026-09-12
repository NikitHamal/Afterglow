/* reset-rig.js — undo ALL live debug overrides installed during a verify
   session (gain freezes, foot-tune sweeps, IK no-ops, drive wrappers).
   ALWAYS run this between experiments; otherwise a frozen part silently
   invalidates the next test.

   Defaults mirror the shipped values in glbModel.js. */
(function () {
  var D = {
    spine: 1.0, head: 1.0, arms: 0.85, hands: 0.85,
    legs: 1.0, feet: 0.85, breast: 0.0, tail: 0.0
  };
  Object.keys(D).forEach(function (k) { if (GLB_PARTS[k]) GLB_PARTS[k].gain = D[k]; });

  if (typeof GLB_FOOT_TUNE !== 'undefined') {
    GLB_FOOT_TUNE.ankle0 = -1.6;
    GLB_FOOT_TUNE.ankleK = 0.85;
    GLB_FOOT_TUNE.dangleK = 0.7;
    GLB_FOOT_TUNE.toeK = 0.5;
    GLB_FOOT_TUNE.toeAbs = false;
  }
  if (window.__origAimArmAt3) aimArmAt3 = window.__origAimArmAt3;
  if (window.__origDrive) glbDriveRig = window.__origDrive;
  window.__legKeep = null;

  // hand task defaults (solo idle writes these every frame anyway)
  if (typeof GLB_HAND3 !== 'undefined') {
    GLB_HAND3.L = { curl: 0.35, spread: 0.14, land: null };
    GLB_HAND3.R = { curl: 0.35, spread: 0.14, land: null };
  }
  var g = {};
  Object.keys(D).forEach(function (k) { g[k] = GLB_PARTS[k].gain; });
  return JSON.stringify({ gains: g, tune: (typeof GLB_FOOT_TUNE !== 'undefined' ? GLB_FOOT_TUNE : null), ikRestored: !!window.__origAimArmAt3 });
})()
