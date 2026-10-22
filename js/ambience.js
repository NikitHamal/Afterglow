// Afterglow — module: ambience (loaded after audio.js / mechanics.js)
'use strict';
/* ============================================================
   ROOM BED · BED CREAKS · CLIMAX RISER · STINGERS · UI SFX
   All procedural WebAudio. Everything no-ops until ai() has
   created the context (first user gesture), and every entry
   point is safe to call headless / suspended / muted.
   ambStart() is invoked by ai(); ambTick(dt) by the update()
   wrapper at the end of mechanics.js.
   ============================================================ */
const AMB = {
  on: false, riserG: null, riserF: null, padG: null,
  creakCd: 0, prevVel: 0, prevState: '', prevOrg: 0, dropT: 0
};

/* Loopable stereo noise. 'brown' = room rumble, 'pink' = rain hiss.
   The tail is crossfaded into the head and the loop region starts after
   the fade, so the seam is inaudible even on the drifting brown noise. */
function ambNoiseBuf(sec, color){
  const sr = AC.sampleRate, n = (sr * sec) | 0, m = (sr * 0.25) | 0;
  const b = AC.createBuffer(2, n, sr);
  for(let c = 0; c < 2; c++){
    const d = b.getChannelData(c);
    let l = 0, b0 = 0, b1 = 0, b2 = 0;
    for(let i = 0; i < n; i++){
      const w = Math.random() * 2 - 1;
      if(color === 'brown'){ l = (l + 0.02 * w) / 1.02; d[i] = l * 3.5; }
      else { b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913;
        d[i] = (b0 + b1 + b2 + w * 0.1848) * 0.11; }
    }
    for(let i = 0; i < m; i++){ const k = i / m; d[n - m + i] = d[n - m + i] * (1 - k) + d[i] * k; }
  }
  return { buf: b, start: m / sr, end: n / sr };
}
function ambLoop(nb, dest){
  const s = AC.createBufferSource(); s.buffer = nb.buf; s.loop = true;
  s.loopStart = nb.start; s.loopEnd = nb.end;
  s.connect(dest); s.start(AC.currentTime + 0.05, nb.start);
  return s;
}

function ambStart(){
  if(!AC || AMB.on || !ambBus) return;
  AMB.on = true;
  const t = AC.currentTime;

  // ---- room tone: the low, warm air of a closed bedroom at night
  const tone = AC.createGain(); tone.gain.setValueAtTime(0, t); tone.gain.linearRampToValueAtTime(0.05, t + 3);
  const tlp = AC.createBiquadFilter(); tlp.type = 'lowpass'; tlp.frequency.value = 240;
  tlp.connect(tone); tone.connect(ambBus);
  ambLoop(ambNoiseBuf(6, 'brown'), tlp);

  // ---- soft rain on the window (screen-right), breathing in slow gusts
  const rain = AC.createGain(); rain.gain.setValueAtTime(0, t); rain.gain.linearRampToValueAtTime(0.022, t + 4);
  const rhp = AC.createBiquadFilter(); rhp.type = 'highpass'; rhp.frequency.value = 700;
  const rlp = AC.createBiquadFilter(); rlp.type = 'lowpass'; rlp.frequency.value = 5200;
  rhp.connect(rlp); rlp.connect(rain);
  const gust = AC.createOscillator(), gg = AC.createGain();
  gust.frequency.value = 0.06; gg.gain.value = 0.008; gust.connect(gg); gg.connect(rain.gain); gust.start(t);
  if(AC.createStereoPanner){ const p = AC.createStereoPanner(); p.pan.value = 0.45; rain.connect(p); p.connect(ambBus); }
  else rain.connect(ambBus);
  ambLoop(ambNoiseBuf(5, 'pink'), rhp);

  // ---- climax riser: filtered noise swell + a low suspended pad, both silent until she's close
  AMB.riserF = AC.createBiquadFilter(); AMB.riserF.type = 'bandpass'; AMB.riserF.frequency.value = 400; AMB.riserF.Q.value = 1.6;
  AMB.riserG = AC.createGain(); AMB.riserG.gain.value = 0;
  AMB.riserF.connect(AMB.riserG); route(AMB.riserG, 0, bus);
  ambLoop(ambNoiseBuf(4, 'pink'), AMB.riserF);
  AMB.padG = AC.createGain(); AMB.padG.gain.value = 0;
  const padLp = AC.createBiquadFilter(); padLp.type = 'lowpass'; padLp.frequency.value = 900;
  padLp.connect(AMB.padG); route(AMB.padG, 0, bus);
  [[110, -6], [110, 6], [164.8, 0], [220, -3]].forEach(([f, dt]) => {
    const o = AC.createOscillator(); o.type = 'triangle'; o.frequency.value = f; o.detune.value = dt;
    const g = AC.createGain(); g.gain.value = 0.25; o.connect(g); g.connect(padLp); o.start(t);
  });
}

