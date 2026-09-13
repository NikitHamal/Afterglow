// time each draw stage (side + fpv), warm
(function(){
  function bench(fn, n){ n=n||50; var a=performance.now(); for(var i=0;i<n;i++) fn(); return +((performance.now()-a)/n).toFixed(3); }
  var out={};
  var E=herExpression(), br=2;
  out.room   = bench(function(){ X.save(); drawRoom(); X.restore(); });
  out.bed    = bench(function(){ X.save(); drawBed(); X.restore(); });
  out.her    = bench(function(){ X.save(); drawHer(); X.restore(); });
  out.vulva  = bench(function(){ X.save(); drawVulva(); X.restore(); });
  out.shaft  = bench(function(){ X.save(); drawShaft(); X.restore(); });
  out.head   = bench(function(){ X.save(); drawHerHead(E); X.restore(); });
  out.breast = bench(function(){ X.save(); drawBreast(E,br); X.restore(); });
  out.him    = bench(function(){ X.save(); drawHim(E); X.restore(); });
  out.near   = bench(function(){ var o=drawHer(E); X.save(); drawHerNear(E,o); X.restore(); });
  out.light  = bench(function(){ X.save(); drawLight(); X.restore(); });
  out.fluids = bench(function(){ X.save(); drawFluids(); X.restore(); });
  out.hud    = bench(function(){ hudTick(); }, 60);
  // fpv
  out.fpvRoom = bench(function(){ X.save(); drawFPVRoom(); X.restore(); });
  out.fpvBody = bench(function(){ X.save(); drawFPVBody(E,br); X.restore(); });
  out.fpvHead = bench(function(){ X.save(); drawFPVHead(E); X.restore(); });
  out.fpvShaft= bench(function(){ X.save(); drawFPVShaft(); X.restore(); });
  out.fpvHands= bench(function(){ X.save(); drawFPVHands(br); X.restore(); });
  return JSON.stringify(out);
})();
