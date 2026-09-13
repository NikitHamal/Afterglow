// pixel-identity: cached (warm) vs force-rebuild-every-shade (cold) across frames
(function(){
  var C=X;
  function hash(){ var d=C.getImageData(0,0,CV.width,CV.height).data; var h=0; for(var i=0;i<d.length;i+=499) h=(h*31+d[i])>>>0; return h; }
  var savedT=G.t, savedP=G.pleasure, savedAr=G.ar, savedD=G.depth;
  var mism=[];
  for(var f=0;f<20;f++){
    G.t=savedT+f*0.11; G.pleasure=20+f*3; G.ar=10+f*3; G.depth=0.2+f*0.03;
    if(typeof _gcache!=='undefined') _gcache.clear();
    draw(); var h1=hash();          // cold: every gradient rebuilt
    draw(); var h2=hash();          // warm: cache hits
    if(h1!==h2) mism.push(f);
  }
  G.t=savedT; G.pleasure=savedP; G.ar=savedAr; G.depth=savedD;
  return JSON.stringify({frames:20, mismatches:mism, cacheType:(typeof _gcache), size:(typeof _gcache!=='undefined'?_gcache.size:-1)});
})();
