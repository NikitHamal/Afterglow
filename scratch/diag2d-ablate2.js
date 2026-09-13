// clean ablation with warmup, no gradient stubbing
(function(){
  function bench(fn,n){ n=n||80; fn(); var a=performance.now(); for(var i=0;i<n;i++) fn(); return +((performance.now()-a)/n).toFixed(3); }
  var out={};
  out.baseline = bench(function(){draw();});
  out.baseline2 = bench(function(){draw();});

  var s0=window.shade; window.shade=function(){}; out.noShade = bench(function(){draw();}); window.shade=s0;
  out.noShade_restore = bench(function(){draw();});

  // force source-over (counted usage )
  var C=X, realDesc=Object.getOwnPropertyDescriptor(C,'globalCompositeOperation');
  Object.defineProperty(C,'globalCompositeOperation',{configurable:true,get:function(){return 'source-over';},set:function(){}});
  out.noComposite = bench(function(){draw();});
  delete C.globalCompositeOperation;
  if(realDesc) Object.defineProperty(C,'globalCompositeOperation',realDesc);
  out.noComposite_restore = bench(function(){draw();});
  return JSON.stringify(out);
})();
