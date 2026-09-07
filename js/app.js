// Afterglow — module: app (loaded by index.html)
'use strict';
function draw(){
  X.setTransform(CV.width/W,0,0,CV.height/H,0,0);
  X.clearRect(0,0,W,H);
  const sx=(R()-.5)*2*G.shake+chaos(5)*.6, sy=(R()-.5)*2*G.shake;
  X.save(); X.translate(sx,sy);
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
      if(G.oral>.03) drawOralSide();
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
      drawFluids();
    }
    X.restore();
    drawLight();
  }
  if(G.viewFade>0){ X.fillStyle=`rgba(5,2,4,${clamp(G.viewFade,0,1)})`; X.fillRect(0,0,W,H); }
  hudTick();
}

/* ---------------- HUD DOM sync ---------------- */
const $=id=>document.getElementById(id);
const plFill=$('plFill'), plBar=$('plBar'), plMood=$('plMood'), dpFill=$('dpFill'), hotEl=$('hotZone'),
  dpMark=$('dpMark'), stamFill=$('stamFill'), stamBox=$('stamBox'), stamVal=$('stamVal'), stamLbl=$('stamLbl'),
  tBand=$('tBand'), tMark=$('tMark'), comboChip=$('comboChip'), comboB=comboChip.querySelector('b'),
  clock=$('clock'), strk=$('strk'), btnC=$('btnClimax');
const MOODS=[[0,'snug'],[22,'blushing'],[45,'melting'],[68,'trembling'],[85,'on the edge'],[99.9,'about to break']];
let lastOrg=-1;
function hudTick(){
  plFill.style.transform=`scaleY(${G.pleasure/100})`;
  plBar.classList.toggle('hot',G.pleasure>82);
  let m='melting'; for(const [th,l] of MOODS){ if(G.pleasure<=th){m=l;break} }
  if(G.state!=='intro') plMood.textContent = G.state==='orgasm'?'climaxing' : G.state==='climax'||G.state==='finish'?'full of you' : m;
  dpFill.style.transform=`scaleY(${G.depth.toFixed(3)})`;
  const z=hotZone();
  hotEl.style.bottom=(z.c*100)+'%'; hotEl.style.height=(z.w*100)+'%';
  dpMark.style.bottom=`calc(${(G.depth*100).toFixed(1)}% - 1px)`;
  stamFill.style.transform=`scaleX(${G.stamina/100})`;
  stamVal.textContent=Math.round(G.stamina);
  stamBox.classList.toggle('tired',G.tired);
  stamLbl.textContent=G.tired?'catching breath':'stamina';
  const b=sweetBand(), MAXR=3;
  tBand.style.left=clamp((b.c-b.hw)/MAXR*100,0,96)+'%';
  tBand.style.width=clamp(b.hw*2/MAXR*100,2,100)+'%';
  tMark.style.left=clamp(G.rate/MAXR*100,0,99)+'%';
  if(G.combo>1.05){ comboChip.classList.add('on'); comboB.textContent=G.combo.toFixed(1); }
  else comboChip.classList.remove('on');
  const mn=(G.sesT/60)|0, sc=('0'+((G.sesT%60)|0)).slice(-2);
  clock.textContent=G.round>1?`R${G.round} · ${mn}:${sc}`:`${mn}:${sc}`;
  strk.textContent=G.strokes+(G.strokes===1?' stroke':' strokes');
  btnC.disabled=!(G.sesT>CFG.climaxAt&&(G.state==='play'||G.state==='orgasm'));
  btnC.classList.toggle('ready',G.pleasure>85&&G.state==='play');
  $('btnKiss').classList.toggle('on',G.kissT>0);
  $('btnRub').classList.toggle('on',G.rubT>0);
  const bRl=$('btnRub'), zl='RUB · '+(RUBLBL[rubZoneName()]||'BOTH')+' <kbd>C</kbd>';
  if(bRl._zl!==zl){ bRl._zl=zl; bRl.innerHTML=zl; }
  const bPl=$('btnPos'), pl='POS · '+posName()+' <kbd>Tab</kbd>';
  if(bPl._pl!==pl){ bPl._pl=pl; bPl.innerHTML=pl; }
  $('btnOral').classList.toggle('on',G.oralT>0);
  const bz=$('btnZoom'); if(bz) bz.classList.toggle('on', G.view==='fpv' && G.fpvFocus!=='full');
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
function resize(){
  const w=stage.clientWidth, dpr=Math.min(2,window.devicePixelRatio||1);
  CV.width=w*dpr; CV.height=w*dpr*9/16;
  stage.style.fontSize=(w/1280*10)+'px';
  dragK=1/(w*CFG.dragSens);
  document.querySelectorAll('#plTicks').forEach(()=>{});
}
window.addEventListener('resize',resize); resize();

let last=performance.now();
function tick(ts){
  const dt=clamp((ts-last)/1000,0,.05); last=ts;
  update(dt); draw();
  requestAnimationFrame(tick);
}
requestAnimationFrame(tick);
setMute(muted);
if(lsGet('ag_view')==='fpv'){ G.view='fpv'; document.getElementById('btnView').classList.add('on'); }
// character system — init after DOM + canvas ready
loadCharState();
initCharUI();
