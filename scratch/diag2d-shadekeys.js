// measure unique shade() keys across animated frames
(function(){
  var calls=0, keys=new Set(), keys1=new Set();
  function key(a,q){ var s=''; for(var i=0;i<a.length;i++){ var v=a[i]; s+=(typeof v==='number'? Math.round(v/q)*q : String(v))+'|'; } return s; }
  var s0=window.shade;
  window.shade=function(x,y,rx,ry,col,rot,lc){
    calls++; var a=[x,y,rx,ry,col,rot,lc];
    keys.add(key(a,0.5)); keys1.add(key(a,1));
    return s0.apply(null,arguments);
  };
  var savedT=G.t, frames=8;
  for(var f=0;f<frames;f++){ G.t=savedT+f*0.05; draw(); }
  G.t=savedT;
  window.shade=s0;
  return JSON.stringify({frames:frames, calls:calls, uniq_q0p5:keys.size, uniq_q1:keys1.size});
})();
