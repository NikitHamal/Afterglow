// sweep calibration candidates offline and print the projected layout
function makeCam(p){ return p; }
function proj(C,u,d,h){
  const dd=Math.max(C.near,d-C.eyeD), sc=C.k/dd;
  return [C.cx+u*sc, C.horizon+(C.eyeH-h)*sc, sc];
}
const BODY = {
  footL:[ 0.30,0.30,0.14], footR:[-0.30,0.30,0.14],
  kneeL:[ 0.42,0.62,0.44], kneeR:[-0.42,0.62,0.44],
  hipL:[ 0.19,0.88,0.11], hipR:[-0.19,0.88,0.11],
  mons:[0,0.94,0.15],
  waist:[0,1.10,0.10], ribcage:[0,1.28,0.10],
  sternum:[0,1.34,0.13], breastL:[0.15,1.30,0.17], breastR:[-0.15,1.30,0.17],
  shL:[0.21,1.48,0.10], shR:[-0.21,1.48,0.10], neck:[0,1.56,0.13],
  chin:[0,1.62,0.16], crown:[0,1.76,0.18]
};
const cands = [
  {name:'A low+close', cx:640,eyeH:0.22,eyeD:-0.62,horizon:300,k:1150,near:0.20},
  {name:'B mid',       cx:640,eyeH:0.30,eyeD:-0.58,horizon:250,k:1000,near:0.22},
  {name:'C tall',      cx:640,eyeH:0.26,eyeD:-0.50,horizon:210,k:1250,near:0.24},
  {name:'D wide lens', cx:640,eyeH:0.34,eyeD:-0.70,horizon:280,k:820, near:0.18},
];
for(const C of cands){
  let ys=[], xs=[];
  const rows=[];
  for(const [k,v] of Object.entries(BODY)){
    const p=proj(C,v[0],v[1],v[2]);
    ys.push(p[1]); xs.push(p[0]);
    rows.push('   '+k.padEnd(9)+'('+p[0].toFixed(0).padStart(5)+','+p[1].toFixed(0).padStart(5)+') sc='+p[2].toFixed(0));
  }
  const ymin=Math.min(...ys), ymax=Math.max(...ys), xmin=Math.min(...xs), xmax=Math.max(...xs);
  console.log('=== '+C.name+'  '+JSON.stringify(C));
  console.log('   spanY='+(ymax-ymin).toFixed(0)+'px  ['+ymin.toFixed(0)+'..'+ymax.toFixed(0)+']   spanX='+(xmax-xmin).toFixed(0)+'px ['+xmin.toFixed(0)+'..'+xmax.toFixed(0)+']');
  console.log('   scale near/far='+(proj(C,0,0.30,0)[2]/proj(C,0,1.76,0)[2]).toFixed(2)+'x');
  if(process.argv.includes('-v')) rows.forEach(r=>console.log(r));
}
