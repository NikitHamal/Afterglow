// cost of getTransform() vs createRadialGradient()+2 addColorStop vs Map.get
(function(){
  var C=X; function bench(fn,n){ n=n||20000; fn(); var a=performance.now(); for(var i=0;i<n;i++) fn(); return +(((performance.now()-a)/n)*1000).toFixed(4); }
  var out={};
  out.getTransform_us = bench(function(){ C.getTransform(); });
  out.radialGrad_us = bench(function(){ var g=C.createRadialGradient(10,20,0,10,20,30); g.addColorStop(0,'#fff4ea'); g.addColorStop(1,'rgba(0,0,0,0)'); });
  var m=new Map(); m.set('k',1);
  out.mapGet_us = bench(function(){ m.get('k'); });
  var s='a|b|c|d|e|f|1|2|3|4|5|#fff4ea';
  out.strConcat_us = bench(function(){ var k='a|b|'+1.5+'|'+2.5; m.get(k); });
  return JSON.stringify(out);
})();
