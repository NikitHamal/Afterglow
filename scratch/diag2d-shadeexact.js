// exact-key cached shade(): fully pixel-identical; measure hit rate + identity across frames
(function(){
  var C=X, TAU=Math.PI*2, cache=new Map(), MAX=2000, hits=0, miss=0;
  function exactShade(x,y,rx,ry,col,rot,lc){
    rot=rot||0; var cx=Number.isFinite(x)?x:0, cy=Number.isFinite(y)?y:0;
    var rrx=Number.isFinite(rx)?Math.abs(rx):10, rry=Number.isFinite(ry)?Math.abs(ry):10;
    var m=C.getTransform();
    var k=m.a+'|'+m.b+'|'+m.c+'|'+m.d+'|'+m.e+'|'+m.f+'|'+cx+'|'+cy+'|'+rrx+'|'+rry+'|'+rot+'|'+(lc?lc:col);
    var g=cache.get(k);
    if(!g){ var rad=Math.max(rrx,rry)*1.15; g=C.createRadialGradient(cx,cy,0,cx,cy,rad>0?rad:10); g.addColorStop(0,lc?lc:col); g.addColorStop(1,'rgba(0,0,0,0)'); if(cache.size>=MAX)cache.clear(); cache.set(k,g); miss++; }
    else hits++;
    C.fillStyle=g; C.beginPath(); C.ellipse(cx,cy,Math.max(1,rrx),Math.max(1,rry),rot,0,TAU); C.fill();
  }
  function hash(){ var d=C.getImageData(0,0,CV.width,CV.height).data; var h=0; for(var i=0;i<d.length;i+=499) h=(h*31+d[i])>>>0; return h; }
  var s0=window.shade;
  var savedT=G.t, savedP=G.pleasure, savedAr=G.ar, savedD=G.depth;
  var mismatches=[];
  for(var f=0; f<24; f++){
    G.t=savedT+f*0.11; G.pleasure=20+f*3; G.ar=10+f*3; G.depth=0.2+f*0.03;
    window.shade=s0; draw(); var h0=hash();
    window.shade=exactShade; draw(); var h1=hash();
    if(h0!==h1) mismatches.push(f);
  }
  window.shade=s0; G.t=savedT; G.pleasure=savedP; G.ar=savedAr; G.depth=savedD;
  return JSON.stringify({frames:24, mismatches:mismatches, hits:hits, miss:miss, hitRate:+(hits/(hits+miss)).toFixed(3), cacheSize:cache.size});
})();
