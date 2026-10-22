// Afterglow — module: app (loaded by index.html)
'use strict';
function draw(){
  atmCrossfadeCheck();               // grab the outgoing frame before clearing
  X.setTransform(CV.width/W,0,0,CV.height/H,0,0);
  X.clearRect(0,0,W,H);
  atmEnsure();                       // (re)build cached room/bed layers
  // GROUNDED CAMERA: impact motion lives in the bodies (breast bounce, flesh
  // jiggle, thrust depth) — never in the world. No scene shake on insertion.
  X.save();
  if(G.view==='fpv'){
    if(G.oral>.03) drawOralFPV();
    else if((G.pos|0)!==0) drawPoseFPV();
    else drawFPV();
    X.restore();
    drawLight();
  }
  else {
    drawRoom(); drawBed();
    if(G.state!=='intro'){
      atmBodyBegin();
      if(G.solo) drawSoloSide();
      else if(G.oral>.03) drawOralSide();
      else if((G.pos|0)!==0) drawPoseSide();
      else {
        const out=drawHer();
        drawVulva(); drawShaft();
        drawHerHead(out.E);
        drawBreast(out.E,out.br);
        drawHim(out.E);
        drawHerNear(out.E,out);
        drawRubFX(out.br);
      }
      atmBodyEnd();
      drawFluids();
    }
    X.restore();
    drawLight();
  }
  if(G.viewFade>0){ X.fillStyle=`rgba(5,2,4,${clamp(G.viewFade,0,1)})`; X.fillRect(0,0,W,H); }
  hudTick();
}

/* SOLO SHOWCASE (2D): her alone, full body, slow alluring roll.
   His parts (shaft, him) are skipped; a gentle sway + roll replaces thrust. */
function drawSoloSide(){
  const t=G.t||0;
  X.save();
  X.translate(Math.sin(t*1.1)*10, -Math.abs(Math.sin(t*1.1))*6);
  X.translate(W/2,H/2); X.rotate(Math.sin(t*1.1)*0.012); X.translate(-W/2,-H/2);
  const out=drawHer();
  drawHerHead(out.E);
  drawBreast(out.E,out.br);
  drawHerNear(out.E,out);
  drawRubFX(out.br);
  X.restore();
}

/* ---------------- HUD DOM sync ---------------- */
const $=id=>document.getElementById(id);
const plFill=$('plFill'), plBar=$('plBar'), plMood=$('plMood'), dpFill=$('dpFill'), hotEl=$('hotZone'),
  dpMark=$('dpMark'), stamFill=$('stamFill'), stamBox=$('stamBox'), stamVal=$('stamVal'), stamLbl=$('stamLbl'),
  tBand=$('tBand'), tMark=$('tMark'), comboChip=$('comboChip'), comboB=comboChip.querySelector('b'),
  clock=$('clock'), strk=$('strk'), btnC=$('btnClimax');
