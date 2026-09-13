// transform-aware cache-key uniqueness for shade(), across animated frames
(function(){
  var calls=0, keys=new Set(), keysT=new Set();
  function q(v,q){ return Math.round(v/q)*q; }
  function keyT(a){ // args + transform + colors
    var m=X.getTransform();
    var s=q(m.a,.01)+','+q(m.b,.01)+','+q(m.c,.01)+','+q(m.d,.01)+','+q(m.e,.5)+','+q(m.f,.5);
    s+='|'+q(a[0],.5)+','+q(a[1],.5)+','+q(a[2],.5)+','+q(a[3],.5)+','+(a[5]?q(a[5],.01):0);
    s+='|'+a[4]+'|'+(a[6]||'');
    return s;
  }
  function key(a){ return q(a[0],.5)+','+q(a[1],.5)+','+q(a[2],.5)+','+q(a[3],.5)+'|'+a[4]+'|'+(a[6]||''); }
  var s0=window.shade;
  window.shade=function(x,y,rx,ry,col,rot,lc){
    calls++; var a=[x,y,rx,ry,col,rot,lc];
    keys.add(key(a)); keysT.add(keyT(a));
    return s0.apply(null,arguments);
  };
  var savedT=G.t, frames=8;
  for(var f=0;f<frames;f++){ G.t=savedT+f*0.05; draw(); }
  G.t=savedT;
  window.shade=s0;
  return JSON.stringify({frames:frames, calls:calls, uniq_noTransform:keys.size, uniq_withTransform:keysT.size});
})();
