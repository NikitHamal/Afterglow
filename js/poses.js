// Afterglow — module: poses (realistic positions, anatomy, and camera perspectives)
'use strict';

/* ---------------- 7 realistic positions ---------------- */
const POSES=[
  {name:'MISSIONARY',line:'like this… look into my eyes… ♥'},
  {name:'LEGS-UP / DEEP',line:'wrap my legs high… put it all in… ♥'},
  {name:'DOGGY (ARCHED)',line:'take me from behind… arch my back… ♥'},
  {name:'PRONE BONE (FLAT)',line:'push me flat into the sheets… so deep… ♥'},
  {name:'COWGIRL (RIDING)',line:'my turn on top… I’m going to ride you… ♥'},
  {name:'REV COWGIRL',line:'watch me bounce for you… don’t stop… ♥'},
  {name:'SPOONING',line:'hold me tight from behind… slow and warm… ♥'}
];

function posName(){ return (POSES[G.pos|0]||POSES[0]).name; }
function cyclePose(){
  if(G.state!=='play'||G.tired) return;
  G.pos=((G.pos|0)+1)%POSES.length; G.nod=1;
  say(POSES[G.pos].line,1.8);
  playMoan(.38,{dur:.42,pmul:1.16,vol:.85});
}

/* Fluid origins per position for ejaculation jets & climax */
function poseJetOrigins(){
  if(G.oral>.5) return {s:[745,460],f:[640,520]};
  switch(G.pos|0){
    case 1: return {s:[655,482],f:[640,530]}; // legs up
    case 2: return {s:[668,442],f:[640,582]}; // doggy
    case 3: return {s:[650,490],f:[640,575]}; // prone bone
    case 4: return {s:[706,478],f:[640,555]}; // cowgirl
    case 5: return {s:[706,475],f:[640,560]}; // rev cowgirl
    case 6: return {s:[646,522],f:[560,598]}; // spoon
    default: return {s:[648,498],f:[640,546]}; // missionary
  }
}
function poseFluidOrigin(){ return poseJetOrigins().s; }
function spawnJets(){
  const o=poseJetOrigins();
  for(let i=0;i<8;i++) G.jets.push({view:'side',x:o.s[0]+rr(-4,4),y:o.s[1]+rr(-3,3),
    vx:rr(-75,15),vy:rr(-195,-55),life:rr(.55,.98)});
  for(let i=0;i<7;i++) G.jets.push({view:'fpv',x:o.f[0]+rr(-5,5),y:o.f[1]+rr(-4,4),
    vx:rr(-32,32),vy:rr(-175,-45),life:rr(.55,.95)});
}

/* ---------------- shared anatomical drawing helpers ---------------- */
// Scene-local capsule: routes every limb through the shared skin system.
function capsule(a, b, r1, r2, T){
  if(T && T.b) limbS(a, b, r1, r2, T, { belly: 1.06 });
  else if(typeof T === 'string') limbS(a, b, r1, r2, skTone(T), { belly: 1.06 });
  else limbS(a, b, r1, r2, herT(), { belly: 1.06 });
}

