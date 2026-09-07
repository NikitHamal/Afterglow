// Afterglow — module: mechanics (loaded by index.html)
// Ultra-High-Fidelity Biomechanical Engine & Physiological Simulation
'use strict';

/* ============================================================
   NEUROLOGICAL & BIOMECHANICAL SWEET SPOTS
   ============================================================ */

/**
 * Calculates the neurological cadence resonance band ("sweet tempo").
 * As arousal and pleasure climb, her nervous system craves tighter,
 * more rhythmic, and steadily accelerating cadence.
 */
function sweetBand(){
  const p = sm(0.15, 0.95, (G.pleasure || 0) / 100);
  const arousalMod = (G.ar || 0) / 100 * 0.22;
  // Dynamic breathing resonance drift
  const bioDrift = Math.sin((G.t || 0) * 0.28) * 0.14 + Math.cos((G.t || 0) * 0.08) * 0.06;
  const center = lerp(CFG.band[0], CFG.band[1], p) + arousalMod + bioDrift;
  const halfWidth = lerp(CFG.band[2], CFG.band[3], p) * (1 - p * 0.25); // band narrows at high pleasure (requires precision)
  return { c: center, hw: Math.max(0.12, halfWidth) };
}

/**
 * Wandering anatomical erogenous hot zone:
 * Alternates naturally between Anterior-Wall stimulation (G-spot, ~0.45–0.68)
 * and deep Cervical/Fornix bottoming (~0.78–0.96).
 */
function hotZone(){
  const t = G.t || 0;
  // Dual-frequency orbital drift representing anatomical shift and pelvic tilting
  const gSpotCycle = Math.sin(t * 0.09) * 0.22;
  const fornixCycle = Math.sin(t * 0.035 + 1.8) * 0.18;
  const center = clamp(0.56 + gSpotCycle + fornixCycle, 0.32, 0.90);
  const width = (CFG.hotW || 0.18) * (1 + ((G.ar || 0) / 100) * 0.35); // hot zone expands as tissue engorges
  return { c: center, w: width };
}

/* ============================================================
   PHYSICAL STROKE IMPACT & KINEMATICS RESOLUTION
   ============================================================ */

