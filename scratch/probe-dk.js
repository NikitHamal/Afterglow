// solve depthK + topY so mons lands at yA and crown at yB
const k=820,kpad=0.95,eyeH=0.34,lift=0.15;
function y(d,h,topY,depthK){ const sc=k/(d+kpad); return topY+d*depthK+(eyeH-h)*sc; }
function span(topY,depthK){
  const pts={mons:[0.94,lift+0.04],waist:[1.10,lift+0.004],ribs:[1.28,lift],
    stern:[1.34,lift+0.006],sh:[1.48,lift],neck:[1.56,lift],chin:[1.62,lift+0.02],crown:[1.76,lift+0.03],
    knee:[0.62,lift+0.28],ank:[0.40,0.15],toe:[0.20,0.11]};
  const o={}; for(const n in pts) o[n]=y(pts[n][0],pts[n][1],topY,depthK);
  return o;
}
console.log('target: crown~110  mons~560  knee~360  toe~700');
for(const depthK of [-380,-450,-520,-600,-680]){
  for(const topY of [820,880,940]){
    const o=span(topY,depthK);
    const spanC=o.mons-o.crown;
    console.log('depthK='+depthK+' topY='+topY
      +'  knee='+o.knee.toFixed(0)+' mons='+o.mons.toFixed(0)+' waist='+o.waist.toFixed(0)
      +' ribs='+o.ribs.toFixed(0)+' stern='+o.stern.toFixed(0)+' sh='+o.sh.toFixed(0)
      +' chin='+o.chin.toFixed(0)+' crown='+o.crown.toFixed(0)
      +' | trunkSpan='+spanC.toFixed(0)+' ank='+o.ank.toFixed(0)+' toe='+o.toe.toFixed(0));
  }
}
