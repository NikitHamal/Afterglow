// Afterglow — module: side (missionary side-view couple, coherent anatomy)
'use strict';

/* ============================================================
   AUTONOMIC EXPRESSION & NEUROLOGICAL STATE
   ============================================================ */
function herExpression(){
  const p = (G.pleasure || 0) / 100;
  const ar = (G.ar || 0) / 100;
  const t = G.t || 0;
  let eye = lerp(0.72, 0.12, sm(0.08, 0.92, p));
  let rolled = 0;
  let mouth = p * 0.36;
  let blush = 0.12 + p * 0.44 + ar * 0.22;
  let brow = lerp(-0.06, 0.46, sm(0.20, 0.90, p));
  let tilt = 0.12 + p * 0.0035;
  if(Array.isArray(G.mouths)){
    G.mouths.forEach(m => {
      const u = (t - m.t0) / m.dur;
      if(u > 0 && u < 1) mouth += Math.sin(Math.PI * u) ** 0.75 * m.i * 0.65;
    });
  }
  if(G.speech) mouth += 0.28;
  if((G.blinkPh || 0) > 0) eye *= (1 - G.blinkPh);
  if(G.state === 'orgasm'){
    const e = Math.sin(Math.PI * clamp((G.orgT || 0) / 5.2, 0, 1));
    rolled = 0.70 + e * 0.30;
    eye = lerp(eye, 0.06, e);
    mouth = Math.max(mouth, 0.88 * e);
    tilt = 0.50; blush = 1.0; brow = 0.65;
  }
  if((G.after || 0) > 0){ eye = Math.min(eye, 0.16); mouth = Math.max(mouth, 0.18); blush = Math.max(blush, 0.45); }
  if(G.state === 'finish' && (G.finishT || 0) > 1){ eye = 0.06; mouth = 0.22; }
  if((G.kiss || 0) > 0.6){ mouth = Math.min(mouth, 0.45); }
  return { eye: clamp(eye, 0, 1), rolled: clamp(rolled, 0, 1), mouth: clamp(mouth, 0, 1), blush: clamp(blush, 0, 1), brow, tilt };
}

function lerpp(a, b, t){ return [lerp(a[0], b[0], t), lerp(a[1], b[1], t)]; }

// recessed (far-side) limb tones so depth reads
function herFarT(){ const s = getSkin(); return skTone(skDark(s.her, 0.30)); }
function himFarT(){ const s = getSkin(); return skTone(skDark(s.him, 0.30)); }

/* ============================================================
   HER SUPINE BODY: pillow hair, far limbs, connected torso
   ============================================================ */