function registerStroke(peak, valley, dur){
  G.strokes = (G.strokes || 0) + 1;
  G.lastStrokeT = G.t;
  G.strokeFlash = 0.55;

  const inOrg = G.state === 'orgasm';
  const tempo = clamp(1 / Math.max(0.001, dur), 0.35, 4.0);
  const band = sweetBand();
  const inBand = Math.abs(tempo - band.c) <= band.hw;
  if(inBand) G.inBand = (G.inBand || 0) + 1;

  const strokeRange = peak - valley;
  const z = hotZone();
  // Hot zone hit detection: stroke peak lands inside or passes cleanly through the erogenous zone
  const hitHot = (peak >= z.c - z.w * 0.5 && peak <= z.c + z.w * 0.5) ||
                 (valley <= z.c && peak >= z.c);

  // Dynamic vaginal / oral lubrication & tissue engorgement
  G.lube = clamp((G.lube || 0.35) + 0.025 + (G.ar / 100) * 0.04, 0.2, 1.0);
  G.engorgement = clamp((G.engorgement || 0.1) + 0.018 * (peak + 0.2), 0, 1.0);

  // --- Pleasure Gain Curve (Nonlinear Plateau & Edging Dynamics) ---
  if(!inOrg && G.state === 'play'){
    // Base sensory drive: deep strokes stimulate fornix, shallow tease clitoris/vestibule
    const depthCurve = 0.40 + 0.60 * Math.pow(peak, 1.35);
    const displacementFactor = clamp(strokeRange / 0.60, 0.45, 1.25);
    
    // Cadence resonance multiplier
    const tempoAccuracy = 1 - clamp(Math.abs(tempo - band.c) / (band.hw * 1.5), 0, 1);
    const resonanceBonus = 1.0 + tempoAccuracy * 0.65;

    // Kiss & intimacy amplifier
    const intimacyMul = (G.kiss > 0.4 ? (CFG.kissGain || 1.35) : 1.0);

    // Plateau resistance curve: pleasure builds rapidly early, demands precision rhythm near climax
    const plateauTension = 1.08 - Math.pow(G.pleasure / (CFG.gainDamp || 115), 1.8);

    let gain = (CFG.pleaBase || 3.2) * depthCurve * displacementFactor 
             * (G.combo || 1) * (G.sens || 1) * intimacyMul * plateauTension * resonanceBonus;

    if(hitHot) {
      gain *= 1.85;
      G.shaftPulse = Math.min(1.4, (G.shaftPulse || 0) + 0.45); // involuntary pelvic spasm response
    }

    // Oral stimulation dynamics
    if(G.oral > 0.4) gain *= 1.18;

    G.pleasure = clamp(G.pleasure + gain, G.floor || 0, 100);

    // Combo streak scaling
    if(inBand){
      G.combo = Math.min(3.8, (G.combo || 1) + 0.16 + (hitHot ? 0.14 : 0));
    } else {
      G.combo = Math.max(1.0, (G.combo || 1) - 0.22);
    }
    G.comboBest = Math.max(G.comboBest || 1, G.combo);
  }

  // --- Physical Momentum & Multi-Phase Tissue Spring Oscillation ---
  G.rate = (G.rate || 0) * 0.45 + tempo * 0.55;
  const strokeMomentum = peak * clamp(tempo / 1.8, 0.6, 2.2);

  // Breast heave & transverse secondary ripple
  G.breast.v += (55 + 160 * strokeMomentum);
  // Buttock pelvic impact rebound
  G.butt.v += (42 + 95 * strokeMomentum);

  // Physical impact shockwave (pelvic collision at deep thrust)
  G.impact = Math.min(1.2, 0.45 + peak * 0.75);
  G.nod = Math.min(1.2, (G.nod || 0) + 0.55 * peak);

  // --- Dynamic Audio & Haptic Synthesis ---
  if(G.oral > 0.5){
    // Oral mechanics & gag acoustics
    if(typeof playOralSlurp === 'function') {
      playOralSlurp(clamp(peak * 1.35 * (G.lube || 0.8), 0.4, 1.4));
    }
    if(peak > 0.72){
      if(typeof playThroatHum === 'function') playThroatHum();
      if(R() < 0.40 && typeof playOralGag === 'function') {
        playOralGag();
        G.shaftPulse = Math.min(1.5, (G.shaftPulse || 0) + 0.6); // tight esophageal clamp
      }
    }
    if(R() < 0.32 && typeof playGrunt === 'function') playGrunt();
  } else {
    // Vaginal intercourse acoustics
    if(peak > 0.68){
      // Deep pelvic collision clap
      const slapVolume = clamp((peak - 0.45) * 1.5 * ((G.rate || 1) * 0.55), 0.35, 1.0);
      if(typeof playSlap === 'function') playSlap(slapVolume);
      G.shake = Math.min(3.6, (G.shake || 0) + 1.35 * peak);
    }
    if(peak > 0.28 && typeof playSquelch === 'function'){
      // Viscous hydrothermal suction squelch
      const squelchIntensity = 0.42 + 0.75 * ((G.pleasure || 0) / 100) * (G.lube || 0.8) * peak;
      playSquelch(squelchIntensity);
    }

    // Erogenous zone vocalizations
    if(hitHot && (G.hotFlash || 0) <= 0){
      G.hotFlash = 1.35;
      if(typeof playMoan === 'function') playMoan(0.96, { dur: 0.42, pmul: 1.32 });
      const sf = document.getElementById('spotFx');
      if(sf){ sf.classList.remove('pop'); void sf.offsetWidth; sf.classList.add('pop'); }
      if(!inOrg && (G.sayCd || 0) <= 0 && typeof say === 'function' && Array.isArray(LINES.hot)) {
        say(pick(LINES.hot), 2.0);
      }
      for(let i = 0; i < 4; i++){
        G.hearts.push({
          x: 660 + rr(-12, 12),
          y: 500 + rr(-8, 8),
          vy: -rr(20, 38),
          ph: R() * TAU,
          life: 1.6,
          s: rr(0.5, 0.95)
        });
      }
    } else if(G.pleasure > 36 && R() < 0.48 && typeof playMoan === 'function'){
      playMoan(0.44 + (G.pleasure / 100) * 0.4, { dur: 0.36, pmul: 1.15 });
    }
    if(peak > 0.82 && R() < 0.26 && typeof playGrunt === 'function') playGrunt();
  }

  // --- Climax Threshold Check ---
  if(G.pleasure >= 100 && !inOrg && G.state === 'play'){
    beginOrgasm();
  }
}

