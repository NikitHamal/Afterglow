// Afterglow — module: mechanics (loaded by index.html)
'use strict';
/* ---------------- mechanics ---------------- */
function sweetBand(){
  const p=sm(.25,.95,G.pleasure/100);
  return { c:lerp(CFG.band[0],CFG.band[1],p)+.12*Math.sin(G.t*.33),
           hw:lerp(CFG.band[2],CFG.band[3],p) };
}
function hotZone(){ return {c:clamp(.30+.30*Math.sin(G.t*.10)+.13*Math.sin(G.t*.041+1.7),.06,.78), w:CFG.hotW}; }

function registerStroke(peak,valley,dur){
  G.strokes++; G.lastStrokeT=G.t; G.strokeFlash=.5;
  const inOrg=G.state==='orgasm';
  const tempo=clamp(1/dur,.4,3.5), band=sweetBand();
  const inBand=Math.abs(tempo-band.c)<=band.hw; if(inBand)G.inBand++;
  const z=hotZone(), hot=peak>=z.c-.04&&peak<=z.c+z.w+.06;
  if(!inOrg){
    let gain=CFG.pleaBase*(.35+.65*peak)*clamp((peak-valley)/.65,.45,1)
            *G.combo*G.sens*(G.kissT?CFG.kissGain:1)*(1.05-G.pleasure/CFG.gainDamp);
    if(hot)gain*=1.75;
    G.pleasure=Math.min(100,G.pleasure+gain);
    if(inBand) G.combo=Math.min(3.2,G.combo+.14+(hot?.12:0));
    else G.combo=Math.max(1,G.combo-.18);
    G.comboBest=Math.max(G.comboBest,G.combo);
  }
  // feedback
  G.rate=G.rate*.5+tempo*.5;
  G.breast.v+=(40+140*peak); G.butt.v+=30*peak; G.impact=Math.min(1,.5+peak); G.nod=1;
  if(peak>.82){ playSlap(peak*G.rate*.4); G.shake=Math.min(3,G.shake+1.2*peak); }
  if(G.pleasure>32&&R()<.5) playSquelch(.6+.5*G.pleasure/100);
  if(hot&&G.hotFlash<=0){ G.hotFlash=1.2; playMoan(.9,{dur:.3,pmul:1.3});
    const sf=document.getElementById('spotFx'); sf.classList.remove('pop'); void sf.offsetWidth; sf.classList.add('pop');
    if(!inOrg&&G.sayCd<=0) say(pick(LINES.hot),1.8);
    for(let i=0;i<3;i++) G.hearts.push({x:660+rr(-10,10),y:500+rr(-6,6),vy:-rr(18,34),ph:R()*TAU,life:1.6,s:rr(.5,.9)});
  } else if(G.pleasure>50&&R()<.3){ playMoan(.3,{dur:.32,pmul:1.15,vol:.5}); }
  if(peak>.8&&R()<.16) playGrunt();
  if(G.pleasure>=100&&!inOrg&&G.state==='play') beginOrgasm();
}
function beginOrgasm(){
  G.state='orgasm'; G.orgT=0; G.orgasms++; G.combo=1;
  G.stamina=Math.min(100,G.stamina+CFG.orgBonus);
  G.shake=6; G.bloom=.5; say(pick(LINES.org),2.4); tierHearts();
  [[0,.9,.5],[.32,1.1,.7],[.6,1.25,.9],[.85,1.35,1],[1.15,1.4,.9],[1.6,1.5,.95],[2.2,1.32,.8],[3.0,1.5,1],[4.0,1.15,.75]]
    .forEach(([at,d,p])=>playMoan(1,{dur:d,pmul:p,at}));
  for(let i=0;i<7;i++) G.hearts.push({x:640+rr(-30,50),y:470+rr(-25,10),vy:-rr(22,44),ph:R()*TAU,life:rr(1.4,2.4),s:rr(.6,1.2)});
}
function tierHearts(){ const el=document.getElementById('orgHearts'); let h='';
  for(let i=0;i<Math.min(G.orgasms,6);i++) h+='<svg viewBox="0 0 24 24"><path d="M12 20.3S3.6 15 1.9 9.9C.7 6.3 3.2 3 6.6 3c2.2 0 3.9 1.3 4.7 2.7h1.4C13.5 4.3 15.2 3 17.4 3c3.4 0 5.9 3.3 4.7 6.9C20.4 15 12 20.3 12 20.3z"/></svg>';
  el.innerHTML=h;
}
function startClimax(){
  G.state='climax'; G.climax=true; G.spurts=0; G.climaxT=0;
  G.kissT=0; G.rubT=0;
  G.sync=(G.state==='orgasm'||G.pleasure>=88||G.orgT<6&&G.orgasms>0);
  document.getElementById('mashTip').style.display='block';
}
function spurt(){
  if(G.state!=='climax')return;
  G.spurts++; G.climaxT=0; playSquelch(1.2); playGrunt();
  G.bloom=Math.max(G.bloom,.14); G.shake=3; G.breast.v+=130;
  for(let i=0;i<2;i++) G.hearts.push({x:655+rr(-8,8),y:500+rr(-5,8),vy:-rr(20,36),ph:R()*TAU,life:1.3,s:rr(.5,.9)});
  if(G.spurts>=CFG.spurtNeed){
    G.state='finish'; G.finishT=0; G.endedShown=false;
    document.getElementById('mashTip').style.display='none';
    G.shake=7; G.bloom=.4; playGrunt(); playGrunt();
    if(G.sync) say('yes—pour it inside… ♥',2.6); else say(pick(LINES.cream),2.6);
  }
}
function endStats(){
  G.endedShown=true;
  const acc=G.strokes?Math.round(100*G.inBand/G.strokes):0;
  let score=G.orgasms*32+G.pleasure/10+acc*.3+Math.min(G.comboBest,3)*6+(G.sync?26:0);
  const grade=score>=86?'S':score>=62?'A':score>=38?'B':'C';
  const note={S:'soulmates, honestly',A:'a perfect night for her',B:'steamy… she almost broke',C:'a quickie for you, a warm-up for her'}[grade];
  const m=(G.sesT/60)|0, s=('0'+((G.sesT%60)|0)).slice(-2);
  document.getElementById('stTime').textContent=`${m}:${s} · round ${G.round}`;
  document.getElementById('stStrokes').textContent=G.strokes;
  document.getElementById('stOrg').textContent=G.orgasms+(G.orgasms?' ♥':'');
  document.getElementById('stCombo').textContent='×'+G.comboBest.toFixed(1);
  document.getElementById('stAcc').textContent=acc+'%';
  document.getElementById('stFin').textContent=G.sync?'together, inside':'rough but happy';
  document.getElementById('grade').textContent=grade;
  document.getElementById('gradeNote').textContent='“'+note+'”';
  document.getElementById('syncBadge').classList.toggle('show',G.sync);
  document.getElementById('endOv').classList.remove('hide');
}

