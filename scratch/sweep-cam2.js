// The depth span hip->shoulder is only 0.60 m, so scale barely changes.
// To make her read large, the camera must sit much closer to her pelvis.
const B = {
  knee:0.60, hip:0.86, mons:0.92, navel:1.05, waist:1.09, ribs:1.27,
  sternum:1.33, shoulder:1.46, chin:1.60, crown:1.74,
  hipHalf:0.150, waistHalf:0.114, ribHalf:0.132, shHalf:0.168,
  kneeSpread:0.235, footSpread:0.205, lift:0.15
};
const bdy = 1.009;

// Alternative: shift the whole body toward the lens (subtract an offset)
function evalCam(c, dOff){
  const P = (u,d,h) => {
    const dd = d - dOff;
    const sc = c.k / (dd + c.kpad);
    return [c.cx + u*sc, c.topY + dd*c.depthK + (c.eyeH - h)*sc, sc];
  };
  const knee = P(B.kneeSpread*bdy, B.knee, B.lift);
  const hipL = P(-B.hipHalf*bdy, B.hip, B.lift);
  const hipR = P( B.hipHalf*bdy, B.hip, B.lift);
  const mons = P(0, B.mons, B.lift+0.04);
  const shL  = P( B.shHalf*bdy, B.shoulder, B.lift);
  const shR  = P(-B.shHalf*bdy, B.shoulder, B.lift);
  const crown= P(0, B.crown, B.lift+0.06);
  const pelW = hipR[0]-hipL[0];
  const shW  = shL[0]-shR[0];
  return { kneeY:knee[1], hipY:hipL[1], monsY:mons[1], shY:(shL[1]+shR[1])/2, crownY:crown[1],
           pelW, shW, spanY: mons[1]-crown[1], kneeSep: 2*B.kneeSpread*bdy*knee[2],
           falloff: knee[2]/shL[2] };
}

console.log('--- shifting the body toward the lens (dOff) with k=760 kpad=0.55 ---');
console.log('dOff | crownY  shY  monsY hipY kneeY | pelW shW spanY kneeSep falloff');
for(const dOff of [0, 0.10, 0.20, 0.30, 0.40, 0.50]){
  const r = evalCam({cx:640, k:760, topY:1180, depthK:-500, kpad:0.55, eyeH:0.30}, dOff);
  console.log(String(dOff).padEnd(4), '|',
    r.crownY.toFixed(0).padStart(6), r.shY.toFixed(0).padStart(5),
    r.monsY.toFixed(0).padStart(6), r.hipY.toFixed(0).padStart(5),
    r.kneeY.toFixed(0).padStart(6), '|',
    r.pelW.toFixed(0).padStart(4), r.shW.toFixed(0).padStart(4),
    r.spanY.toFixed(0).padStart(5), r.kneeSep.toFixed(0).padStart(6),
    r.falloff.toFixed(2).padStart(6));
}
console.log('\n--- k/kpad sweep at dOff=0.15, depthK=-520, topY=1160 ---');
console.log('k   kpad | crownY  shY  monsY hipY kneeY | pelW shW spanY kneeSep falloff');
for(const [k,kpad] of [[620,0.45],[680,0.50],[740,0.55],[800,0.60],[860,0.65],[920,0.70]]){
  const r = evalCam({cx:640, k, topY:1160, depthK:-520, kpad, eyeH:0.30}, 0.15);
  console.log(String(k).padEnd(4), String(kpad).padEnd(5), '|',
    r.crownY.toFixed(0).padStart(6), r.shY.toFixed(0).padStart(5),
    r.monsY.toFixed(0).padStart(6), r.hipY.toFixed(0).padStart(5),
    r.kneeY.toFixed(0).padStart(6), '|',
    r.pelW.toFixed(0).padStart(4), r.shW.toFixed(0).padStart(4),
    r.spanY.toFixed(0).padStart(5), r.kneeSep.toFixed(0).padStart(6),
    r.falloff.toFixed(2).padStart(6));
}