/* ============================================================
   ORGASMIC CASCADE & CLIMAX STATE
   ============================================================ */

function beginOrgasm(){
  G.state = 'orgasm';
  G.orgT = 0;
  G.orgasms = (G.orgasms || 0) + 1;
  G.combo = 1.0;
  G.stamina = Math.min(100, (G.stamina || 0) + (CFG.orgBonus || 24));
  G.shake = 6.5;
  G.bloom = 0.65;
  G.shaftPulse = 1.6; // strong initial clench

  if(typeof say === 'function' && Array.isArray(LINES.org)) {
    say(pick(LINES.org), 2.8);
  }
  if(R()<0.18 && typeof sfxVoice === 'function') sfxVoice('love');
  tierHearts();

  // Multi-phase syncopated ecstatic vocal cascade
  const vocalScore = [
    [0.00, 0.95, 0.55],
    [0.30, 1.15, 0.75],
    [0.58, 1.30, 0.92],
    [0.82, 1.40, 1.05],
    [1.12, 1.45, 0.95],
    [1.55, 1.55, 1.00],
    [2.15, 1.38, 0.85],
    [2.90, 1.50, 1.02],
    [3.90, 1.20, 0.78]
  ];
  vocalScore.forEach(([at, dur, pitch]) => {
    if(typeof playMoan === 'function') playMoan(1.0, { dur, pmul: pitch, at });
  });

  // Climax particle cloud
  for(let i = 0; i < 9; i++){
    G.hearts.push({
      x: 640 + rr(-40, 50),
      y: 470 + rr(-30, 15),
      vy: -rr(24, 48),
      ph: R() * TAU,
      life: rr(1.6, 2.8),
      s: rr(0.65, 1.35)
    });
  }
}

function tierHearts(){
  const el = document.getElementById('orgHearts');
  if(!el) return;
  let h = '';
  for(let i = 0; i < Math.min(G.orgasms || 0, 6); i++){
    h += '<svg viewBox="0 0 24 24"><path d="M12 20.3S3.6 15 1.9 9.9C.7 6.3 3.2 3 6.6 3c2.2 0 3.9 1.3 4.7 2.7h1.4C13.5 4.3 15.2 3 17.4 3c3.4 0 5.9 3.3 4.7 6.9C20.4 15 12 20.3 12 20.3z"/></svg>';
  }
  el.innerHTML = h;
}

function startClimax(){
  G.state = 'climax';
  G.climax = true;
  G.spurts = 0;
  G.climaxT = 0;
  G.kissT = 0;
  G.rubT = 0;
  G.oralT = 0;
  // Simultaneous orgasm sync check: player ejaculates while she is in climax or highly primed
  G.sync = (G.state === 'orgasm' || (G.pleasure || 0) >= 86 || ((G.orgT || 99) < 6.0 && (G.orgasms || 0) > 0));

  const tip = document.getElementById('mashTip');
  if(tip) tip.style.display = 'block';
}

function spurt(){
  if(G.state !== 'climax') return;
  G.spurts = (G.spurts || 0) + 1;
  G.climaxT = 0;

  if(typeof playSquelch === 'function') playSquelch(1.35);
  if(typeof playCum === 'function') playCum(); // cum in/out foley (swallow if oral)
  if(typeof playGrunt === 'function') playGrunt();
  if(typeof playMoan === 'function'){
    playMoan(0.92, { dur: 0.58, pmul: 1.20 + G.spurts * 0.03 });
  }

  G.bloom = Math.max(G.bloom || 0, 0.35);
  G.shake = 4.2;
  G.breast.v += 145;
  G.butt.v += 90;
  G.nod = 1.2;
  G.shaftPulse = 1.3;

  if(typeof spawnJets === 'function') spawnJets();

  const dp = typeof poseFluidOrigin === 'function' ? poseFluidOrigin() : [640, 500];
  for(let i = 0; i < 3; i++){
    if((G.drips || []).length < 16){
      G.drips.push({ p: 0, sp: rr(0.09, 0.22), x: dp[0] + rr(-6, 6), j: rr(0, 2), ox: dp[0], oy: dp[1] });
    }
  }
  for(let i = 0; i < 3; i++){
    G.hearts.push({
      x: 655 + rr(-10, 10),
      y: 500 + rr(-6, 8),
      vy: -rr(22, 40),
      ph: R() * TAU,
      life: 1.4,
      s: rr(0.55, 1.05)
    });
  }

  if(G.spurts >= (CFG.spurtNeed || 4)){
    G.state = 'finish';
    G.finishT = 0;
    G.endedShown = false;
    const tip = document.getElementById('mashTip');
    if(tip) tip.style.display = 'none';

    G.shake = 7.5;
    G.bloom = 0.55;
    if(typeof playGrunt === 'function') { playGrunt(); playGrunt(); }

    if(G.sync){
      if(typeof say === 'function') say('yes—pour it inside… together… ♥', 3.0);
    } else {
      if(typeof say === 'function' && Array.isArray(LINES.cream)) say(pick(LINES.cream), 2.8);
    }
    if(R()<0.15 && typeof sfxVoice === 'function') sfxVoice('love');
  }
}

