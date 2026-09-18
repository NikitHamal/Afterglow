function proj(C,u,d,h){ const dd=Math.max(C.near,d-C.eyeD), sc=C.k/dd; return [C.cx+u*sc, C.horizon+(C.eyeH-h)*sc, sc]; }
// torso/head all on ONE body plane at h=H0; limbs override h
const H0=0.11;
const BODY = {
  footL:[ 0.30,0.30,0.14], footR:[-0.30,0.30,0.14],
  kneeL:[ 0.44,0.62,0.46], kneeR:[-0.44,0.62,0.46],
  hipL:[ 0.19,0.88,H0],    hipR:[-0.19,0.88,H0],
  mons:[0,0.94,0.14], waist:[0,1.10,H0], ribcage:[0,1.28,H0],
  sternum:[0,1.34,H0], breastL:[0.15,1.30,0.17], breastR:[-0.15,1.30,0.17],
  shL:[0.21,1.48,H0], shR:[-0.21,1.48,H0], neck:[0,1.56,0.13],
  chin:[0,1.62,0.15], crown:[0,1.76,0.16]
};
const C={cx:640,eyeH:0.30,eyeD:-0.55,horizon:250,k:1050,near:0.20};
let ys=[];for(const [k,v] of Object.entries(BODY)){const p=proj(C,v[0],v[1],v[2]);ys.push(p[1]);
  console.log('  '+k.padEnd(9)+'('+p[0].toFixed(0).padStart(5)+','+p[1].toFixed(0).padStart(5)+') sc='+p[2].toFixed(0));}
console.log('spanY '+Math.min(...ys).toFixed(0)+'..'+Math.max(...ys).toFixed(0));
