(function(){
  var C=X;
  function img(){ return C.getImageData(0,0,CV.width,CV.height).data; }
  if(typeof _gcache!=='undefined') _gcache.clear();
  draw(); var a=img();
  draw(); var b=img();
  var minx=1e9,miny=1e9,maxx=-1,maxy=-1,cnt=0;
  for(var y=0;y<CV.height;y++){
    for(var x=0;x<CV.width;x++){
      var i=(y*CV.width+x)*4;
      if(a[i]!==b[i]||a[i+1]!==b[i+1]||a[i+2]!==b[i+2]||a[i+3]!==b[i+3]){
        cnt++; if(x<minx)minx=x; if(x>maxx)maxx=x; if(y<miny)miny=y; if(y>maxy)maxy=y;
      }
    }
  }
  var sa=null,sb=null;
  if(cnt>0){ var i=(miny*CV.width+minx)*4; sa=[a[i],a[i+1],a[i+2],a[i+3]]; sb=[b[i],b[i+1],b[i+2],b[i+3]]; }
  return JSON.stringify({diffPixels:cnt, pct:+((cnt/(CV.width*CV.height))*100).toFixed(3), bbox:[minx,miny,maxx,maxy], sa:sa, sb:sb});
})();