function drawHer(){
  const E = herExpression();
  const T = herT();
  const t = G.t || 0;
  const p = (G.pleasure || 0) / 100;
  const wrap = sm(0.50, 0.88, p);
  const br = Math.sin(t * TAU * (0.16 + p * 0.0045)) * (2.4 + p * 1.6);
  const hairCol = G.char ? G.char.hairColor : '#231318';
  const hc = sp(312, 504, 0.008);

  // ---- hair spread on pillow: soft lobed mass, not radiating strings ----
  X.save(); X.globalCompositeOperation = 'multiply';
  shade(hc[0] - 30, hc[1] + 44, 110, 34, 'rgba(15,5,8,0.5)', 0);
  X.restore();
  const hairBed = () => {
    X.moveTo(hc[0] - 6, hc[1] - 26);
    X.bezierCurveTo(hc[0] - 60, hc[1] - 20, hc[0] - 104, hc[1] + 12, hc[0] - 96, hc[1] + 44);
    X.bezierCurveTo(hc[0] - 88, hc[1] + 66, hc[0] - 30, hc[1] + 70, hc[0] + 16, hc[1] + 58);
    X.bezierCurveTo(hc[0] + 44, hc[1] + 50, hc[0] + 40, hc[1] + 20, hc[0] + 26, hc[1] + 4);
    X.bezierCurveTo(hc[0] + 16, hc[1] - 10, hc[0] + 6, hc[1] - 20, hc[0] - 6, hc[1] - 26);
    X.closePath();
  };
  X.beginPath(); hairBed();
  const hbg = X.createLinearGradient(hc[0] - 90, hc[1] - 20, hc[0] + 30, hc[1] + 60);
  hbg.addColorStop(0, skLight(hairCol, 0.16)); hbg.addColorStop(0.5, hairCol); hbg.addColorStop(1, skDark(hairCol, 0.35));
  X.fillStyle = hbg; X.fill();
  // internal parting curves + sheen so it reads as layered hair
  X.save(); X.beginPath(); hairBed(); X.clip();
  X.strokeStyle = skDark(hairCol, 0.45); X.lineWidth = 2.2; X.lineCap = 'round';
  for(let i = 0; i < 4; i++){
    X.beginPath();
    X.moveTo(hc[0] - 2, hc[1] - 14 + i * 6);
    X.quadraticCurveTo(hc[0] - 56, hc[1] + 4 + i * 12, hc[0] - 88 + i * 10, hc[1] + 40 + i * 6);
    X.stroke();
  }
  X.globalCompositeOperation = 'soft-light';
  shade(hc[0] - 46, hc[1] + 6, 52, 20, 'rgba(255,205,215,0.20)', -0.2);
  X.restore();

  // ---- far arm resting on the sheet ----
  const FT = herFarT();
  const fShld = sp(386, 524, 0.02), fElb = sp(356, 548, 0.01), fWr = sp(322, 554, 0.0);
  limbS(fShld, fElb, 12, 9.5, FT, { belly: 1.05, aoA: 0.3 });
  limbS(fElb, fWr, 9.5, 7, FT, { belly: 1.02 });
  handS(fWr[0], fWr[1], 2.7, 0.88, FT, { curl: 0.12, spread: 0.4 });

  // ---- far leg wrapped around his waist, foot on his back ----
  const fHip = sp(626, 522, 0.07);
  const fKnee = [744 + wrap * 10, 450 + wrap * 6];
  const fAnk = [696 + wrap * 8, 424 + wrap * 6];
  limbS(fHip, fKnee, 23, 15, FT, { belly: 1.12, aoA: 0.3 });
  limbS(fKnee, fAnk, 13, 7.5, FT, { belly: 1.18 });
  footS(fAnk[0] - 6, fAnk[1] - 2, 2.6, 0.85, FT);

  // ---- connected supine torso ----
  const nk = sp(362, 522, 0.015), ch = sp(448, 505 + br * 0.4, 0.03),
        wa = sp(540, 514, 0.05), mo = sp(652, 505, 0.085), hp_ = sp(620, 516, 0.07);
  const backY = 542;
  const torso = () => {
    X.moveTo(nk[0], nk[1] - 8);
    X.bezierCurveTo(ch[0] - 44, ch[1] - 16, ch[0] - 18, ch[1] - 15, ch[0], ch[1] - 12);
    X.bezierCurveTo(wa[0] - 40, wa[1] - 9, wa[0] - 16, wa[1] - 8, wa[0], wa[1] - 7);
    X.bezierCurveTo(mo[0] - 36, mo[1] - 12, mo[0] - 12, mo[1] - 14, mo[0], mo[1] - 11);
    X.bezierCurveTo(hp_[0] + 22, hp_[1] - 8, hp_[0] + 26, hp_[1] + 4, hp_[0] + 16, hp_[1] + 14);
    X.bezierCurveTo(wa[0] + 32, backY + 3, wa[0] - 10, backY + 3, wa[0], backY + 2);
    X.bezierCurveTo(ch[0] + 32, backY + 1, ch[0] - 10, backY, ch[0], backY);
    X.bezierCurveTo(nk[0] + 26, backY - 2, nk[0] + 6, nk[1] + 8, nk[0], nk[1] + 8);
    X.closePath();
  };
  skFillShape(torso, T, [0, ch[1] - 22, 0, backY + 6]);
  skClipIn(torso, () => {
    fAO(wa[0], backY, 92, 10, 0.42);
    fAO(ch[0], backY - 2, 62, 9, 0.34);
    fSh(wa[0] - 6, wa[1] - 1, 34, 9, 'rgba(150,80,64,0.20)', 0);
    fHi(ch[0] - 4, ch[1] - 8, 46, 10, 'rgba(255,240,226,0.26)', -0.06);
    fHi(mo[0] - 8, mo[1] - 8, 26, 8, 'rgba(255,240,226,0.24)', -0.1);
  });
  skLine(torso, T, 1.4, 0.3);

  const nv = sp(566, 512, 0.045);
  X.fillStyle = 'rgba(140,78,62,0.4)';
  X.beginPath(); X.ellipse(nv[0], nv[1], 2.6, 3.6, 0.15, 0, TAU); X.fill();

  const [bR, bG, bB] = hexToRgb(G.char ? G.char.blushColor : '#e86070');
  X.fillStyle = `rgba(${bR},${bG},${bB},${0.08 + E.blush * 0.22})`;
  X.beginPath(); X.ellipse(ch[0] + 4, ch[1] - 3, 40, 13, -0.1, 0, TAU); X.fill();

  return { E, wrap, br };
}

