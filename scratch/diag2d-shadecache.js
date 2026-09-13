// prototype: transform-aware cached shade(); measure time + allocations + pixel identity
(function(){
  var C=X, TAU=Math.PI*2;
  var cache=new Map(), MAX=1200, hits=0, miss=0;
  function q(v,s){ return Math.round(v/s)*s; }
  function cachedShade(x,y,rx,ry,col,rot,lc){
    rot=rot||0;
    var cx=Number.isFinite(x)?x:0, cy=Number.isFinite(y)?y:0;
    var rrx=Number.isFinite(rx)?Math.abs(rx):10, rry=Number.isFinite(ry)?Math.abs(ry):10;
    var m=C.getTransform();
    var k=q(m.a,.01)+','+q(m.b,.01)+','+q(m.c,.01)+','+q(m.d,.01)+','+q(m.e,.5)+','+q(m.f,.5)
      +'|'+q(cx,.5)+','+q(cy,.5)+','+q(rrx,.5)+','+q(rry,.5)+','+q(rot,.01)+'|'+(lc?lc:col);
    var g=cache.get(k);
    if(!g){
      var rad=Math.max(rrx,rry)*1.15;
      g=C.createRadialGradient(cx,cy,0,cx,cy,rad>0?rad:10);
      g.addColorStop(0, lc?lc:col);
      g.addColorStop(1,'rgba(0,0,0,0)');
      if(cache.size>=MAX) cache.clear();
      cache.set(k,g); miss++;
    } else hits++;
    C.fillStyle=g;
    C.beginPath();
    C.ellipse(cx,cy,Math.max(1,rrx),Math.max(1,rry),rot,0,TAU);
    C.fill();
  }
  // baseline image hash
  function hash(){ var d=C.getImageData(0,0,CV.width,CV.height).data; var h=0; for(var i=0;i<d.length;i+=997) h=(h*31+d[i])>>>0; return h; }
  function bench(fn,n){ n=n||120; fn(); var a=performance.now(); for(var i=0;i<n;i++) fn(); return +((performance.now()-a)/n).toFixed(3); }

  var s0=window.shade;
  var baseTime=bench(function(){draw();});
  draw(); var baseHash=hash();

  window.shade=cachedShade;
  var cacheTime=bench(function(){draw();});
  draw(); var cacheHash=hash();
  window.shade=s0;

  return JSON.stringify({baseTime:baseTime, cacheTime:cacheTime, ratio:+(cacheTime/baseTime).toFixed(2),
    baseHash:baseHash, cacheHash:cacheHash, identical:baseHash===cacheHash, hits:hits, miss:miss, cacheSize:cache.size});
})();
