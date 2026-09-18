function proj(C,u,d,h){ const dd=Math.max(C.near,d-C.eyeD), sc=C.k/dd; return [C.cx+u*sc, C.horizon+(C.eyeH-h)*sc, sc]; }
const BODY = {
  footL:[ 0.30,0.30,0.14], footR:[-0.30,0.30,0.14],
  kneeL:[ 0.44,0.62,0.46], kneeR:[-0.44,0.62,0.46],
  hipL:[ 0.19,0.88,0.11], hipR:[-0.19,0.88,0.11],
  mons:[0,0.94,0.15], waist:[0,1.10,0.10], ribcage:[0,1.28,0.10],
  sternum:[0,1.34,0.13], breastL:[0.15,1.30,0.17], breastR:[-0.15,1.30,0.17],
  shL:[0.21,1.48,0.10], shR:[-0.21,1.48,0.10], neck:[0,1.56,0.13],
  chin:[0,1.62,0.16], crown:[0,1.76,0.18]
};
const cands = [
  {name:'A1', cx:640,eyeH:0.22,eyeD:-0.62,horizon:300,k:1150,near:0.20},
  {name:'A2 lower', cx:640,eyeH:0.22,eyeD:-0.62,horizon:340,k:1150,near:0.20},
  {name:'A3 lower+big', cx:640,eyeH:0.22,eyeD:-0.62,horizon:360,k:1250,near:0.20},
  {name:'A4', cx:640,eyeH:0.20,eyeD:-0.58,horizon:355,k:1300,near:0.19},
  {name:'A5', cx:640,eyeH:0.24,eyeD:-0.60,horizon:375,k:1350,near:0.19},
];
for(const C of cands){
  let ys=[],rows=[];
  for(const [k,v] of Object.entries(BODY)){ const p=proj(C,v[0],v[1],v[2]); ys.push(p[1]);
    rows.push('   '+k.padEnd(9)+'('+p[0].toFixed(0).padStart(5)+','+p[1].toFixed(0).padStart(5)+') sc='+p[2].toFixed(0)); }
  const ymin=Math.min(...ys), ymax=Math.max(...ys);
  console.log('=== '+C.name+'  hz='+C.horizon+' k='+C.k+' eyeH='+C.eyeH+'  spanY='+(ymax-ymin).toFixed(0)+' ['+ymin.toFixed(0)+'..'+ymax.toFixed(0)+']');
  if(process.argv.includes('-v')) rows.forEach(r=>console.log(r));
}
