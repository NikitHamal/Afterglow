// Proper pitched pinhole camera for the FPV bed view.
// The viewer kneels between her knees, leaning over her, looking DOWN her body.
const D = Math.PI/180;
const CAM = { cx:640, cy:360, k:800, dOff:0.75, eyeH:0.85, pitch:50*D };

function proj(u, d, h){
  const cosP = Math.cos(CAM.pitch), sinP = Math.sin(CAM.pitch);
  const f = d - CAM.dOff;
  const v = h - CAM.eyeH;
  let z = f*cosP - v*sinP;
  if(z < 0.12) z = 0.12;
  const yc = f*sinP + v*cosP;
  const sc = CAM.k / z;
  return [CAM.cx + u*sc, CAM.cy - yc*sc, sc];
}
const B = { ankle:0.42, knee:0.60, hip:0.86, mons:0.92, navel:1.05, waist:1.09,
  ribs:1.27, sternum:1.33, shoulder:1.46, chin:1.60, crown:1.74,
  hipHalf:0.150, waistHalf:0.114, ribHalf:0.132, shHalf:0.168,
  kneeSpread:0.235, footSpread:0.205, lift:0.15 };
const bdy = 1.009;

const rows = [
  ['crown',    0,               B.crown,   B.lift+0.06],
  ['chin',     0,               B.chin+0.02, B.lift+0.055],
  ['shoulderL', B.shHalf*bdy,   B.shoulder, B.lift],
  ['breastL',  0.132*bdy,       B.sternum-0.03, B.lift+0.030],
  ['sternum',  0,               B.sternum, B.lift+0.006],
  ['ribs',     0,               B.ribs,    B.lift],
  ['waist',    0,               B.waist,   B.lift+0.004],
  ['navel',    0,               B.navel,   B.lift+0.015],
  ['mons',     0,               B.mons,    B.lift+0.04],
  ['hipL',     B.hipHalf*bdy,   B.hip,     B.lift],
  ['kneeL',    B.kneeSpread*bdy,B.knee,    B.lift+0.14],
  ['ankleL',   B.footSpread*bdy,B.ankle,   0.15],
  ['elbowL',   0.232*bdy,       B.shoulder-0.30, B.lift-0.075],
  ['wristL',   0.212*bdy,       B.shoulder-0.56, B.lift-0.065],
];
console.log('name       x      y      sc     width(px) of 0.30m');
for(const [n,u,d,h] of rows){
  const p = proj(u,d,h);
  console.log(n.padEnd(10), p[0].toFixed(0).padStart(5), p[1].toFixed(0).padStart(6),
    p[2].toFixed(0).padStart(6), '  ', (0.30*p[2]).toFixed(0));
}
const sh = proj(B.shHalf*bdy, B.shoulder, B.lift);
const hp = proj(B.hipHalf*bdy, B.hip, B.lift);
console.log('\nshoulder width px =', (2*B.shHalf*bdy*sh[2]).toFixed(0));
console.log('hip width px      =', (2*B.hipHalf*bdy*hp[2]).toFixed(0));
console.log('head width px     =', (0.145*proj(0,B.chin+0.02,0)[2]).toFixed(0));
console.log('vulva scale       =', proj(0,B.mons,0)[2].toFixed(0), '-> art 30px*sc/430 =', (30*proj(0,B.mons,0)[2]/430).toFixed(0));
console.log('vertical span crown->hip =', (hp[1]-proj(0,B.crown,B.lift+0.06)[1]).toFixed(0), 'px');
console.log('scale falloff near/far =', (proj(0,B.knee,0)[2]/proj(0,B.crown,0)[2]).toFixed(2));
