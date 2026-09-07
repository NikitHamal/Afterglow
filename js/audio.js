// Afterglow — module: audio (loaded by index.html)
'use strict';
/* ---------------- audio (organic audio engine: glottal voice + room IR + stereo) ----------------
   OPTIONAL REAL SAMPLES: drop your own licensed files in audio/sfx/<key>N.mp3|.ogg
   (or provide audio/sfx.json as {"key":["audio/sfx/x.mp3",...]}) and the engine plays
   them instead of synthesis. Missing files = pure synthesis, everything still works.
   Keys: moan grunt slap lip hum gag heart breath                                               */
function lsGet(k){ try{ return localStorage.getItem(k); }catch(_){ return null; } }
function lsSet(k,v){ try{ localStorage.setItem(k,v); }catch(_){} }
let AC=null, master=null, bus=null, revIn=null, muted=lsGet('ag_mute')==='1';
function ai(){ if(AC) return;
  AC=new (window.AudioContext||window.webkitAudioContext)();
  const comp=AC.createDynamicsCompressor();
  // Transparent master peak limiter: allows full, loud acoustic presence without digital clipping
  comp.threshold.value=-3; comp.knee.value=4; comp.ratio.value=12; comp.attack.value=.002; comp.release.value=.09;
  master=AC.createGain(); master.gain.value=muted?0:1.0;
  bus=AC.createGain(); bus.gain.value=1.0;
  // Room acoustics: procedural small-bedroom impulse response (convolution) + short predelay
  revIn=AC.createGain();
  const revWet=AC.createGain(); revWet.gain.value=.20;
  try{
    const c=AC.createConvolver(); c.buffer=irBuf();
    const pre=AC.createDelay(1); pre.delayTime.value=.016;
    revIn.connect(pre); pre.connect(c); c.connect(revWet); revWet.connect(master);
  }catch(_){
    // Fallback: feedback comb + lowpass damping
    const dl1=AC.createDelay(1); dl1.delayTime.value=.048;
    const dl2=AC.createDelay(1); dl2.delayTime.value=.079;
    const fb1=AC.createGain(); fb1.gain.value=.30;
    const fb2=AC.createGain(); fb2.gain.value=.20;
    const lp=AC.createBiquadFilter(); lp.type='lowpass'; lp.frequency.value=2400;
    revIn.connect(dl1); dl1.connect(lp); lp.connect(fb1); fb1.connect(dl1);
    revIn.connect(dl2); dl2.connect(lp); lp.connect(fb2); fb2.connect(dl2);
    lp.connect(revWet); revWet.connect(master);
  }
  bus.connect(master); bus.connect(revIn); master.connect(comp); comp.connect(AC.destination);
  sfxInit();
}
// Generated stereo room IR: decaying diffuse noise + a few scattered early reflections
function irBuf(){
  const sr=AC.sampleRate, n=Math.floor(sr*.85), b=AC.createBuffer(2,n,sr);
  for(let c=0;c<2;c++){
    const d=b.getChannelData(c); let lp=0, peak=0;
    for(let i=0;i<n;i++){
      const x=i/sr*1000;
      let v=Math.random()*2-1;
      if(x>8&&x<58){
        const r=(Math.sin(i*.37+c*1.7)+1)*.5;
        if(r>.88) v+=(r-.88)*30*(Math.random()*2-1);
      }
      lp+=.06*(v-lp);
      d[i]=(lp*.62+v*.38)*Math.exp(-x/250);
      if(Math.abs(d[i])>peak) peak=Math.abs(d[i]);
    }
    for(let i=0;i<n;i++) d[i]/=peak;
  }
  return b;
}
let NB=null;
function noise(){ if(!NB){ NB=AC.createBuffer(1,AC.sampleRate*1.5,AC.sampleRate);
  const d=NB.getChannelData(0); for(let i=0;i<d.length;i++)d[i]=R()*2-1;} return NB; }
function env(g,t,a,peak,d){ g.gain.setValueAtTime(.0001,t); g.gain.exponentialRampToValueAtTime(peak,t+a);
  g.gain.exponentialRampToValueAtTime(.0001,t+a+d); }

