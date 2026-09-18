/* diag-fpv-proj.js — sanity-check the bed-space projection: print where the
   key body landmarks land on screen. Reads FPV_CAM from fpv.js so the probe
   never drifts from source. */
(function(){
  const L = [];
  const show = (label, u, d, h) => {
    const p = fpvProj(u, d, h);
    L.push(label.padEnd(20) + ' u=' + String(u).padStart(7) + ' d=' + String(d).padStart(5) + ' h=' + String(h).padStart(5)
      + '  ->  (' + p[0].toFixed(0).padStart(5) + ',' + p[1].toFixed(0).padStart(5) + ')  sc=' + p[2].toFixed(0));
  };
  L.push('CAM ' + JSON.stringify(FPV_CAM));
  L.push('d = metres from lens, h = metres above the mattress');
  L.push('--- feet (spread wide, near the lens) ---');
  show('R foot',  -0.30, 0.30, 0.14);
  show('L foot',   0.30, 0.30, 0.14);
  L.push('--- knees (raised, spread) ---');
  show('R knee',  -0.42, 0.62, 0.44);
  show('L knee',   0.42, 0.62, 0.44);
  L.push('--- hips / mons (composition anchor) ---');
  show('R hip',   -0.19, 0.88, 0.11);
  show('L hip',    0.19, 0.88, 0.11);
  show('mons',     0.00, 0.94, 0.15);
  L.push('--- waist, ribs ---');
  show('waist',    0.00, 1.10, 0.10);
  show('ribcage',  0.00, 1.28, 0.10);
  L.push('--- chest ---');
  show('sternum',  0.00, 1.34, 0.13);
  show('R breast', -0.15, 1.30, 0.17);
  show('L breast', 0.15, 1.30, 0.17);
  L.push('--- shoulders, neck ---');
  show('R shoulder', -0.21, 1.48, 0.10);
  show('L shoulder',  0.21, 1.48, 0.10);
  show('neck',         0.00, 1.56, 0.13);
  L.push('--- head ---');
  show('chin',        0.00, 1.62, 0.16);
  show('nose',        0.00, 1.66, 0.17);
  show('crown',        0.00, 1.76, 0.18);
  L.push('');
  L.push('scale: foot=' + fpvS(0.30).toFixed(0) + ' knee=' + fpvS(0.62).toFixed(0)
    + ' hip=' + fpvS(0.88).toFixed(0) + ' head=' + fpvS(1.66).toFixed(0)
    + '   near/far = ' + (fpvS(0.30)/fpvS(1.76)).toFixed(2) + 'x');
  return L.join('\n');
})()