function poseHead(x,y,dir,E,s=1){
  const hc=G.char?G.char.hairColor:'#231318';
  hairMassS(x-dir*12*s,y-4*s,24*s,26*s,dir*.22,hc);
  const T=herT();
  skFillRad(()=>{ X.ellipse(x,y,16*s,20*s,0,0,TAU); }, T, x-dir*5*s,y-7*s,2*s,26*s);
  skClipIn(()=>{ X.ellipse(x,y,16*s,20*s,0,0,TAU); },()=>{
    fAO(x-dir*10*s,y+2*s,7*s,14*s,.24);
    fHi(x+dir*4*s,y-9*s,7*s,9*s,'rgba(255,240,225,0.26)');
  });
  // Eyelashes / bedroom eyes
  X.strokeStyle='rgba(65,35,42,.95)'; X.lineWidth=2.2*s; X.lineCap='round'; X.beginPath();
  X.moveTo(x+dir*2*s,y-4*s); X.quadraticCurveTo(x+dir*7*s,y-2*s,x+dir*12*s,y-4*s); X.stroke();
  // Cheeks blush
  const [bR,bG,bB]=hexToRgb(G.char?G.char.blushColor:'#e86070');
  shade(x+dir*5*s,y+4*s,9*s,6*s,`rgba(${bR},${bG},${bB},${.10+E.blush*.26})`,0);
  // Parted luscious lips
  const mo=E.mouth;
  X.fillStyle=G.char?G.char.lipColor:'#b3555f';
  X.beginPath(); X.ellipse(x+dir*11*s,y+8*s,3+mo*3.2*s,2+mo*5*s,0,0,TAU); X.fill();
  if(mo>.2){
    X.fillStyle='#5a1622';
    X.beginPath(); X.ellipse(x+dir*11*s,y+8*s,1.8+mo*1.8*s,1.2+mo*3.2*s,0,0,TAU); X.fill();
  }
  X.fillStyle='rgba(255,225,225,.34)';
  X.beginPath(); X.ellipse(x+dir*10*s,y+6*s,1.5*s,1*s,0,0,TAU); X.fill();
  // Hair strands falling
  tressS(x-dir*8*s,y-18*s,x+dir*8*s,y+2*s,x+dir*2*s,y+18*s,x-dir*2*s,y+32*s,
    4.2*s,1.2*s,hc,'rgba(255,200,210,0.14)');
}

function himHead(x,y,dir){
  const T=himT();
  skFillRad(()=>{ X.ellipse(x,y,18,20,0,0,TAU); }, T, x-dir*6,y-7,2,26);
  skClipIn(()=>{ X.ellipse(x,y,18,20,0,0,TAU); },()=>{
    fAO(x-dir*12,y+4,8,15,.22);
    fHi(x+dir*4,y-10,7,8,'rgba(255,238,220,0.20)');
  });
  X.fillStyle=skDark(T.base,.42); X.beginPath();
  X.moveTo(x-dir*18,y-12); X.quadraticCurveTo(x,y-32,x+dir*18,y-12);
  X.quadraticCurveTo(x+dir*10,y-16,x,y-14); X.quadraticCurveTo(x-dir*10,y-12,x-dir*18,y-12); X.fill();
  X.strokeStyle='rgba(74,44,47,.9)'; X.lineWidth=2; X.lineCap='round'; X.beginPath();
  X.moveTo(x+dir*3,y-1); X.lineTo(x+dir*11,y-1); X.stroke();
}

function poseBreast(x,y,r,E){
  if(!Number.isFinite(r)||r<=0) r=18;
  const er=clamp(G.ar/100,0,1), T=herT();
  X.save(); X.globalCompositeOperation='multiply';
  shade(x+r*.12,y+r*1.06,r*.88,r*.34,'rgba(118,58,48,.30)',0);
  X.restore();
  const bg=X.createRadialGradient(x-r*.28,y-r*.44,r*.12,x,y,r*1.3);
  bg.addColorStop(0,T.hi); bg.addColorStop(.5,T.b); bg.addColorStop(.85,T.s); bg.addColorStop(1,T.s);
  X.beginPath(); X.ellipse(x,y,r*.95,r*1.15,0,0,TAU); X.fillStyle=bg; X.fill();
  skClipIn(()=>{ X.ellipse(x,y,r*.95,r*1.15,0,0,TAU); },()=>{
    fAO(x,y+r*.92,r*.9,r*.4,.30);
    fSh(x-r*.62,y+r*.1,r*.42,r*.86,'rgba(175,100,80,0.24)',0);
    fHi(x-r*.24,y-r*.44,r*.42,r*.46,'rgba(255,238,222,0.30)');
    fSSS(x+r*.3,y+r*.3,r*.5,r*.6,0);
  });
  const aer=r*.26+er*r*.07, npr=r*.14+er*r*.09;
  const ag=X.createRadialGradient(x-aer*.2,y+r*.52-aer*.2,0,x,y+r*.52,aer*1.15);
  ag.addColorStop(0,'rgba(226,150,128,0.95)'); ag.addColorStop(1,'rgba(196,112,96,0.72)');
  X.fillStyle=ag; X.beginPath(); X.arc(x,y+r*.52,aer,0,TAU); X.fill();
  X.fillStyle=G.char?G.char.nippleColor:'#c25f63';
  X.beginPath(); X.arc(x,y+r*.52+npr*.4,npr,0,TAU); X.fill();
  X.fillStyle='rgba(255,240,235,.42)';
  X.beginPath(); X.ellipse(x-npr*.35,y+r*.52-npr*.1,npr*.45,npr*.32,0,0,TAU); X.fill();
}

