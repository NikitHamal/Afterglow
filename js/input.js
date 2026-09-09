// Afterglow — module: input (loaded by index.html)
'use strict';
/* ---------------- input ---------------- */
let dragY=null, dragK=1/300;
function pdown(e){
  if(e.target.closest('button') || e.target.closest('.hud') || e.target.closest('#custPanel') || e.target.closest('.ov')) return;
  if(G.state==='intro')return;
  if(G.state==='climax'){ spurt(); return; }
  if(G.state!=='play'&&G.state!=='orgasm')return;
  dragY=e.clientY; G.dragOn=true; stage.classList.add('drag');
  if(stage.setPointerCapture&&e.pointerId!==undefined) try{stage.setPointerCapture(e.pointerId)}catch(_){}
}
function pmove(e){ if(!G.dragOn||dragY===null)return;
  const dy=e.clientY-dragY; dragY=e.clientY;
  G.target=clamp(G.target+dy*dragK,0,1);
}
function pup(){ G.dragOn=false; dragY=null; stage.classList.remove('drag'); }
stage.addEventListener('pointerdown',pdown);
window.addEventListener('pointermove',pmove);
window.addEventListener('pointerup',pup);
window.addEventListener('pointercancel',pup);

function toggleKiss(){ if(G.state!=='play'||G.tired)return; G.kissT=G.kissT?0:1;
  if(G.kissT){ playMoan(.35,{dur:.5,pmul:1.15,vol:.85}); say('mmh… ♥',1.6); } }
/* SOLO SHOWCASE (S): just her, no male. The current girl performs a slow
   alluring sway driven by updateSolo3 (3D) / a 2D body-roll — works for the
   procedural girls and every GLB preset, since GLBs ride her3.root. */
function toggleSolo(){
  if(G.state!=='play'&&G.state!=='orgasm')return;
  G.solo=!G.solo; G.soloPh=0; G.soloMoanT=2.5;
  if(G.solo){
    G.oralT=0; G.oral=0; G.kissT=0; G.rubT=0;
    // 2D closeups are couple-framed; drop back to the full-body side view
    if(typeof CAM3==='undefined' && G.view!=='side') toggleView();
    say(G.solo?('watch me… ♥'):('come back… I want you… ♥'),1.8);
    if(typeof playMoan==='function') playMoan(.4,{dur:.6,pmul:1.1,vol:.9});
  }
  const b=document.getElementById('btnSolo'); if(b) b.classList.toggle('on',G.solo);
}
function toggleRub(){ // tap: start (keep spot) / advance to next spot · double-tap (or 0): stop
  if(G.state!=='play'||G.tired)return;
  if(!G.rubT){ G.rubT=1; rubFeedback(rubZoneName()); }
  else cycleRubZone();
}
function stopRub(){ G.rubT=0; }
const RUBSAY={
  both:['both hands… greedy… ♥','mmh… both of them, yes… ♥'],
  left:['ah… that one… ♥','gentle… she likes that one… ♥'],
  right:['ngh… yes, there… ♥',"that one's sensitive… ♥"],
  low:["ah—! not there—… don't stop… ♥",'mmHN… you found it… ♥']};
