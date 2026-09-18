'use strict';
/* Corrected model: for a body lying AWAY from the lens, the screen-Y of a
   landmark is driven by its DISTANCE, with the height term as a correction.

     sc = k / (d + kpad)          -> near things big, far things small
     y  = horizonY + d * depthK * sc_ref + (eyeH - h) * sc

   i.e. depth recedes DOWN the screen (positive y) while the perspective scale
   shrinks it. Height raises the landmark.
*/
function makeProj(P){
  return function(u, d, h){
    const sc = P.k / (d + P.kpad);
    const y  = P.topY + d * P.depthK + (P.eyeH - h) * sc;
    return [P.cx + u * sc, y, sc];
  };
}
const BODY = {
  foot:  [0.30, 0.30, 0.14],
  knee:  [0.44, 0.62, 0.46],
  hip:   [0.19, 0.88, 0.15],
  mons:  [0.00, 0.94, 0.17],
  waist: [0.00, 1.10, 0.15],
  ribs:  [0.00, 1.28, 0.15],
  stern: [0.00, 1.34, 0.16],
  neck:  [0.00, 1.56, 0.16],
  crown: [0.00, 1.76, 0.18]
};
const cands = [
  {name:'Q1', cx:640, topY:690, depthK:-300, k:760, kpad:0.85, eyeH:0.30},
  {name:'Q2', cx:640, topY:740, depthK:-330, k:700, kpad:0.80, eyeH:0.28},
  {name:'Q3', cx:640, topY:660, depthK:-280, k:820, kpad:0.95, eyeH:0.34},
  {name:'Q4', cx:640, topY:760, depthK:-350, k:660, kpad:0.75, eyeH:0.26},
  {name:'Q5', cx:640, topY:710, depthK:-310, k:730, kpad:0.88, eyeH:0.32}
];
for(const P of cands){
  const pr = makeProj(P);
  const out = ['=== ' + P.name + ' topY=' + P.topY + ' depthK=' + P.depthK + ' k=' + P.k + ' kpad=' + P.kpad];
  let ys=[], xw=[];
  for(const name of Object.keys(BODY)){
    const v = BODY[name]; const p = pr(v[0], v[1], v[2]);
    ys.push(p[1]);
    out.push('   ' + name.padEnd(6) + '(' + p[0].toFixed(0).padStart(5) + ',' + p[1].toFixed(0).padStart(5) + ') sc=' + p[2].toFixed(0));
  }
  const kl = pr(-0.44,0.62,0.46), kr = pr(0.44,0.62,0.46);
  ys.push(kl[1], kr[1]);
  out.push('   kneeL (' + kl[0].toFixed(0) + ',' + kl[1].toFixed(0) + ')  kneeR (' + kr[0].toFixed(0) + ',' + kr[1].toFixed(0) + ')');
  out.push('   spanY ' + Math.min(...ys).toFixed(0) + '..' + Math.max(...ys).toFixed(0) + ' (' + (Math.max(...ys)-Math.min(...ys)).toFixed(0) + 'px)');
  out.push('   scale foot/crown ' + (pr(0,0.30,0)[2]/pr(0,1.76,0)[2]).toFixed(2) + 'x   breastHalfW=' + (0.15*pr(0,1.30,0)[2]).toFixed(0) + 'px  headHalfW=' + (0.09*pr(0,1.66,0)[2]).toFixed(0) + 'px');
  console.log(out.join('\n'));
}