// Stereo placement: node -> panner -> bus (everything still gets room via bus->revIn)
function route(node,p){
  const has=AC.createStereoPanner;
  if(has&&p){ const pan=AC.createStereoPanner(); pan.pan.value=clamp(p,-1,1);
    node.connect(pan); pan.connect(bus); }
  else node.connect(bus);
}

/* ---- optional real-sample layer (synthesis stays as automatic fallback) ---- */
const SFXDEF={ moan:['audio/sfx/moan1.mp3','audio/sfx/moan1.ogg'],
  grunt:['audio/sfx/grunt1.mp3','audio/sfx/grunt1.ogg'],
  slap:['audio/sfx/slap1.mp3','audio/sfx/slap1.ogg'],
  lip:['audio/sfx/lip1.mp3','audio/sfx/lip1.ogg'],
  hum:['audio/sfx/hum1.mp3','audio/sfx/hum1.ogg'],
  gag:['audio/sfx/gag1.mp3','audio/sfx/gag1.ogg'],
  heart:['audio/sfx/heart1.mp3','audio/sfx/heart1.ogg'],
  breath:['audio/sfx/breath1.mp3','audio/sfx/breath1.ogg'] };
const _sfx={};
function sfxReg(k,files){
  const a=new Audio(); let i=0;
  a.oncanplaythrough=()=>{ _sfx[k]={a,ready:true}; };
  a.onerror=()=>{ if(i<files.length-1){ i++; a.src=files[i]; a.load(); } };
  a.src=files[i]; a.load();
}
function sfxInit(){
  for(const k in SFXDEF) sfxReg(k,SFXDEF[k]);
  if(location.protocol==='http:'||location.protocol==='https:'){
    fetch('audio/sfx.json').then(r=>r.ok?r.json():null).then(m=>{
      if(!m) return; for(const k in m) sfxReg(k,m[k]);
    }).catch(()=>{});
  }
}
function sfxHit(k,vol,rate){
  const s=_sfx[k];
  if(!s||!s.ready) return false;
  try{ s.a.volume=vol; s.a.playbackRate=rate||1; s.a.currentTime=0;
    const p=s.a.play(); if(p&&p.catch) p.catch(()=>{}); return true; }
  catch(_){ return false; }
}

// Natural formant filter for female vocal tract: F0 chest resonance + vowel formants
function naturalFormant(src, f0, vowel='ah'){
  const out=AC.createGain();
  const bp0=AC.createBiquadFilter(); bp0.type='bandpass'; bp0.frequency.value=f0; bp0.Q.value=1.4;
  const g0=AC.createGain(); g0.gain.value=1.4;
  src.connect(bp0); bp0.connect(g0); g0.connect(out);

  const formants = vowel==='oh' ? [[500,4.5,1.8],[880,6,1.2],[2200,8,.6]] :
                   vowel==='oo' ? [[340,4.5,1.9],[820,6,1.0],[2100,8,.5]] :
                   vowel==='eh' ? [[580,5.0,1.7],[1750,7,1.2],[2600,9,.6]] :
                   [[720,4.8,2.0],[1220,6.5,1.4],[2650,9,.7]]; // 'ah'
  formants.forEach(([f,q,g])=>{
    const bp=AC.createBiquadFilter(); bp.type='bandpass'; bp.frequency.value=f; bp.Q.value=q;
    const gg=AC.createGain(); gg.gain.value=g;
    src.connect(bp); bp.connect(gg); gg.connect(out);
  });
  return out;
}

// Glottal-pulse wavetable (saw with a closed phase) — the voiced engine behind moans and hums
let VB=null;
function voiceBuf(){
  if(VB) return VB;
  VB=AC.createBuffer(1,2048,AC.sampleRate);
  const d=VB.getChannelData(0);
  for(let i=0;i<2048;i++){
    const x=i/2048;
    d[i]= x<.88 ? 1-x/.88 : 0;
  }
  return VB;
}
const VCYC=()=>AC.sampleRate/2048;