function rubZoneName(){ return RUBZONES[G.rubZone|0]||'both'; }
function rubFeedback(zone){
  G.nod=1;
  playMoan(zone==='low'?.48:.35,{dur:.42,pmul:zone==='low'?1.35:1.15,vol:.85});
  G.speech={txt:pick(RUBSAY[zone]||RUBSAY.both),until:G.t+1.8};
  G.sayCd=Math.max(G.sayCd,1.2);
}
function setRubZone(z){ // direct pick (1-4): starts rubbing immediately — full control
  if(G.state!=='play')return;
  if(G.tired){ say(pick(LINES.tired)); return; }
  G.rubZone=((z%4)+4)%4; G.rubT=1;
  rubFeedback(rubZoneName());
}
function cycleRubZone(){ // Q or repeated C-tap
  if(G.state!=='play'||G.tired)return;
  if(!G.rubT){ G.rubT=1; rubFeedback(rubZoneName()); return; }
  setRubZone((G.rubZone|0)+1);
}
function keyFX(){ if((G.state==='play'||G.state==='orgasm')&&G.sesT>CFG.climaxAt) startClimax(); }
function toggleView(){
  G.view=(G.view==='fpv'?'side':'fpv'); G.viewFade=1;
  lsSet('ag_view',G.view);
  const bv=document.getElementById('btnView'); if(bv) bv.classList.toggle('on',G.view==='fpv');
}
function toggleFaceSide(){
  G.faceSide = (G.faceSide === 'profile' ? 'camera' : 'profile');
  say(G.faceSide === 'camera' ? 'look at me… ♥' : 'ah… so good… ♥', 1.8);
}
function cycleFPVFocus(){
  if(G.view!=='fpv'){
    toggleView();
    G.fpvFocus='hips';
    say('sweet spot closeup', 1.4);
    const bz=document.getElementById('btnZoom');
    if(bz) bz.classList.add('on');
    return;
  }
  const modes=['full','hips','breasts','face'];
  let idx=modes.indexOf(G.fpvFocus||'full');
  G.fpvFocus=modes[(idx+1)%modes.length];
  const labels={full:'normal view', hips:'sweet spot closeup', breasts:'breasts closeup', face:'face closeup'};
  say(labels[G.fpvFocus]||'zoom', 1.4);
  const bz=document.getElementById('btnZoom');
  if(bz) bz.classList.toggle('on', G.fpvFocus!=='full');
}
window.addEventListener('keydown',e=>{
  if(e.code==='Space'){e.preventDefault();
    if(e.repeat) return;
    if(G.state==='climax'){ spurt(); return; }
    if(G.state==='intro') return;
    if(G.state==='play'||G.state==='orgasm'){ G.spaceHeld=true;
      if(G.tired){ G.spaceHeld=false; say(pick(LINES.tired)); }
      else {
        G.autoPh=Math.asin(clamp((G.target-.55)/.38,-1,1));
        if(G.solo && !G.rubT) { G.rubZone=3; G.rubT=1; rubFeedback('low'); }
      } } }
  else if(e.code==='KeyE')toggleKiss();
  else if(e.code==='KeyC'){ const now=performance.now();
    if(G.rubT&&now-(G._lastRubTap||0)<350){ G._lastRubTap=0; stopRub(); }
    else { G._lastRubTap=now; toggleRub(); } }
  else if(e.code==='KeyQ')cycleRubZone();
  else if(e.code==='Digit1')setRubZone(0);
  else if(e.code==='Digit2')setRubZone(1);
  else if(e.code==='Digit3')setRubZone(2);
  else if(e.code==='Digit4')setRubZone(3);
  else if(e.code==='Digit0')stopRub();
  else if(e.code==='KeyT')toggleFaceSide();
  else if(e.code==='KeyZ')cycleFPVFocus();
  else if(e.code==='Tab'){ e.preventDefault(); cyclePose(); }
  else if(e.code==='KeyO')toggleOral();
  else if(e.code==='KeyS')toggleSolo();
  else if(e.code==='KeyX')keyFX();
  else if(e.code==='KeyV')toggleView();
  else if(e.code==='KeyP'){ if(typeof toggleCustomPanel==='function') toggleCustomPanel(); }
  else if(e.code==='KeyM'){ai();setMute(!muted);}
  else if(e.code==='KeyF'){toggleFull();}
});
window.addEventListener('keyup',e=>{
  if(e.code==='Space') {
    G.spaceHeld=false;
    if(G.solo && G.rubT) stopRub();
  }
});
const bK=document.getElementById('btnKiss'),bR=document.getElementById('btnRub'),bC=document.getElementById('btnClimax');
if(bK) bK.onclick=e=>{e.stopPropagation();toggleKiss()};
if(bR) bR.onclick=e=>{ e.stopPropagation(); const now=performance.now();
  if(G.rubT&&now-(G._lastRubTap||0)<350){ G._lastRubTap=0; stopRub(); }
  else { G._lastRubTap=now; toggleRub(); } };
if(bC) bC.onclick=e=>{e.stopPropagation();keyFX()};
const bP=document.getElementById('btnPos');
if(bP) bP.onclick=e=>{e.stopPropagation();cyclePose()};
const bO=document.getElementById('btnOral');
if(bO) bO.onclick=e=>{e.stopPropagation();toggleOral()};
const bSolo=document.getElementById('btnSolo');
if(bSolo) bSolo.onclick=e=>{e.stopPropagation();toggleSolo()};
const bM=document.getElementById('btnMute');
if(bM) bM.onclick=e=>{e.stopPropagation();ai();setMute(!muted)};
const bRest=document.getElementById('btnRestart');
if(bRest) bRest.onclick=e=>{e.stopPropagation();restart()};
const bV=document.getElementById('btnView');
if(bV) bV.onclick=e=>{e.stopPropagation();toggleView()};
const bZ=document.getElementById('btnZoom');
if(bZ) bZ.onclick=e=>{e.stopPropagation();cycleFPVFocus()};
const bCust=document.getElementById('btnCustom');
if(bCust) bCust.onclick=e=>{e.stopPropagation();if(typeof toggleCustomPanel==='function')toggleCustomPanel()};
function toggleFull(){ if(document.fullscreenElement)document.exitFullscreen(); else stage.requestFullscreen&&stage.requestFullscreen(); }
const bF=document.getElementById('btnFull');
if(bF) bF.onclick=e=>{e.stopPropagation();toggleFull()};

document.getElementById('btnBegin').onclick=()=>{
  ai(); AC.resume&&AC.resume();
  document.getElementById('intro').classList.add('hide');
  G.state='play'; G.sesT=0;
};
document.getElementById('btnKeep').onclick=()=>{ // round 2+
  document.getElementById('endOv').classList.add('hide');
  Object.assign(G,{state:'play',climax:false,spurts:0,climaxT:0,endedShown:false,finishT:0,
    stamina:100,tired:false, pleasure:Math.max(G.pleasure,52), sync:false, round:G.round+1});
  G.drips.length=0; G.glisten.length=Math.min(G.glisten.length,6);
  say(pick(LINES.keep));
};
document.getElementById('btnAgain').onclick=()=>restart();
function restart(){
  document.getElementById('endOv').classList.add('hide');
  Object.assign(G,{state:'play',sesT:0,target:.28,depth:.28,pDepth:.28,maxD:0,valley:.28,
    strokes:0,inBand:0,rate:0,combo:1,comboBest:1,pleasure:12,floor:2,sens:1,stamina:100,tired:false,
    kissT:0,rubT:0,oralT:0,orgasms:0,orgT:0,after:0,climax:false,spurts:0,climaxT:0,finishT:0,sync:false,
    endedShown:false,round:1,speech:null,sayCd:0,spaceHeld:false,shaftPulse:0,solo:false,soloPh:0});
  G.drips.length=0; G.slisten=0; G.glisten.length=0; G.hearts.length=0; G.sweat.length=0; G.jets.length=0;
  document.getElementById('orgHearts').innerHTML='';
}