/* ---------------- update ---------------- */
function update(dt){
  G.t+=dt; G.ar=G.pleasure;
  G.sayCd=Math.max(0,G.sayCd-dt); G.shake*=Math.pow(.02,dt); G.bloom*=Math.pow(.05,dt);
  G.viewFade=Math.max(0,G.viewFade-dt*3.2);
  G.impact*=Math.pow(.05,dt); G.nod*=Math.pow(.004,dt); G.strokeFlash=Math.max(0,G.strokeFlash-dt);
  G.hotFlash=Math.max(0,G.hotFlash-dt); G.after=Math.max(0,G.after-dt);
  // springs
  const sp=(s,k,d)=>{s.v+=(-s.p*k-s.v*d)*dt; s.p+=s.v*dt; s.p=clamp(s.p,-16,16)};
  sp(G.breast,240,9); sp(G.butt,300,10);
  G.mouths=G.mouths.filter(m=>G.t-m.t0<m.dur+.2);
  if(G.speech&&G.t>G.speech.until){G.speech=null;}
  // blink
  G.blink-=dt; if(G.blink<-.13)G.blink=rr(1.8,5.5); 
  G.blinkPh=G.blink<0?1-Math.abs(G.blink/.13):0;

  if(G.state==='intro'||G.state==='play'||G.state==='orgasm'){
    if(G.state!=='intro')G.sesT+=dt;
    // eased kiss/rub
    G.kiss=lerp(G.kiss,G.kissT&&G.state==='play'?1:0,1-Math.pow(.002,dt));
    G.rub=lerp(G.rub,G.rubT&&G.state==='play'?1:0,1-Math.pow(.003,dt));
    // SPACE auto-thrust (manual drag overrides while held)
    if(G.tired) G.spaceHeld=false;
    if(G.spaceHeld&&!G.dragOn&&(G.state==='play'||G.state==='orgasm')){
      if(G.state!=='orgasm'){
        G.autoPh+=TAU*sweetBand().c*dt;
        G.target=.55+.38*Math.sin(G.autoPh);
      } else G.spaceHeld=false;
    }
    // depth spring
    if(G.state!=='orgasm'){ G.depth+=(G.target-G.depth)*Math.min(1,dt*16); }
    else G.depth+=((Math.sin(G.t*1.1)*.5+.5)*.5-G.depth)*Math.min(1,dt*6);
    // stroke detect
    const v=(G.depth-G.pDepth)/Math.max(dt,.001); G.pDepth=G.depth;
    G.vel=G.vel*.75+v*.25;
    const sv=G.vel;
    if(sv>0&&G.prevV<=0){ G.valley=G.depth; }
    if(sv>0){ G.maxD=Math.max(G.maxD,G.depth); }
    if(sv<0&&G.prevV>0){
      const peak=G.maxD; G.maxD=G.depth;
      const dur=clamp(G.t-(G.lastStrokeT||G.t-1),.25,4);
      if(peak-G.valley>CFG.strokeMinRange&&(G.state==='play'||G.state==='orgasm')){
        registerStroke(peak,G.valley,dur); G.strokeDur=dur;
      }
    }
    G.prevV=sv;
    G.rate*=Math.pow(G.stall||.45,dt);
    if(G.t-G.lastStrokeT>2.2) G.combo=Math.max(1,G.combo-.14*dt);
    if(G.state==='play'){
      // idle pleasure decay
      if(G.t-G.lastStrokeT>1.8)G.pleasure=Math.max(G.floor,G.pleasure-CFG.pleaDecay*dt);
      // rub
      if(G.rub>.5){ G.pleasure=Math.min(100,G.pleasure+(CFG.rubGain[0]+CFG.rubGain[1]*G.pleasure)*dt); }
      if(G.rub>.15){
        G.breast.v+=G.rub*46*dt*10*Math.sin(G.t*9);
        if(R()<dt*1.4*G.rub) playSquelch(.35*G.rub);
        if(R()<dt*2.2*G.rub) G.hearts.push({x:452+rr(-14,14),y:462+rr(-8,8),vy:-rr(20,36),ph:R()*TAU,life:1.2,s:rr(.4,.7)});
        if(R()<dt*.9*G.rub) playMoan(.25,{dur:.3,pmul:1.2,vol:.45});
      }
      // stamina
      let drain=0;
      if(G.rate>.25){ const band=sweetBand();
        drain=.7+G.rate*CFG.stamDrill*(.3+.7*G.depth);
        if(G.rate>band.c+band.hw)drain+=.9;
      } else drain=-CFG.stamRegen;
      if(G.kiss>.5)drain+=CFG.kissCost; if(G.rub>.5)drain+=CFG.rubCost;
      if(drain>0)G.stamina=Math.max(0,G.stamina-drain*dt);
      else if(!G.tired)G.stamina=Math.min(100,G.stamina-drain*dt);
      if(G.stamina<=0&&!G.tired){ G.tired=true; G.tiredT=0; G.kissT=0;G.rubT=0; G.spaceHeld=false; say(pick(LINES.tired)); }
      // moans / breaths / heart
      scheduleVoice(dt);
    }
    if(G.state==='orgasm'){
      G.orgT+=dt;
      if(G.orgT>1.4)G.pleasure=Math.max(CFG.orgFloor,100-(G.orgT-1.4)/(4)* (100-CFG.orgFloor));
      const bump=Math.sin(Math.min(1,G.orgT/.6)*Math.PI); G.breast.v+=bump*80;
      if(R()<dt*9)G.shake=Math.min(4,G.shake+1.5);
      if(G.orgT>5.2){ G.state='play'; G.after=8; G.floor=Math.max(G.floor,CFG.orgFloor); G.sens+=CFG.sensStep;
        say(pick(LINES.after)); }
    }
  }
  else if(G.state==='climax'){
    G.sesT+=dt; G.climaxT+=dt; G.spurtHold=(G.spurtHold||0);
    G.depth=.78+(G.spurtHold||0)*.16; G.spurtHold=Math.max(0,(G.spurtHold||0)-dt*3);
    G.breast.v+=30*dt*10*Math.sin(G.t*40);
    if(G.climaxT>5)spurt();
    scheduleVoice(dt);
  }
  else if(G.state==='finish'){
    G.sesT+=dt; G.finishT+=dt;
    G.depth=.965+Math.sin(G.t*30)*.012*Math.max(0,1-G.finishT/2.2);
    if(G.finishT<2.2){ G.breast.v+=140*dt*Math.sin(G.t*34); }
    if(G.finishT>1){
      if(G.drips.length<7&&R()<dt*1.4) G.drips.push({p:0,sp:rr(.06,.13),x:648+rr(-6,4),j:rr(0,2)});
    }
    if(G.finishT>3&&!G.endedShown) endStats();
  }
  // particles
  G.sweat.forEach(s=>{s.y+=s.vy*dt; s.life-=dt*.35; s.x+=Math.sin(G.t*2+s.ph)*6*dt;});
  G.sweat=G.sweat.filter(s=>s.life>0);
  const hb=(G.pleasure>52&&G.pleasure<100&&G.state==='play')||G.state==='orgasm';
  if(hb&&G.sweat.length<16&&R()<dt*2.2) G.sweat.push({x:318+rr(-12,16),y:462+rr(-6,2),vy:rr(4,9),ph:R()*TAU,life:1});
  if(G.pleasure>70&&G.sweat.length<16&&R()<dt*2.5) G.sweat.push({x:330+rr(-8,8),y:478+rr(0,4),vy:rr(4,8),ph:R()*TAU,life:1});
  G.hearts.forEach(h=>{h.y+=h.vy*dt;h.life-=dt;h.ph+=dt*3;});
  G.hearts=G.hearts.filter(h=>h.life>0);
  G.drips.forEach(d=>{d.p+=d.sp*dt;});
  G.drips=G.drips.filter(d=>{
    if(d.p>1){ if(G.glisten.length<40)G.glisten.push({x:d.x+rr(-3,3),y:556+rr(-2,4),a:.5});
      if(R()<.3&&G.glisten.length<50)G.glisten.push({x:640+rr(-8,8),y:546+rr(-4,4),a:.4});
      return false; } return true; });
  G.glisten.forEach(g=>g.a=Math.min(g.a,.5));
}
function scheduleVoice(dt){
  G.nextMoan-=dt;
  if(G.nextMoan<=0){
    const a=G.pleasure/100;
    playMoan(.15+a*.85+(G.state==='orgasm'?.2:0),{pmul:G.kiss>.5?1.15:1,vol:G.kiss>.5?.55:1});
    G.nextMoan=lerp(6.5,1.5,a)*(0.6+R()*.8)*(G.state==='orgasm'?.4:1);
    if(G.after>0&&R()<.5)say(pick(LINES.after));
    else if(R()<.55&&G.sayCd<=0)tierLine();
  }
  G.nextBreath-=dt;
  if(G.nextBreath<=0){ playBreath(false); G.nextBreath=1/(lerp(.16,.5,G.pleasure/100)); }
  if(G.tired){ G.tiredT+=dt; if((G.nBreathM=(G.nBreathM||0)-dt)<0){playBreath(true);G.nBreathM=.55;}
    if(G.tiredT>5){G.tired=false;G.stamina=62;} }
  if(G.pleasure>72){ G.nextBeat-=dt;
    if(G.nextBeat<=0){playHeart();G.nextBeat=60/(72+(G.pleasure-72)*1.2+G.orgasms*8);} }
}
