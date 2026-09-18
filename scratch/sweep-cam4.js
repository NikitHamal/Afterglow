// Push the whole composition up: reduce topY so everything rises.
const B = {
  knee:0.60, hip:0.86, mons:0.92, navel:1.05, waist:1.09, ribs:1.27,
  sternum:1.33, shoulder:1.46, chin:1.60, crown:1.74,
  hipHalf:0.150, waistHalf:0.114, ribHalf:0.132, shHalf:0.168,
  kneeSpread:0.235, footSpread:0.205, lift:0.15
};
const bdy = 1.009;
function evalCam(c, dOff){
  const P = (u,d,h) => {
    const dd = Math.max(0.02, d - dOff);
    const sc = c.k / (dd + c.kpad);
    return [c.cx + u*sc, c.topY + dd*c.depthK + (c.eyeH - h)*sc, sc];
  };
  const knee = P(B.kneeSpread*bdy, B.knee, B.lift);
  const hipL = P(-B.hipHalf*bdy, B.hip, B.lift);
  const hipR = P( B.hipHalf*bdy, B.hip, B.lift);
  const mons = P(0, B.mons, B.lift+0.04);
  const shL  = P( B.shHalf*bdy, B.shoulder, B.lift);
  const crown= P(0, B.crown, B.lift+0.06);
  return { kneeY:knee[1], hipY:hipL[1], monsY:mons[1], shYr:shL[1], crownY:crown[1],
           pelW:hipR[0]-hipL[0], shW:2*B.shHalf*bdy*shL[2],
           kneeSep:2*B.kneeSpread*bdy*knee[2], falloff:knee[2]/shL[2] };
}
console.log('k   kpad dOff depthK topY | crownY  shY monsY hipY kneeY | pelW shW kneeSep');
for(const cfg of [
  [760, 0.60, 0.24, -640, 900],
  [820, 0.64, 0.26, -660, 920],
  [760, 0.60, 0.26, -700, 900],
  [820, 0.64, 0.28, -700, 930],
  [700, 0.56, 0.22, -640, 880],
  [760, 0.60, 0.30, -760, 950],
  [820, 0.62, 0.24, -620, 900],
]){
  const [k,kpad,dOff,depthK,topY] = cfg;
  const r = evalCam({cx:640, k, topY, depthK, kpad, eyeH:0.30}, dOff);
  console.log(String(k).padEnd(4), String(kpad).padEnd(4), String(dOff).padEnd(5),
    String(depthK).padEnd(6), String(topY).padEnd(4), '|',
    r.crownY.toFixed(0).padStart(6), r.shYr.toFixed(0).padStart(4),
    r.monsY.toFixed(0).padStart(5), r.hipY.toFixed(0).padStart(4),
    r.kneeY.toFixed(0).padStart(5), '|',
    r.pelW.toFixed(0).padStart(4), r.shW.toFixed(0).padStart(4),
    r.kneeSep.toFixed(0).padStart(5));
}