// Rounded cheek / mons mound: dir -1|1 puts the crease toward the midline, 0 omits it.
function poseGlute(x,y,rx,ry,dir=0,rot=0){
  const T=herT();
  skFillRad(()=>{ X.ellipse(x,y,rx,ry,rot,0,TAU); },T,x-rx*.3,y-ry*.42,rx*.1,Math.max(rx,ry)*1.35);
  skClipIn(()=>{ X.ellipse(x,y,rx,ry,rot,0,TAU); },()=>{
    if(dir) fAO(x-dir*rx*.6,y+ry*.08,rx*.3,ry*.82,.28);
    fAO(x,y+ry*.86,rx*.78,ry*.24,.22);
    fHi(x-rx*.3,y-ry*.4,rx*.44,ry*.4,'rgba(255,240,225,0.26)');
    fSSS(x+rx*.42,y+ry*.28,rx*.4,ry*.5,0);
  });
}

// Shaded glans cap over a shaft tip.
function poseGlans(x,y,rx,ry,pu=1){
  const g=X.createRadialGradient(x-rx*.3,y-ry*.4,1,x,y,Math.max(rx,ry)*1.15);
  g.addColorStop(0,'#e9aa8f'); g.addColorStop(.6,'#c7846f'); g.addColorStop(1,'#ab6a58');
  X.fillStyle=g; X.beginPath(); X.ellipse(x,y,rx*pu,ry*pu,0,0,TAU); X.fill();
  X.fillStyle='rgba(255,240,232,.3)';
  X.beginPath(); X.ellipse(x-rx*.28,y-ry*.38,rx*.3,ry*.2,-.4,0,TAU); X.fill();
}

function poseVulva(x,y,open,angle=0,view='side',sc=.78){
  const eng=clamp(G.ar/100,0,1);
  X.save(); X.translate(x,y); X.rotate(angle); X.scale(sc,sc);
  vulvaS(0,0,open,eng,herT(),{view:view});
  X.restore();
}

function poseShaft(base,tip,pu=1){
  const T=himT();
  limbS(base,tip,12*pu,10*pu,T,{belly:1.04});
  // Vein
  X.strokeStyle='rgba(150,85,65,.22)'; X.lineWidth=2; X.lineCap='round'; X.beginPath();
  X.moveTo(base[0],base[1]-4);
  X.quadraticCurveTo(lerp(base[0],tip[0],.5)+2,lerp(base[1],tip[1],.5)-5,tip[0]-6,tip[1]); X.stroke();
  // Glans
  const gg=X.createRadialGradient(tip[0]-3*pu,tip[1]-4*pu,1,tip[0],tip[1],11*pu);
  gg.addColorStop(0,'#e9aa8f'); gg.addColorStop(.6,'#c7846f'); gg.addColorStop(1,'#b67562');
  X.fillStyle=gg;
  X.beginPath(); X.ellipse(tip[0],tip[1],9.5*pu,10.5*pu,0,0,TAU); X.fill();
  X.fillStyle='rgba(255,240,232,.28)';
  X.beginPath(); X.ellipse(tip[0]-2.5*pu,tip[1]-4*pu,3.2*pu,2.2*pu,-.4,0,TAU); X.fill();
}