/* ============================================================
   HER HEAD & FACE (3/4 to viewer) with silky layered hair
   ============================================================ */
function drawHerHead(E){
  const T = herT();
  const neckB = sp(364, 520, 0.015);
  const headC = sp(312, 504, 0.008);
  const hairCol = G.char ? G.char.hairColor : '#231318';

  neckS([headC[0] + 12, headC[1] + 14], [neckB[0] - 2, neckB[1] - 2], 12, T);

  X.save();
  X.translate(headC[0], headC[1] - (G.pleasure || 0) * 0.02);
  X.rotate(-0.06 + Math.sin((G.t || 0) * TAU * 0.33) * 0.015 - (G.nod || 0) * 0.06);

  hairMassS(-2, -4, 38, 38, 0, hairCol);

  const face = () => {
    X.moveTo(-27, -24);
    X.bezierCurveTo(-33, -44, 20, -50, 31, -25);
    X.bezierCurveTo(39, -5, 35, 18, 21, 27);
    X.bezierCurveTo(11, 33, -8, 31, -17, 23);
    X.bezierCurveTo(-27, 15, -33, 4, -27, -24);
    X.closePath();
  };
  X.beginPath(); face();
  const fg = X.createRadialGradient(-8, -16, 4, 0, 0, 46);
  fg.addColorStop(0, T.hi); fg.addColorStop(0.5, T.b); fg.addColorStop(0.85, T.s); fg.addColorStop(1, T.d);
  X.fillStyle = fg; X.fill();
  skClipIn(face, () => {
    fSh(-17, 10, 12, 10, 'rgba(160,90,72,0.18)', 0.18);
    fSh(19, 12, 10, 9, 'rgba(160,90,72,0.16)', -0.18);
    fHi(-6, -22, 20, 10, 'rgba(255,242,230,0.3)', -0.1);
  });

  const [br, bg, bb] = hexToRgb(G.char ? G.char.blushColor : '#e86070');
  X.fillStyle = `rgba(${br},${bg},${bb},${0.18 + E.blush * 0.40})`;
  X.beginPath(); X.ellipse(-13, -4, 11, 7, 0.15, 0, TAU); X.fill();
  X.beginPath(); X.ellipse(13, -6, 11, 7, -0.15, 0, TAU); X.fill();

  X.strokeStyle = 'rgba(150,85,75,0.5)'; X.lineWidth = 1.5;
  X.beginPath(); X.moveTo(-1, -12); X.quadraticCurveTo(-3, -6, 0, -4); X.stroke();
  X.fillStyle = 'rgba(255,245,240,0.5)'; X.beginPath(); X.arc(0, -5, 1.3, 0, TAU); X.fill();

  const Eo = E.eye, roll = E.rolled;
  for(const s of [-1, 1]){
    const ex = s * 12.5, ey = -17;
    const openness = Eo * (1 - roll * 0.5);
    if(openness > 0.10){
      X.lineWidth = 2.2; X.strokeStyle = '#422428';
      X.beginPath(); X.moveTo(ex - 7, ey + 1); X.quadraticCurveTo(ex, ey - 5 * openness - 1, ex + 7, ey + 1);
      X.quadraticCurveTo(ex, ey + 4 * openness + 1, ex - 7, ey + 1); X.closePath();
      X.fillStyle = '#f8ede8'; X.fill(); X.stroke();
      X.fillStyle = G.char ? G.char.eyeColor : '#4a2c33';
      X.beginPath(); X.arc(ex + roll * 1.5, ey - 1.2 * openness - roll * 2, 4.0 * Math.max(openness, 0.3), 0, TAU); X.fill();
      X.fillStyle = '#180a0e';
      X.beginPath(); X.arc(ex + roll * 1.5, ey - 1.2 * openness - roll * 2, 2.2 * Math.max(openness, 0.3), 0, TAU); X.fill();
      X.fillStyle = 'rgba(255,255,255,0.95)';
      X.beginPath(); X.arc(ex + roll * 1.5 - 1.2, ey - 2.0 * openness - roll * 2 - 1, 1.4, 0, TAU); X.fill();
      X.fillStyle = 'rgba(255,255,255,0.5)';
      X.beginPath(); X.arc(ex + roll * 1.5 + 1.6, ey - 0.6 * openness - roll * 2, 0.7, 0, TAU); X.fill();
    } else {
      X.lineWidth = 2.2; X.strokeStyle = '#422428';
      X.beginPath(); X.moveTo(ex - 7, ey + 1); X.quadraticCurveTo(ex, ey + 4.5, ex + 7, ey + 1); X.stroke();
    }
    X.strokeStyle = '#381c20'; X.lineWidth = 1.3;
    X.beginPath(); X.moveTo(ex + s * 6, ey + 1); X.lineTo(ex + s * 10, ey - 2.5); X.stroke();
    X.strokeStyle = hairCol; X.lineWidth = 1.8;
    X.beginPath(); X.moveTo(ex - 7, ey - 8); X.quadraticCurveTo(ex, ey - 11 - E.brow * 4, ex + 7, ey - 7 - E.brow * 5); X.stroke();
  }

  const mo = E.mouth, lipCol = G.char ? G.char.lipColor : '#b3555f';
  if(mo > 0.08){
    X.fillStyle = '#5c1622';
    X.beginPath(); X.ellipse(0, 11, 7.5 + mo * 2.2, 2.5 + mo * 7.5, 0, 0, TAU); X.fill();
    if(mo > 0.22){ X.fillStyle = 'rgba(255,248,245,0.95)'; X.beginPath(); X.ellipse(0, 8 + mo * 1.2, 5.2, 1.8, 0, 0, TAU); X.fill(); }
    X.fillStyle = 'rgba(215,95,115,0.95)';
    X.beginPath(); X.ellipse(0, 12 + mo * 3.5, 5.6, 2.4, 0, 0, TAU); X.fill();
  } else {
    X.strokeStyle = lipCol; X.lineWidth = 2.2;
    X.beginPath(); X.moveTo(-7, 10); X.quadraticCurveTo(0, 12, 7, 10); X.stroke();
    X.fillStyle = lipCol; X.beginPath(); X.ellipse(0, 12, 5.0, 2.2, 0, 0, TAU); X.fill();
  }
  X.fillStyle = 'rgba(255,250,250,0.5)';
  X.beginPath(); X.ellipse(2, 12.5 + mo * 3, 2.4, 1.1, 0, 0, TAU); X.fill();

  // fringe + temple locks framing the face
  tressS(-28, -24, -12, -44, 12, -44, 28, -20, 11, 3, hairCol, 'rgba(255,205,215,0.22)');
  tressS(-30, -16, -38, -2, -38, 10, -32, 22, 7, 2, hairCol);
  tressS(30, -18, 38, -4, 38, 8, 34, 20, 7, 2, hairCol);
  X.restore();
}

