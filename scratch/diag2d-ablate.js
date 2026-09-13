// ablation: upper-bound cost of gradient creation, shade(), capsule(), and composite ops
(function(){
  var C=X;
  function bench(fn,n){ n=n||60; var a=performance.now(); for(var i=0;i<n;i++) fn(); return +((performance.now()-a)/n).toFixed(3); }
  var out={};
  out.baseline = bench(draw);

  // 1) stub gradient creation to a single shared object (isolates creation cost)
  var cr=C.createRadialGradient.bind(C), cl=C.createLinearGradient.bind(C);
  var dummy;
  try{ dummy=cr(0,0,0,0,0,10);}catch(e){ dummy=cl(0,0,0,0); }
  C.createRadialGradient=function(){return dummy;};
  C.createLinearGradient=function(){return dummy;};
  out.noGradientAlloc = bench(draw);
  C.createRadialGradient=cr; C.createLinearGradient=cl;

  // 2) shade() no-op
  var s0=window.shade;
  window.shade=function(){};
  out.noShade = bench(draw);
  window.shade=s0;

  // 3) capsule() no-op
  var c0=window.capsule;
  window.capsule=function(){};
  out.noCapsule = bench(draw);
  window.capsule=c0;

  // 4) composite ops forced to source-over
  var proto=C; var oldSet=null;
  Object.defineProperty(C,'globalCompositeOperation',{
    configurable:true,
    get:function(){return 'source-over';},
    set:function(){}
  });
  out.noComposite = bench(draw);
  delete C.globalCompositeOperation; // restore prototype descriptor
  return JSON.stringify(out);
})();