/* ---------------- side-view positions ---------------- */
function drawPoseSide(){
  const E=herExpression(), br=Math.sin(G.t*TAU*(0.16+G.pleasure*.004))*2.2;
  const p=G.pos|0;
  if(p===1) drawLegsUpSide(E,br);
  else if(p===2) drawDoggySide(E,br);
  else if(p===3) drawProneBoneSide(E,br);
  else if(p===4) drawCowgirlSide(E,br);
  else if(p===5) drawRevCowgirlSide(E,br);
  else drawSpoonSide(E,br);
}

// POS 1: LEGS-UP / DEEP MISSIONARY (pelvis propped high, legs back over his shoulders)
function drawLegsUpSide(E,br){
  const sk=getSkin(), D=G.depth, jig=G.breast.p*.85, HIS=['#dba06f','#a96c44'];
  // Far leg held high
  capsule([620,490],[680,360],28,20,herFarT());
  capsule([680,360],[770,330],18,12,herFarT());
  // Her back arched on bed, pelvis tilted upward towards him
  capsule([365,520],[600,480],36,32,herT());
  poseGlute(620,485,42,36,0,-.25);
  // Elevated vulva
  const open=4+9*D, V=[655,482];
  poseVulva(V[0],V[1],open,.3);
  // Bouncing breasts
  poseBreast(450,478+jig*.5,18,E);
  poseBreast(487,472+jig*.5,17,E);
  // Near leg pulled back over his shoulder
  capsule([625,485],[710,380],30,22,herT());
  capsule([710,380],[805,355],20,13,herFarT());
  // Her head on pillow
  poseHead(295,518,-1,E);
  // Him leaning forward over her
  capsule([810,560],[760,460],40,34,himT());
  capsule([760,460],[695,385],44,38,himT());
  himHead(678,350,-1);
  // His hands gripping her lifted thighs/hips
  capsule([720,410],[685,445],18,14,himT());
  handS(682,448,3.93,1.05,himT(),{curl:.7,spread:.3});
  // Deep thrusting shaft
  const B=[765,510], pu=1+.28*(G.shaftPulse||0);
  const tip=[lerp(B[0],V[0],.3+.68*D),lerp(B[1],V[1],.3+.68*D)];
  poseShaft(B,tip,pu);
}

// POS 2: DOGGY STYLE (arched spine, lifted rounded buttocks, hands gripping her hips)
function drawDoggySide(E,br){
  const sk=getSkin(), D=G.depth, jig=G.breast.p*.9, HIS=['#dba06f','#a96c44'];
  // Her arms kneeling on bed, facing left
  capsule([365,475],[345,540],17,14,herT());
  capsule([345,540],[360,578],13,10,herFarT());
  // Arched back & waist sloping down then rising to ass
  capsule([368,460],[595,425],35,30,herT());
  X.save(); X.globalCompositeOperation='soft-light';
  shade(480,432,68,22,'rgba(255,230,205,.30)',-.1); X.restore();
  // Rounded buttocks & gluteal curve
  poseGlute(625,420,42,46,0);
  X.strokeStyle=`rgba(${hexToRgb(herT().dk).join(',')},.24)`; X.lineWidth=2; X.lineCap='round'; X.beginPath();
  X.moveTo(642,382); X.quadraticCurveTo(652,416,642,450); X.stroke();
  // Rear entry vulva
  const open=4+9*D, V=[668,442];
  poseVulva(V[0],V[1],open,.15);
  // Hanging breasts
  poseBreast(398,490+jig*.5,18,E);
  poseBreast(434,484+jig*.5,17,E);
  // Her kneeling thighs
  capsule([608,445],[582,560],32,24,herT());
  capsule([582,560],[638,575],22,14,herFarT());
  // Neck + head turned, looking back over shoulder with bedroom eyes
  capsule([330,498],[372,464],13,16,herT());
  poseHead(318,496,-1,E);
  // Him kneeling upright behind her
  capsule([835,565],[798,465],36,32,himT());
  capsule([798,465],[740,360],42,36,himT());
  himHead(724,324,-1);
  // His hands holding her waist/hips
  capsule([750,380],[698,412],20,16,himT());
  capsule([698,412],[660,426],15,12,himT());
  handS(658,427,4.36,1.0,himT(),{curl:.7,spread:.3});
  // Shaft entering along rear axis
  const B=[788,455], pu=1+.28*(G.shaftPulse||0);
  const tip=[lerp(B[0],V[0],.3+.68*D),lerp(B[1],V[1],.3+.68*D)];
  poseShaft(B,tip,pu);
}

