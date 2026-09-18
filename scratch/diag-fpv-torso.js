/* diag-fpv-torso.js — capture the exact torso path bbox by monkeypatching the
   canvas ops during one drawFPVBody call, then report min/max. */
(function(){
  const ops = [];
  const names = ['moveTo','bezierCurveTo','quadraticCurveTo','lineTo'];
  const orig = {};
  names.forEach(n => { orig[n] = X[n].bind(X); });

  let capture = false;
  names.forEach(n => {
    X[n] = function(){
      if(capture) ops.push([n, Array.from(arguments)]);
      return orig[n].apply(X, arguments);
    };
  });

  const origFill = X.fill.bind(X);
  let fills = [];
  X.fill = function(){ if(capture) fills.push(ops.length); return origFill(); };

  capture = true;
  try { drawFPVBody(herExpression(), 0); } catch(e){ ops.push(['ERR', [e.message]]); }
  capture = false;
  names.forEach(n => { X[n] = orig[n]; });
  X.fill = origFill;

  // group into subpaths (every moveTo starts a new one)
  const paths = []; let cur = null;
  for(const [n, args] of ops){
    if(n === 'ERR') return 'ERR: ' + args[0];
    if(n === 'moveTo'){ cur = { pts: [], fills: 0 }; paths.push(cur); }
    if(!cur) continue;
    for(let i = 0; i < args.length; i += 2) cur.pts.push([args[i], args[i+1]]);
  }
  const out = [];
  const named = ['legs-then-pelvis','pelvis','torso','breastL','breastR','misc'];
  paths.forEach((p, i) => {
    if(p.pts.length < 3) return;
    const xs = p.pts.map(q => q[0]), ys = p.pts.map(q => q[1]);
    const x0 = Math.min(...xs), x1 = Math.max(...xs), y0 = Math.min(...ys), y1 = Math.max(...ys);
    if(!isFinite(x0)) return;
    out.push('path#' + i + ' n=' + p.pts.length
      + '  x[' + x0.toFixed(0) + '..' + x1.toFixed(0) + ']'
      + '  y[' + y0.toFixed(0) + '..' + y1.toFixed(0) + ']'
      + '  w=' + (x1-x0).toFixed(0) + ' h=' + (y1-y0).toFixed(0));
  });
  return out.join('\n');
})()
