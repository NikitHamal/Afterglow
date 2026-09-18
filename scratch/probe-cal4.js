// solve for horizon & k so crown->yTop and mons->yMons given eyeH,eyeD,near
function proj(C,u,d,h){ const dd=Math.max(C.near,d-C.eyeD), sc=C.k/dd; return [C.cx+u*sc, C.horizon+(C.eyeH-h)*sc, sc]; }
function span(C){ const ys=[];
  const P={foot:[0,0.30,0.14],knee:[0,0.62,0.44],hip:[0,0.88,0.11],mons:[0,0.94,0.11],
    waist:[0,1.10,0.11],ribs:[0,1.28,0.11],sternum:[0,1.34,0.11],
    neck:[0,1.56,0.11],chin:[0,1.62,0.11],crown:[0,1.76,0.11]};
  for(const v of Object.values(P)) ys.push(proj(C,0,v[1],v[2])[1]);
  return {min:Math.min(...ys),max:Math.max(...ys),sp:Math.max(...ys)-Math.min(...ys)};
}
const want={mons:540, crown:120};
console.log('eyeH eyeD near | hz k -> crownY monsY span');
for(const eyeH of [0.24,0.30,0.36]) for(const eyeD of [-0.45,-0.55,-0.65]) for(const k of [700,850,1000]){
  const near=0.20;
  // y = hz + (eyeH-h)*k/dd   => two eqs, solve hz,k
  const ddM=Math.max(near,0.94-eyeD), ddC=Math.max(near,1.76-eyeD);
  const aM=(eyeH-0.11)/ddM, aC=(eyeH-0.11)/ddC;
  // want[mons] = hz + aM*k ; want[crown] = hz + aC*k
  const kk=(want.mons-want.crown)/(aM-aC);
  const hz=want.mons-aM*kk;
  const C={cx:640,eyeH,eyeD,horizon:hz,k:kk,near};
  const s=span(C);
  console.log(eyeH.toFixed(2),eyeD.toFixed(2),near, '| hz='+hz.toFixed(0).padStart(4), 'k='+kk.toFixed(0).padStart(5),
    '-> crown='+proj(C,0,1.76,0.11)[1].toFixed(0).padStart(4), 'mons='+proj(C,0,0.94,0.11)[1].toFixed(0).padStart(4),
    'span='+s.sp.toFixed(0), 'foot='+proj(C,0,0.30,0.14)[1].toFixed(0), 'knee='+proj(C,0,0.62,0.44)[1].toFixed(0));
}