// Organic female moan / sigh / whimper (warm, loud, clearly audible)
function playMoan(i,o={}){
  if(!AC||muted)return;
  const t=AC.currentTime+(o.at||.012);
  const dur=o.dur??(0.36+i*.56+R()*.22);
  const f0=(o.f0??(225+R()*35))*(o.pmul??(0.94+i*.28));
  const vol=clamp(.44+i*.50,.36,.98)*(o.vol??1);

  if(sfxHit('moan',vol,rr(.94,1.07))){
    if(!o.silent) G.mouths.push({t0:G.t,dur:Math.min(dur,1.1),i});
    return;
  }
  const cyc=VCYC();
  // Voiced glottal pulses (looped wavetable = true harmonic series)
  const v=AC.createBufferSource(); v.buffer=voiceBuf(); v.loop=true;
  v.playbackRate.setValueAtTime(f0*.95/cyc,t);
  v.playbackRate.linearRampToValueAtTime(f0*1.07/cyc,t+dur*.32);
  v.playbackRate.exponentialRampToValueAtTime(f0*.82/cyc,t+dur);
  // Second voice, slightly detuned, carries the vowel morph
  const v2=AC.createBufferSource(); v2.buffer=voiceBuf(); v2.loop=true; v2.detune.value=rr(-7,7);
  v2.playbackRate.setValueAtTime(f0*1.005/cyc,t);
  v2.playbackRate.linearRampToValueAtTime(f0*1.02/cyc,t+dur*.3);
  v2.playbackRate.exponentialRampToValueAtTime(f0*.80/cyc,t+dur);
  // Vibrato (Hz-depth scaled into playback-rate units)
  const vib=AC.createOscillator(), vg=AC.createGain();
  vib.frequency.value=5.3+R()*1.5;
  vg.gain.setValueAtTime(0,t);
  vg.gain.linearRampToValueAtTime(f0*.024/cyc,t+dur*.35);
  vib.connect(vg); vg.connect(v.playbackRate); vg.connect(v2.playbackRate);

  const vowelA= i>0.65?(R()<.5?'ah':'oh'):(R()<.4?'oo':'ah');
  const vowelB= vowelA==='ah'?(R()<.5?'oh':'oo'):'ah';
  const fA=naturalFormant(v,f0,vowelA), fB=naturalFormant(v2,f0*.985,vowelB);
  const gA=AC.createGain(), gB=AC.createGain();
  v.connect(fA); fA.connect(gA); v2.connect(fB); fB.connect(gB);
  gA.gain.setValueAtTime(.0001,t); gA.gain.exponentialRampToValueAtTime(1,t+dur*.3);
  gA.gain.exponentialRampToValueAtTime(.25,t+dur);
  gB.gain.setValueAtTime(.0001,t); gB.gain.linearRampToValueAtTime(.55,t+dur*.35);
  gB.gain.exponentialRampToValueAtTime(.0001,t+dur);
  const mix=AC.createGain(); mix.gain.value=.9;
  gA.connect(mix); gB.connect(mix);
  // Body lowpass keeps the tone warm instead of buzzy
  const body=AC.createBiquadFilter(); body.type='lowpass'; body.frequency.value=2400;
  mix.connect(body);
  const g=AC.createGain(); body.connect(g);
  g.gain.setValueAtTime(.0001,t);
  g.gain.exponentialRampToValueAtTime(vol,t+dur*.20);
  g.gain.setValueAtTime(vol*.92,t+dur*.50);
  g.gain.exponentialRampToValueAtTime(.0001,t+dur);
  // Amplitude shimmer — voices are never perfectly steady
  const tr=AC.createOscillator(), trg=AC.createGain();
  tr.frequency.value=5+R()*1.2; trg.gain.value=vol*.05;
  tr.connect(trg); trg.connect(g.gain);
  route(g,rr(-.12,.12));
  // Breathy whisper texture (stereo pair)
  const nA=AC.createBufferSource(); nA.buffer=noise(); nA.loop=true;
  const nB=AC.createBufferSource(); nB.buffer=noise(); nB.loop=true;
  const nfA=AC.createBiquadFilter(); nfA.type='bandpass'; nfA.frequency.value=1500+i*500; nfA.Q.value=1.2;
  const nfB=AC.createBiquadFilter(); nfB.type='bandpass'; nfB.frequency.value=2100+i*500; nfB.Q.value=1.5;
  const ngA=AC.createGain(); env(ngA,t,.03,vol*.28,dur*.7);
  const ngB=AC.createGain(); env(ngB,t,.05,vol*.22,dur*.65);
  nA.connect(nfA); nfA.connect(ngA); route(ngA,-.35);
  nB.connect(nfB); nfB.connect(ngB); route(ngB,.35);

  const te=t+dur+.1;
  v.start(t); v2.start(t); vib.start(t); tr.start(t); nA.start(t); nB.start(t);
  v.stop(te); v2.stop(te); vib.stop(te); tr.stop(te); nA.stop(te); nB.stop(te);
  if(!o.silent) G.mouths.push({t0:G.t,dur:Math.min(dur,1.1),i});
}

