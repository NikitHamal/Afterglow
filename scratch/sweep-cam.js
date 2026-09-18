// Sweep FPV camera params to find a framing where her body fills 1280x720.
const B = {
  knee:0.60, hip:0.86, mons:0.92, navel:1.05, waist:1.09, ribs:1.27,
  sternum:1.33, shoulder:1.46, chin:1.60, crown:1.74,
  hipHalf:0.150, waistHalf:0.114, ribHalf:0.132, shHalf:0.168,
  kneeSpread:0.235, footSpread:0.205, lift:0.15
};
const bdy = 0.82 + 0.45*0.42;   // 1.009

function evalCam(c){
  const P = (u,d,h) => {
    const sc = c.k / (d + c.kpad);
    return [c.cx + u*sc, c.topY + d*c.depthK + (c.eyeH - h)*sc, sc];
  };
  // ignore knees raised for now
  const knee = P(B.kneeSpread*bdy, B.knee, B.lift);
  const hipL = P(-B.hipHalf*bdy, B.hip, B.lift);
  const hipR = P( B.hipHalf*bdy, B.hip, B.lift);
  const mons = P(0, B.mons, B.lift+0.04);
  const shL  = P( B.shHalf*bdy, B.shoulder, B.lift);
  const shR  = P(-B.shHalf*bdy, B.shoulder, B.lift);
  const crown= P(0, B.crown, B.lift+0.06);
  const pelW = hipR[0]-hipL[0];
  const shW  = shL[0]-shR[0];
  const spanY = mons[1]-crown[1];
  return { kneeY:knee[1], hipY:hipL[1], monsY:mons[1], shY:(shL[1]+shR[1])/2, crownY:crown[1],
           pelW, shW, spanY, kneeSep: 2*B.kneeSpread*bdy*knee[2], sc_h:hipL[2],
           falloff: knee[2]/shL[2] };
}

const cands = [
  { k: 700, topY: 1180, depthK: -500, kpad: 0.95, eyeH: 0.30 },
  { k: 760, topY: 1200, depthK: -520, kpad: 1.00, eyeH: 0.30 },
  { k: 820, topY: 1220, depthK: -540, kpad: 1.05, eyeH: 0.30 },
  { k: 880, topY: 1240, depthK: -560, kpad: 1.10, eyeH: 0.30 },
  { k: 820, topY: 1200, depthK: -500, kpad: 1.00, eyeH: 0.28 },
  { k: 900, topY: 1260, depthK: -560, kpad: 1.15, eyeH: 0.28 },
  { k: 950, topY: 1280, depthK: -580, kpad: 1.20, eyeH: 0.28 },
];
console.log('k   topY depthK kpad eyeH | crownY  shY  monsY hipY kneeY | pelW shW spanY kneeSep falloff');
for(const c of cands){
  const r = evalCam({cx:640, ...c});
  console.log(
    String(c.k).padEnd(4), String(c.topY).padEnd(5), String(c.depthK).padEnd(6),
    String(c.kpad).padEnd(5), String(c.eyeH).padEnd(5), '|',
    r.crownY.toFixed(0).padStart(6), r.shY.toFixed(0).padStart(5),
    r.monsY.toFixed(0).padStart(6), r.hipY.toFixed(0).padStart(5),
    r.kneeY.toFixed(0).padStart(6), '|',
    r.pelW.toFixed(0).padStart(4), r.shW.toFixed(0).padStart(4),
    r.spanY.toFixed(0).padStart(5), r.kneeSep.toFixed(0).padStart(6),
    r.falloff.toFixed(2).padStart(6)
  );
}