// POS 3: PRONE BONE (flat on stomach, thighs tight, heavy rear embrace)
function drawProneBoneSide(E,br){
  const sk=getSkin(), D=G.depth, HIS=['#dba06f','#a96c44'];
  // Her lying flat on bed, head turned on pillow
  poseHead(290,526,-1,E);
  // Flat slender back and tight buttocks
  capsule([330,528],[610,505],32,28,herT());
  poseGlute(628,495,38,36,0);
  // Legs pressed straight together on bed
  capsule([628,505],[790,535],28,18,herT());
  capsule([790,535],[910,545],18,12,herFarT());
  // Entry angle
  const open=3+8*D, V=[650,490];
  poseVulva(V[0],V[1],open,.25);
  // Contact shadow where his chest presses her flat
  X.save(); X.globalCompositeOperation='multiply';
  shade(500,492,150,15,'rgba(64,30,24,.34)',-.06);
  X.restore();
  // Him lying right on top of her, pressing her flat
  capsule([420,495],[680,470],42,36,himT());
  capsule([680,470],[830,510],36,26,himT());
  himHead(400,470,-1);
  // His arm reaching around her ribs
  capsule([480,485],[530,515],20,16,himT());
  capsule([530,515],[575,525],15,12,himT());
  handS(576,525,1.79,.95,himT(),{curl:.68,spread:.3});
  // Shaft gliding between her tight thighs
  const B=[720,495], pu=1+.28*(G.shaftPulse||0);
  const tip=[lerp(B[0],V[0],.3+.68*D),lerp(B[1],V[1],.3+.68*D)];
  poseShaft(B,tip,pu);
}

// POS 4: COWGIRL (her on top straddling his waist, riding up and down)
function drawCowgirlSide(E,br){
  const sk=getSkin(), D=G.depth, jig=G.breast.p*.95, HIS=['#dba06f','#a96c44'];
  const hipsY=510-42*D;
  // Him lying flat on bed
  himHead(950,548,1);
  capsule([928,545],[710,540],40,35,himT());
  capsule([710,540],[555,556],32,24,himT());
  // Shaft pointing straight up into her
  const pu=1+.28*(G.shaftPulse||0);
  capsule([706,535],[706,hipsY+8],12*pu,10*pu,himT());
  // Her hips & torso rising and falling
  poseGlute(700,hipsY,36,28,0);
  capsule([700,hipsY-10],[688,hipsY-170],35,28,herT());
  // Bouncing breasts as she rides
  poseBreast(668,hipsY-135+jig*.6,18,E);
  poseBreast(708,hipsY-133+jig*.6,17,E);
  // Her arms planted on his chest
  capsule([688,hipsY-158],[658,hipsY-68],17,14,herT());
  capsule([658,hipsY-68],[638,hipsY-36],13,10,herFarT());
  capsule([692,hipsY-158],[732,hipsY-68],17,14,herT());
  capsule([732,hipsY-68],[750,hipsY-36],13,10,herFarT());
  handS(636,hipsY-34,3.70,.85,herT(),{curl:.55,spread:.3});
  handS(752,hipsY-34,2.63,.85,herT(),{curl:.55,spread:.3});
  // Straddling legs
  capsule([700,hipsY],[590,560],28,22,herT());
  capsule([700,hipsY],[810,560],28,22,herT());
  // Her head tilted in pleasure
  poseHead(686,hipsY-208,-1,E);
}