// Low masculine chest breath / grunt (voiced pulse through a dark lowpass)
function playGrunt(){
  if(!AC||muted)return;
  const t=AC.currentTime+.015;
  if(sfxHit('grunt',.55,rr(.92,1.1))) return;
  const f=rr(105,135), cyc=VCYC();
  const v=AC.createBufferSource(); v.buffer=voiceBuf(); v.loop=true;
  v.playbackRate.setValueAtTime(f*1.06/cyc,t);
  v.playbackRate.exponentialRampToValueAtTime(f*.68/cyc,t+.17);
  const flt=AC.createBiquadFilter(); flt.type='lowpass'; flt.frequency.value=520;
  const g=AC.createGain(); env(g,t,.02,.52,.18);
  v.connect(flt); flt.connect(g); route(g,rr(-.2,.2));
  const n=AC.createBufferSource(); n.buffer=noise(); n.loop=true;
  const nf=AC.createBiquadFilter(); nf.type='bandpass'; nf.frequency.value=520; nf.Q.value=1.3;
  const ng=AC.createGain(); env(ng,t,.015,.28,.15);
  n.connect(nf); nf.connect(ng); route(ng,rr(-.35,.35));
  v.start(t); n.start(t); v.stop(t+.24); n.stop(t+.24);
}

// Filtered noise burst, now stereo-aware (pan optional, else random)
function bust(dur,f,fgain,type,q,at,pan){
  const t=AC.currentTime+(at||.008);
  const n=AC.createBufferSource(); n.buffer=noise(); n.loop=true; n.playbackRate.value=rr(.88,1.12);
  const f1=AC.createBiquadFilter(); f1.type=type; f1.frequency.value=f; f1.Q.value=q;
  const g=AC.createGain(); env(g,t,.008,fgain,dur);
  n.connect(f1); f1.connect(g); route(g,pan??rr(-.3,.3)); n.start(t); n.stop(t+dur+.08); return t;
}

// Deep intimate flesh impact (body slapping skin)
function playSlap(v=1){
  if(!AC||muted)return;
  const t=AC.currentTime+.008;
  const vol=clamp(.38+v*.55,.32,.98);
  if(sfxHit('slap',vol,rr(.94,1.06))) return;
  const pan=rr(-.4,.4);
  // Punchy body thump: 135Hz -> 62Hz kick (audible on phones, laptop speakers, and headphones)
  const o=AC.createOscillator(); o.type='sine';
  o.frequency.setValueAtTime(135,t); o.frequency.exponentialRampToValueAtTime(62,t+.085);
  const og=AC.createGain(); env(og,t,.004,vol*1.3,.09);
  o.connect(og); route(og,pan*.5); o.start(t); o.stop(t+.12);
  // Mid flesh impact thud
  bust(.075, 240, vol*1.0, 'lowpass', .85, .005, pan);
  // Wet transient skin slap
  bust(.05, 480, vol*.95, 'bandpass', 1.2, .006, pan);
  bust(.035, 1400, vol*.45, 'bandpass', 1.8, .008, pan*.8);
  // Sharp skin click (fast highpass tick)
  bust(.012, 5200, vol*.30, 'highpass', 1.6, .004, pan);
}

