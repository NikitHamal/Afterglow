(function(){
  return new Promise(function(res){
    var n=0, t0=performance.now(), gt0=(typeof G!=='undefined')?G.t:0;
    function tick(){
      n++;
      if(performance.now()-t0 < 3000){ requestAnimationFrame(tick); }
      else {
        var dt=(performance.now()-t0)/1000;
        res("rAF fps: "+(n/dt).toFixed(1)+"  frames:"+n+
            "  G.t advanced: "+(((typeof G!=='undefined'?G.t:0)-gt0)).toFixed(2)+"s over "+dt.toFixed(1)+"s"+
            "  G.t="+((typeof G!=='undefined'?G.t:0)).toFixed(2)+
            "  rub="+(typeof G!=='undefined'?G.rub:0)+" zone="+(typeof G!=='undefined'?G.rubZone:0));
      }
    }
    requestAnimationFrame(tick);
  });
})()