// POS 5: REVERSE COWGIRL (her facing away, back arched, buttocks front-and-center)
function drawRevCowgirlSide(E,br){
  const sk=getSkin(), D=G.depth, jig=G.breast.p*.9, HIS=['#dba06f','#a96c44'];
  const hipsY=510-42*D;
  // Him lying flat, head left
  himHead(450,545,-1);
  capsule([480,545],[710,540],40,35,himT());
  capsule([710,540],[880,555],34,26,himT());
  // Shaft up
  const pu=1+.28*(G.shaftPulse||0);
  capsule([706,535],[706,hipsY+8],12*pu,10*pu,himT());
  // Her arched back facing away (looking right)
  poseGlute(705,hipsY,38,30,0);
  capsule([705,hipsY-10],[718,hipsY-170],36,28,herT());
  // Buttocks curve
  poseGlute(688,hipsY+4,28,32,0,-.2);
  // Arms reaching down to his knees
  capsule([715,hipsY-155],[760,hipsY-65],17,14,herT());
  capsule([760,hipsY-65],[800,hipsY-30],13,10,herFarT());
  handS(802,hipsY-28,2.29,.9,herT(),{curl:.5,spread:.3});
  // Her head arched back
  poseHead(724,hipsY-205,1,E);
}

// POS 6: SPOONING (side-by-side embrace, his chest pressed to her back)
function drawSpoonSide(E,br){
  const sk=getSkin(), D=G.depth, HIS=['#b57c53','#96613e'];
  // Him behind
  himHead(405,496,-1);
  capsule([440,502],[680,506],38,34,himT());
  capsule([680,510],[535,562],34,26,himT());
  // Contact shadow on his chest along her back line
  X.save(); X.globalCompositeOperation='multiply';
  shade(500,502,140,12,'rgba(64,30,24,.30)',.02);
  X.restore();
  // Her body in front, head on pillow
  poseHead(295,520,-1,E);
  capsule([330,524],[605,526],34,28,herT());
  poseGlute(620,522,34,36,0);
  // His arm wrapped around her waist, hand resting in front
  capsule([518,498],[560,516],20,16,himT());
  capsule([560,516],[602,522],15,12,himT());
  handS(604,522,1.71,.95,himT(),{curl:.68,spread:.3});
  // Legs together, softly bent along the bed
  capsule([605,528],[700,548],26,20,herT());
  capsule([700,548],[782,556],18,12,herFarT());
  // Entry vulva
  const open=3+8*D, V=[646,522];
  poseVulva(V[0],V[1],open,.2);
  // Shaft entering from behind
  const B=[670,508], pu=1+.28*(G.shaftPulse||0);
  const tip=[lerp(B[0],V[0],.3+.68*D),lerp(B[1],V[1],.3+.68*D)];
  poseShaft(B,tip,pu);
}

/* ---------------- first-person view positions ---------------- */
function drawPoseFPV(){
  drawFPVRoom();
  const E=herExpression(), br=Math.sin(G.t*TAU*(0.16+G.pleasure*.004))*2;
  const p=G.pos|0;
  if(p===1) drawLegsUpFPV(E,br);
  else if(p===2) drawDoggyFPV(E,br);
  else if(p===3) drawProneBoneFPV(E,br);
  else if(p===4) drawCowgirlFPV(E,br);
  else if(p===5) drawRevCowgirlFPV(E,br);
  else drawSpoonFPV(E,br);
  drawFPVFluids();
}