/* ============================================================
   PROFILE BREAST: integrated teardrop falling with gravity
   ============================================================ */
function drawBreast(E, br){
  const jig = (G.breast ? G.breast.p : 0) * 0.95;
  const bsz = 0.72 + (G.char ? G.char.breastSize : 0.45) * 0.62;
  const p = sp(452, 489 + br, 0.035);
  breastS(p[0], p[1], 30 * bsz, 1.35, herT(), { jig });
}

/* ============================================================
   HIM: kneeling, inclined over her — one connected figure
   ============================================================ */
function drawHim(E){
  const d = G.depth || 0;
  const T = himT(), FT = himFarT();

  const pel = hp(712, 468, 1.0);
  const mid = hp(624, 428, 0.6);
  const shld = hp(540, 404, 0.25);
  const head = hp(472, 388, 0.2);

  // ---- far leg kneeling (behind) ----
  limbS([pel[0] + 16, pel[1] + 10], [830, 560], 26, 16, FT, { belly: 1.12, aoA: 0.3 });
  limbS([830, 560], [912, 568], 15, 9, FT, { belly: 1.2 });
  footS(918, 570, 0.3, 0.95, FT);

  // ---- far arm gripping her waist ----
  limbS([shld[0] + 10, shld[1] + 6], [536, 466], 14, 10.5, FT, { belly: 1.08, aoA: 0.3 });
  limbS([536, 466], [556, 502], 10.5, 7.5, FT, { belly: 1.04 });
  handS(558, 506, 1.7, 0.95, FT, { curl: 0.45 });

  // ---- torso: thick inclined trunk ----
  const torso = () => {
    X.moveTo(pel[0] + 12, pel[1] - 16);
    X.bezierCurveTo(mid[0] + 16, mid[1] - 22, shld[0] + 22, shld[1] - 20, shld[0] + 4, shld[1] - 14);
    X.bezierCurveTo(shld[0] - 16, shld[1] - 8, shld[0] - 20, shld[1] + 8, shld[0] - 12, shld[1] + 18);
    X.bezierCurveTo(mid[0] - 20, mid[1] + 26, pel[0] - 22, pel[1] + 24, pel[0] - 12, pel[1] + 22);
    X.bezierCurveTo(pel[0] + 6, pel[1] + 20, pel[0] + 16, pel[1] + 2, pel[0] + 12, pel[1] - 16);
    X.closePath();
  };
  skFillShape(torso, T, [shld[0], shld[1] - 22, pel[0], pel[1] + 26]);
  skClipIn(torso, () => {
    X.save(); X.globalCompositeOperation = 'multiply';
    X.strokeStyle = 'rgba(80,38,20,0.4)'; X.lineWidth = 2.6; X.lineCap = 'round';
    X.beginPath(); X.moveTo(shld[0] + 4, shld[1] - 6);
    X.quadraticCurveTo(mid[0] + 8, mid[1] - 4, pel[0] + 4, pel[1] - 8); X.stroke(); X.restore();
    fSh(mid[0], mid[1] + 16, 46, 11, 'rgba(90,44,24,0.3)', 0.35);
    fHi(mid[0] + 6, mid[1] - 12, 50, 9, 'rgba(255,225,190,0.26)', 0.35);
    fAO(shld[0], shld[1], 20, 14, 0.3);
  });
  skLine(torso, T, 1.4, 0.3);
  // shadow he casts on her beneath his pelvis/torso
  fAO(pel[0] - 6, pel[1] + 26, 40, 12, 0.34, 0.3);

  // ---- near leg kneeling on bed ----
  limbS([pel[0] + 16, pel[1] + 12], [800, 556], 28, 18, T, { belly: 1.14, aoA: 0.32 });
  limbS([800, 556], [884, 564], 17, 10, T, { belly: 1.2 });
  footS(890, 566, 0.28, 1.0, T);

  // ---- neck + head leaning over her ----
  neckS([head[0] + 8, head[1] + 12], [shld[0] + 2, shld[1] - 4], 15, T);
  X.save();
  X.translate(head[0], head[1]);
  X.rotate(0.45 + d * 0.06);
  const skull = () => {
    X.moveTo(-21, -21);
    X.bezierCurveTo(-27, -37, 27, -39, 29, -17);
    X.bezierCurveTo(31, -2, 27, 13, 15, 21);
    X.bezierCurveTo(2, 27, -15, 25, -21, 17);
    X.bezierCurveTo(-29, 8, -27, -9, -21, -21);
    X.closePath();
  };
  X.beginPath(); skull();
  const hg = X.createRadialGradient(-8, -16, 4, 0, 0, 40);
  hg.addColorStop(0, T.hi); hg.addColorStop(0.55, T.b); hg.addColorStop(1, T.s);
  X.fillStyle = hg; X.fill();
  X.beginPath();
  X.moveTo(-13, 17); X.bezierCurveTo(-25, 25, -34, 17, -34, 6);
  X.bezierCurveTo(-38, -2, -27, -9, -21, -11); X.closePath(); X.fill();
  skClipIn(skull, () => { fSh(-6, -9, 19, 6, 'rgba(100,55,30,0.3)', -0.15); fHi(-4, -21, 15, 8, 'rgba(255,225,190,0.24)'); });
  // downcast eye + brow + nose + mouth (profile-leaning)
  X.strokeStyle = 'rgba(60,30,20,0.75)'; X.lineWidth = 1.8;
  X.beginPath(); X.moveTo(-16, -8); X.quadraticCurveTo(-10, -6, -5, -8); X.stroke();
  X.strokeStyle = 'rgba(120,65,35,0.5)'; X.lineWidth = 1.4;
  X.beginPath(); X.moveTo(-14, -2); X.quadraticCurveTo(-18, 6, -15, 11); X.stroke();
  X.strokeStyle = 'rgba(120,50,45,0.6)'; X.lineWidth = 1.6;
  X.beginPath(); X.moveTo(-16, 15); X.quadraticCurveTo(-11, 16, -7, 15); X.stroke();
  // short tousled hair + fringe
  hairMassS(3, -20, 26, 16, -0.15, '#1c100a');
  tressS(-14, -28, -6, -36, 6, -36, 14, -26, 8, 2, '#1c100a', 'rgba(255,210,170,0.16)');
  tressS(-22, -16, -28, -6, -28, 4, -24, 10, 6, 2, '#1c100a');
  tressS(-20, -26, -26, -18, -28, -10, -26, -4, 6, 2, '#1c100a');
  X.fillStyle = T.s; X.beginPath(); X.ellipse(8, 4, 5.5, 7.5, 0.1, 0, TAU); X.fill();
  X.strokeStyle = 'rgba(95,50,28,0.4)'; X.lineWidth = 1.0;
  X.beginPath(); X.moveTo(8, -1); X.quadraticCurveTo(11, 4, 8, 9); X.stroke();
  X.restore();

  // ---- near arm reaching down to grip her hip ----
  limbS(shld, [566, 462], 16, 11.5, T, { belly: 1.1, aoA: 0.32 });
  limbS([566, 462], [602, 498], 11.5, 8, T, { belly: 1.06 });
  handS(606, 502, 1.9, 1.0, T, { curl: 0.45, spread: 0.4 });
}