function nextRound(){
  Object.assign(G, {
    state: 'play',
    climax: false,
    spurts: 0,
    climaxT: 0,
    endedShown: false,
    finishT: 0,
    stamina: 100,
    tired: false,
    pleasure: Math.max(G.pleasure || 0, 54),
    sync: false,
    round: (G.round || 1) + 1,
    floor: Math.max(G.floor || 0, (CFG.orgFloor || 32))
  });
  if(G.drips) G.drips.length = 0;
  if(G.jets) G.jets.length = 0;
  if(G.glisten) G.glisten.length = Math.min(G.glisten.length, 8);

  const tip = document.getElementById('mashTip');
  if(tip) tip.style.display = 'none';
  if(typeof say === 'function' && Array.isArray(LINES.keep)) say(pick(LINES.keep));
}

function endStats(){
  G.endedShown = true;
  const acc = G.strokes ? Math.round(100 * (G.inBand || 0) / G.strokes) : 0;
  let score = (G.orgasms || 0) * 35 + (G.pleasure || 0) * 0.12 + acc * 0.32 + Math.min(G.comboBest || 1, 3.8) * 6 + (G.sync ? 28 : 0);
  const grade = score >= 90 ? 'S' : score >= 65 ? 'A' : score >= 40 ? 'B' : 'C';
  const notes = {
    S: 'soulmates, honestly — breathless perfection',
    A: 'a deeply intense night for her',
    B: 'steamy… she almost completely lost control',
    C: 'a quick thrill for you, a warm-up for her'
  };

  const m = ((G.sesT || 0) / 60) | 0;
  const s = ('0' + (((G.sesT || 0) % 60) | 0)).slice(-2);
  const setEl = (id, val) => { const el = document.getElementById(id); if(el) el.textContent = val; };

  setEl('stTime', `${m}:${s} · round ${G.round || 1}`);
  setEl('stStrokes', G.strokes || 0);
  setEl('stOrg', (G.orgasms || 0) + (G.orgasms ? ' ♥' : ''));
  setEl('stCombo', '×' + (G.comboBest || 1).toFixed(1));
  setEl('stAcc', acc + '%');
  setEl('stFin', G.sync ? 'simultaneous climax, deep inside' : 'intense finish');
  setEl('grade', grade);
  setEl('gradeNote', '“' + (notes[grade] || '') + '”');

  const sb = document.getElementById('syncBadge');
  if(sb) sb.classList.toggle('show', !!G.sync);
  const ov = document.getElementById('endOv');
  if(ov) ov.classList.remove('hide');
}

/* ============================================================
   CONTINUOUS SIMULATION LOOP (dt INTEGRATION)
   ============================================================ */

