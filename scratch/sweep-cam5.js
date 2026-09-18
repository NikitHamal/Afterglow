const B = { knee:0.60, hip:0.86, mons:0.92, navel:1.05, waist:1.09, ribs:1.27,
  sternum:1.33, shoulder:1.46, chin:1.60, crown:1.74,
  hipHalf:0.150, waistHalf:0.114, ribHalf:0.132, shHalf:0.168,
  kneeSpread:0.235, footSpread:0.205, lift:0.15 };
const bdy = 1.009;
function ev(c, dOff){
  const P=(u,d,h)=>{const dd=Math.max(0.02,d-dOff);const sc=c.k/(dd+c.kpad);
    return [c.cx+u*sc, c.topY+dd*c.depthK+(c.eyeH-h)*sc, sc];};
  const kn=P(B.kneeSpread*bdy,B.knee,B.lift), hL=P(-B.hipHalf*bdy,B.hip,B.lift),
        hR=P(B.hipHalf*bdy,B.hip,B.lift), mo=P(0,B.mons,B.lift+0.04),
        sh=P(B.shHalf*bdy,B.shoulder,B.lift), cr=P(0,B.crown,B.lift+0.06);
  return {crY:cr[1], shY:sh[1], moY:mo[1], hiY:hL[1], knY:kn[1],
          pelW:hR[0]-hL[0], shW:2*B.shHalf*bdy*sh[2], knSep:2*B.kneeSpread*bdy*kn[2],
          falloff:kn[2]/sh[2]};
}
console.log('k  kpad dOff depthK topY | crY shY moY hiY knY | pelW shW knSep falloff');
for(const c of [[820,0.64,0.26,-680,960],[820,0.64,0.25,-660,950],[840,0.66,0.26,-670,955],
                [860,0.66,0.26,-680,965],[880,0.68,0.26,-690,975],[840,0.64,0.25,-650,940]]){
  const r=ev({cx:640,k:c[0],topY:c[4],depthK:c[3],kpad:c[1],eyeH:0.30},c[2]);
  console.log(String(c[0]).padEnd(3),String(c[1]).padEnd(4),String(c[2]).padEnd(4),
    String(c[3]).padEnd(6),String(c[4]).padEnd(4),'|',
    r.crY.toFixed(0).padStart(4),r.shY.toFixed(0).padStart(4),r.moY.toFixed(0).padStart(4),
    r.hiY.toFixed(0).padStart(4),r.knY.toFixed(0).padStart(4),'|',
    r.pelW.toFixed(0).padStart(4),r.shW.toFixed(0).padStart(3),
    r.knSep.toFixed(0).padStart(4),r.falloff.toFixed(2).padStart(5));
}
