// measure unique-key ratio for gradients under position quantization + save/restore pairing
(function(){
  var C=X, calls={rad:0,lin:0}, uniq={up05r:new Set(),up1r:new Set(),up05l:new Set(),up1l:new Set()};
  var cr=C.createRadialGradient.bind(C), cl=C.createLinearGradient.bind(C);
  function key(a,q){ var s=''; for(var i=0;i<a.length;i++){ var v=a[i]; s+= (typeof v==='number'? Math.round(v/q)*q : v)+'|'; } return s; }
  C.createRadialGradient=function(){var a=[].slice.call(arguments);calls.rad++;uniq.up05r.add(key(a,0.5));uniq.up1r.add(key(a,1));return cr.apply(null,a);};
  C.createLinearGradient=function(){var a=[].slice.call(arguments);calls.lin++;uniq.up05l.add(key(a,0.5));uniq.up1l.add(key(a,1));return cl.apply(null,a);};
  // advance t across frames to capture animation drift
  var frames=8, savedT=G.t;
  for(var f=0; f<frames; f++){ G.t = savedT + f*0.05; draw(); }
  G.t=savedT;
  C.createRadialGradient=cr; C.createLinearGradient=cl;
  return JSON.stringify({
    frames:frames,
    radialCalls:calls.rad, radialUniq_0p5:uniq.up05r.size, radialUniq_1:uniq.up1r.size,
    linearCalls:calls.lin, linearUniq_0p5:uniq.up05l.size, linearUniq_1:uniq.up1l.size
  });
})();