// FPV 1: LEGS-UP / DEEP MISSIONARY (her thighs pinned back beside your chest)
function drawLegsUpFPV(E,br){
  const sk=getSkin(), D=G.depth, pu=1+.28*(G.shaftPulse||0);
  // Her arched back and pelvis elevated high towards you
  capsule([640,680],[640,430],95,72,herT());
  // Elevated glistening mons & vulva
  poseGlute(640,570,72,48,0);
  const open=5+10*D;
  poseVulva(640,548,open,0,'front',1.9);
  // Her thighs held high beside your shoulders
  for(const s of [-1,1]){
    capsule([640+s*65,570],[640+s*180,360],44,32,herT());
    capsule([640+s*180,360],[640+s*120,220],30,20,herFarT());
  }
  // Shaft driving straight in from below
  const tipY=lerp(650,548,D);
  capsule([640,795],[640,tipY],26*pu,17*pu,himT());
  poseGlans(640,tipY,17,12,pu);
}

// FPV 2: DOGGY STYLE (sculpted ass right in front of you, hands gripping her hips)
function drawDoggyFPV(E,br){
  const sk=getSkin(), D=G.depth, pu=1+.28*(G.shaftPulse||0);
  // Her arched back sloping away
  capsule([640,710],[640,390],92,68,herT());
  X.save(); X.globalCompositeOperation='soft-light';
  shade(640,510,32,110,'rgba(255,230,205,.28)',0); X.restore();
  // Sculpted, rounded buttocks with gluteal crease
  const T=herT();
  for(const s of [-1,1]) poseGlute(640+s*48,560,50,62,s);
  X.strokeStyle=`rgba(${hexToRgb(T.dk).join(',')},.26)`; X.lineWidth=2.4; X.lineCap='round'; X.beginPath();
  X.moveTo(640,506); X.quadraticCurveTo(646,552,640,600); X.stroke();
  // Rear entry vulva parting
  const open=5+10*D;
  poseVulva(640,588,open,0,'front',2.1);
  // Your hands firmly gripping her hips
  for(const s of [-1,1]){
    capsule([640+s*230,770],[640+s*86,615],32,25,himFarT());
    handS(640+s*82,612,-s*.75,1.4,himFarT(),{curl:.66,spread:.3});
  }
  // Shaft rising from below into her
  const tipY=lerp(695,592,D);
  capsule([640,805],[640,tipY],26*pu,17*pu,himT());
  if(D<.55) poseGlans(640,tipY-5,17,12,pu);
}

// FPV 3: PRONE BONE (flat on stomach, tight thighs, deep friction)
function drawProneBoneFPV(E,br){
  const sk=getSkin(), D=G.depth, pu=1+.28*(G.shaftPulse||0);
  // Flat back extending forward
  capsule([640,710],[640,360],88,64,herT());
  // Buttocks pressed flat
  poseGlute(640,580,70,54,0);
  // Tightly pressed thighs
  for(const s of [-1,1])
    capsule([640+s*38,600],[640+s*32,740],34,26,herFarT());
  // Stretched vulva
  const open=4+9*D;
  poseVulva(640,584,open,0,'front',1.8);
  // Shaft
  const tipY=lerp(680,582,D);
  capsule([640,795],[640,tipY],25*pu,16*pu,himT());
}

