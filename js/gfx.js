// Afterglow — module: gfx (loaded by index.html)
'use strict';
/* ============================================================
   RENDERING — painter's algorithm, everything procedural
   ============================================================ */
const Dskin={her:'#f0c4a0',herSh:'#cf9273',herDk:'#c98a6d',him:'#d9a37a',himSh:'#b0744c'};
function hp(x,y,f){ const d=G.depth; return [x-95*d*f, y+26*d*f]; }   // his thrust shear
function sp(x,y,f){ const d=G.depth*.12+G.impact*.05; return [x-95*d*f, y+26*d*f]; } // her recoil
function chaos(x){ return Math.sin(G.t*37+x*13)*G.shake; }

function capsule(a,b,r1,r2,fill,bob=0){
  if(!a||!b||!isFinite(a[0])||!isFinite(a[1])||!isFinite(b[0])||!isFinite(b[1])) return;
  const ang=Math.atan2(b[1]-a[1],b[0]-a[0]);
  X.beginPath();
  X.arc(a[0],a[1]+bob,r1,ang+Math.PI/2,ang+Math.PI*1.5);
  X.arc(b[0],b[1]+bob,r2,ang-Math.PI/2,ang+Math.PI/2);
  X.closePath();
  const cx=(a[0]+b[0])/2, cy=(a[1]+b[1])/2;
  const r=Math.hypot(b[0]-a[0],b[1]-a[1])/2+Math.max(r1,r2);
  if(fill instanceof CanvasGradient || fill instanceof CanvasPattern){
    X.fillStyle=fill; X.fill();
  } else {
    const g=X.createLinearGradient(a[0],a[1],b[0],b[1]);
    if(typeof fill==='string'){ g.addColorStop(0,fill); g.addColorStop(.5,fill); g.addColorStop(1,fill); }
    else if(Array.isArray(fill)){ g.addColorStop(0,fill[0]); g.addColorStop(1,fill[1]); }
    else { X.fillStyle=fill; X.fill(); return; }
    X.fillStyle=g; X.fill();
  }
  // soft inner core highlight
  X.save(); X.globalCompositeOperation='soft-light';
  const sg=X.createRadialGradient(cx-r*.1,cy-r*.15,0,cx,cy,r*.9);
  sg.addColorStop(0,'rgba(255,235,215,.18)');
  sg.addColorStop(1,'rgba(255,235,215,0)');
  X.fillStyle=sg; X.fill(); X.restore();
  // rim shadow
  X.save(); X.globalCompositeOperation='multiply';
  const rg=X.createRadialGradient(cx,cy,r*.5,cx,cy,r);
  rg.addColorStop(0,'rgba(0,0,0,0)');
  rg.addColorStop(1,'rgba(0,0,0,.28)');
  X.fillStyle=rg; X.fill(); X.restore();
}
function sg(y0,y1,c1,c2){ const g=X.createLinearGradient(0,y0,0,y1); g.addColorStop(0,c1); g.addColorStop(1,c2); return g; }
function shade(x,y,rx,ry,col,rot=0,lc){ const g=X.createRadialGradient(x,y,0,x,y,Math.max(rx,ry)*1.1);
  if(lc) g.addColorStop(0,lc); else g.addColorStop(0,col);
  g.addColorStop(1,'rgba(0,0,0,0)'); X.fillStyle=g; X.beginPath(); X.ellipse(x,y,rx,ry,rot,0,TAU); X.fill(); }
function drawRoom(){
  X.fillStyle='#171012'; X.fillRect(0,0,W,H);
  X.fillStyle='rgba(0,0,0,.25)'; X.fillRect(0,0,W,90);
  // window + moonlight
  X.fillStyle='rgba(150,160,168,.06)'; X.fillRect(1040,70,190,240);
  X.strokeStyle='rgba(180,190,196,.10)'; X.lineWidth=4;
  X.strokeRect(1040,70,190,240); X.beginPath(); X.moveTo(1135,70);X.lineTo(1135,310);X.moveTo(1040,190);X.lineTo(1230,190);X.stroke();
  X.fillStyle='rgba(214,222,228,.13)'; X.beginPath();X.arc(1178,128,26,0,TAU);X.fill();
  X.fillStyle='rgba(20,14,16,.55)'; X.beginPath();X.moveTo(1005,40);X.lineTo(1075,40);X.lineTo(1028,330);X.lineTo(995,330);X.closePath();X.fill();
  // nightstand + lamp
  X.fillStyle='#241418'; X.fillRect(52,470,150,110);
  X.fillStyle='rgba(255,180,110,.05)'; X.fillRect(52,470,150,8);
  X.strokeStyle='#3a2228'; X.lineWidth=2; X.strokeRect(70,488,112,30);
  const g=X.createRadialGradient(127,400,10,127,400,320);
  g.addColorStop(0,'rgba(255,190,120,.30)'); g.addColorStop(1,'rgba(255,190,120,0)');
  X.save(); X.globalCompositeOperation='screen'; X.fillStyle=g; X.beginPath();X.arc(127,400,320,0,TAU);X.fill(); X.restore();
  X.strokeStyle='#2e1b1f'; X.lineWidth=5; X.beginPath();X.moveTo(127,468);X.lineTo(127,428);X.stroke();
  X.fillStyle='#3d2229'; X.beginPath();X.moveTo(97,430);X.lineTo(157,430);X.lineTo(143,392);X.lineTo(111,392);X.closePath();X.fill();
  X.fillStyle='rgba(255,200,130,.55)'; X.beginPath();X.moveTo(111,394);X.lineTo(143,394);X.lineTo(152,428);X.lineTo(102,428);X.closePath();X.fill();
}
function drawBed(){
  X.fillStyle='#2b171c'; X.fillRect(0,556,W,H-556);
  X.fillStyle=sg(540,720,'#4b2a32','#33191f'); X.fillRect(30,536,W-60,50);
  X.strokeStyle='rgba(0,0,0,.25)'; X.lineWidth=3;
  for(let i=0;i<5;i++){ X.beginPath(); const y=552+i*9;
    X.moveTo(60+((i*67)%140),y+40); X.quadraticCurveTo(400, y+18+((i*23)%30), 1230-((i*53)%160),y+36); X.stroke(); }
  // pillow
  X.fillStyle=sg(500,570,'#efe0cd','#b7a48e');
  X.beginPath(); X.ellipse(288,543,105,30,-.04,0,TAU); X.fill();
  X.fillStyle='rgba(120,96,78,.35)'; X.beginPath(); X.ellipse(360,548,34,17,-.1,0,TAU); X.fill();
  // body shadows
  X.fillStyle='rgba(10,4,7,.38)';
  X.beginPath(); X.ellipse(520,575,300,26,0,0,TAU); X.fill();
  X.beginPath(); X.ellipse(880,590,120,20,0,0,TAU); X.fill();
}
function heartPath(x,y,s){ X.beginPath();
  X.moveTo(x,y+s*.9); X.bezierCurveTo(x-s*1.3,y+s*.1,x-s*.8,y-s*.9,x,y-s*.25);
  X.bezierCurveTo(x+s*.8,y-s*.9,x+s*1.3,y+s*.1,x,y+s*.9); X.closePath();
}
/* ---------- her ---------- */