// Warm wet intimacy squelch (internal friction & lubricated slide)
// o.dir: +1 push / -1 pull — resonance sweeps with the stroke
// o.wet: 0 dry skin glide .. 1 soaking — balances suction body vs glossy sizzle
function playSquelch(v=1,o={}){
  if(!AC||muted)return;
  const t=AC.currentTime+.008;
  const vol=clamp(.38+v*.52,.32,.95);
  const dir=o.dir||0, wet=o.wet??1, pan=rr(-.45,.45);
  // Low wet suction body
  const f1=AC.createBiquadFilter(); f1.type='lowpass'; f1.frequency.value=340; f1.Q.value=.9;
  const g1=AC.createGain(); env(g1,t,.01,vol*(.6+.5*wet),.16);
  const n1=AC.createBufferSource(); n1.buffer=noise(); n1.loop=true; n1.playbackRate.value=rr(.85,1.15);
  n1.connect(f1); f1.connect(g1); route(g1,pan);
  // Resonant suction glide — center frequency follows the stroke direction
  const f2=AC.createBiquadFilter(); f2.type='bandpass';
  const base=(1100+R()*350)*(dir<0?1.18:dir>0?.9:1);
  f2.frequency.setValueAtTime(base,t);
  f2.frequency.exponentialRampToValueAtTime(base*(dir<0?1.35:.6),t+.1); f2.Q.value=1.6;
  const g2=AC.createGain(); env(g2,t,.012,vol*(1.05-.45*wet),.1);
  const n2=AC.createBufferSource(); n2.buffer=noise(); n2.loop=true; n2.playbackRate.value=rr(.9,1.1);
  n2.connect(f2); f2.connect(g2); route(g2,pan*.7);
  // Lubricated bubble pops — pitch dives on push, rises on pull
  const pops=1+(R()*2|0);
  for(let k=0;k<pops;k++){
    const pb=AC.createOscillator(); pb.type='sine';
    const f=rr(170,240)*(o.fmul||1);
    const pt=t+.02+R()*.05;
    pb.frequency.setValueAtTime(dir<0?f*.6:f,pt);
    pb.frequency.exponentialRampToValueAtTime(dir<0?f*1.1:f*.45,pt+.06);
    const og=AC.createGain(); env(og,pt,.004,vol*.5,.06);
    pb.connect(og); route(og,pan); pb.start(pt); pb.stop(pt+.08);
  }
  n1.start(t); n2.start(t); n1.stop(t+.2); n2.stop(t+.2);
}
// One knead half-stroke of rubbing: skin slide + wet friction, alternating push/pull.
// Call phase-locked to the hand motion (not randomly) so sound matches the visual.
function playRubStroke(dir,zone){
  if(!AC||muted)return;
  const r=G.rub, low=zone==='low';
  if(r<.12) return;
  if(low){ playSquelch(.5*r+.3,{dir,wet:1});
    if(dir<0) playSlide(.45*r); }
  else { playSlide(.4*r);
    if(dir>0) playSquelch(.35*r+.2,{dir,wet:.55}); }
}

// Sensual slide upon pulling back / retraction
function playSlide(v=1){
  if(!AC||muted)return;
  const vol=clamp(.32+v*.48,.28,.85);
  const pan=rr(-.5,.5);
  bust(.17, 360+R()*60, vol*1.0, 'lowpass', .85, .004, pan);
  bust(.12, 820+R()*150, vol*.55, 'bandpass', 1.4, .015, pan*.7);
  // Skin-gloss friction chirp
  const f2=AC.createBiquadFilter(); f2.type='bandpass'; f2.frequency.value=1200+R()*300; f2.Q.value=2.2;
  const g2=AC.createGain(); env(g2,AC.currentTime+.01,.008,vol*.35,.05);
  const n=AC.createBufferSource(); n.buffer=noise(); n.loop=true;
  n.connect(f2); f2.connect(g2); route(g2,pan);
  n.start(AC.currentTime+.01); n.stop(AC.currentTime+.1);
}