/* glass-tick of a single raindrop on the pane */
function ambDrop(){
  const t = AC.currentTime + 0.01, f = rr(2200, 5200), v = rr(0.004, 0.016);
  const o = AC.createOscillator(); o.type = 'sine';
  o.frequency.setValueAtTime(f, t); o.frequency.exponentialRampToValueAtTime(f * 0.7, t + 0.03);
  const g = AC.createGain(); env(g, t, 0.002, v, 0.035);
  o.connect(g); route(g, rr(0.3, 0.9), ambBus); o.start(t); o.stop(t + 0.06);
}

/* Wooden bed-frame creak: stick-slip friction = a low saw chopped by a fast
   square LFO, pushed through two resonant "plank" bandpasses. */
function playCreak(v){
  if(!AC || muted) return;
  const t = AC.currentTime + 0.01, dur = rr(0.16, 0.3), f = rr(70, 105);
  const o = AC.createOscillator(); o.type = 'sawtooth';
  o.frequency.setValueAtTime(f, t); o.frequency.linearRampToValueAtTime(f * rr(0.8, 1.2), t + dur);
  const chop = AC.createOscillator(); chop.type = 'square'; chop.frequency.value = rr(28, 55);
  const am = AC.createGain(); am.gain.value = 0.5;
  const cg = AC.createGain(); cg.gain.value = 0.5; chop.connect(cg); cg.connect(am.gain);
  const b1 = AC.createBiquadFilter(); b1.type = 'bandpass'; b1.frequency.value = rr(520, 820); b1.Q.value = 7;
  const b2 = AC.createBiquadFilter(); b2.type = 'bandpass'; b2.frequency.value = rr(1200, 1700); b2.Q.value = 9;
  const g = AC.createGain(); const vol = 0.11 * v;
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(vol, t + dur * 0.35);
  g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
  o.connect(am); am.connect(b1); am.connect(b2); b1.connect(g); b2.connect(g);
  route(g, scrPan('body') + 0.15, bus);
  o.start(t); chop.start(t); o.stop(t + dur + 0.05); chop.stop(t + dur + 0.05);
}

/* chord bloom with a long, soft decay — used as orgasm / finish stingers */
function ambChord(freqs, peak, atk, dec, type){
  const t = AC.currentTime + 0.02;
  const g = AC.createGain(); g.gain.setValueAtTime(0.0001, t);
  g.gain.exponentialRampToValueAtTime(peak, t + atk); g.gain.exponentialRampToValueAtTime(0.0001, t + atk + dec);
  const lp = AC.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.setValueAtTime(5000, t);
  lp.frequency.exponentialRampToValueAtTime(900, t + atk + dec);
  lp.connect(g); route(g, 0, bus);
  freqs.forEach((f, i) => {
    [-5, 5].forEach(d => {
      const o = AC.createOscillator(); o.type = type || 'sine'; o.frequency.value = f; o.detune.value = d + rr(-2, 2);
      const og = AC.createGain(); og.gain.value = 1 / (1 + i * 0.6);
      o.connect(og); og.connect(lp); o.start(t); o.stop(t + atk + dec + 0.1);
    });
  });
}
function ambSubDrop(v){
  const t = AC.currentTime + 0.01;
  const o = AC.createOscillator(); o.type = 'sine';
  o.frequency.setValueAtTime(82, t); o.frequency.exponentialRampToValueAtTime(34, t + 0.9);
  const g = AC.createGain(); env(g, t, 0.01, v, 0.9);
  o.connect(g); route(g, 0, bus); o.start(t); o.stop(t + 1);
}
function ambOrgasmStinger(){
  if(!AC || muted) return;
  ambSubDrop(0.32);
  ambChord([220, 277.18, 329.63, 440, 554.37], 0.05, 0.05, 3.6);
  bust(0.9, 2400, 0.08, 'highpass', 0.7, 0.01, 0);   // airy release
}
function ambFinishStinger(){
  if(!AC || muted) return;
  ambSubDrop(0.22);
  ambChord([196, 246.94, 293.66, 392], 0.045, 0.35, 4.2, 'triangle');
}

function ambTick(dt){
  if(!AC || !AMB.on) return;
  const t = AC.currentTime, st = G.state;

  // raindrops on the glass, a few a second
  if(!muted && R() < dt * 5) ambDrop();

  // bed creaks on every direction change of a real thrust
  AMB.creakCd -= dt;
  const v = G.vel || 0;
  if((st === 'play' || st === 'orgasm') && !G.paused && AMB.creakCd <= 0 &&
     Math.sign(v) !== Math.sign(AMB.prevVel) && Math.abs(AMB.prevVel) > 0.9){
    const k = clamp(Math.abs(AMB.prevVel) / 3.2, 0.25, 1) * (G.oral > 0.5 ? 0.4 : 1);
    if(R() < 0.75) playCreak(k);
    AMB.creakCd = 0.2;
  }
  AMB.prevVel = v;

  // riser: swells from 78% pleasure to the edge, filter opening as she climbs
  let x = 0;
  if(st === 'play') x = clamp(((G.pleasure || 0) - 78) / 22, 0, 1);
  else if(st === 'climax') x = 0.55;
  const tc = x > 0 ? 0.35 : 0.08;
  AMB.riserG.gain.setTargetAtTime(0.085 * x * x, t, tc);
  AMB.riserF.frequency.setTargetAtTime(400 + 2600 * x, t, 0.4);
  AMB.padG.gain.setTargetAtTime(0.03 * x, t, tc);

  // release stingers on state edges
  if((G.orgasms || 0) > AMB.prevOrg){
    AMB.riserG.gain.cancelScheduledValues(t); AMB.riserG.gain.setValueAtTime(0, t);
    AMB.padG.gain.cancelScheduledValues(t); AMB.padG.gain.setValueAtTime(0, t);
    ambOrgasmStinger();
  }
  AMB.prevOrg = G.orgasms || 0;
  if(st === 'finish' && AMB.prevState !== 'finish') ambFinishStinger();
  AMB.prevState = st;
}

