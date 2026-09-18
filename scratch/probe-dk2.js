const k=820,kpad=0.95,eyeH=0.34,lift=0.15;
const topY=930, depthK=-500;
function y(d,h){ const sc=k/(d+kpad); return topY+d*depthK+(eyeH-h)*sc; }
function x(u,d){ return 640+u*k/(d+kpad); }
function p(u,d,h){ return [x(u,d), y(d,h), k/(d+kpad)]; }
const B={hipHalf:0.19,waistHalf:0.15,ribHalf:0.20,shHalf:0.21,kneeSpread:0.44,footSpread:0.30};
const bdy=1.030;
const drawUp=0.28+62*0.0026, openAmt=0.055+62*0.0011;
const items=[
 ['kneeL', (B.kneeSpread+openAmt)*bdy, 0.62, lift+drawUp],
 ['kneeR',-(B.kneeSpread+openAmt)*bdy, 0.62, lift+drawUp],
 ['ankL',  B.footSpread*bdy, 0.46, 0.15],
 ['ankR', -B.footSpread*bdy, 0.46, 0.15],
 ['toeL',  B.footSpread*bdy*1.08, 0.30, 0.11],
 ['toeR', -B.footSpread*bdy*1.08, 0.30, 0.11],
 ['hipL', -B.hipHalf*bdy, 0.88, lift],
 ['hipR',  B.hipHalf*bdy, 0.88, lift],
 ['mons', 0, 0.94, lift+0.04],
 ['waist',0, 1.10, lift+0.004],
 ['ribs', 0, 1.28, lift],
 ['stern',0, 1.34, lift+0.006],
 ['shL',  B.shHalf*bdy, 1.48, lift],
 ['shR', -B.shHalf*bdy, 1.48, lift],
 ['chin', 0, 1.62, lift+0.02],
 ['crown',0, 1.76, lift+0.03],
];
for(const it of items){ const q=p(it[1],it[2],it[3]);
  console.log(it[0].padEnd(6)+'('+q[0].toFixed(0).padStart(5)+','+q[1].toFixed(0).padStart(5)+') sc='+q[2].toFixed(0)); }
const ribsP=p(0,1.28,lift), waistP=p(0,1.10,lift+0.004), hipP=p(0,0.88,lift);
console.log('torso widths: ribs='+(2*B.ribHalf*bdy*ribsP[2]).toFixed(0)+' waist='+(2*B.waistHalf*bdy*waistP[2]).toFixed(0)+' hips='+(2*B.hipHalf*bdy*hipP[2]).toFixed(0));
console.log('knee sep='+Math.abs(p(B.kneeSpread*bdy,0.62,0)[0]-p(-B.kneeSpread*bdy,0.62,0)[0]).toFixed(0)+'px');