/* ============================================================
   HER FOREGROUND LEG + NEAR ARM (over him)
   ============================================================ */
function drawHerNear(E, out){
  const { wrap } = out;
  const T = herT();
  const hip = sp(626, 522, 0.07);
  const K = [680 + wrap * 6, 532 + wrap * 4];
  const A = [722 + wrap * 8, 548 + wrap * 4];

  // contact shadow under the thigh resting on the mattress
  fAO(lerp(hip[0], K[0], 0.5), 548, 46, 9, 0.35);
  limbS(hip, K, 26, 16, T, { belly: 1.14, aoA: 0.3 });
  fHi(K[0] + 2, K[1] - 6, 9, 11, 'rgba(255,240,225,0.34)', 0.2);
  limbS(K, A, 14.5, 8, T, { belly: 1.2 });
  footS(A[0] + 4, A[1] + 2, 0.32 - wrap * 0.25, 0.95, T);

  const sh = sp(396, 512, 0.02), el = sp(452, 470, 0.05), wr = sp(524, 436, 0.09);
  limbS(sh, el, 12.5, 9.5, T, { belly: 1.06, aoA: 0.3 });
  limbS(el, wr, 9.5, 6.5, T, { belly: 1.03 });
  handS(wr[0], wr[1], -0.7, 0.92, T, { curl: 0.5 });
}

