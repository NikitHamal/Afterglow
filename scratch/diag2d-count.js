// count capsule calls + gradients per frame, and per-composite-op usage
(function(){
  var n={capsule:0, shade:0, comp:{}};
  var c0=window.capsule, s0=window.shade;
  window.capsule=function(){n.capsule++;return c0.apply(null,arguments);};
  window.shade=function(){n.shade++;return s0.apply(null,arguments);};
  var C=X;
  Object.defineProperty(C,'globalCompositeOperation',{
    configurable:true,
    get:function(){return this.__gco||'source-over';},
    set:function(v){this.__gco=v; n.comp[v]=(n.comp[v]||0)+1;}
  });
  try{ draw(); } finally {
    window.capsule=c0; window.shade=s0;
    delete C.globalCompositeOperation;
    try{ delete C.__gco; }catch(e){}
  }
  return JSON.stringify(n);
})();