// FPV 4: COWGIRL (her riding on top, straddling you, bouncing breasts above)
function drawCowgirlFPV(E,br){
  const sk=getSkin(), D=G.depth, jig=G.breast.p*.95, bsz=.7+(G.char?G.char.breastSize:.45)*.6;
  const hipsY=600-60*D;
  // Your abdomen in foreground
  const HT=himT();
  skFillRad(()=>{ X.ellipse(640,812,220,95,0,0,TAU); },HT,640,772,20,250);
  skClipIn(()=>{ X.ellipse(640,812,220,95,0,0,TAU); },()=>{
    fAO(640,744,130,26,.24);
    fHi(596,790,92,30,'rgba(255,238,220,0.16)');
  });
  // Shaft up into her
  const pu=1+.28*(G.shaftPulse||0);
  capsule([640,790],[640,hipsY+6],24*pu,16*pu,himT());
  // Her straddling thighs & hips
  for(const s of [-1,1])
    capsule([640+s*65,hipsY],[s<0?515:765,725],46,36,herT());
  poseGlute(640,hipsY,74,42,0);
  // Torso rising to chest
  capsule([640,hipsY-10],[640,360],62,48,herT());
  // Bouncing breasts above
  for(const s of [-1,1]){
    const bx=640+s*62, by=330+jig*.7+br*.5;
    poseBreast(bx,by,34*bsz,E);
  }
  shade(640,338+jig*.7+br*.5,11,40*bsz,'rgba(160,92,70,0.24)',0);
  // Face above looking down at you
  const hc=G.char?G.char.hairColor:'#231318', FT=herT();
  hairMassS(640,220,56,52,0,hc);
  for(const s of [-1,1])
    tressS(640+s*34,238,640+s*48,292,640+s*46,342,640+s*40,384,20,7,hc,'rgba(255,200,210,0.12)');
  skFillRad(()=>{ X.ellipse(640,232,32,36,0,0,TAU); },FT,640,216,4,48);
  skClipIn(()=>{ X.ellipse(640,232,32,36,0,0,TAU); },()=>{
    fAO(640,264,26,14,.22);
    fHi(640,212,20,12,'rgba(255,240,225,0.24)');
  });
  X.strokeStyle='rgba(65,35,42,.95)'; X.lineWidth=2.6; X.lineCap='round';
  for(const s of [-1,1]){ X.beginPath();
    X.moveTo(640+s*8,229); X.quadraticCurveTo(640+s*16,234,640+s*24,228); X.stroke(); }
  const [bR,bG,bB]=hexToRgb(G.char?G.char.blushColor:'#e86070');
  for(const s of [-1,1]) shade(640+s*20,243,12,8,`rgba(${bR},${bG},${bB},${.10+E.blush*.26})`,0);
  const mo=E.mouth;
  X.fillStyle=G.char?G.char.lipColor:'#b3555f';
  X.beginPath(); X.ellipse(640,253,6+mo*3,3+mo*5,0,0,TAU); X.fill();
  if(mo>.2){ X.fillStyle='#5a1622';
    X.beginPath(); X.ellipse(640,253,3+mo*2,1.6+mo*3.2,0,0,TAU); X.fill(); }
  X.fillStyle='rgba(255,225,225,.3)';
  X.beginPath(); X.ellipse(638,250,2,1.2,0,0,TAU); X.fill();
}

// FPV 5: REVERSE COWGIRL (her on top facing away, ass bouncing in your face)
function drawRevCowgirlFPV(E,br){
  const sk=getSkin(), D=G.depth, pu=1+.28*(G.shaftPulse||0);
  const hipsY=580-55*D;
  // Torso rising away
  capsule([640,hipsY-10],[640,330],64,48,herT());
  // Round buttocks bouncing right in front of the camera
  for(const s of [-1,1]) poseGlute(640+s*52,hipsY,54,64,s);
  const T=herT();
  X.strokeStyle=`rgba(${hexToRgb(T.dk).join(',')},.26)`; X.lineWidth=2.4; X.lineCap='round'; X.beginPath();
  X.moveTo(640,hipsY-48); X.quadraticCurveTo(646,hipsY,640,hipsY+48); X.stroke();
  // Entry
  const open=5+9*D;
  poseVulva(640,hipsY+15,open,0,'front',2.0);
}

// FPV 6: SPOONING (side-by-side rear angle)
function drawSpoonFPV(E,br){
  const sk=getSkin(), D=G.depth, pu=1+.28*(G.shaftPulse||0);
  capsule([420,730],[700,300],86,60,herT());
  poseGlute(500,622,56,72,0,.2);
  const open=4+8*D;
  poseVulva(558,602,open,.2,'side',1.5);
  const tip=[lerp(560,560,.3+.68*D),lerp(700,602,.3+.68*D)];
  capsule([560,700],tip,20*pu,15*pu,himT());
}
