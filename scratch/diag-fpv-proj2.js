// Print projected landmark table for the current FPV camera + skeleton.
const FPV_CAM = { cx: 640, topY: 1020, depthK: -392, k: 560, kpad: 0.72, eyeH: 0.30 };
function fpvProj(u, d, h){
  const sc = FPV_CAM.k / (d + FPV_CAM.kpad);
  const y  = FPV_CAM.topY + d * FPV_CAM.depthK + (FPV_CAM.eyeH - h) * sc;
  return [FPV_CAM.cx + u * sc, y, sc];
}
const B = {
  footFwd:0.34, ankle:0.42, knee:0.60, thighMid:0.72, hip:0.86, mons:0.92,
  navel:1.05, waist:1.09, ribs:1.27, sternum:1.33, shoulder:1.46, neck:1.54,
  chin:1.60, brow:1.66, crown:1.74,
  hipHalf:0.150, waistHalf:0.114, ribHalf:0.132, shHalf:0.168, headHalf:0.072,
  kneeSpread:0.235, footSpread:0.205, lift:0.15
};
const bdy = 0.82 + 0.45*0.42;   // goatchan bodyScale=0.45
const rows = [];
const put = (n, u, d, h) => { const p = fpvProj(u, d, h); rows.push([n, p[0], p[1], p[2]]); };

put('toe-L',     B.footSpread*bdy*1.08,  0.20, 0.11);
put('ankle-L',   B.footSpread*bdy,       0.42, 0.15);
put('knee-L',   (B.kneeSpread)*bdy,      0.60, B.lift+0.28);
put('thighMid-L',(B.kneeSpread*0.62)*bdy,0.72, B.lift+0.20);
put('hip-L',    -B.hipHalf*bdy,          0.86, B.lift);
put('hip-R',     B.hipHalf*bdy,          0.86, B.lift);
put('mons',      0,                      0.92, B.lift+0.04);
put('navel',     0,                      1.05, B.lift+0.015);
put('waist',     0,                      1.09, B.lift+0.004);
put('ribs',      0,                      1.27, B.lift);
put('sternum',   0,                      1.33, B.lift+0.006);
put('shoulder-L',  B.shHalf*bdy,         1.46, B.lift);
put('shoulder-R', -B.shHalf*bdy,         1.46, B.lift);
put('chin',      0,                      1.64, B.lift+0.045);
put('crown',     0,                      1.74, B.lift+0.06);

console.log('name             x      y      sc     | width(px) for 0.30m');
for(const [n,x,y,sc] of rows){
  console.log(n.padEnd(14), x.toFixed(0).padStart(5), y.toFixed(0).padStart(6), sc.toFixed(0).padStart(6), '   ', (0.30*sc).toFixed(0));
}
console.log('\nscale falloff near(0.42)/far(1.46) =', (fpvProj(0,0.42,0)[2]/fpvProj(0,1.46,0)[2]).toFixed(2));
console.log('pelvis width px  =', (2*B.hipHalf*bdy*fpvProj(0,0.86,0)[2]).toFixed(0));
console.log('shoulder width px=', (2*B.shHalf*bdy*fpvProj(0,1.46,0)[2]).toFixed(0));
console.log('thigh diam px @knee =', (2*0.062*bdy*fpvProj(0,0.60,0)[2]).toFixed(0));
console.log('knee separation px =', (2*B.kneeSpread*bdy*fpvProj(0,0.60,0)[2]).toFixed(0));
