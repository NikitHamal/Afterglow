// Afterglow — module: side (loaded by index.html)
'use strict';
function herExpression(){
  let eye=lerp(.72,.10,sm(5,95,G.pleasure)), rolled=0, mouth=G.pleasure/100*.35, blush=.08+G.pleasure*.008;
  let brow=lerp(-.05,.45,sm(20,92,G.pleasure)), tilt=.10+G.pleasure*.0032;
  G.mouths.forEach(m=>{ const u=(G.t-m.t0)/m.dur; if(u>0&&u<1) mouth+=Math.sin(Math.PI*u)**.7*m.i*.6; });
  if(G.speech) mouth+=.25;
  if(G.blinkPh>0) eye*=1-G.blinkPh;
  if(G.state==='orgasm'){ const e=Math.sin(Math.PI*clamp(G.orgT/5.2,0,1));
    rolled=.7+e*.3; eye=lerp(eye,.06,e); mouth=Math.max(mouth,.85*e); tilt=.5; blush=1; brow=.6; }
  if(G.after>0){ eye=Math.min(eye,.14); mouth=Math.max(mouth,.15); }
  if(G.state==='finish'&&G.finishT>1){ eye=.06; mouth=.2; }
  if(G.kiss>.6){ mouth=Math.min(mouth,.5); }
  return {eye:clamp(eye,0,1),rolled:clamp(rolled,0,1),mouth:clamp(mouth,0,1),blush:clamp(blush,0,1),brow,tilt};
}
const legUp={k:[700,398],a:[800,348]}, legWrap={k:[748,418],a:[856,434]};
function lerpp(a,b,t){return [lerp(a[0],b[0],t),lerp(a[1],b[1],t)]}
function drawHer(){
  const E=herExpression();
  const wrap=sm(62,88,G.pleasure)*(G.state==='orgasm'?1:0);
  // (hair spill)
  X.fillStyle='#231318';
  X.beginPath(); X.moveTo(345,492);
  X.bezierCurveTo(300,470,240,486,214,528); X.bezierCurveTo(196,560,220,586,268,590);
  X.bezierCurveTo(320,596,360,580,372,560); X.bezierCurveTo(360,536,352,510,345,492); X.fill();
  X.strokeStyle='rgba(120,70,80,.35)'; X.lineWidth=2;
  for(let i=0;i<5;i++){ X.beginPath(); X.moveTo(340,500);
    X.quadraticCurveTo(280-((i*37)%60),520+((i*17)%40),214+((i*23)%40),570+((i*11)%18)); X.stroke(); }
  // far leg (behind him)
  const K2=lerpp(legUp.k,legWrap.k,wrap), A2=lerpp(legUp.a,legWrap.a,wrap);
  X.save(); X.translate(chaos(2)*.4,0);
  capsule(sp(608,505,.06),[K2[0]-16,K2[1]+8],26,20,Dskin.herDk);
  capsule([K2[0]-16,K2[1]+8],[A2[0]-42,A2[1]+22],18,12,Dskin.herDk);
  X.restore();
  // torso
  const br = Math.sin(G.t*TAU*(0.16+G.pleasure*.004))*2;
  X.beginPath();
  let p=sp(398,524,.02); X.moveTo(p[0],p[1]);
  p=sp(505,540,.03); X.quadraticCurveTo(p[0],p[1],(p=sp(600,527,.07))[0],p[1]);
  p=sp(650,548,.08); X.quadraticCurveTo(p[0],p[1],(p=sp(666,557,.09))[0],p[1]);
  p=sp(680,522,.09); X.quadraticCurveTo((p=sp(678,508,.09))[0],p[1],(p=sp(664,492,.08))[0],p[1]); // pelvis/mons
  p=sp(622,498,.05); X.quadraticCurveTo(p[0],p[1],(p=sp(556,503,.04))[0],p[1]);
  p=sp(492,494+br,.02); X.quadraticCurveTo(p[0],p[1],(p=sp(452,472+br,.02))[0],p[1]); // chest
  p=sp(420,486+br,.015); X.quadraticCurveTo((p=sp(404,498,.01))[0],p[1],sp(398,524,.02)[0],sp(398,524,.02)[1]);
  X.closePath();
  X.fillStyle=sg(440,560,'#f4cba8','#d69a75'); X.fill();
  // soft belly / rib shading
  shade(540,510,70,24,'rgba(170,100,75,.20)',0);
  shade(640,520,40,16,'rgba(170,100,75,.18)',.3);
  // side highlight (rim light)
  X.save(); X.globalCompositeOperation='soft-light';
  shade(440,500,30,80,'rgba(255,230,205,.30)',-.1);
  X.restore();
  // chest blush patch
  X.fillStyle=`rgba(226,96,110,${.05+E.blush*.20})`;
  X.beginPath(); X.ellipse(470,487,34,20,-.25,0,TAU); X.fill();
  // arousal rubor: mottled flush across chest/belly
  if(G.ar>30){ const ra=(G.ar-30)/70;
    X.fillStyle=`rgba(220,90,105,${.05+.10*ra})`;
    [[480,492,16,9],[520,498,20,10],[560,500,18,9],[440,496,12,7]].forEach(([x,y,rx,ry])=>{
      const q=sp(x,y,.03); X.beginPath(); X.ellipse(q[0],q[1],rx,ry,-.2,0,TAU); X.fill(); });
  }
  // navel: hood shadow + opening
  X.strokeStyle='rgba(150,95,70,.45)'; X.lineWidth=2;
  { const n0=sp(540,505,.04);
    X.fillStyle='rgba(140,85,65,.35)';
    X.beginPath(); X.ellipse(n0[0],n0[1]-3,4.5,2.5,0,0,TAU); X.fill();
    X.beginPath(); X.ellipse(n0[0],n0[1]+1.5,3,4.5,0,0,TAU); X.stroke(); }
  // linea alba + iliac crest + ASIS dimples
  X.strokeStyle='rgba(150,95,70,.22)'; X.lineWidth=1.5; X.beginPath();
  { const l1=sp(540,505,.04), l2=sp(505,498,.03);
    X.moveTo(l1[0],l1[1]-8); X.quadraticCurveTo(l2[0],l2[1],l2[0]-4,l2[1]-16); X.stroke(); }
  X.fillStyle='rgba(140,85,65,.30)';
  { const d1=sp(596,512,.06), d2=sp(588,528,.06);
    X.beginPath(); X.ellipse(d1[0],d1[1],2.5,3.5,.3,0,TAU); X.fill();
    X.beginPath(); X.ellipse(d2[0],d2[1],2,3,.3,0,TAU); X.fill(); }
  X.beginPath(); const c=sp(430,486+br,.02); X.moveTo(c[0]-8,c[1]);X.quadraticCurveTo(c[0]+2,c[1]+6,c[0]+16,c[1]+3); X.stroke();
  // rib cage line
  X.strokeStyle='rgba(150,95,70,.18)'; X.lineWidth=1.5; X.beginPath();
  const rc=sp(500,500+br,.02);
  X.moveTo(rc[0]-6,rc[1]); X.quadraticCurveTo(rc[0]+2,rc[1]+8,rc[0]+10,rc[1]); X.stroke();
  // mons shading + pubic hair
  { const m=sp(668,500,.085);
    X.fillStyle='rgba(170,100,75,.20)';
    X.beginPath(); X.ellipse(m[0],m[1],12,14,.15,0,TAU); X.fill();
    X.strokeStyle='rgba(60,32,30,.55)'; X.lineWidth=1.3;
    for(let i=0;i<34;i++){
      const hx=m[0]-10+((i*37)%20), hy=m[1]-14+((i*23)%22);
      const a=-.4+((i*13)%10)*.09;
      X.beginPath(); X.moveTo(hx,hy); X.lineTo(hx+Math.cos(a)*5,hy+Math.sin(a)*5-2); X.stroke();
    } }
  return {E,wrap,K2,A2,br};
}
function drawHerHead(E){
  const neck=sp(392,516,.015);
  const piv=[382,510];
  X.save();
  X.translate(piv[0],piv[1]-G.pleasure*.02);
  // neck
  capsule([piv[0]+6,piv[1]-8],sp(352,478,.008),15,15,sg(470,520,'#f0c4a0','#d69a75'));
  X.rotate(-Math.PI/2-E.tilt-G.nod*.12);
  const sway=Math.sin(G.t*TAU*.33)*.015*(1+G.pleasure*.02);
  X.rotate(sway);
  // skull + jaw
  X.fillStyle=sg(-90,-10,'#f4cba8','#e2a983');
  X.beginPath(); X.ellipse(0,-36,33,38,0,0,TAU); X.fill();
  X.beginPath(); X.moveTo(-30,-32); X.quadraticCurveTo(-27,4,-6,10); X.quadraticCurveTo(12,13,26,-8);
  X.quadraticCurveTo(34,-24,30,-40); X.closePath(); X.fill();
  // face shading: cheek hollows, jaw, under-eye
  X.save();
  shade(14,-22,18,12,'rgba(170,100,75,.22)',.4);  // right cheek hollow
  shade(-18,-22,16,11,'rgba(170,100,75,.20)',-.4); // left cheek hollow
  shade(0,8,20,10,'rgba(170,100,75,.18)',0);       // under jaw
  shade(0,-26,40,8,'rgba(170,100,75,.10)',0);      // under eye line
  // forehead / nose bridge highlight
  X.globalCompositeOperation='soft-light';
  shade(-6,-58,6,30,'rgba(255,235,215,.40)',0);
  X.restore();
  // face (features)
  // face (features)
  const Eo=E.eye, roll=E.rolled;
  X.strokeStyle='#4a2c2f'; 
  for(const s of [-1,1]){
    const ex=s*11, ey=-46;
    // lash line / lid
    X.lineWidth=2.6; X.beginPath();
    const openness=Eo*(1-roll*.5);
    if(openness>.12){ X.moveTo(ex-8,ey); X.quadraticCurveTo(ex,ey-6*openness-2,ex+8,ey+1);
      X.stroke();
      // white & iris
      X.beginPath(); X.moveTo(ex-8,ey); X.quadraticCurveTo(ex,ey-6*openness-2,ex+8,ey+1); X.quadraticCurveTo(ex,ey+5*openness,ex-8,ey); X.fillStyle='#f6e9e4'; X.fill();
      X.fillStyle='#4a2c33'; X.beginPath(); X.arc(ex+roll*3,ey-2*openness+(-3*roll),4.2*Math.max(openness,.25),0,TAU); X.fill();
      X.fillStyle='rgba(255,255,255,.8)'; X.beginPath(); X.arc(ex+roll*3-1,ey-3*openness-1,1.2,0,TAU); X.fill();
    } else { X.beginPath(); X.moveTo(ex-8,ey+1); X.quadraticCurveTo(ex,ey+3,ex+8,ey+1); X.stroke(); }
    // lashes (upper flick + lower)
    X.lineWidth=1.4; X.beginPath(); X.moveTo(ex+8,ey+1); X.lineTo(ex+11,ey-2); X.stroke();
    X.beginPath(); X.moveTo(ex-6,ey+4*openness); X.lineTo(ex-8,ey+4*openness+2.5); X.stroke();
    // brow
    X.lineWidth=2.2; X.beginPath();
    X.moveTo(ex-7,ey-12); X.quadraticCurveTo(ex+1,ey-15-E.brow*5,ex+9,ey-11-E.brow*7); X.stroke();
    // blush
    X.fillStyle=`rgba(230,96,110,${.10+E.blush*.38})`;
  }
  X.fillStyle=`rgba(230,96,110,${.10+E.blush*.38})`;
  X.beginPath(); X.ellipse(-16,-34,11,7,.3,0,TAU); X.fill();
  X.beginPath(); X.ellipse(14,-33,9,6,-.2,0,TAU); X.fill();
  // nose + nostril + philtrum
  X.strokeStyle='rgba(150,90,80,.55)'; X.lineWidth=1.8; X.beginPath();
  X.moveTo(-2,-38); X.quadraticCurveTo(-5,-33,-3,-30); X.stroke();
  X.lineWidth=1.4; X.beginPath(); X.arc(-4,-30,1.4,0,TAU); X.stroke();
  X.strokeStyle='rgba(150,90,80,.35)'; X.beginPath();
  X.moveTo(-1,-28); X.lineTo(-1,-25); X.moveTo(1,-28); X.lineTo(1,-25); X.stroke();
  // mouth (cupid's bow, teeth + tongue when open)
  const mo=E.mouth, mx=0,my=-22;
  X.fillStyle='#b3555f';
  X.beginPath();
  if(mo>.06){ X.ellipse(mx,my,7+mo*3,2+mo*9,0,0,TAU); X.fillStyle='#7e333e'; X.fill();
    if(mo>.3){ X.fillStyle='rgba(250,240,238,.9)';
      X.beginPath(); X.ellipse(mx,my-3-mo*2,5.5+mo*1.5,2.2,0,0,TAU); X.fill(); }
    X.beginPath(); X.ellipse(mx,my+3+mo*4,6+mo*2,1.5+mo*2.5,0,0,TAU); X.fillStyle='rgba(190,95,110,.9)'; X.fill(); // tongue hint
    X.strokeStyle='#a04a55'; X.lineWidth=1.6; X.beginPath();
    X.moveTo(mx-8,my-3); X.quadraticCurveTo(mx-2,my-5,mx,my-3.5);
    X.quadraticCurveTo(mx+2,my-5,mx+8,my-3); X.stroke();
    X.beginPath(); X.moveTo(mx-8,my-2); X.quadraticCurveTo(mx,my-4-mo*2,mx+8,my-2); X.stroke();
  } else { X.lineWidth=2; X.beginPath(); X.moveTo(mx-7,my); X.quadraticCurveTo(mx+1,my+2,mx+7,my); X.stroke(); }
  // ear (behind jaw)
  X.fillStyle=sg(-90,-10,'#eebd97','#d69a75');
  X.beginPath(); X.ellipse(24,-34,5,7,0,0,TAU); X.fill();
  X.strokeStyle='rgba(150,90,80,.5)'; X.lineWidth=1.4; X.beginPath();
  X.arc(24,-34,2.6,0,TAU); X.stroke();
  // beauty mark
  X.fillStyle='rgba(90,50,50,.6)'; X.beginPath(); X.arc(8,-28,1.1,0,TAU); X.fill();
  // bangs + side hair
  X.fillStyle='#2a181d';
  X.beginPath(); X.moveTo(-33,-52); X.quadraticCurveTo(-6,-84,30,-56);
  X.quadraticCurveTo(38,-40,30,-46);
  X.quadraticCurveTo(24,-62,4,-58); X.quadraticCurveTo(22,-54,14,-46);
  X.quadraticCurveTo(8,-58,-8,-56); X.quadraticCurveTo(2,-52,-6,-44);
  X.quadraticCurveTo(-12,-58,-28,-50); X.quadraticCurveTo(-22,-60,-33,-52); X.closePath();
  X.moveTo(-33,-46); X.quadraticCurveTo(-44,-30,-38,-8); X.quadraticCurveTo(-30,-28,-33,-46); X.fill();
  X.strokeStyle='rgba(130,75,85,.4)'; X.lineWidth=2;
  X.beginPath(); X.moveTo(-28,-58); X.quadraticCurveTo(-8,-72,18,-64); X.stroke();
  X.restore();
}
function drawBreast(E,br){
  const jig=G.breast.p*.9, squash=sm(.86,1,G.depth);
  const p=sp(452,470+br,.035);
  X.save(); X.translate(p[0],p[1]+jig); X.rotate(-.45);
  X.fillStyle=sg(-34,26,'#f6d0ac','#d69a75');
  X.beginPath(); X.ellipse(0,0,26*(1+squash*.08)-jig*.12,27*(1-squash*.18)+jig*.18,0,0,TAU); X.fill();
  // underside shadow
  X.save(); X.globalCompositeOperation='multiply';
  shade(6,18,22,10,'rgba(170,100,75,.40)',0);
  X.restore();
  // top highlight
  X.save(); X.globalCompositeOperation='soft-light';
  shade(-10,-14,12,8,'rgba(255,235,215,.55)',-.4);
  X.restore();
  // areola (+puffs with arousal)
  const aer=6.5+1.5*clamp(G.ar/100,0,1);
  X.fillStyle='rgba(214,140,120,.85)'; X.beginPath(); X.arc(4,-16,aer,0,TAU); X.fill();
  // Montgomery tubercles
  X.fillStyle='rgba(190,120,105,.7)';
  for(let i=0;i<7;i++){ const a=i/7*TAU+.4;
    X.beginPath(); X.arc(4+Math.cos(a)*aer*.68,-16+Math.sin(a)*aer*.68,.8,0,TAU); X.fill(); }
  // inframammary fold
  X.strokeStyle='rgba(150,90,70,.35)'; X.lineWidth=1.8; X.beginPath();
  X.moveTo(-14,20); X.quadraticCurveTo(2,26,16,18); X.stroke();
  // nipple (erectile: firms up with arousal)
  const er=clamp(G.ar/100,0,1), nr=3.4+1.8*er;
  X.fillStyle='#c25f63'; X.beginPath(); X.arc(4,-16-er*2,nr,0,TAU); X.fill();
  // nipple highlight
  X.fillStyle='rgba(255,255,255,.3)'; X.beginPath(); X.arc(3,-17-er*2,1.2,0,TAU); X.fill();
  // soft sheen
  X.fillStyle='rgba(255,255,255,.18)'; X.beginPath(); X.ellipse(-8,-10,7,4,-.7,0,TAU); X.fill();
  X.restore();
}
/* ---------- his rubbing hand (side view) ---------- */
function drawRubFX(br){
  const rub=G.rub;
  if(rub<.03||G.state==='climax'||G.state==='finish') return;
  const jig=G.breast.p*.9;
  const bp=sp(452,470+br,.035);
  const bx=bp[0]+2, by=bp[1]+jig;
  const w1=Math.sin(G.t*9)*7*rub, w2=Math.cos(G.t*7.3)*6*rub;
  const hx=bx+8+w1, hy=by-4+w2;
  X.save(); X.globalAlpha=clamp(rub*1.4,0,1);
  // forearm from his shoulder
  const sh=hp(500,424,.2);
  const el=[(sh[0]+hx)/2+30,(sh[1]+hy)/2-22];
  capsule(sh,el,22,18,sg(400,560,'#d5a071','#a96c44'));
  capsule(el,[hx,hy],17,14,sg(400,560,'#cf9a6b','#a2653f'));
  // pressure shadow on her breast
  X.save(); X.globalCompositeOperation='multiply';
  shade(hx-6,hy+8,20,12,'rgba(150,80,60,.35)',-.3);
  X.restore();
  // palm + fanned fingers kneading in a circle
  X.fillStyle=sg(400,560,'#d5a071','#a96c44');
  X.beginPath(); X.ellipse(hx,hy,15,11,-.5,0,TAU); X.fill();
  X.strokeStyle='rgba(120,70,45,.55)'; X.lineWidth=1.6;
  for(let i=0;i<3;i++){
    const a=-.9+i*.55+Math.sin(G.t*9+i)*.12;
    const fx=hx+Math.cos(a)*14, fy=hy+Math.sin(a)*12;
    X.beginPath(); X.moveTo(hx+Math.cos(a)*6,hy+Math.sin(a)*5);
    X.lineTo(fx+Math.cos(a)*9,fy+Math.sin(a)*7); X.stroke();
  }
  // circular motion streak
  X.strokeStyle=`rgba(255,220,200,${.30*rub})`; X.lineWidth=2.5;
  X.beginPath(); X.ellipse(bx,by,30,26,-.45,G.t*9,G.t*9+1.7); X.stroke();
  // expanding ripple rings
  for(let i=0;i<2;i++){
    const u=((G.t*1.6+i*.5)%1);
    X.strokeStyle=`rgba(255,150,170,${(1-u)*.30*rub})`; X.lineWidth=2;
    X.beginPath(); X.ellipse(bx,by,12+u*36,11+u*32,-.45,0,TAU); X.stroke();
  }
  X.restore();
}
function drawVulva(){
  const eng=clamp(G.ar/100,0,1), open=3+8*G.depth;
  // labia majora (outer folds)
  X.strokeStyle='rgba(190,120,105,.65)'; X.lineWidth=3.5;
  X.beginPath(); X.ellipse(646,499,7,14,.12,0,TAU); X.stroke();
  X.beginPath(); X.ellipse(659,499,7,14,-.12,0,TAU); X.stroke();
  X.strokeStyle='rgba(150,85,70,.35)'; X.lineWidth=2;
  X.beginPath(); X.ellipse(641,499,10,17,.12,0,TAU); X.stroke();
  X.beginPath(); X.ellipse(664,499,10,17,-.12,0,TAU); X.stroke();
  // labia minora (inner, pinker + engorged)
  X.strokeStyle=`rgba(${200+20*eng|0},${120+20*eng|0},${125+15*eng|0},.8)`; X.lineWidth=2.5;
  X.beginPath(); X.ellipse(649,500,3.5,9+3*eng,.08,0,TAU); X.stroke();
  X.beginPath(); X.ellipse(656,500,3.5,9+3*eng,-.08,0,TAU); X.stroke();
  // clitoral hood + glans (engorges with arousal)
  X.fillStyle=sg(480,500,'#f4cba8','#d69a75');
  X.beginPath(); X.ellipse(652.5,490,4.5,3.5,0,Math.PI,0); X.fill();
  X.fillStyle=`rgba(${205+25*eng|0},110,120,.95)`;
  X.beginPath(); X.arc(652.5,491.5,1.4+1.6*eng,0,TAU); X.fill();
  X.fillStyle='rgba(255,255,255,.5)';
  X.beginPath(); X.arc(652,491,0.8,0,TAU); X.fill();
  // vaginal introitus (opens + stretches with depth)
  X.fillStyle='rgba(96,32,42,.6)';
  X.beginPath(); X.ellipse(652.5,502,3.2,open*.55,0,0,TAU); X.fill();
  X.strokeStyle='rgba(150,80,85,.7)'; X.lineWidth=2;
  X.beginPath(); X.ellipse(652.5,502,3.2,open*.55,0,0,TAU); X.stroke();
  // wetness gloss
  if(G.ar>25){ X.strokeStyle=`rgba(255,255,255,${.15+.30*eng})`; X.lineWidth=2.5;
    X.beginPath(); X.moveTo(655.5,492); X.quadraticCurveTo(657,498+open*.4,654,503+open*.5); X.stroke(); }
}
function drawShaft(){
  const A=hp(760,468,1);
  const V=[653,495];
  const ang=Math.atan2(V[1]-A[1],V[0]-A[0]);
  // shaft (visible outside portion)
  capsule(A,V,11,13,'#d8a082');
  // glans when shallow
  if(G.depth<.45){ const gl=[V[0]+Math.cos(ang)*6,V[1]+Math.sin(ang)*6];
    capsule(V,gl,9,11,'#db9a86'); }
  // balls
  const bs=1-G.depth*.5;
  if(bs>.25&&G.depth<.8){ const bp=[A[0]-Math.cos(ang)*8,A[1]-Math.sin(ang)*8];
    capsule([bp[0]-4,bp[1]+10],[bp[0]-10,bp[1]+16],10*bs,8*bs,'#c78e6a');
    capsule([bp[0]+4,bp[1]+12],[bp[0]+8,bp[1]+18],9*bs,7*bs,'#d0936d'); }
  // wet sheen
  if(G.ar>28){ X.strokeStyle='rgba(255,255,255,.30)'; X.lineWidth=3;
    X.beginPath(); X.moveTo(A[0],A[1]-7); X.lineTo(V[0]+4,V[1]-8); X.stroke(); }
}
/* ---------- him ---------- */
function drawHim(E){
  const k=G.kiss;
  const neck=hp(452,390,.18), sh=hp(522,385,.22), mid=hp(612,396,.45), low=hp(688,410,.68);
  const b1=hp(772,432,1), b2=hp(800,bufferY(462),1);
  function bufferY(y){return y}
  const b3=hp(796,502,1), uf=hp(752,502,.7), bf=hp(692,464,.6), cu=hp(526,452,.33), cf=hp(462,430,.24), throat=hp(434,406,.2);
  // legs
  X.save(); X.translate(chaos(1)*.5,0);
  capsule(hp(790,488,.5),hp(905,553,.16),48,42,sg(380,600,'#b57c53','#96613e'));
  capsule(hp(905,553,.16),hp(988,576,.06),34,24,sg470());
  function sg470(){return sg(470,620,'#a86e48','#8a5836')}
  capsule(hp(988,576,.06),hp(1032,560,.04),20,12,sg470());
  // foot: heel, toes with nails
  X.fillStyle='#96613e';
  const f=hp(1032,560,.04); X.beginPath(); X.ellipse(f[0],f[1],14,8,.2,0,TAU); X.fill();
  for(let i=0;i<4;i++){
    X.fillStyle='#96613e';
    X.beginPath(); X.arc(f[0]+10+i*4.5,f[1]-4+((i%2)*2),3-i*.4,0,TAU); X.fill();
    X.fillStyle='rgba(240,220,210,.6)';
    X.beginPath(); X.ellipse(f[0]+10+i*4.5,f[1]-5+((i%2)*2),1.5,1,0,0,TAU); X.fill();
  }
  X.restore();
  // calf muscle shading
  X.save(); X.globalCompositeOperation='multiply';
  { const cm=hp(905,553,.16); shade(cm[0]-8,cm[1]-30,26,34,'rgba(70,40,22,.18)',.2); }
  X.restore();
  // torso
  X.beginPath(); X.moveTo(neck[0],neck[1]);
  X.quadraticCurveTo(sh[0]-14,sh[1]-16,sh[0],sh[1]);
  X.quadraticCurveTo(mid[0],mid[1]-6,low[0],low[1]);
  X.quadraticCurveTo(b1[0],b1[1]-10,b2[0],b2[1]+G.butt.p*.5);
  X.quadraticCurveTo(b3[0]+6,b3[1]-2,b3[0],b3[1]);
  X.quadraticCurveTo(uf[0],uf[1],bf[0],bf[1]);
  X.quadraticCurveTo(cu[0],cu[1],cf[0],cf[1]);
  X.quadraticCurveTo(throat[0],throat[1],neck[0],neck[1]);
  X.closePath();
  X.fillStyle=sg(370,510,'#dba06f','#a96c44'); X.fill();
  // soft body shading: lat, lower back, side
  X.save(); X.globalCompositeOperation='multiply';
  shade(mid[0],mid[1]+20,80,60,'rgba(80,40,20,.18)',0);
  shade(b2[0]+10,b2[1]+10,30,30,'rgba(80,40,20,.22)',.2);
  shade(low[0]+6,low[1]+8,40,20,'rgba(80,40,20,.16)',.1);
  X.restore();
  // rim light along back
  X.save(); X.globalCompositeOperation='soft-light';
  shade(sh[0]-10,sh[1]+10,20,80,'rgba(255,210,170,.35)',.3);
  shade(b1[0]-10,b1[1]+10,18,60,'rgba(255,210,170,.30)',.4);
  X.restore();
  // shading: spine groove, trapezius, lats, scapula
  X.strokeStyle='rgba(90,52,32,.45)'; X.lineWidth=2.5;
  X.beginPath(); X.moveTo(sh[0]+4,sh[1]+16); X.quadraticCurveTo(mid[0]+8,mid[1]+16,low[0]+6,low[1]-6); X.stroke();
  X.strokeStyle='rgba(90,52,32,.30)'; X.lineWidth=2;
  X.beginPath(); X.moveTo(neck[0]+8,neck[1]+14); X.quadraticCurveTo(sh[0]-2,sh[1]+2,sh[0]+10,sh[1]+12); X.stroke();
  X.beginPath(); X.moveTo(sh[0]-16,sh[1]+26); X.quadraticCurveTo(mid[0]-14,mid[1]+8,mid[0]-6,mid[1]+22); X.stroke();
  X.fillStyle='rgba(90,52,32,.20)';
  X.beginPath(); X.ellipse(sh[0]-22,sh[1]+22,10,14,.3,0,TAU); X.fill();
  // glute cleft + fold + cheek shading
  X.strokeStyle='rgba(70,38,24,.55)'; X.lineWidth=3;
  X.beginPath(); X.moveTo(b2[0]-2,b2[1]+6); X.quadraticCurveTo(b2[0]-8,b2[1]+26,b3[0]-4,b3[1]-8); X.stroke();
  X.strokeStyle='rgba(70,38,24,.35)'; X.lineWidth=2;
  X.beginPath(); X.moveTo(b2[0]+16,b2[1]+22); X.quadraticCurveTo(b2[0]+2,b2[1]+32,b3[0]+10,b3[1]+8); X.stroke();
  X.save(); X.globalCompositeOperation='multiply';
  shade(b2[0]+16,b2[1]+4,26,22,'rgba(80,40,20,.20)',.3);
  X.restore();
  X.strokeStyle='rgba(255,215,175,.16)'; X.lineWidth=4;
  X.beginPath(); X.moveTo(sh[0]-6,sh[1]-10); X.quadraticCurveTo(mid[0]-4,mid[1]-12,low[0]-2,low[1]-10); X.stroke();
  // contact shadow on her
  X.strokeStyle='rgba(30,12,10,.28)'; X.lineWidth=8; X.beginPath();
  X.moveTo(cu[0],cu[1]+4); X.quadraticCurveTo((cu[0]+bf[0])/2,(cu[1]+bf[1])/2+6,bf[0],bf[1]+2); X.stroke();
  // near arm (planted) with elbow + fingered hand
  const el=hp(436,500,.06), ha=hp(398,560,0);
  capsule(hp(478,428,.16),el,26,22,sg(400,560,'#d5a071','#a96c44'));
  X.fillStyle='rgba(255,215,175,.12)';
  X.beginPath(); X.ellipse(el[0]-6,el[1]-8,8,10,-.4,0,TAU); X.fill();
  capsule(el,ha,20,15,sg(400,560,'#cf9a6b','#a2653f'));
  X.fillStyle='#c08f60'; X.beginPath(); X.ellipse(ha[0],ha[1],15,9,-.5,0,TAU); X.fill();
  X.strokeStyle='rgba(110,65,40,.55)'; X.lineWidth=1.8;
  { const fa=Math.atan2(ha[1]-el[1],ha[0]-el[0]);
    for(let i=0;i<4;i++){ const a=fa-.45+i*.3, L=12-i*1.5;
      X.beginPath(); X.moveTo(ha[0]+Math.cos(a)*10,ha[1]+Math.sin(a)*6);
      X.lineTo(ha[0]+Math.cos(a)*(10+L),ha[1]+Math.sin(a)*(6+L)); X.stroke(); } }
  // head
  let hd=hp(408,368,.2); let rot=.15+G.depth*.06+G.nod*.08;
  if(k>.02){ hd=[lerp(hd[0],352,k),lerp(hd[1],S_kissY(),k)]; rot=lerp(rot,.55,k); }
  function S_kissY(){return 432}
  X.save(); X.translate(hd[0],hd[1]); X.rotate(rot);
  X.fillStyle=sg(-50,10,'#dba06f','#a96c44');
  X.beginPath(); X.ellipse(-4,0,24,26,0,0,TAU); X.fill();
  X.beginPath(); X.moveTo(-14,20); X.quadraticCurveTo(-26,26,-34,18); X.quadraticCurveTo(-40,4,-30,-6); X.closePath(); X.fill(); // jaw→neck merge
  // face shading: cheek, jaw, temple
  X.save(); X.globalCompositeOperation='multiply';
  shade(-18,4,12,8,'rgba(90,50,28,.30)',.3);
  shade(12,2,8,7,'rgba(90,50,28,.22)',-.3);
  shade(-4,18,14,5,'rgba(90,50,28,.20)',0);
  X.restore();
  X.save(); X.globalCompositeOperation='soft-light';
  shade(-8,-12,8,16,'rgba(255,225,195,.35)',0);
  X.restore();
  X.fillStyle='#22150d'; X.beginPath();
  X.moveTo(-28,-14); X.quadraticCurveTo(-6,-34,20,-16); X.quadraticCurveTo(24,-2,16,2);
  X.quadraticCurveTo(2,-12,-14,-6); X.quadraticCurveTo(-22,-2,-28,-14); X.closePath(); X.fill();
  // hair highlight
  X.save(); X.globalCompositeOperation='soft-light';
  shade(-4,-22,16,8,'rgba(180,140,90,.40)',-.3);
  X.restore();
  X.fillStyle='#a96c44'; X.beginPath(); X.ellipse(6,6,5,7,0,0,TAU); X.fill(); // ear
  X.restore();
  // sweat on his back
  if(G.pleasure>60||G.tired){ X.fillStyle='rgba(255,255,255,.10)';
    X.beginPath(); X.ellipse(mid[0]+30,mid[1]+50,26,60,.2,0,TAU); X.fill(); }
}
function drawHerNear(E,out){
  const {wrap,K2,A2,br}=out;
  const org=G.state==='orgasm'?Math.sin(Math.PI*clamp(G.orgT/5.2,0,1)):0;
  const trem=org*(Math.sin(G.t*39)+Math.sin(G.t*53)*.5)*4;
  const K=[lerp(K2[0],K2[0]+8,0)+trem, K2[1]+trem*.6];
  const A=[A2[0]+2,A2[1]];
  const hip=sp(622,502,.07);
  capsule(hip,K,30,25,sg(340,520,'#f4cba8','#d69a75'));
  capsule(K,A,21,14,sg(340,520,'#f4cba8','#d69a75'));
  // knee: patella + shading
  X.fillStyle='rgba(255,235,215,.20)';
  X.beginPath(); X.ellipse(K[0]+4,K[1]-4,9,11,.2,0,TAU); X.fill();
  X.save(); X.globalCompositeOperation='multiply';
  shade(K[0]-8,K[1]+12,12,8,'rgba(170,100,75,.30)',.4);
  X.restore();
  // quadriceps line
  X.strokeStyle='rgba(190,120,100,.25)'; X.lineWidth=2;
  X.beginPath(); X.moveTo(hip[0]+16,hip[1]-12); X.quadraticCurveTo(K[0]-2,K[1]-22,K[0]+10,K[1]-10); X.stroke();
  // foot: heel, arch, 5 toes with nails
  const curl=.3+E.blush*.4+org*.5;
  X.save(); X.translate(A[0],A[1]); X.rotate(.4-curl*.5+(wrap? .5:1.2)*0);
  X.fillStyle=sg(340,520,'#f4cba8','#d69a75');
  X.beginPath(); X.moveTo(-10,-8); X.quadraticCurveTo(14,-6,20,4); X.quadraticCurveTo(16,14,-2,14);
  X.quadraticCurveTo(-14,12,-10,-8); X.fill();
  X.save(); X.globalCompositeOperation='multiply';
  shade(-2,8,10,5,'rgba(170,100,75,.30)',0);
  X.restore();
  for(let i=0;i<5;i++){
    const tx=2+i*4.6, ty=2-Math.sin(i/4*Math.PI)*3-curl*2, tr=3.4-i*.45;
    X.fillStyle=sg(340,520,'#f4cba8','#d69a75');
    X.beginPath(); X.arc(tx,ty-curl*3,tr,0,TAU); X.fill();
    X.fillStyle='rgba(250,230,225,.7)';
    X.beginPath(); X.ellipse(tx,ty-curl*3-1,tr*.55,tr*.4,0,0,TAU); X.fill();
  }
  X.restore();
  // inner thigh line
  X.strokeStyle='rgba(190,120,100,.35)'; X.lineWidth=2;
  X.beginPath(); X.moveTo(hip[0]+12,hip[1]-8); X.quadraticCurveTo(K[0]+2,K[1]+14,K[0]+18,K[1]+6); X.stroke();
  // near arm — pose blend, hand with fingers
  const a2=sm(42,70,G.pleasure);
  const shd=sp(398,506,.01);
  let el,ha;
  if(G.state==='orgasm'||G.state==='finish'&&G.finishT<2){ el=[600,556]; ha=[620,552]; }
  else if(a2<1){ el=lerpp([468,468],[330,556],a2); ha=lerpp([532,442],[252,542],a2); }
  capsule(shd,el,19,15,sg(420,560,'#f4cba8','#d69a75'));
  capsule(el,ha,15,11,sg(420,560,'#f4cba8','#d69a75'));
  X.fillStyle='#e8b693'; X.beginPath(); X.ellipse(ha[0],ha[1],10,7,.3,0,TAU); X.fill();
  X.strokeStyle='rgba(180,110,90,.5)'; X.lineWidth=1.6;
  { const fa=Math.atan2(ha[1]-el[1],ha[0]-el[0]);
    for(let i=0;i<4;i++){ const a=fa-.5+i*.33, L=11-i*1.4;
      X.beginPath(); X.moveTo(ha[0]+Math.cos(a)*7,ha[1]+Math.sin(a)*5);
      X.lineTo(ha[0]+Math.cos(a)*(7+L),ha[1]+Math.sin(a)*(5+L)); X.stroke(); } }
}
function drawFluids(){
  // creampie drips
  G.drips.forEach(d=>{
    const pts=[[d.x,505],[640,526],[636,548],[632,566]];
    let p=d.p*(pts.length-1), i=Math.min(p|0,pts.length-2), f=p-i;
    const y=lerp(pts[i][1],pts[i+1][1],f), x=lerp(pts[i][0],pts[i+1][0],f)+Math.sin(d.p*9+d.j)*1.5;
    X.fillStyle='rgba(245,238,235,.6)';
    X.beginPath(); X.ellipse(x,y,2.2,3.6,0,0,TAU); X.fill();
    X.fillStyle='rgba(245,238,235,.25)';
    X.beginPath(); X.ellipse(x,y-6,1.4,2.5,0,0,TAU); X.fill();
  });
  G.glisten.forEach(g=>{ X.fillStyle=`rgba(245,238,235,${g.a*.5})`;
    X.beginPath(); X.ellipse(g.x,g.y,2.5,1.6,0,0,TAU); X.fill(); });
  // sweat
  G.sweat.forEach(s=>{ X.fillStyle=`rgba(230,245,255,${.4*s.life})`;
    X.beginPath(); X.ellipse(s.x,s.y,1.4,2.2,0,0,TAU); X.fill(); });
  // floating hearts
  G.hearts.forEach(h=>{ X.fillStyle=`rgba(255,110,140,${.55*Math.min(1,h.life)})`;
    heartPath(h.x+Math.sin(h.ph)*6,h.y,7*h.s); X.fill(); });
}
function drawLight(){
  // lamp caustic
  X.save(); X.globalCompositeOperation='screen';
  const g=X.createRadialGradient(127,400,40,127,400,620);
  g.addColorStop(0,'rgba(255,175,105,.16)'); g.addColorStop(1,'rgba(255,175,105,0)');
  X.fillStyle=g; X.fillRect(0,0,W,H); X.restore();
  // arousal warmth near the edge
  if(G.ar>80&&G.state==='play'){ const a=(G.ar-80)/20*.5*(0.6+0.4*Math.sin(G.t*4));
    const rg=X.createRadialGradient(640,430,240,640,430,760);
    rg.addColorStop(0,'rgba(255,90,130,0)'); rg.addColorStop(1,`rgba(255,60,110,${.16*a})`);
    X.fillStyle=rg; X.fillRect(0,0,W,H); }
  const vg=X.createRadialGradient(640,360,300,640,420,860);
  vg.addColorStop(0,'rgba(8,4,10,0)'); vg.addColorStop(1,'rgba(8,4,10,.62)');
  X.fillStyle=vg; X.fillRect(0,0,W,H);
  if(G.bloom>.01){ X.fillStyle=`rgba(255,235,225,${G.bloom*.45})`; X.fillRect(0,0,W,H); }
  // dust motes
  X.save(); X.globalCompositeOperation='screen';
  for(let i=0;i<12;i++){ const t=G.t*.1+i*3.7;
    const x=140+((i*97)%320)+Math.sin(t+i)*40, y=380+Math.cos(t*.8+i*2)*120+((i*53)%140);
    X.fillStyle='rgba(255,210,160,.05)';
    X.beginPath(); X.arc(x,y,1.5+(i%3),0,TAU); X.fill(); }
  X.restore();
}