/* ---------------- interface sounds (dry, no room) ---------------- */
function uiOk(){ return AC && !muted && uiBus && AC.state === 'running'; }
function uiTick(){
  if(!uiOk()) return;
  const t = AC.currentTime + 0.005;
  const o = AC.createOscillator(); o.type = 'sine'; o.frequency.value = rr(2300, 2500);
  const g = AC.createGain(); env(g, t, 0.002, 0.05, 0.02);
  o.connect(g); g.connect(uiBus); o.start(t); o.stop(t + 0.04);
}
function uiClick(){
  if(!uiOk()) return;
  const t = AC.currentTime + 0.005;
  const o = AC.createOscillator(); o.type = 'triangle';
  o.frequency.setValueAtTime(1300, t); o.frequency.exponentialRampToValueAtTime(700, t + 0.04);
  const g = AC.createGain(); env(g, t, 0.002, 0.16, 0.05);
  o.connect(g); g.connect(uiBus); o.start(t); o.stop(t + 0.08);
  const n = AC.createBufferSource(); n.buffer = noise();
  const hp = AC.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = 4000;
  const ng = AC.createGain(); env(ng, t, 0.001, 0.05, 0.012);
  n.connect(hp); hp.connect(ng); ng.connect(uiBus); n.start(t); n.stop(t + 0.03);
}
function uiWhoosh(){
  if(!uiOk()) return;
  const t = AC.currentTime + 0.01, d = 0.5;
  const n = AC.createBufferSource(); n.buffer = noise(); n.loop = true;
  const bp = AC.createBiquadFilter(); bp.type = 'bandpass'; bp.Q.value = 1.4;
  bp.frequency.setValueAtTime(300, t); bp.frequency.exponentialRampToValueAtTime(2200, t + d * 0.45);
  bp.frequency.exponentialRampToValueAtTime(500, t + d);
  const g = AC.createGain(); g.gain.setValueAtTime(0.0001, t);
  g.gain.exponentialRampToValueAtTime(0.22, t + d * 0.45); g.gain.exponentialRampToValueAtTime(0.0001, t + d);
  n.connect(bp); bp.connect(g);
  if(AC.createStereoPanner){ const p = AC.createStereoPanner();
    p.pan.setValueAtTime(-0.7, t); p.pan.linearRampToValueAtTime(0.7, t + d); g.connect(p); p.connect(uiBus); }
  else g.connect(uiBus);
  n.start(t); n.stop(t + d + 0.05);
}
/* bell chime; `step` walks up a major pentatonic so each combo tier sings higher */
function uiChime(step){
  if(!uiOk()) return;
  const PENTA = [0, 2, 4, 7, 9, 12, 14, 16, 19];
  const f = 587.33 * Math.pow(2, PENTA[clamp(step | 0, 0, PENTA.length - 1)] / 12);
  const t = AC.currentTime + 0.01;
  [[1, 0.09, 0.9], [2.76, 0.03, 0.4], [5.4, 0.012, 0.2]].forEach(([m, v, d]) => {
    const o = AC.createOscillator(); o.type = 'sine'; o.frequency.value = f * m;
    const g = AC.createGain(); env(g, t, 0.003, v, d);
    o.connect(g); g.connect(uiBus); if(revIn) g.connect(revIn); o.start(t); o.stop(t + d + 0.05);
  });
}
function uiSparkle(){
  if(!uiOk()) return;
  for(let i = 0; i < 5; i++){
    const t = AC.currentTime + 0.01 + i * 0.028, f = rr(2600, 5200);
    const o = AC.createOscillator(); o.type = 'sine'; o.frequency.value = f;
    const g = AC.createGain(); env(g, t, 0.002, 0.045 * (1 - i * 0.12), 0.12);
    const p = AC.createStereoPanner ? AC.createStereoPanner() : null;
    if(p){ p.pan.value = rr(-0.6, 0.6); o.connect(g); g.connect(p); p.connect(uiBus); if(revIn) p.connect(revIn); }
    else { o.connect(g); g.connect(uiBus); }
    o.start(t); o.stop(t + 0.16);
  }
}
if(typeof window !== 'undefined'){
  Object.assign(window, { ambStart, ambTick, playCreak, uiTick, uiClick, uiWhoosh, uiChime, uiSparkle });
}