// Gentle intimate breathing (female/male) — rising inhale, falling exhale
function playBreath(male){
  if(!AC||muted)return;
  const t=AC.currentTime+.01;
  if(sfxHit('breath',male?.28:.24,rr(.9,1.1))) return;
  const inD=male?.5:.32, outD=male?.7:.45, pan=rr(-.4,.4);
  const n=AC.createBufferSource(); n.buffer=noise(); n.loop=true; n.playbackRate.value=rr(.9,1.1);
  const bp=AC.createBiquadFilter(); bp.type='bandpass';
  bp.frequency.setValueAtTime(male?380:560,t);
  bp.frequency.linearRampToValueAtTime(male?760:1150,t+inD);
  bp.Q.value=male?1.0:1.3;
  const g=AC.createGain(); env(g,t,.03,male?.30:.26,inD);
  n.connect(bp); bp.connect(g); route(g,pan);
  const n2=AC.createBufferSource(); n2.buffer=noise(); n2.loop=true; n2.playbackRate.value=rr(.9,1.1);
  const bp2=AC.createBiquadFilter(); bp2.type='bandpass';
  const t2=t+inD+.02;
  bp2.frequency.setValueAtTime(male?900:1350,t2);
  bp2.frequency.exponentialRampToValueAtTime(male?420:700,t2+outD);
  bp2.Q.value=male?.9:1.2;
  const g2=AC.createGain(); env(g2,t2,.05,male?.26:.22,outD);
  n2.connect(bp2); bp2.connect(g2); route(g2,-pan);
  n.start(t); n2.start(t2); n.stop(t+inD+.1); n2.stop(t2+outD+.1);
}

function playHeart(){
  if(!AC||muted)return;
  if(sfxHit('heart',.5,1)) return;
  [[0,.38,-.25],[.14,.24,.25]].forEach(([at,v,pa])=>{
    const t=AC.currentTime+at;
    const o=AC.createOscillator(); o.type='sine'; o.frequency.value=58;
    const g=AC.createGain(); env(g,t,.01,v,.12); o.connect(g); route(g,pa); o.start(t); o.stop(t+.18);
  });
}

// Procedural oral sounds (wet slurping, vacuum lip pop, muffled throat hum, deep throat flutter)
function playOralSlurp(v=1){
  if(!AC||muted)return;
  const t=AC.currentTime+.008;
  const vol=clamp(.36+v*.54,.30,.96), pan=rr(-.4,.4);
  // Oral cavity suction — resonance droops as the mouth closes around
  const f1=AC.createBiquadFilter(); f1.type='bandpass';
  f1.frequency.setValueAtTime(560+R()*140,t);
  f1.frequency.exponentialRampToValueAtTime(340+R()*80,t+.16); f1.Q.value=1.6;
  const g1=AC.createGain(); env(g1,t,.01,vol*1.0,.17);
  const n1=AC.createBufferSource(); n1.buffer=noise(); n1.loop=true; n1.playbackRate.value=rr(.9,1.1);
  n1.connect(f1); f1.connect(g1); route(g1,pan);
  // High glossy saliva sizzle
  bust(.11, 1400+R()*400, vol*.70, 'bandpass', 1.7, .015, pan*.7);
  // Tongue glide chirp
  const f2=AC.createBiquadFilter(); f2.type='bandpass'; f2.frequency.value=2200+R()*500; f2.Q.value=2.4;
  const g2=AC.createGain(); env(g2,t+.01,.008,vol*.30,.06);
  const n2=AC.createBufferSource(); n2.buffer=noise(); n2.loop=true;
  n2.connect(f2); f2.connect(g2); route(g2,-pan*.5);
  n1.start(t); n2.start(t+.01); n1.stop(t+.22); n2.stop(t+.1);
}

function playLipPop(){
  if(!AC||muted)return;
  const t=AC.currentTime+.005;
  if(sfxHit('lip',.6,rr(.92,1.08))) return;
  const pan=rr(-.3,.3);
  // Vacuum release pop
  const o=AC.createOscillator(); o.type='sine';
  o.frequency.setValueAtTime(360,t); o.frequency.exponentialRampToValueAtTime(120,t+.045);
  const og=AC.createGain(); env(og,t,.002,.72,.05);
  o.connect(og); route(og,pan); o.start(t); o.stop(t+.06);
  bust(.035, 1850, .45, 'bandpass', 2.2, .003, pan);
  // Tongue snap tick
  bust(.015, 7000, .30, 'highpass', 2, .001, pan);
}