const MOODS=[[0,'snug'],[22,'blushing'],[45,'melting'],[68,'trembling'],[85,'on the edge'],[99.9,'about to break']];
let lastOrg=-1, comboTier=0, lastPl='';
const tempoBox=$('tempoBox');
function comboPop(txt, sub){
  const el=document.createElement('div');
  el.className='comboPop'; el.innerHTML=txt+'<small>'+sub+'</small>';
  stage.appendChild(el);
  setTimeout(()=>el.remove(),1200);
}
const _hudC=new Map();
function _st(el,k,v){ const m=_hudC.get(el)||(_hudC.set(el,{}),_hudC.get(el)); if(m[k]!==v){ m[k]=v; el.style[k]=v; } }
function _tx(el,v){ const m=_hudC.get(el)||(_hudC.set(el,{}),_hudC.get(el)); if(m.$t!==v){ m.$t=v; el.textContent=v; } }
function _cl(el,c,on){ const m=_hudC.get(el)||(_hudC.set(el,{}),_hudC.get(el)); on=!!on; if(m['c'+c]!==on){ m['c'+c]=on; el.classList.toggle(c,on); } }
const btnKissEl=$('btnKiss'), btnRubEl=$('btnRub'), btnPosEl=$('btnPos'), btnOralEl=$('btnOral'), btnSoloEl=$('btnSolo'), btnZoomEl=$('btnZoom');
function hudTick(){
  _st(plFill,'transform',`scaleY(${(G.pleasure/100).toFixed(3)})`);
  _cl(plBar,'hot',G.pleasure>82);
  let m='melting'; for(const [th,l] of MOODS){ if(G.pleasure<=th){m=l;break} }
  if(G.state!=='intro') _tx(plMood, G.state==='orgasm'?'climaxing' : G.state==='climax'||G.state==='finish'?'full of you' : m);
  _st(dpFill,'transform',`scaleY(${G.depth.toFixed(3)})`);
  const z=hotZone();
  _st(hotEl,'bottom',(z.c*100).toFixed(2)+'%'); _st(hotEl,'height',(z.w*100).toFixed(2)+'%');
  _st(dpMark,'bottom',`calc(${(G.depth*100).toFixed(1)}% - 1px)`);
  _st(stamFill,'transform',`scaleX(${(G.stamina/100).toFixed(3)})`);
  _tx(stamVal,String(Math.round(G.stamina)));
  _cl(stamBox,'tired',G.tired);
  _tx(stamLbl,G.tired?'catching breath':'stamina');
  const b=sweetBand(), MAXR=3;
  _st(tBand,'left',clamp((b.c-b.hw)/MAXR*100,0,96).toFixed(2)+'%');
  _st(tBand,'width',clamp(b.hw*2/MAXR*100,2,100).toFixed(2)+'%');
  _st(tMark,'left',clamp(G.rate/MAXR*100,0,99).toFixed(2)+'%');
  _cl(comboChip,'on',G.combo>1.05);
  if(G.combo>1.05) _tx(comboB,G.combo.toFixed(1));
  // sweet-band feedback + combo milestones
  _cl(tempoBox,'inband', G.rate>.25 && Math.abs(G.rate-b.c)<=b.hw);
  const tier=Math.floor((G.combo-1)/.5);
  if(tier>comboTier && tier>0) comboPop('×'+G.combo.toFixed(1), tier>=3?'unstoppable':tier>=2?'in sync':'nice rhythm');
  comboTier=tier;
  _cl(stage,'intro', G.state==='intro');
  const plv=(G.pleasure/100).toFixed(2); if(plv!==lastPl){ lastPl=plv; stage.style.setProperty('--pl',plv); }
  const mn=(G.sesT/60)|0, sc=('0'+((G.sesT%60)|0)).slice(-2);
  _tx(clock,G.round>1?`R${G.round} · ${mn}:${sc}`:`${mn}:${sc}`);
  _tx(strk,G.strokes+(G.strokes===1?' stroke':' strokes'));
  const dis=!(G.sesT>CFG.climaxAt&&(G.state==='play'||G.state==='orgasm')); if(btnC.disabled!==dis) btnC.disabled=dis;
  _cl(btnC,'ready',G.pleasure>85&&G.state==='play');
  _cl(btnKissEl,'on',G.kissT>0);
  _cl(btnRubEl,'on',G.rubT>0);
  const bRl=btnRubEl, zl='RUB · '+(RUBLBL[rubZoneName()]||'BOTH')+' <kbd>C</kbd>';
  if(bRl._zl!==zl){ bRl._zl=zl; bRl.innerHTML=zl; }
  const bPl=btnPosEl, pl='POS · '+posName()+' <kbd>Tab</kbd>';
  if(bPl._pl!==pl){ bPl._pl=pl; bPl.innerHTML=pl; }
  const bOr = btnOralEl;
  if(bOr){
    _cl(bOr,'on', (G.oralT || 0) > 0);
    const ot = ((G.oralT === 2) ? 'BLOW' : ((G.oralT === 1) ? 'LICK' : 'ORAL')) + ' <kbd>O</kbd>';
    if(bOr._ot !== ot){ bOr._ot = ot; bOr.innerHTML = ot; }
  }
  if(btnSoloEl) _cl(btnSoloEl,'on',!!G.solo);
  if(btnZoomEl) _cl(btnZoomEl,'on', G.view==='fpv' && G.fpvFocus!=='full');
  // speech bubble
  if(G.speech){ bub.textContent=G.speech.txt; bub.classList.add('show');
    if(G.view==='fpv'){
      const f=G.fpvFocus||'full';
      const bTop=f==='face'?8:(f==='breasts'?18:13);
      bub.style.left=(640/10-7)+'em'; bub.style.top=bTop+'em';
    } else {
      const bx = lerp(245, 285, G.faceBlend || 0);
      bub.style.left=(bx/10)+'em'; bub.style.top=(405/10-2.2)+'em';
    }
  } else bub.classList.remove('show');
  if(G.orgasms!==lastOrg){ lastOrg=G.orgasms; tierHearts(); }
}

