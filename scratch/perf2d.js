// Average cost of one draw() pass plus the two things that dominate it: shade()
// blob count and gradient object creation. Measured synchronously (no rAF, no
// present) so realism passes can be compared against each other on equal footing.
(function(){
  const N = 12;
  for(let i = 0; i < 2; i++){ G.t += 1 / 60; draw(); }
  let shades = 0, grads = 0;
  const oShade = (typeof shade === 'function') ? shade : null;
  const oR = X.createRadialGradient, oL = X.createLinearGradient;
  if(oShade){ try{ shade = function(){ shades++; return oShade.apply(this, arguments); }; }catch(e){} }
  X.createRadialGradient = function(){ grads++; return oR.apply(X, arguments); };
  X.createLinearGradient  = function(){ grads++; return oL.apply(X, arguments); };
  const t0 = performance.now();
  for(let i = 0; i < N; i++){ G.t += 1 / 60; draw(); }
  const ms = (performance.now() - t0) / N;
  if(oShade){ try{ shade = oShade; }catch(e){} }
  X.createRadialGradient = oR; X.createLinearGradient = oL;
  return {
    view: G.view,
    avgDrawMs: +ms.toFixed(2),
    fpsEq: +(1000 / ms).toFixed(1),
    shadeCallsPerFrame: Math.round(shades / N),
    gradientCreatesPerFrame: Math.round(grads / N)
  };
})()