function playThroatHum(){
  if(!AC||muted)return;
  const t=AC.currentTime+.01;
  if(sfxHit('hum',.55,rr(.92,1.08))) return;
  const dur=.38+R()*.25, f=rr(140,185), cyc=VCYC();
  const v=AC.createBufferSource(); v.buffer=voiceBuf(); v.loop=true;
  v.playbackRate.setValueAtTime(f*.95/cyc,t);
  v.playbackRate.linearRampToValueAtTime(f*1.06/cyc,t+dur*.35);
  v.playbackRate.exponentialRampToValueAtTime(f*.88/cyc,t+dur);
  const flt=AC.createBiquadFilter(); flt.type='lowpass'; flt.frequency.value=430;
  const g=AC.createGain(); env(g,t,.04,.55,dur);
  v.connect(flt); flt.connect(g); route(g,rr(-.2,.2));
  const vib=AC.createOscillator(), vg=AC.createGain();
  vib.frequency.value=5.8; vg.gain.value=f*.012/cyc;
  vib.connect(vg); vg.connect(v.playbackRate);
  v.start(t); vib.start(t); v.stop(t+dur+.1); vib.stop(t+dur+.1);
}

function playOralGag(){
  if(!AC||muted)return;
  const t=AC.currentTime+.008;
  if(sfxHit('gag',.6,rr(.95,1.05))) return;
  bust(.12, 320, .65, 'bandpass', 1.1, .005, 0);
  bust(.18, 180, .58, 'lowpass', .9, .01, 0);
  const o=AC.createOscillator(); o.type='sine';
  o.frequency.setValueAtTime(95,t); o.frequency.linearRampToValueAtTime(135,t+.06);
  o.frequency.exponentialRampToValueAtTime(75,t+.14);
  const og=AC.createGain(); env(og,t,.01,.42,.14);
  o.connect(og); route(og,0); o.start(t); o.stop(t+.16);
  // Retch sub-sweep
  const s=AC.createOscillator(); s.type='sine';
  s.frequency.setValueAtTime(88,t); s.frequency.exponentialRampToValueAtTime(52,t+.2);
  const sg=AC.createGain(); env(sg,t,.02,.30,.2);
  s.connect(sg); route(sg,0); s.start(t); s.stop(t+.24);
}

function setMute(m){ muted=m; lsSet('ag_mute',m?'1':'0');
  if(master) master.gain.linearRampToValueAtTime(m?0:1.0,AC.currentTime+.15);
  const el=document.getElementById('icoSnd'); if(el) el.style.opacity=m?.35:1;
}
if(muted){ const _el=document.getElementById('icoSnd'); if(_el) _el.style.opacity='.35'; }

/* ---------------- speech ---------------- */
const bub=document.getElementById('bubble');
function say(txt,dur=2.6){ if(G.sayCd>0) return; G.sayCd=3.2; G.speech={txt,until:G.t+dur}; }
const LINES={
  idle:["hnn… don't be shy…","take your time… I'm yours tonight…","cold tonight? warm me up yourself…"],
  warm:["mm… you're already leaking… ♥","so warm inside…","ah… like that, yes…","slow… feel all of me…"],
  mid:["deeper… put it all in…","ngh! there…","you're stretching me so good…","ah! ah… don't slow down…"],
  high:["I'm close—I'm close…!!","right there, don't stop, don't stop!!","ahH ♥ I can't think straight…","harder… break me… ♥"],
  hot:["THERE!! right there!! ♥♥"],
  org:["cumming—I'm cummiii—♥♥!!","AAh—!! …haa haa… ♥"],
  after:["…mmm. that was… ♥","you're… still hard inside me… ♥","one more? you animal… ♥"],
  tired:["hehe… already out of breath?","rest up… I'm not letting you go yet ♥"],
  keep:["again…? greedy… ♥"],
  cream:["so warm… it's dripping out… ♥","mmhf… you saved that all week for me? ♥"]
};
const pick=a=>a[(R()*a.length)|0];
function tierLine(){
  const a=G.pleasure;
  let arr = a<25?LINES.idle : a<48?LINES.warm : a<72?LINES.mid : LINES.high;
  say(pick(arr));
}
