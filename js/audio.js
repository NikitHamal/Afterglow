// Afterglow — module: audio (loaded by index.html)
'use strict';
/* ---------------- audio (all synthesized) ---------------- */
function lsGet(k){ try{ return localStorage.getItem(k); }catch(_){ return null; } }
function lsSet(k,v){ try{ localStorage.setItem(k,v); }catch(_){} }
let AC=null, master=null, bus=null, revIn=null, muted=lsGet('ag_mute')==='1';
function ai(){ if(AC) return;
  AC=new (window.AudioContext||window.webkitAudioContext)();
  const comp=AC.createDynamicsCompressor(); comp.threshold.value=-20; comp.ratio.value=6;
  master=AC.createGain(); master.gain.value=muted?0:.9;
  bus=AC.createGain();
  // tiny room: feedback delay
  revIn=AC.createGain(); const dl=AC.createDelay(1); dl.delayTime.value=.128;
  const fb=AC.createGain(); fb.gain.value=.3; const lp=AC.createBiquadFilter(); lp.type='lowpass'; lp.frequency.value=2400;
  const wet=AC.createGain(); wet.gain.value=.17;
  revIn.connect(dl); dl.connect(lp); lp.connect(fb); fb.connect(dl); lp.connect(wet); wet.connect(master);
  bus.connect(master); bus.connect(revIn); master.connect(comp); comp.connect(AC.destination);
}
let NB=null;
function noise(){ if(!NB){ NB=AC.createBuffer(1,AC.sampleRate*1.2,AC.sampleRate);
  const d=NB.getChannelData(0); for(let i=0;i<d.length;i++)d[i]=R()*2-1;} return NB; }
function env(g,t,a,peak,d){ g.gain.setValueAtTime(.0001,t); g.gain.exponentialRampToValueAtTime(peak,t+a);
  g.gain.exponentialRampToValueAtTime(.0001,t+a+d); }
function formant(src,t,dur,f0,dur2){
  const out=AC.createGain();
  [[660,9,1],[1120,11,.5],[2450,12,.2]].forEach(([f,q,g])=>{
    const bp=AC.createBiquadFilter(); bp.type='bandpass'; bp.frequency.value=f; bp.Q.value=q;
    const gg=AC.createGain(); gg.gain.value=g; src.connect(bp); bp.connect(gg); gg.connect(out);
  });
  return out;
}
function playMoan(i,o={}){ // her voice — formant-synth
  if(!AC||muted)return;
  const t=AC.currentTime+.02, dur=o.dur??(0.4+i*.7+R()*.3);
  const f0=(o.f0??(196+R()*24))*(o.pmul??(0.92+i*.32));
  const o1=AC.createOscillator(),o2=AC.createOscillator();
  o1.type=o2.type='sawtooth'; o2.detune.value=8;
  [o1,o2].forEach(o=>o.frequency.setValueAtTime(f0*.93,t));
  o1.frequency.linearRampToValueAtTime(f0*1.1,t+dur*.35);
  o1.frequency.exponentialRampToValueAtTime(f0*.82,t+dur);
  o2.frequency.setValueAtTime(f0*.95,t); o2.frequency.exponentialRampToValueAtTime(f0*.84,t+dur);
  const vib=AC.createOscillator(),vg=AC.createGain(); vib.frequency.value=5+R()*2; vg.gain.value=f0*.02;
  vib.connect(vg); vg.connect(o1.frequency); vg.connect(o2.frequency);
  const mix=AC.createGain(); mix.gain.value=.16; o1.connect(mix); o2.connect(mix);
  const voce=formant(mix,t,dur,f0); const g=AC.createGain(); voce.connect(g);
  const vol=(.05+i*.12)*(o.vol??1);
  g.gain.setValueAtTime(.0001,t); g.gain.exponentialRampToValueAtTime(vol,t+.08);
  g.gain.setValueAtTime(vol*.8,t+dur*.55); g.gain.exponentialRampToValueAtTime(.0001,t+dur);
  g.connect(bus);
  // breath layer
  const n=AC.createBufferSource(); n.buffer=noise(); n.loop=true;
  const nb=AC.createBiquadFilter(); nb.type='bandpass'; nb.frequency.value=2800; nb.Q.value=.8;
  const ng=AC.createGain(); env(ng,t,.06,vol*.28,dur*.8);
  n.connect(nb); nb.connect(ng); ng.connect(bus);
  o1.start(t);o2.start(t);vib.start(t);n.start(t);
  const te=t+dur+.1; o1.stop(te);o2.stop(te);vib.stop(te);n.stop(te);
  if(!o.silent) G.mouths.push({t0:G.t,dur:Math.min(dur,1.1),i});
}
function playGrunt(){ if(!AC||muted)return; const t=AC.currentTime+.02;
  const o=AC.createOscillator(); o.type='sawtooth';
  const f=rr(92,118); o.frequency.setValueAtTime(f,t); o.frequency.exponentialRampToValueAtTime(f*.75,t+.18);
  const mix=AC.createGain(); mix.gain.value=.35; o.connect(mix);
  const voce=formant(mix,t,.2,f);
  const g=AC.createGain(); voce.connect(g); g.gain.setValueAtTime(.0001,t);
  g.gain.exponentialRampToValueAtTime(.11,t+.03); g.gain.exponentialRampToValueAtTime(.0001,t+.2);
  g.connect(bus); o.start(t); o.stop(t+.25);
}
function bust(dur,f,fgain,type,q,at){ const t=AC.currentTime+(at||.01);
  const n=AC.createBufferSource(); n.buffer=noise(); n.loop=true; n.playbackRate.value=rr(.85,1.15);
  const f1=AC.createBiquadFilter(); f1.type=type; f1.frequency.value=f; f1.Q.value=q;
  const g=AC.createGain(); env(g,t,.01,fgain,dur);
  n.connect(f1); f1.connect(g); g.connect(bus); n.start(t); n.stop(t+dur+.1); return t;
}
function playSlap(v){ if(!AC||muted)return; const vol=clamp(.06+v*.22,0,.3);
  bust(.07,520,vol,'bandpass',.9); bust(.09,230,vol*.9,'lowpass',.7); }
function playSquelch(v=1){ if(!AC||muted)return; bust(.2,389,.1*v,'lowpass',.8); bust(.06,880,.05*v,'bandpass',1.2); }
function playBreath(male){ if(!AC||muted)return;
  bust(male?.8:.5, male?420:740, male?.05:.032,'bandpass',male?.9:1.4); }
function playHeart(){ if(!AC||muted)return;
  [[0,.09],[.15,.055]].forEach(([at,v])=>{ const t=AC.currentTime+at;
    const o=AC.createOscillator(); o.type='sine'; o.frequency.value=56;
    const g=AC.createGain(); env(g,t,.008,v,.11); o.connect(g); g.connect(bus); o.start(t); o.stop(t+.2); });
}
function setMute(m){ muted=m; lsSet('ag_mute',m?'1':'0');
  if(master) master.gain.linearRampToValueAtTime(m?0:.9,AC.currentTime+.2);
  document.getElementById('icoSnd').style.opacity=m?.35:1; }

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
