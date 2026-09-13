// interleaved A/B/A/B timing of cached shade to separate signal from noise
(function(){
  var C=X, TAU=Math.PI*2, cache=new Map();
  function q(v,s){ return Math.round(v/s)*s; }
  function cachedShade(x,y,rx,ry,col,rot,lc){
    rot=rot||0; var cx=Number.isFinite(x)?x:0, cy=Number.isFinite(y)?y:0;
    var rrx=Number.isFinite(rx)?Math.abs(rx):10, rry=Number.isFinite(ry)?Math.abs(ry):10;
    var m=C.getTransform();
    var k=q(m.a,.01)+','+q(m.b,.01)+','+q(m.c,.01)+','+q(m.d,.01)+','+q(m.e,.5)+','+q(m.f,.5)+'|'+q(cx,.5)+','+q(cy,.5)+','+q(rrx,.5)+','+q(rry,.5)+','+q(rot,.01)+'|'+(lc?lc:col);
    var g=cache.get(k);
    if(!g){ var rad=Math.max(rrx,rry)*1.15; g=C.createRadialGradient(cx,cy,0,cx,cy,rad>0?rad:10); g.addColorStop(0,lc?lc:col); g.addColorStop(1,'rgba(0,0,0,0)'); cache.set(k,g); }
    C.fillStyle=g; C.beginPath(); C.ellipse(cx,cy,Math.max(1,rrx),Math.max(1,rry),rot,0,TAU); C.fill();
  }
  function bench(n){ var a=performance.now(); for(var i=0;i<n;i++) draw(); return +( (performance.now()-a)/n ).toFixed(3); }
  var s0=window.shade;
  draw(); draw(); // warm
  var A=[],B=[];
  for(var r=0;r<4;r++){ window.shade=s0; A.push(bench(60)); window.shade=cachedShade; B.push(bench(60)); }
  window.shade=s0;
  function avg(a){ return +(a.reduce(function(x,y){return x+y;},0)/a.length).toFixed(3); }
  return JSON.stringify({baseRuns:A, cacheRuns:B, baseAvg:avg(A), cacheAvg:avg(B)});
})();