function update(dt){
  const t = (G.t += dt);
  G.ar = G.pleasure || 0;

  // Linear decay timers
  G.sayCd = Math.max(0, (G.sayCd || 0) - dt);
  G.shake = (G.shake || 0) * Math.pow(0.015, dt);
  G.bloom = (G.bloom || 0) * Math.pow(0.04, dt);
  G.viewFade = Math.max(0, (G.viewFade || 0) - dt * 3.4);
  G.impact = (G.impact || 0) * Math.pow(0.04, dt);
  G.nod = (G.nod || 0) * Math.pow(0.003, dt);
  G.strokeFlash = Math.max(0, (G.strokeFlash || 0) - dt);
  G.hotFlash = Math.max(0, (G.hotFlash || 0) - dt);
  G.after = Math.max(0, (G.after || 0) - dt);

  // Involuntary pelvic floor pulse relaxation
  G.shaftPulse = Math.max(0, (G.shaftPulse || 0) - dt * 1.6);

  // Natural lubrication evaporation/absorption vs arousal maintenance
  G.lube = clamp((G.lube || 0.4) - dt * 0.012 + ((G.ar || 0) / 100) * dt * 0.018, 0.2, 1.0);

  // --- Dual-Phase Second-Order Damped Springs (Breasts & Butt) ---
  const integrateSpring = (s, k, d) => {
    s.v += (-s.p * k - s.v * d) * dt;
    s.p += s.v * dt;
    s.p = clamp(s.p, -18, 18);
  };
  integrateSpring(G.breast, 250, 8.8);
  integrateSpring(G.butt, 310, 9.8);

  // Cleanup completed facial phonemes
  if(Array.isArray(G.mouths)){
    G.mouths = G.mouths.filter(m => t - m.t0 < m.dur + 0.2);
  }
  if(G.speech && t > G.speech.until){ G.speech = null; }

  // Autonomic eye blinking cycle
  G.blink = (G.blink || 2.5) - dt;
  if(G.blink < -0.14) G.blink = rr(2.0, 5.8);
  G.blinkPh = G.blink < 0 ? 1 - Math.abs(G.blink / 0.14) : 0;

  // --- Main Interactive Play States ---
  if(G.state === 'intro' || G.state === 'play' || G.state === 'orgasm'){
    if(G.state !== 'intro') G.sesT = (G.sesT || 0) + dt;

    // Smooth mode transitions
    G.kiss = lerp(G.kiss || 0, G.kissT && G.state === 'play' ? 1 : 0, 1 - Math.pow(0.002, dt));
    G.rub = lerp(G.rub || 0, G.rubT && G.state === 'play' ? 1 : 0, 1 - Math.pow(0.003, dt));
    G.oral = lerp(G.oral || 0, G.oralT && G.state === 'play' ? 1 : 0, 1 - Math.pow(0.004, dt));

    // SPACE Auto-Thrust harmonic drive
    if(G.tired) G.spaceHeld = false;
    if(G.spaceHeld && !G.dragOn && (G.state === 'play' || G.state === 'orgasm')){
      if(G.state !== 'orgasm'){
        const band = sweetBand();
        G.autoPh = (G.autoPh || 0) + TAU * band.c * dt;
        // Asymmetrical stroke profile: faster insertion, slightly slower withdrawal
        const rawWave = Math.sin(G.autoPh);
        const wave = rawWave > 0 ? Math.pow(rawWave, 0.85) : -Math.pow(-rawWave, 1.15);
        G.target = 0.54 + 0.40 * wave;
      } else {
        G.spaceHeld = false;
      }
    }

    // Depth Spring: Viscous hydrodynamics and flesh elastic resistance
    const targetDepth = G.target !== undefined ? G.target : 0;
    if(G.state !== 'orgasm'){
      // Tissue resistance steepens near full penetration (0.85+)
      const elasticResistance = targetDepth > 0.82 ? 1.0 - (targetDepth - 0.82) * 0.45 : 1.0;
      const springRate = Math.min(1.0, dt * 17.5 * elasticResistance);
      G.depth = (G.depth || 0) + (targetDepth - G.depth) * springRate;
    } else {
      // Involuntary shivering during orgasm
      const orgSpasm = (Math.sin(t * 12) * 0.5 + 0.5) * 0.15 * Math.sin(Math.PI * clamp((G.orgT || 0) / 5.2, 0, 1));
      const orgDrift = (Math.sin(t * 1.1) * 0.5 + 0.5) * 0.45 + orgSpasm;
      G.depth = (G.depth || 0) + (orgDrift - G.depth) * Math.min(1.0, dt * 6.5);
    }

    // Kinematic Velocity & Inflection Stroke Detector
    const v = (G.depth - (G.pDepth || G.depth)) / Math.max(dt, 0.001);
    G.pDepth = G.depth;
    G.vel = (G.vel || 0) * 0.72 + v * 0.28;
    const sv = G.vel;

    // Valley detection (reversal from pull-back to thrust forward)
    if(sv > 0 && (G.prevV || 0) <= 0){
      G.valley = G.depth;
    }
    if(sv > 0){
      G.maxD = Math.max(G.maxD || 0, G.depth);
    }
    // Peak detection (reversal from insertion to withdrawal)
    if(sv < 0 && (G.prevV || 0) > 0){
      const peak = G.maxD || G.depth;
      G.maxD = G.depth;
      const dur = clamp(t - (G.lastStrokeT || t - 1), 0.22, 4.0);
      if(peak - (G.valley || 0) > (CFG.strokeMinRange || 0.12) && (G.state === 'play' || G.state === 'orgasm')){
        registerStroke(peak, G.valley || 0, dur);
        G.strokeDur = dur;
      }
    }

    // Retraction Audio (moist suction release or lip pop)
    if(sv < -0.40 && (G.prevV || 0) >= -0.40 && (G.state === 'play' || G.state === 'orgasm')){
      if(G.oral > 0.5){
        if(typeof playLipPop === 'function') playLipPop();
      } else {
        if((G.depth||1) <= 0.15 && typeof playPullout === 'function'){
          playPullout(); // near-full withdrawal pop
        } else if(typeof playSlide === 'function' && G.depth > 0.30){
          playSlide(clamp(-sv * 0.35 * (G.lube || 0.8), 0.32, 1.15));
        }
      }
    }
    G.prevV = sv;

    // Rhythm rate decay & combo degradation on stall
    G.rate = (G.rate || 0) * Math.pow(G.stall || 0.45, dt);
    if(t - (G.lastStrokeT || t) > 2.2){
      G.combo = Math.max(1.0, (G.combo || 1) - 0.16 * dt);
    }

    // --- State-Specific Dynamics ---
    if(G.state === 'play'){
      // Natural pleasure decay if neglected
      if(t - (G.lastStrokeT || t) > 1.8){
        G.pleasure = Math.max(G.floor || 0, G.pleasure - (CFG.pleaDecay || 2.2) * dt);
      }

      // Caressing / Rubbing Mechanics (Zone modulated)
      const rz = typeof rubZoneName === 'function' ? rubZoneName() : 'both';
      const zm = (typeof RUBGAIN !== 'undefined' && RUBGAIN[rz]) ? RUBGAIN[rz] : 1.0;
      const isLow = rz === 'low', isBoth = rz === 'both';

      if(G.rub > 0.45){
        const rubStep = ((CFG.rubGain ? CFG.rubGain[0] : 3.5) * zm + (CFG.rubGain ? CFG.rubGain[1] : 0.05) * G.pleasure) * dt;
        G.pleasure = Math.min(100, G.pleasure + rubStep);
      }
      if(G.rub > 0.12){
        G.breast.v += G.rub * (isBoth ? 68 : 50) * dt * 10 * Math.sin(t * 9.2);
        if(G.oral < 0.5 && typeof playRubStroke === 'function'){
          const halfCycle = Math.floor(t * 9.2 / Math.PI);
          if(G._rubHalf === undefined || (G.rubPrev || 0) <= 0.12) G._rubHalf = halfCycle;
          if(halfCycle > G._rubHalf){
            G._rubHalf = halfCycle;
            playRubStroke((halfCycle % 2 === 0) ? 1 : -1, rz);
            G._rubN = (G._rubN || 0) + 1;
            if(G._rubN % (isLow ? 3 : 4) === 0 && typeof playMoan === 'function'){
              playMoan(isLow ? 0.48 : 0.32, {
                dur: 0.36,
                pmul: (isLow ? 1.36 : 1.20) * (G.kiss > 0.5 ? 1.1 : 1.0),
                vol: (isLow ? 0.92 : 0.78) * (G.kiss > 0.5 ? 0.7 : 1.0)
              });
            }
          }
        }
      }
      G.rubPrev = G.rub;

      // Oral mode continuous warm suction
      if(G.oral > 0.04){
        const oralStep = ((CFG.rubGain ? CFG.rubGain[0] : 3.5) * 1.20 + (CFG.rubGain ? CFG.rubGain[1] : 0.05) * G.pleasure) * dt;
        G.pleasure = Math.min(100, G.pleasure + oralStep);
        G.breast.v += G.oral * 24 * dt * 10 * Math.sin(t * 6.2);
      }

      // Stamina & Exhaustion Drain
      let drain = 0;
      if(G.rate > 0.25){
        const band = sweetBand();
        drain = 0.65 + G.rate * (CFG.stamDrill || 4.2) * (0.3 + 0.7 * G.depth);
        if(G.rate > band.c + band.hw) drain += 0.95; // penalty for thrashing out of rhythm
      } else {
        drain = -(CFG.stamRegen || 8.0);
      }
      if(G.kiss > 0.5) drain += (CFG.kissCost || 1.2);
      if(G.rub > 0.5) drain += (CFG.rubCost || 1.4);
      if(G.oral > 0.5) drain += 1.1;

      if(drain > 0) G.stamina = Math.max(0, (G.stamina || 100) - drain * dt);
      else if(!G.tired) G.stamina = Math.min(100, (G.stamina || 100) - drain * dt);

      if((G.stamina || 0) <= 0 && !G.tired){
        G.tired = true;
        G.tiredT = 0;
        G.kissT = 0; G.rubT = 0; G.oralT = 0;
        G.spaceHeld = false;
        if(typeof say === 'function' && Array.isArray(LINES.tired)) say(pick(LINES.tired));
      }

      scheduleVoice(dt);
    }

    if(G.state === 'orgasm'){
      G.orgT = (G.orgT || 0) + dt;
      // Involuntary vaginal spasm clamping during climax
      G.shaftPulse = 0.8 + 0.6 * Math.sin(G.orgT * 8.5);

      if(G.orgT > 1.4){
        G.pleasure = Math.max(CFG.orgFloor || 32, 100 - ((G.orgT - 1.4) / 4.0) * (100 - (CFG.orgFloor || 32)));
      }
      const pelvicBump = Math.sin(Math.min(1.0, G.orgT / 0.6) * Math.PI);
      G.breast.v += pelvicBump * 90;
      if(R() < dt * 9) G.shake = Math.min(4.2, (G.shake || 0) + 1.6);

      // Orgasm complete: enter hypersensitive afterglow
      if(G.orgT > 5.4){
        G.state = 'play';
        G.after = 8.5; // afterglow duration
        G.floor = Math.max(G.floor || 0, CFG.orgFloor || 32);
        G.sens = (G.sens || 1) + (CFG.sensStep || 0.12);
        if(typeof say === 'function' && Array.isArray(LINES.after)) say(pick(LINES.after));
      }
    }
  }
  else if(G.state === 'climax'){
    G.sesT = (G.sesT || 0) + dt;
    G.climaxT = (G.climaxT || 0) + dt;
    G.spurtHold = Math.max(0, (G.spurtHold || 0) - dt * 3.2);
    G.depth = 0.80 + (G.spurtHold || 0) * 0.16;
    G.breast.v += 35 * dt * 10 * Math.sin(t * 42);
    if(G.climaxT > 5.2) spurt();
    scheduleVoice(dt);
  }
  else if(G.state === 'finish'){
    G.sesT = (G.sesT || 0) + dt;
    G.finishT = (G.finishT || 0) + dt;
    // Settling into deep relaxation inside her
    G.depth = 0.97 + Math.sin(t * 28) * 0.010 * Math.max(0, 1 - G.finishT / 2.4);
    if(G.finishT < 2.4){
      G.breast.v += 140 * dt * Math.sin(t * 32);
    }
    if(G.finishT > 1.0 && (G.drips || []).length < 12 && R() < dt * 2.4){
      const dp = typeof poseFluidOrigin === 'function' ? poseFluidOrigin() : [640, 500];
      G.drips.push({ p: 0, sp: rr(0.08, 0.18), x: dp[0] + rr(-6, 6), j: rr(0, 2), ox: dp[0], oy: dp[1] });
    }
    if(G.finishT > 3.4 && !G.endedShown){
      G.endedShown = true;
      nextRound();
    }
  }

  // --- Ballistic Fluid Particle Dynamics ---
  if(Array.isArray(G.jets)){
    for(const j of G.jets){
      j.vy += 980 * dt; // gravity
      j.x += j.vx * dt;
      j.y += j.vy * dt;
      j.life -= dt;
    }
    G.jets = G.jets.filter(j => j.life > 0);
  }

  // Sweat bead accumulation and run down skin
  if(Array.isArray(G.sweat)){
    G.sweat.forEach(s => {
      s.y += s.vy * dt;
      s.life -= dt * 0.32;
      s.x += Math.sin(t * 2.2 + s.ph) * 5.5 * dt;
    });
    G.sweat = G.sweat.filter(s => s.life > 0);
    const hotBody = ((G.pleasure > 52 && G.pleasure < 100 && G.state === 'play') || G.state === 'orgasm');
    if(hotBody && G.sweat.length < 18 && R() < dt * 2.4){
      G.sweat.push({ x: 318 + rr(-14, 18), y: 462 + rr(-6, 3), vy: rr(4.5, 9.5), ph: R() * TAU, life: 1 });
    }
    if(G.pleasure > 72 && G.sweat.length < 18 && R() < dt * 2.8){
      G.sweat.push({ x: 330 + rr(-8, 8), y: 478 + rr(0, 5), vy: rr(4.5, 8.5), ph: R() * TAU, life: 1 });
    }
  }

  // Euphoric floating hearts
  if(Array.isArray(G.hearts)){
    G.hearts.forEach(h => {
      h.y += h.vy * dt;
      h.life -= dt;
      h.ph += dt * 3.2;
    });
    G.hearts = G.hearts.filter(h => h.life > 0);
  }

  // Viscous sheet drips & glistening wet spots
  if(Array.isArray(G.drips)){
    G.drips.forEach(d => { d.p += d.sp * dt; });
    G.drips = G.drips.filter(d => {
      if(d.p > 1.0){
        if(Array.isArray(G.glisten) && G.glisten.length < 45){
          G.glisten.push({ x: d.x + rr(-3, 3), y: 556 + rr(-2, 4), a: 0.55 });
        }
        if(R() < 0.32 && Array.isArray(G.glisten) && G.glisten.length < 55){
          G.glisten.push({ x: 640 + rr(-8, 8), y: 546 + rr(-4, 4), a: 0.45 });
        }
        return false;
      }
      return true;
    });
  }
  if(Array.isArray(G.glisten)){
    G.glisten.forEach(g => { g.a = Math.min(g.a, 0.55); });
  }
}

