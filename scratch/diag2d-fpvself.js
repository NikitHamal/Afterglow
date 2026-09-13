// FPV control: do two CONSECUTIVE plain draws differ? (state mutation test)
(function(){
  var C=X;
  function hash(){ var d=C.getImageData(0,0,CV.width,CV.height).data; var h=0; for(var i=0;i<d.length;i+=499) h=(h*31+d[i])>>>0; return h; }
  var n=8, selfDiff=[];
  for(var f=0; f<n; f++){ draw(); var a=hash(); draw(); var b=hash(); selfDiff.push(a===b?0:1); }
  return JSON.stringify({consecutivePlainMismatch:selfDiff, mutate:(selfDiff.indexOf(1)>=0)});
})();