/* ============================================================
   VULVA & SHAFT
   ============================================================ */
function drawVulva(){
  const eng = clamp((G.ar || 0) / 100, 0, 1);
  const open = 3.5 + 9.5 * (G.depth || 0);
  vulvaS(652, 498, open, eng, herT(), { view: 'side' });
}

function drawShaft(){
  const A = hp(700, 484, 1.0);
  const V = [653, 500];
  const ang = Math.atan2(V[1] - A[1], V[0] - A[0]);
  const pu = 1 + 0.36 * (G.shaftPulse || 0);
  const d = G.depth || 0;
  const T = himT();
  limbS(A, V, 12 * pu, 12.5 * pu, T, { belly: 1.02, line: false, lit: [Math.cos(ang - Math.PI / 2), Math.sin(ang - Math.PI / 2)] });
  X.strokeStyle = 'rgba(142,72,56,0.35)'; X.lineWidth = 2.2; X.lineCap = 'round';
  X.beginPath(); X.moveTo(A[0] - 4, A[1] - 8);
  X.quadraticCurveTo(lerp(A[0], V[0], 0.5) - 2, lerp(A[1], V[1], 0.5) - 7, V[0] - 6, V[1] - 5); X.stroke();
  if(d < 0.46){
    const gl = [V[0] + Math.cos(ang) * 7, V[1] + Math.sin(ang) * 7];
    X.fillStyle = skLight(T.b, 0.18);
    X.beginPath(); X.ellipse(gl[0], gl[1], 9.8 * pu, 11.5 * pu, ang + 0.28, 0, TAU); X.fill();
    X.fillStyle = 'rgba(255,250,245,0.4)';
    X.beginPath(); X.arc(gl[0] - 2, gl[1] - 3, 2.4, 0, TAU); X.fill();
  }
  if((G.ar || 0) > 26){
    X.save(); X.globalCompositeOperation = 'screen';
    X.strokeStyle = 'rgba(255,255,255,0.32)'; X.lineWidth = 3.0; X.lineCap = 'round';
    X.beginPath(); X.moveTo(A[0], A[1] - 8); X.lineTo(V[0] + 4, V[1] - 9); X.stroke();
    X.restore();
  }
}

/* ============================================================
   RUB HER INTERACTIONS
   ============================================================ */
function drawRubHand(hx, hy, ph, small){
  const T = himT();
  const sh = hp(500, 424, 0.2);
  const el = [(sh[0] + hx) / 2 + 30, (sh[1] + hy) / 2 - 22];
  limbS(sh, el, 20, 15, T, { belly: 1.08, aoA: 0.3 });
  limbS(el, [hx, hy], small ? 13 : 16, small ? 10 : 12, T, { belly: 1.05 });
  handS(hx, hy, -0.6, small ? 0.8 : 1.0, T, { curl: 0.42 });
}

function drawRubFX(br){
  const rub = G.rub || 0;
  if(rub < 0.03 || G.state === 'climax' || G.state === 'finish') return;
  const zone = (G.rubZone || 0) | 0;
  const p = sp(448, 492 + br, 0.035);
  const hx = zone === 3 ? 652 : p[0] + Math.sin(G.t * 9) * 8 * rub;
  const hy = zone === 3 ? 496 : p[1] + Math.sin(G.t * 18) * 4 * rub;
  drawRubHand(hx, hy, 0, zone === 3);
}
