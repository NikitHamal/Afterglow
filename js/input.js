// Afterglow — module: input (loaded by index.html)
'use strict';
/* ---------------- input ---------------- */
let dragY=null, dragK=1/300;
function pdown(e){ if(G.state==='intro')return;
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
  if(G.kissT){ playMoan(.3,{dur:.5,pmul:1.15,vol:.6}); say('mmh… ♥',1.6); } }
function toggleRub(){ if(G.state!=='play'||G.tired)return; G.rubT=G.rubT?0:1; }
function keyFX(){ if((G.state==='play'||G.state==='orgasm')&&G.sesT>CFG.climaxAt) startClimax(); }
function toggleView(){
  G.view=(G.view==='fpv'?'side':'fpv'); G.viewFade=1;
  lsSet('ag_view',G.view);
  const bv=document.getElementById('btnView'); if(bv) bv.classList.toggle('on',G.view==='fpv');
}
window.addEventListener('keydown',e=>{
  if(e.code==='Space'){e.preventDefault();
    if(e.repeat) return;
    if(G.state==='climax'){ spurt(); return; }
    if(G.state==='intro') return;
    if(G.state==='play'||G.state==='orgasm'){ G.spaceHeld=true;
      if(G.tired){ G.spaceHeld=false; say(pick(LINES.tired)); }
      else G.autoPh=Math.asin(clamp((G.target-.55)/.38,-1,1)); }}
  else if(e.code==='KeyE')toggleKiss();
  else if(e.code==='KeyC')toggleRub();
  else if(e.code==='KeyX')keyFX();
  else if(e.code==='KeyV')toggleView();
  else if(e.code==='KeyM'){ai();setMute(!muted);}
  else if(e.code==='KeyF'){toggleFull();}
});
window.addEventListener('keyup',e=>{ if(e.code==='Space') G.spaceHeld=false; });
const bK=document.getElementById('btnKiss'),bR=document.getElementById('btnRub'),bC=document.getElementById('btnClimax');
bK.onclick=e=>{e.stopPropagation();toggleKiss()}; bR.onclick=e=>{e.stopPropagation();toggleRub()};
bC.onclick=e=>{e.stopPropagation();keyFX()};
document.getElementById('btnMute').onclick=()=>{ai();setMute(!muted)};
document.getElementById('btnRestart').onclick=()=>restart();
document.getElementById('btnView').onclick=e=>{e.stopPropagation();toggleView()};
function toggleFull(){ if(document.fullscreenElement)document.exitFullscreen(); else stage.requestFullscreen&&stage.requestFullscreen(); }
document.getElementById('btnFull').onclick=toggleFull;

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
    kissT:0,rubT:0,orgasms:0,orgT:0,after:0,climax:false,spurts:0,climaxT:0,finishT:0,sync:false,
    endedShown:false,round:1,speech:null,sayCd:0,spaceHeld:false});
  G.drips.length=0; G.slisten=0; G.glisten.length=0; G.hearts.length=0; G.sweat.length=0;
  document.getElementById('orgHearts').innerHTML='';
}
