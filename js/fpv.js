// Afterglow — module: fpv (loaded by index.html)
'use strict';
/* ============================================================
   FIRST-PERSON VIEW — camera is his eyes looking down her body.
   She lies with head top-center, hips bottom-center. Shaft rises
   from the bottom edge; G.depth drives the tip position.
   ============================================================ */
function fpvTremor(){
  const org=G.state==='orgasm'?Math.sin(Math.PI*clamp(G.orgT/5.2,0,1)):0;
  return org*(Math.sin(G.t*39)+Math.sin(G.t*53)*.5)*4;
}
function drawFPVRoom(){
  X.fillStyle='#141013'; X.fillRect(0,0,W,H);
  // ceiling + far wall
  X.fillStyle=sg(0,260,'#1d151a','#141013'); X.fillRect(0,0,W,300);
  // window + moon (far, above her head)
  X.fillStyle='rgba(150,160,168,.07)'; X.fillRect(950,30,190,150);
  X.strokeStyle='rgba(180,190,196,.12)'; X.lineWidth=4;
  X.strokeRect(950,30,190,150);
  X.beginPath(); X.moveTo(1045,30);X.lineTo(1045,180);X.moveTo(950,105);X.lineTo(1140,105);X.stroke();
  X.fillStyle='rgba(214,222,228,.14)'; X.beginPath();X.arc(1090,66,22,0,TAU);X.fill();
  // lamp glow left
  const g=X.createRadialGradient(180,120,10,180,120,420);
  g.addColorStop(0,'rgba(255,190,120,.22)'); g.addColorStop(1,'rgba(255,190,120,0)');
  X.save(); X.globalCompositeOperation='screen'; X.fillStyle=g; X.beginPath();X.arc(180,120,420,0,TAU);X.fill(); X.restore();
  X.fillStyle='#3d2229'; X.beginPath();X.moveTo(150,140);X.lineTo(210,140);X.lineTo(198,108);X.lineTo(162,108);X.closePath();X.fill();
  X.fillStyle='rgba(255,200,130,.5)'; X.beginPath();X.moveTo(162,110);X.lineTo(198,110);X.lineTo(206,138);X.lineTo(154,138);X.closePath();X.fill();
  // bed sheets (lower 2/3)
  X.fillStyle=sg(280,720,'#4b2a32','#2c161c'); X.fillRect(0,260,W,460);
  X.strokeStyle='rgba(0,0,0,.25)'; X.lineWidth=3;
  for(let i=0;i<5;i++){ X.beginPath(); const y=420+i*44;
    X.moveTo(80+((i*67)%140),y); X.quadraticCurveTo(640,y-18+((i*23)%30),1200-((i*53)%160),y); X.stroke(); }
  // pillow under her head
  X.fillStyle=sg(80,190,'#efe0cd','#b7a48e');
  X.beginPath(); X.ellipse(640,152,150,44,-.01,0,TAU); X.fill();
  X.fillStyle='rgba(120,96,78,.3)'; X.beginPath(); X.ellipse(640,160,90,24,0,0,TAU); X.fill();
  // body shadow on sheets
  X.fillStyle='rgba(10,4,7,.35)';
  X.beginPath(); X.ellipse(640,470,190,130,0,0,TAU); X.fill();
}
function drawFPVHead(E){
  const breathe=Math.sin(G.t*TAU*.33)*1.5;
  const ks=1+G.kiss*.10;
  const hx=640+Math.sin(G.t*.5)*2, hy=150+breathe*.4+G.pleasure*.02;
  // hair spill on pillow
  X.fillStyle='#231318';
  X.beginPath(); X.ellipse(hx,hy+8,74,40,0,0,TAU); X.fill();
  X.strokeStyle='rgba(120,70,80,.3)'; X.lineWidth=2;
  for(let i=0;i<6;i++){ const a=Math.PI*(.12+.76*i/5);
    X.beginPath(); X.moveTo(hx+Math.cos(a)*42,hy+8+Math.sin(a)*22);
    X.quadraticCurveTo(hx+Math.cos(a)*82,hy+10+Math.sin(a)*44,hx+Math.cos(a)*108,hy+16+Math.sin(a)*54); X.stroke(); }
  X.save(); X.translate(hx,hy); X.scale(ks,ks); X.rotate(E.tilt*.35+G.nod*.1+Math.sin(G.t*TAU*.33)*.01);
  // neck
  X.fillStyle=sg(30,70,'#f0c4a0','#d69a75');
  X.beginPath(); X.ellipse(0,54,20,16,0,0,TAU); X.fill();
  // skull + jaw
  X.fillStyle=sg(-50,10,'#f4cba8','#e2a983');
  X.beginPath(); X.ellipse(0,0,42,48,0,0,TAU); X.fill();
  X.beginPath(); X.moveTo(-38,-6); X.quadraticCurveTo(-34,34,-8,44); X.quadraticCurveTo(14,46,34,22);
  X.quadraticCurveTo(42,2,38,-12); X.closePath(); X.fill();
  // cheek / jaw shading
  X.save();
  shade(24,10,20,13,'rgba(170,100,75,.22)',.4);
  shade(-24,10,20,13,'rgba(170,100,75,.22)',-.4);
  shade(0,40,24,10,'rgba(170,100,75,.18)',0);
  X.globalCompositeOperation='soft-light';
  shade(-6,-30,10,30,'rgba(255,235,215,.40)',0);
  X.restore();
  // eyes
  const Eo=E.eye, roll=E.rolled;
  X.strokeStyle='#4a2c2f';
  for(const s of [-1,1]){
    const ex=s*17, ey=-8;
    X.lineWidth=2.6; X.beginPath();
    const openness=Eo*(1-roll*.5);
    if(openness>.12){
      X.moveTo(ex-10,ey); X.quadraticCurveTo(ex,ey-7*openness-2,ex+10,ey+1); X.stroke();
      X.beginPath(); X.moveTo(ex-10,ey); X.quadraticCurveTo(ex,ey-7*openness-2,ex+10,ey+1);
      X.quadraticCurveTo(ex,ey+6*openness,ex-10,ey); X.closePath();
      X.fillStyle='#f6e9e4'; X.fill();
      X.fillStyle='#4a2c33'; X.beginPath(); X.arc(ex+roll*4,ey-2*openness+(-3*roll),5*Math.max(openness,.25),0,TAU); X.fill();
      X.fillStyle='rgba(255,255,255,.8)'; X.beginPath(); X.arc(ex+roll*4-1.5,ey-3*openness-1,1.5,0,TAU); X.fill();
    } else { X.beginPath(); X.moveTo(ex-10,ey+1); X.quadraticCurveTo(ex,ey+3,ex+10,ey+1); X.stroke(); }
    X.lineWidth=1.4; X.beginPath(); X.moveTo(ex+(s>0?10:-10),ey+1); X.lineTo(ex+(s>0?13:-13),ey-2); X.stroke();
    X.lineWidth=2.2; X.beginPath();
    X.moveTo(ex-8,ey-14); X.quadraticCurveTo(ex+1,ey-17-E.brow*6,ex+10,ey-13-E.brow*8); X.stroke();
  }
  // blush
  X.fillStyle=`rgba(230,96,110,${.10+E.blush*.38})`;
  X.beginPath(); X.ellipse(-27,8,13,8,.3,0,TAU); X.fill();
  X.beginPath(); X.ellipse(27,8,13,8,-.3,0,TAU); X.fill();
  // nose
  X.strokeStyle='rgba(150,90,80,.55)'; X.lineWidth=1.8; X.beginPath();
  X.moveTo(-2,0); X.quadraticCurveTo(-5,5,-3,8); X.stroke();
  // mouth (lips part with teeth + tongue when open)
  const mo=E.mouth;
  if(mo>.06){
    X.beginPath(); X.ellipse(0,28,8+mo*3.5,2.5+mo*10,0,0,TAU); X.fillStyle='#7e333e'; X.fill();
    if(mo>.3){ X.fillStyle='rgba(250,240,238,.9)';
      X.beginPath(); X.ellipse(0,24+mo*2,6+mo*2,2.6,0,0,TAU); X.fill(); }
    X.beginPath(); X.ellipse(0,31+mo*4,7+mo*2,2+mo*3,0,0,TAU); X.fillStyle='rgba(190,95,110,.9)'; X.fill();
    X.strokeStyle='#a04a55'; X.lineWidth=1.6; X.beginPath();
    X.moveTo(-9,26); X.quadraticCurveTo(0,24-mo*2,9,26); X.stroke();
    X.strokeStyle='rgba(255,200,200,.45)'; X.lineWidth=1.4; X.beginPath();
    X.moveTo(-6,32+mo*4); X.quadraticCurveTo(0,33.5+mo*4,6,32+mo*4); X.stroke();
  } else { X.strokeStyle='#a04a55'; X.lineWidth=2; X.beginPath(); X.moveTo(-8,28); X.quadraticCurveTo(1,30,8,28); X.stroke();
    X.strokeStyle='rgba(255,200,200,.35)'; X.lineWidth=1.2; X.beginPath(); X.moveTo(-5,29.5); X.quadraticCurveTo(1,30.5,6,29.5); X.stroke(); }
  // beauty mark
  X.fillStyle='rgba(90,50,50,.6)'; X.beginPath(); X.arc(10,20,1.3,0,TAU); X.fill();
  // bangs
  X.fillStyle='#2a181d';
  X.beginPath(); X.moveTo(-42,-22); X.quadraticCurveTo(-8,-62,38,-26);
  X.quadraticCurveTo(44,-10,36,-16);
  X.quadraticCurveTo(30,-34,6,-30); X.quadraticCurveTo(26,-26,16,-16);
  X.quadraticCurveTo(8,-30,-10,-28); X.quadraticCurveTo(0,-24,-8,-14);
  X.quadraticCurveTo(-14,-30,-34,-22); X.quadraticCurveTo(-28,-32,-42,-22); X.closePath(); X.fill();
  X.strokeStyle='rgba(130,75,85,.4)'; X.lineWidth=2;
  X.beginPath(); X.moveTo(-34,-30); X.quadraticCurveTo(-8,-46,24,-36); X.stroke();
  // hair sheen strands
  X.save(); X.globalCompositeOperation='screen';
  X.strokeStyle='rgba(200,140,150,.30)'; X.lineWidth=2.5;
  X.beginPath(); X.moveTo(-24,-40); X.quadraticCurveTo(-8,-50,12,-44); X.stroke();
  X.strokeStyle='rgba(200,140,150,.18)'; X.lineWidth=2;
  X.beginPath(); X.moveTo(-38,-28); X.quadraticCurveTo(-44,-6,-38,14); X.stroke();
  X.beginPath(); X.moveTo(38,-24); X.quadraticCurveTo(44,-2,38,16); X.stroke();
  X.restore();
  // blink lid
  if(G.blinkPh>0){ X.strokeStyle='rgba(74,44,47,.9)'; X.lineWidth=3;
    for(const s of [-1,1]){ X.beginPath(); X.moveTo(s*17-10,-8); X.lineTo(s*17+10,-7); X.stroke(); } }
  X.restore();
}
function drawFPVBody(E,br){
  const trem=fpvTremor();
  const bounce=G.depth*8+G.impact*6;
  const cx=640, topY=208+bounce*.3;
  // ---- legs (spread toward camera, foreshortened) ----
  const spread=1+G.pleasure*.0015;
  for(const s of [-1,1]){
    const hip=[cx+s*62,560+bounce];
    const knee=[cx+s*(200*spread)+trem*s,648+bounce*.6];
    const foot=[cx+s*(270*spread),762];
    capsule(hip,knee,48,38,sg(500,700,'#f4cba8','#d69a75'));
    capsule(knee,foot,36,24,sg(500,720,'#eebd97','#cf9273'));
    X.fillStyle='rgba(255,235,215,.22)';
    X.beginPath(); X.ellipse(knee[0],knee[1]-10,16,19,0,0,TAU); X.fill();
    X.strokeStyle='rgba(190,120,100,.28)'; X.lineWidth=2.5;
    X.beginPath(); X.moveTo(hip[0]+s*20,hip[1]-30);
    X.quadraticCurveTo(knee[0]-s*8,knee[1]-60,knee[0]+s*4,knee[1]-24); X.stroke();
  }
  X.strokeStyle='rgba(190,120,100,.30)'; X.lineWidth=2.5;
  for(const s of [-1,1]){ X.beginPath(); X.moveTo(cx+s*40,520+bounce);
    X.quadraticCurveTo(cx+s*110,560+bounce,cx+s*150,620); X.stroke(); }
  // ---- torso ----
  X.beginPath();
  X.moveTo(cx-20,topY);
  X.quadraticCurveTo(cx-72,300,cx-58,380);
  X.quadraticCurveTo(cx-52,460,cx-80,540);
  X.quadraticCurveTo(cx-42,568,cx,568);
  X.quadraticCurveTo(cx+42,568,cx+80,540);
  X.quadraticCurveTo(cx+52,460,cx+58,380);
  X.quadraticCurveTo(cx+72,300,cx+20,topY);
  X.closePath();
  X.fillStyle=sg(200,580,'#f4cba8','#d69a75'); X.fill();
  // torso shading
  X.save(); X.globalCompositeOperation='multiply';
  shade(cx-52,420,26,90,'rgba(170,100,75,.20)',.06);
  shade(cx+52,420,26,90,'rgba(170,100,75,.20)',-.06);
  shade(cx,540,60,26,'rgba(170,100,75,.20)',0);
  shade(cx,392,44,20,'rgba(170,100,75,.24)',0);
  X.restore();
  X.save(); X.globalCompositeOperation='soft-light';
  shade(cx,300,30,110,'rgba(255,230,205,.30)',0);
  shade(cx,480,36,60,'rgba(255,230,205,.22)',0);
  X.restore();
  // chest blush
  X.fillStyle=`rgba(226,96,110,${.05+E.blush*.20})`;
  X.beginPath(); X.ellipse(cx,318,52,26,0,0,TAU); X.fill();
  // navel
  X.strokeStyle='rgba(150,95,70,.4)'; X.lineWidth=2; X.beginPath();
  X.ellipse(cx,452+bounce*.5,4.5,6.5,0,0,TAU); X.stroke();
  // collarbone hints
  X.beginPath(); X.moveTo(cx-34,232); X.quadraticCurveTo(cx-12,240,cx+2,236);
  X.moveTo(cx+34,232); X.quadraticCurveTo(cx+12,240,cx-2,236); X.stroke();
  // ---- arms (hands rise to pillow as she melts) ----
  const a2=sm(42,70,G.pleasure);
  for(const s of [-1,1]){
    const sh=[cx+s*66,252+bounce*.3];
    const el=[cx+s*142,322];
    const ha=[lerp(cx+s*122,cx+s*62,a2),lerp(432,196,a2)];
    capsule(sh,el,20,16,sg(220,340,'#f4cba8','#d69a75'));
    capsule(el,ha,15,11,sg(220,340,'#f0bd96','#cf9273'));
    X.fillStyle='#e8b693'; X.beginPath(); X.ellipse(ha[0],ha[1],10,7,s*.3,0,TAU); X.fill();
  }
  // ---- breasts (teardrop, nipples firm up with arousal) ----
  const jig=G.breast.p*.9, squash=sm(.86,1,G.depth);
  const erect=clamp(.35+.65*(G.ar/100),0,1);
  for(const s of [-1,1]){
    const bx=cx+s*56, by=336+br*.7+jig*.55+bounce*.4;
    const rx=34*(1+squash*.06)-jig*.1, ry=38*(1-squash*.12)+jig*.14;
    X.save(); X.translate(bx,by); X.rotate(s*.12);
    X.fillStyle=sg(-40,40,'#f6d0ac','#d69a75');
    X.beginPath();
    X.moveTo(0,-ry);
    X.bezierCurveTo(rx*.95,-ry*.72,rx*.9,ry*.35,rx*.42,ry);
    X.quadraticCurveTo(0,ry+3,-rx*.42,ry);
    X.bezierCurveTo(-rx*.9,ry*.35,-rx*.95,-ry*.72,0,-ry);
    X.closePath(); X.fill();
    X.save(); X.globalCompositeOperation='multiply';
    shade(8,22,28,12,'rgba(170,100,75,.38)',0);
    X.restore();
    X.save(); X.globalCompositeOperation='soft-light';
    shade(-12,-16,15,10,'rgba(255,235,215,.55)',-.4);
    X.restore();
    // areola ring + erect nipple
    X.fillStyle='rgba(206,130,112,.55)'; X.beginPath(); X.ellipse(s*4,-ry+18,10.5,7.5,0,0,TAU); X.fill();
    X.fillStyle='rgba(214,140,120,.9)'; X.beginPath(); X.ellipse(s*4,-ry+18,8,5.8,0,0,TAU); X.fill();
    X.fillStyle='#c25f63'; X.beginPath(); X.ellipse(s*4,-ry+16,3.2+2.2*erect,4.5+3.5*erect,0,0,TAU); X.fill();
    X.fillStyle='rgba(255,235,235,.5)'; X.beginPath(); X.ellipse(s*4-1.5,-ry+14-erect*2,1.4,2.2,-.2,0,TAU); X.fill();
    X.restore();
  }
  // cleavage shadow
  shade(cx,336,14,30,'rgba(150,80,70,.30)',0);
  // ---- vulva (front view: hood, minora, introitus) + arousal gloss ----
  const vy=556+bounce, eng=clamp(G.ar/100,0,1), iopen=4+9*G.depth;
  if(G.ar>25){
    X.save(); X.globalCompositeOperation='screen';
    shade(cx,vy,26+10*(G.ar/100),20,'rgba(255,170,170,.18)',0);
    X.restore();
  }
  // majora
  X.strokeStyle='rgba(190,120,105,.6)'; X.lineWidth=3;
  X.beginPath(); X.ellipse(cx-8,vy,6,15,.1,0,TAU); X.stroke();
  X.beginPath(); X.ellipse(cx+8,vy,6,15,-.1,0,TAU); X.stroke();
  // minora
  X.strokeStyle=`rgba(${200+20*eng|0},${120+20*eng|0},${125+15*eng|0},.85)`; X.lineWidth=2.2;
  X.beginPath(); X.ellipse(cx-3.5,vy+1,3,10+3*eng,.06,0,TAU); X.stroke();
  X.beginPath(); X.ellipse(cx+3.5,vy+1,3,10+3*eng,-.06,0,TAU); X.stroke();
  // hood + glans
  X.fillStyle=sg(vy-20,vy,'#f4cba8','#d69a75');
  X.beginPath(); X.ellipse(cx,vy-12,5,4,0,Math.PI,0); X.fill();
  X.fillStyle=`rgba(${205+25*eng|0},110,120,.95)`;
  X.beginPath(); X.arc(cx,vy-10.5,1.5+1.6*eng,0,TAU); X.fill();
  // introitus (stretches with depth)
  X.fillStyle='rgba(96,32,42,.6)';
  X.beginPath(); X.ellipse(cx,vy+2,3.4,iopen*.5,0,0,TAU); X.fill();
  X.strokeStyle='rgba(150,80,85,.7)'; X.lineWidth=2;
  X.beginPath(); X.ellipse(cx,vy+2,3.4,iopen*.5,0,0,TAU); X.stroke();
  if(G.ar>30){ X.strokeStyle='rgba(255,255,255,.35)'; X.lineWidth=2.2;
    X.beginPath(); X.ellipse(cx+2.5,vy-2,3.5,6,0,-.3,1.1); X.stroke(); }
  // pubic hair
  X.strokeStyle='rgba(60,32,30,.5)'; X.lineWidth=1.2;
  for(let i=0;i<30;i++){
    const hx2=cx-16+((i*37)%32), hy2=vy-34+((i*23)%18);
    const a2=-.5+((i*13)%10)*.1;
    X.beginPath(); X.moveTo(hx2,hy2); X.lineTo(hx2+Math.cos(a2)*4.5,hy2+Math.sin(a2)*4.5-2); X.stroke();
  }
}
function drawFPVHands(br){
  const rub=G.rub, kiss=G.kiss;
  // left forearm: bottom-left to her waist / shoulder when embracing
  const lBase=[190,745], lEl=[380,622];
  let lHand=[500,540+G.depth*6];
  lHand=[lerp(lHand[0],572,kiss),lerp(lHand[1],252,kiss)];
  capsule(lBase,lEl,34,28,sg(600,745,'#d5a071','#a96c44'));
  capsule(lEl,lHand,26,20,sg(540,620,'#cf9a6b','#a2653f'));
  X.fillStyle='#c08f60'; X.beginPath(); X.ellipse(lHand[0],lHand[1],20,14,-.4,0,TAU); X.fill();
  // right forearm: waist normally, her breast while rubbing, shoulder while embracing
  const rBase=[1090,745], rEl=[900,622];
  let rHand, rubbing=rub>.12&&kiss<.5;
  if(rub>.12){ const w1=Math.sin(G.t*9)*11*rub, w2=Math.cos(G.t*7.3)*9*rub;
    rHand=[696+w1,332+br*.6+G.breast.p*.5+w2]; }
  else rHand=[780,540+G.depth*6];
  rHand=[lerp(rHand[0],708,kiss),lerp(rHand[1],252,kiss)];
  capsule(rBase,rEl,34,28,sg(600,745,'#d5a071','#a96c44'));
  capsule(rEl,rHand,26,20,sg(540,620,'#cf9a6b','#a2653f'));
  X.fillStyle='#c08f60'; X.beginPath(); X.ellipse(rHand[0],rHand[1],20,14,.4,0,TAU); X.fill();
  if(rubbing){
    X.fillStyle='#c08f60';
    for(let i=0;i<3;i++){ const a=.5+i*.5+Math.sin(G.t*9+i)*.15;
      X.beginPath(); X.ellipse(rHand[0]+Math.cos(a)*18,rHand[1]+Math.sin(a)*14,7,4.5,a,0,TAU); X.fill(); }
    X.strokeStyle=`rgba(255,220,200,${.30*rub})`; X.lineWidth=2.5;
    X.beginPath(); X.ellipse(696,334,40,38,0,G.t*9,G.t*9+1.7); X.stroke();
    for(let i=0;i<2;i++){ const u=((G.t*1.6+i*.5)%1);
      X.strokeStyle=`rgba(255,150,170,${(1-u)*.30*rub})`; X.lineWidth=2;
      X.beginPath(); X.ellipse(696,334,16+u*44,15+u*40,0,0,TAU); X.stroke(); }
  }
}
function drawFPVShaft(){
  const d=G.depth;
  const sway=Math.sin(G.t*1.7)*3+chaos(3)*.3;
  let tipY, tipX=640+sway;
  if(G.state==='climax'||G.state==='finish') tipY=544+Math.sin(G.t*30)*2;
  else tipY=lerp(655,546,clamp(d,0,1));
  const wb=30, wt=19, baseY=792;
  // contact shadow at entry (tighter + darker when deep)
  X.fillStyle=`rgba(25,8,10,${.20+.28*clamp(d,0,1)})`;
  X.beginPath(); X.ellipse(640,557,36-10*clamp(d,0,1),11,0,0,TAU); X.fill();
  if(G.ar>40){ X.strokeStyle=`rgba(255,240,235,${.10+.20*(G.ar/100)})`; X.lineWidth=3;
    X.beginPath(); X.ellipse(640,556,26,9,0,0,TAU); X.stroke(); }
  // tapered shaft body with cylindrical shading
  const cg=X.createLinearGradient(640-wb,0,640+wb,0);
  cg.addColorStop(0,'#a96c44'); cg.addColorStop(.28,'#d99a6d');
  cg.addColorStop(.46,'#f0c096'); cg.addColorStop(.62,'#dd9c6f'); cg.addColorStop(1,'#96613e');
  X.beginPath();
  X.moveTo(640-wb,baseY);
  X.quadraticCurveTo(640-wb+6,(baseY+tipY)/2,tipX-wt,tipY+8);
  X.quadraticCurveTo(tipX,tipY-2,tipX+wt,tipY+8);
  X.quadraticCurveTo(640+wb-6,(baseY+tipY)/2,640+wb,baseY);
  X.closePath(); X.fillStyle=cg; X.fill();
  // underside AO + lamp-side highlight
  X.save(); X.globalCompositeOperation='multiply';
  shade(640-16,(baseY+tipY)/2,11,Math.abs(baseY-tipY)/2,'rgba(90,50,28,.22)',0);
  X.restore();
  X.save(); X.globalCompositeOperation='soft-light';
  shade(640-4,(baseY+tipY)/2,13,Math.abs(baseY-tipY)/2,'rgba(255,225,195,.35)',0);
  X.restore();
  // dorsal vein
  X.strokeStyle='rgba(140,80,60,.35)'; X.lineWidth=3; X.beginPath();
  X.moveTo(632,760); X.quadraticCurveTo(628,(760+tipY)/2+20,636,tipY+50); X.stroke();
  X.strokeStyle='rgba(140,80,60,.25)'; X.lineWidth=2; X.beginPath();
  X.moveTo(630,700); X.quadraticCurveTo(640,680,646,660); X.stroke();
  // glans + coronal ridge when visible
  if(tipY>585&&G.state!=='climax'&&G.state!=='finish'){
    X.fillStyle='#c98873';
    X.beginPath(); X.ellipse(tipX,tipY+8,wt+2.5,7,0,0,TAU); X.fill();
    const gg=X.createLinearGradient(tipX-wt,0,tipX+wt,0);
    gg.addColorStop(0,'#c98873'); gg.addColorStop(.45,'#e8ab90'); gg.addColorStop(1,'#b87a66');
    X.beginPath(); X.ellipse(tipX,tipY-4,wt-1,15,0,0,TAU); X.fillStyle=gg; X.fill();
    X.strokeStyle='rgba(120,60,55,.6)'; X.lineWidth=2; X.beginPath();
    X.moveTo(tipX,tipY-16); X.lineTo(tipX,tipY-8); X.stroke();
    X.fillStyle='rgba(255,255,255,.30)'; X.beginPath(); X.ellipse(tipX-6,tipY-8,3.5,7,-.2,0,TAU); X.fill();
  }
  // balls when pulled back
  if(d<.5&&G.state!=='finish'){
    const bs=(1-d*.9);
    X.fillStyle=sg(700,770,'#c78e6a','#a96c44');
    X.beginPath(); X.ellipse(606,738,24*bs,20*bs,.2,0,TAU); X.fill();
    X.fillStyle=sg(700,770,'#d0936d','#a96c44');
    X.beginPath(); X.ellipse(674,738,22*bs,19*bs,-.2,0,TAU); X.fill();
    X.strokeStyle='rgba(90,52,32,.35)'; X.lineWidth=2; X.beginPath();
    X.moveTo(640,730); X.quadraticCurveTo(640,744,640,754); X.stroke();
  }
  // wet sheen
  if(G.ar>28){ X.strokeStyle='rgba(255,255,255,.28)'; X.lineWidth=4;
    X.beginPath(); X.moveTo(624,740); X.quadraticCurveTo(628,(740+tipY)/2,tipX-10,tipY+12); X.stroke(); }
}
function drawFPVFluids(){
  // drips below her (reuse sim p)
  G.drips.forEach(d=>{
    const y=lerp(560,650,d.p), x=640+(d.x-648)*.5+Math.sin(d.p*9+d.j)*1.5;
    X.fillStyle='rgba(245,238,235,.6)';
    X.beginPath(); X.ellipse(x,y,2.4,3.8,0,0,TAU); X.fill();
    X.fillStyle='rgba(245,238,235,.25)';
    X.beginPath(); X.ellipse(x,y-6,1.5,2.6,0,0,TAU); X.fill();
  });
  // glisten clustered near hips
  G.glisten.forEach(g=>{ X.fillStyle=`rgba(245,238,235,${g.a*.5})`;
    X.beginPath(); X.ellipse(640+(g.x-640)*.3,556+(g.y-546)*.4,2.6,1.7,0,0,TAU); X.fill(); });
  // sweat: alternate face / chest by index
  G.sweat.forEach((s,i)=>{
    const a=.4*s.life;
    X.fillStyle=`rgba(230,245,255,${a})`;
    if(i%2){ X.beginPath(); X.ellipse(640+(s.x-318)*1.5,140+(s.y-462)*.5,1.4,2.2,0,0,TAU); X.fill(); }
    else { X.beginPath(); X.ellipse(640+(s.x-318)*2.2,300+(s.y-462)*1.2,1.4,2.2,0,0,TAU); X.fill(); }
  });
  // hearts rising from her chest
  G.hearts.forEach(h=>{ X.fillStyle=`rgba(255,110,140,${.55*Math.min(1,h.life)})`;
    heartPath(640+(h.x-655)+Math.sin(h.ph)*8,340+(h.y-485)*.8,h.s); X.fill(); });
}
function drawFPV(){
  drawFPVRoom();
  const E=herExpression();
  const br=Math.sin(G.t*TAU*(0.16+G.pleasure*.004))*2;
  // POV camera life: breathing sway + heartbeat nudge when hot
  const swayX=Math.sin(G.t*.9)*2.5+G.tired*Math.sin(G.t*7)*1.2;
  const swayY=Math.sin(G.t*TAU*.16)*2+G.kiss*10-G.depth*4;
  let beat=0;
  if(G.pleasure>72){ const bpm=72+(G.pleasure-72)*1.2+G.orgasms*8; beat=Math.max(0,Math.sin(G.t*bpm*Math.PI/30))*.9; }
  X.save(); X.translate(swayX+beat*.7,swayY+beat*.5);
  drawFPVBody(E,br);
  drawFPVHands(br);
  drawFPVShaft();
  drawFPVHead(E);
  X.restore();
  drawFPVFluids();
  // out-of-focus foreground (your shoulders) framing the shot
  X.fillStyle='rgba(150,90,60,.20)';
  X.beginPath(); X.ellipse(60,760,220,120,.3,0,TAU); X.fill();
  X.beginPath(); X.ellipse(1220,760,220,120,-.3,0,TAU); X.fill();
  X.fillStyle='rgba(10,4,6,.35)';
  X.beginPath(); X.ellipse(40,780,200,90,.3,0,TAU); X.fill();
  X.beginPath(); X.ellipse(1240,780,200,90,-.3,0,TAU); X.fill();
}