/* ============================================================
   AUTONOMIC VOCAL & RESPIRATORY SCHEDULER
   ============================================================ */

function scheduleVoice(dt){
  G.nextMoan = (G.nextMoan || 2.0) - dt;
  if(G.rub > 0.5) G.nextMoan = Math.max(G.nextMoan, 0.65);

  if(G.nextMoan <= 0){
    const a = (G.pleasure || 0) / 100;
    if(typeof playMoan === 'function'){
      playMoan(0.22 + a * 0.78 + (G.state === 'orgasm' ? 0.25 : 0), {
        pmul: G.kiss > 0.5 ? 1.15 : 1.0,
        vol: G.kiss > 0.5 ? 0.78 : 1.0
      });
    }
    // Breathing cycle interval: gasps speed up with high arousal
    G.nextMoan = lerp(6.2, 1.4, a) * (0.65 + R() * 0.75) * (G.state === 'orgasm' ? 0.42 : 1.0);

    if(G.after > 0 && R() < 0.55 && typeof say === 'function' && Array.isArray(LINES.after)){
      say(pick(LINES.after));
    } else if(R() < 0.52 && (G.sayCd || 0) <= 0 && typeof tierLine === 'function'){
      tierLine();
    }
  }

  // Heaving breath intake and exhalation
  G.nextBreath = (G.nextBreath || 1.5) - dt;
  if(G.nextBreath <= 0 && typeof playBreath === 'function'){
    playBreath(false);
    G.nextBreath = 1.0 / (lerp(0.18, 0.55, (G.pleasure || 0) / 100));
  }

  // Panting recovery when exhausted
  if(G.tired){
    G.tiredT = (G.tiredT || 0) + dt;
    G.nBreathM = (G.nBreathM || 0) - dt;
    if(G.nBreathM < 0 && typeof playBreath === 'function'){
      playBreath(true);
      G.nBreathM = 0.52;
    }
    if(G.tiredT > 5.2){
      G.tired = false;
      G.stamina = 65;
    }
  }

  // Sympathetic Cardiovascular Tachycardia (Heartbeat)
  if((G.pleasure || 0) > 70){
    G.nextBeat = (G.nextBeat || 0.6) - dt;
    if(G.nextBeat <= 0 && typeof playHeart === 'function'){
      playHeart();
      const bpm = 74 + ((G.pleasure - 70) * 1.35) + ((G.orgasms || 0) * 8.5);
      G.nextBeat = 60 / Math.min(185, bpm);
    }
  }
}