/* ---------------- resize & loop ---------------- */
/* adaptive resolution: the canvas backing store follows measured frame time (scale 0.55..1 of the
   device pixel ratio, max 2). Off in dev screenshot modes (?shot / ?clean / ?adapt=0) so captures stay sharp. */
const PERF2={scale:1, acc:0, n:0, slow:0, fast:0, on:!/[?&](adapt=0|shot=|clean=1)/.test(location.search)};
function perf2Tick(raw){
  if(!PERF2.on||document.hidden||raw<=0||raw>0.5) return;
  PERF2.acc+=raw; PERF2.n++;
  if(PERF2.acc<1) return;
  const avg=PERF2.acc/PERF2.n; PERF2.acc=0; PERF2.n=0;
  if(avg>1/40){ PERF2.fast=0; if(++PERF2.slow>=2){ PERF2.slow=0; perf2Scale(PERF2.scale-0.15); } }
  else if(avg<1/55){ PERF2.slow=0; if(++PERF2.fast>=5){ PERF2.fast=0; perf2Scale(PERF2.scale+0.15); } }
  else PERF2.slow=PERF2.fast=0;
}
function perf2Scale(s){ s=clamp(s,0.55,1); if(Math.abs(s-PERF2.scale)<0.01) return; PERF2.scale=s; resize(); }
function resize(){
  const w=stage.clientWidth, dpr=Math.min(2,window.devicePixelRatio||1)*PERF2.scale;
  const cw=Math.round(w*dpr), ch=Math.round(w*dpr*9/16);
  if(CV.width!==cw||CV.height!==ch){ CV.width=cw; CV.height=ch; }
  // HUD em scales with the stage, but never below a legible floor on phones
  stage.style.fontSize=Math.max(w/1280*10, Math.min(7.2, w/1280*15))+'px';
  dragK=1/(w*CFG.dragSens);
}
window.addEventListener('resize',resize); resize();

let last=performance.now();
function tick(ts){
  const raw=(ts-last)/1000, dt=clamp(raw,0,.05); last=ts;
  perf2Tick(raw);
  update(dt); draw();
  requestAnimationFrame(tick);
}
requestAnimationFrame(tick);
setMute(muted);
if(lsGet('ag_view')==='fpv'){ G.view='fpv'; document.getElementById('btnView').classList.add('on'); }
// character system — init after DOM + canvas ready
loadCharState();
initCharUI();
// headless screenshot helper: ?play=1&char=kiyoko&view=fpv auto-starts without intro
try{
  const qs=new URLSearchParams(location.search);
  if(qs.get('play')==='1'){
    const ch=qs.get('char'); if(ch && typeof CHARS!=='undefined' && CHARS[ch]){ Object.assign(G.char, JSON.parse(JSON.stringify(CHARS[ch]))); }
    G.state='play'; G.pleasure=+qs.get('pleasure')||62; G.ar=+qs.get('ar')||68; G.depth=+qs.get('depth')||0.55;
    const v=qs.get('view'); if(v) G.view=v;
    const solo=qs.get('solo'); if(solo) G.solo=solo==='1';
    if(typeof startNight==='function') startNight();
    document.getElementById('intro')?.classList.add('hide');
    document.getElementById('charSel')?.classList.add('hide');
    G.state='play';
  }
}catch(_){}
