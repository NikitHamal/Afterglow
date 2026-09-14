// Sweep the pitched-pinhole camera to find a framing where her crown sits
// near the top of the 720 frame and her hips sit in the lower third, while
// her pelvis stays large and the perspective stays believable.
const D = Math.PI/180;
const B = { ankle:0.42, knee:0.60, hip:0.86, mons:0.92, navel:1.05, waist:1.09,
  ribs:1.27, sternum:1.33, shoulder:1.46, chin:1.60, crown:1.74,
  hipHalf:0.150, shHalf:0.168, kneeSpread:0.235, lift:0.15 };
const bdy = 1.009;

function run(c){
  const cosP = Math.cos(c.pitch), sinP = Math.sin(c.pitch);
  const P = (u,d,h) => {
    const f = d - c.dOff, v = h - c.eyeH;
    const z = Math.max(0.12, f*cosP - v*sinP);
    const yc = f*sinP + v*cosP;
    const sc = c.k / z;
    return [c.cx + u*sc, c.cy - yc*sc, sc];
  };
  const crown = P(0, B.crown, B.lift+0.06);
  const chin  = P(0, B.chin+0.02, B.lift+0.055);
  const sh    = P(B.shHalf*bdy, B.shoulder, B.lift);
  const br    = P(0.132*bdy, B.sternum-0.03, B.lift+0.030);
  const waist = P(0, B.waist, B.lift+0.004);
  const mons  = P(0, B.mons, B.lift+0.04);
  const hip   = P(B.hipHalf*bdy, B.hip, B.lift);
  const knee  = P(B.kneeSpread*bdy, B.knee, B.lift+0.14);
  return { crownY:crown[1], chinY:chin[1], shY:sh[1], shW:2*B.shHalf*bdy*sh[2],
    brY:br[1], waistY:waist[1], monsY:mons[1], hipY:hip[1],
    hipW:2*B.hipHalf*bdy*hip[2], kneeY:knee[1], kneeX:knee[0],
    headW:0.145*chin[2], span:hip[1]-crown[1], falloff:knee[2]/chin[2] };
}

console.log('pitch dOff eyeH  k  | crownY chinY shY  brY  waistY monsY hipY | hipW headW span kneeY');
const cfgs = [];
for(const pitch of [50, 58, 64, 70]){
  for(const dOff of [0.60, 0.75, 0.90]){
    for(const eyeH of [0.75, 0.95]){
      cfgs.push({cx:640, cy:360, k:800, dOff, eyeH, pitch:pitch*D});
    }
  }
}
const good = [];
for(const c of cfgs){
  const r = run(c);
  const ok = r.span < 640 && r.hipW > 320 && r.headW > 70;
  if(ok) good.push([c, r]);
  console.log(
    String(Math.round(c.pitch/D)).padEnd(5), String(c.dOff).padEnd(4),
    String(c.eyeH).padEnd(4), String(c.k).padEnd(3), '|',
    r.crownY.toFixed(0).padStart(6), r.chinY.toFixed(0).padStart(5),
    r.shY.toFixed(0).padStart(4), r.brY.toFixed(0).padStart(4),
    r.waistY.toFixed(0).padStart(6), r.monsY.toFixed(0).padStart(5),
    r.hipY.toFixed(0).padStart(4), '|',
    r.hipW.toFixed(0).padStart(4), r.headW.toFixed(0).padStart(5),
    r.span.toFixed(0).padStart(4), r.kneeY.toFixed(0).padStart(5),
    ok ? '  <== OK' : '');
}
console.log('\nviable configs:', good.length);
for(const [c,r] of good){
  console.log('pitch', Math.round(c.pitch/D), 'dOff', c.dOff, 'eyeH', c.eyeH,
    '=> crown', r.crownY.toFixed(0), 'hip', r.hipY.toFixed(0),
    'hipW', r.hipW.toFixed(0), 'headW', r.headW.toFixed(0), 'span', r.span.toFixed(0));
}
