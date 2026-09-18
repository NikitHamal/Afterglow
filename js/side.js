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
  // feathered halo: three nested scaled copies kill the machined silhouette edge
  // without paying for a canvas blur filter
  for(let i = 0; i < 3; i++){
    X.save();
    X.translate(hc[0], hc[1]); X.scale(1 + i * 0.055, 1 + i * 0.055); X.translate(-hc[0], -hc[1]);
    X.globalAlpha = 0.24 - i * 0.07;
    X.beginPath(); hairBed(); X.fillStyle = skDark(hairCol, 0.22); X.fill();
    X.restore();
  }
  X.beginPath(); hairBed();
  const hbg = X.createLinearGradient(hc[0] - 90, hc[1] - 20, hc[0] + 30, hc[1] + 60);
  // her hair lies under her own head, in shadow, far from the lamp: keep the whole
  // mass dark and let only the lamp-side tip carry a little lift
  hbg.addColorStop(0, skLight(skDark(hairCol, 0.06), 0.06));
  hbg.addColorStop(0.5, skDark(hairCol, 0.22));
  hbg.addColorStop(1, skDark(hairCol, 0.54));
  X.fillStyle = hbg; X.fill();
  // internal parting curves + sheen so it reads as layered hair
  X.save(); X.beginPath(); hairBed(); X.clip();
  X.lineCap = 'round';
  for(let i = 0; i < 6; i++){
    X.strokeStyle = skDark(hairCol, 0.34 + (i % 3) * 0.07);
    X.globalAlpha = 0.30 - (i % 2) * 0.08;
    X.lineWidth = 1.3 + (i % 3) * 0.7;
    X.beginPath();
    X.moveTo(hc[0] - 2, hc[1] - 14 + i * 6);
    X.quadraticCurveTo(hc[0] - 56, hc[1] + 4 + i * 12, hc[0] - 88 + i * 10, hc[1] + 40 + i * 6);
    X.stroke();
  }
  X.globalAlpha = 1;
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
  const fKnee = [750 + wrap * 10, 446 + wrap * 6];
  const fAnk = [792 + wrap * 8, 414 + wrap * 6];
  limbS(fHip, fKnee, 23, 15, FT, { belly: 1.12, aoA: 0.3 });
  limbS(fKnee, fAnk, 13, 7.5, FT, { belly: 1.18 });
  footS(fAnk[0] + 4, fAnk[1] - 2, -1.9, 0.9, FT);

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
  // She lies along screen-X, so her trunk is a HORIZONTAL cylinder. The key has to
  // run ACROSS it (top surface lit, mattress side dark) while the long axis only
  // carries the lamp's falloff from her head end towards her hips. A ramp that
  // runs *along* the trunk — which is what a single skFillShape did here — lands
  // the whole 52px-tall form inside a few percent of a 350px gradient, and the
  // body renders as one flat band no matter how strong the light is.
  const torsoL = [ch[0] - 34, backY - 94];
  const torsoS = [mo[0] + 24, backY + 20];
  skForm(torso, T, torsoL, torsoS, { edge: 0.26, rim: 0.15, cool: 0.10 });
  skClipIn(torso, () => {
    // mattress contact + ribcage underside — softer
    // contact shadow along the whole mattress line
    fAO(wa[0] + 8, backY + 2, 104, 13, 0.32);
    fAO(ch[0] - 4, backY - 1, 62, 10, 0.22);
    // costal margin: two feathered arcs sweeping back from the sternum to the waist
    for(let ci = 0; ci < 2; ci++){
      skArc(ch[0] + 4 + ci * 30, ch[1] - 3 + ci * 4,
            ch[0] + 40 + ci * 30, ch[1] + 13 + ci * 4,
            wa[0] - 4 + ci * 26, wa[1] + 5 + ci * 4,
            11, '146,80,62', 0.22 - ci * 0.06, { n: 10, rot: -0.30 });
    }
    // waist hollow + iliac crest
    fSh(wa[0] - 6, wa[1] - 1, 30, 8, 'rgba(148,78,62,0.13)', 0);
    fSh(mo[0] - 4, mo[1] - 4, 22, 10, 'rgba(148,78,62,0.10)', -0.12);
    // chest crest + belly peak — luminous
    // the ribcage crest catches the lamp, then falls away under itself
    fHi(ch[0] - 10, ch[1] - 19, 84, 9, 'rgba(255,238,220,0.20)', -0.05);
    fSh(ch[0] + 44, ch[1] + 12, 66, 12, 'rgba(126,60,48,0.18)', 0.10);
    fHi(mo[0] - 12, mo[1] - 12, 40, 8, 'rgba(255,238,222,0.15)', -0.10);
    // supine iliac crest, and the hollow the crest throws above itself
    fSh(mo[0] - 24, mo[1] - 1, 32, 12, 'rgba(140,74,58,0.15)', -0.16);
    fSh(mo[0] - 8, mo[1] + 4, 22, 9, 'rgba(150,82,64,0.10)', 0.14);
    // costal margin soft line
    fSh( (ch[0]+wa[0])/2, (ch[1]+wa[1])/2 - 4, 26, 4, 'rgba(158,88,68,0.08)', -0.28);
    // linea alba
    X.save(); X.globalCompositeOperation='multiply';
    shade((wa[0]+mo[0])/2, (wa[1]+mo[1])/2, 32, 4.5, 'rgba(156,90,68,0.08)', 0);
    X.restore();
  });
  skLine(torso, T, 1.25, 0.20);

  // navel: a DEPRESSION. A flat disk reads as a dot painted on the belly; a
  // shadowed upper wall plus a crease and a lit lower lip reads as a hole.
  const nv = sp(566, 512, 0.045);
  const nvr = 3.4, nvTone = hexToRgb(skDark(T.b, 0.36)).join(',');
  X.save();
  X.globalCompositeOperation = 'multiply';
  shade(nv[0], nv[1] - nvr * 0.30, nvr * 1.05, nvr * 0.95, `rgba(${nvTone},0.34)`, 0.15);
  X.restore();
  skArc(nv[0] - nvr * 0.80, nv[1] - nvr * 0.14,
        nv[0], nv[1] - nvr * 1.10,
        nv[0] + nvr * 0.80, nv[1] - nvr * 0.14,
        nvr * 0.50, nvTone, 0.34, { n: 10, thin: 0.8 });
  X.save(); X.globalCompositeOperation = 'screen';
  shade(nv[0], nv[1] + nvr * 0.72, nvr * 0.66, nvr * 0.34, 'rgba(255,238,220,0.16)', 0);
  X.restore();

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
  const fg = X.createRadialGradient(-10, -14, 4, 0, 0, 48);
  fg.addColorStop(0, T.hi); fg.addColorStop(0.42, T.b); fg.addColorStop(0.78, T.s); fg.addColorStop(1, T.d);
  X.fillStyle = fg; X.fill();
  skClipIn(face, () => {
    // form: the lamp is upper-left, so the far cheek, the mandible and the temple
    // all turn away into shadow, and the jaw throws its own occlusion downward
    fSh(20, 6, 12, 16, 'rgba(150,82,64,0.16)', -0.18);
    fSh(14, 22, 16, 7, 'rgba(140,74,58,0.18)', -0.06);
    fSh(-19, 6, 10, 14, 'rgba(150,82,64,0.12)', 0.20);
    fSh(2, 30, 14, 5, 'rgba(140,74,58,0.20)', 0);
    fSh(4, 18, 8, 4, 'rgba(140,74,58,0.14)', 0);
    // lights: forehead, near cheekbone, chin
    fHi(-6, -22, 18, 9, 'rgba(255,244,232,0.20)', -0.1);
    fHi(-11, -6, 10, 7, 'rgba(255,246,236,0.16)', 0.35);
    fHi(1, 20, 6, 4, 'rgba(255,244,234,0.16)', 0);
    fSSS(-14, 6, 9, 7, 0.20);
    fSSS(12, 4, 8, 6, -0.18);
    fSSS(-2, 14, 8, 5, 0.12);
  });

  // Blush is a vascular flush bleeding through the skin, not a pink patch laid on
  // top of it. A flat ellipse at 0.4 alpha was the loudest cartoon tell on her
  // face; a feathered radial in soft-light keeps the cheeks reading as skin.
  const [br, bg, bb] = hexToRgb(G.char ? G.char.blushColor : '#e86070');
  const bA = 0.22 + E.blush * 0.44;
  skClipIn(face, () => {
    X.save();
    X.globalCompositeOperation = 'soft-light';
    for(const cs of [-1, 1]){
      const bx = cs * 13, bgrd = X.createRadialGradient(bx - cs * 3.5, -4, 0.6, bx, -4, 14);
      bgrd.addColorStop(0, `rgba(${br},${bg},${bb},${bA})`);
      bgrd.addColorStop(0.46, `rgba(${br},${bg},${bb},${bA * 0.44})`);
      bgrd.addColorStop(1, `rgba(${br},${bg},${bb},0)`);
      X.fillStyle = bgrd;
      X.beginPath(); X.ellipse(bx, -4, 14, 9.5, cs * 0.18, 0, TAU); X.fill();
    }
    X.restore();
  });

  // nose: a feathered bridge shadow, a lit tip and a nostril. The old single 1.5px
  // stroke read as a beak ruled onto a flat oval.
  // Fringe first: thin locks hugging the hairline, drawn BEFORE the eyes so the
  // brows and lashes sit on top of them, and kept above the brow line — strands
  // that cross the eyes read as scratches raked across the face.
  X.save(); X.globalAlpha = 0.92;
  const hDk0 = skDark(hairCol, 0.34), hLt0 = skLight(hairCol, 0.14);
  const fringe = [
    [-30, -22, -18, -36, -2, -38,  16, -24, 4.4, hLt0,     'rgba(255,205,215,0.16)'],
    [-25, -18, -16, -32, -4, -34,  10, -20, 3.2, hairCol,  null],
    [-20, -22, -10, -38,  2, -38,  20, -26, 2.6, hDk0,     null],
    [ 29, -22,  18, -36,  2, -38, -16, -24, 4.2, hairCol,  'rgba(255,205,215,0.12)'],
    [ 24, -18,  16, -32,  4, -34, -10, -20, 3.0, hDk0,     null],
    [ 19, -22,  10, -38, -2, -38, -20, -26, 2.4, hLt0,     null]
  ];
  for(const L of fringe) tressS(L[0], L[1], L[2], L[3], L[4], L[5], L[6], L[7], L[8], 1.0, L[9], L[10]);
  X.restore();
  const hDk = hDk0, hLt = hLt0;
  skArc(0.4, -11.0, 1.9, -7.2, 0.8, -4.6, 4.4, '150,84,66', 0.14, { n: 7, thin: 0.7 });
  X.save(); X.globalCompositeOperation = 'screen';
  shade(-1.3, -5.4, 3.0, 2.4, 'rgba(255,246,236,0.32)', 0);
  X.restore();
  skArc(0.8, -4.6, 1.9, -3.0, 0.4, -2.6, 2.4, '132,66,54', 0.20, { n: 6, thin: 0.7 });
  X.fillStyle = 'rgba(96,44,40,0.32)';
  X.beginPath(); X.ellipse(1.5, -2.9, 0.72, 0.48, 0.25, 0, TAU); X.fill();

  const Eo = E.eye, roll = E.rolled;
  const eyeCol = G.char ? G.char.eyeColor : '#4a2c33';
  for(const s of [-1, 1]){
    const ex = s * 12.5, ey = -17;
    const openness = Eo * (1 - roll * 0.5);
    const inX = ex - s * 7.4, outX = ex + s * 7.4;
    const eyePath = () => {
      X.moveTo(inX, ey + 1.0);
      X.quadraticCurveTo(ex, ey - 6.4 * openness - 1.2, outX, ey + 0.5);
      X.quadraticCurveTo(ex, ey + 4.2 * openness + 1.4, inX, ey + 1.0);
      X.closePath();
    };
    if(openness > 0.10){
      X.beginPath(); eyePath();
      X.fillStyle = '#f4e8e3'; X.fill();
      // The upper lid has to cast a shadow onto the sclera. Without it a white
      // almond with a dot in it reads as a sticker no matter how good the iris is.
      X.save(); X.beginPath(); eyePath(); X.clip();
      X.globalCompositeOperation = 'multiply';
      shade(ex, ey - 2.2 * openness - 0.8, 8.4, 3.6 * openness + 1.5, 'rgba(122,72,70,0.44)', 0);
      shade(outX - s * 1.6, ey + 0.4, 3.4, 3.4 * openness + 1.2, 'rgba(122,72,70,0.30)', 0);
      X.restore();
      // iris: lit upper-left quadrant, deep limbal ring at the edge
      const irisX = ex + roll * 1.5, irisY = ey - 1.2 * openness - roll * 2;
      const irisR = 4.0 * Math.max(openness, 0.3);
      const irisG = X.createRadialGradient(irisX - irisR * 0.24, irisY - irisR * 0.32, irisR * 0.16,
                                           irisX, irisY, irisR);
      irisG.addColorStop(0,    skLight(eyeCol, 0.34));
      irisG.addColorStop(0.44, eyeCol);
      irisG.addColorStop(0.86, skDark(eyeCol, 0.30));
      irisG.addColorStop(1,    '#190a0d');
      X.fillStyle = irisG;
      X.beginPath(); X.arc(irisX, irisY, irisR, 0, TAU); X.fill();
      // radial striations under the limbal ring
      X.save(); X.beginPath(); X.arc(irisX, irisY, irisR, 0, TAU); X.clip();
      X.globalCompositeOperation = 'multiply';
      for(let k = 0; k < 6; k++){
        const a = k / 6 * TAU + 0.42;
        X.strokeStyle = `rgba(28,12,16,${0.12 + (k % 3) * 0.05})`;
        X.lineWidth = 0.55;
        X.beginPath();
        X.moveTo(irisX + Math.cos(a) * irisR * 0.36, irisY + Math.sin(a) * irisR * 0.36);
        X.lineTo(irisX + Math.cos(a) * irisR, irisY + Math.sin(a) * irisR);
        X.stroke();
      }
      X.restore();
      X.strokeStyle = 'rgba(14,5,9,0.6)'; X.lineWidth = 1.0;
      X.beginPath(); X.arc(irisX, irisY, irisR * 0.98, 0, TAU); X.stroke();
      X.fillStyle = '#0b0406';
      X.beginPath(); X.arc(irisX, irisY, irisR * 0.52, 0, TAU); X.fill();
      X.fillStyle = 'rgba(255,255,255,0.96)';
      X.beginPath(); X.arc(irisX - 1.1, irisY - 1.2, 1.25, 0, TAU); X.fill();
      X.fillStyle = 'rgba(255,250,250,0.40)';
      X.beginPath(); X.arc(irisX + 1.4, irisY + 0.7, 0.7, 0, TAU); X.fill();
      // lash wedge: heavy over the outer third, thinning to the inner corner
      X.fillStyle = 'rgba(28,12,16,0.92)';
      X.beginPath();
      X.moveTo(inX, ey + 0.9);
      X.quadraticCurveTo(ex, ey - 6.4 * openness - 1.4, outX, ey - 0.2);
      X.quadraticCurveTo(ex, ey - 6.4 * openness - 3.6, inX, ey - 0.9);
      X.closePath();
      X.fill();
      // lower lid: a thin LIT waterline, never a dark outline
      skArc(inX + s * 0.6, ey + 3.4 * openness + 1.2, ex, ey + 5.2 * openness + 1.7,
            outX - s * 0.6, ey + 3.0 * openness + 1.0,
            3.0, '255,238,226', 0.30, { n: 7, up: true, thin: 0.7 });
    } else {
      const closed = ey + 2.6;
      X.beginPath();
      X.moveTo(inX, closed);
      X.quadraticCurveTo(ex, closed + 2.6, outX, closed - 0.4);
      X.quadraticCurveTo(ex, closed - 0.9, inX, closed);
      X.closePath();
      X.fillStyle = 'rgba(38,18,22,0.9)'; X.fill();
      skArc(inX + s * 0.6, closed + 1.6, ex, closed + 3.4, outX - s * 0.6, closed + 1.2,
            2.8, '255,238,226', 0.24, { n: 6, up: true, thin: 0.7 });
    }
    // lid crease, then a tapered brow (a constant-width stroke looks drawn on)
    skArc(ex - s * 6.6, ey - 5.4 - E.brow * 3.0, ex, ey - 9.0 - E.brow * 4.4,
          ex + s * 6.6, ey - 4.8 - E.brow * 3.6,
          4.2, '136,78,62', 0.20, { n: 8, rot: s * 0.10, thin: 0.6 });
    skArc(ex - s * 8.4, ey - 8.0 - E.brow * 2.6, ex, ey - 12.0 - E.brow * 5.0,
          ex + s * 7.8, ey - 7.2 - E.brow * 3.4,
          6.4, hexToRgb(hairCol).join(','), 0.60, { n: 11, rot: s * 0.10, thin: 0.5 });
  }

  const mo = E.mouth, lipCol = G.char ? G.char.lipColor : '#b3555f';
  const lipDk = skDark(lipCol, 0.30);
  // philtrum grooves running down from the nose to the upper lip — kept faint and
  // short, or they read as two scars ruled down the middle of the face
  skArc(-2.6, -1.4, -1.7, 1.8, -1.2, 4.4, 2.4, '150,90,72', 0.09, { n: 5, thin: 0.6 });
  skArc( 1.6, -1.4,  1.5, 1.8,  1.0, 4.4, 2.4, '150,90,72', 0.07, { n: 5, thin: 0.6 });
  // the corners sink into the cheek whether the mouth is open or shut
  fSh(-7.2, 10.4, 2.8, 2.4, 'rgba(146,80,74,0.30)',  0.30);
  fSh( 7.2, 10.4, 2.8, 2.4, 'rgba(146,80,74,0.30)', -0.30);
  if(mo > 0.08){
    const mw = 7.5 + mo * 2.2, mh = 2.5 + mo * 7.5;
    // open mouth: the cavity fades at its rim. A hard-edged dark oval reads as a
    // hole punched through the face rather than as parted lips.
    const cav = X.createLinearGradient(0, 11 - mh, 0, 11 + mh);
    cav.addColorStop(0, 'rgba(76,17,25,0)');
    cav.addColorStop(0.16, '#4c1119');
    cav.addColorStop(0.84, '#4c1119');
    cav.addColorStop(1, 'rgba(76,17,25,0)');
    X.fillStyle = cav;
    X.beginPath(); X.ellipse(0, 11, mw, mh, 0, 0, TAU); X.fill();
    X.save();
    X.beginPath(); X.ellipse(0, 11, mw, mh, 0, 0, TAU); X.clip();
    X.fillStyle = 'rgba(206,74,96,0.35)';
    X.beginPath(); X.ellipse(0, 14 + mo * 5, 6.0, 2.6 + mo * 3.4, 0, 0, TAU); X.fill();
    X.restore();
    if(mo > 0.22){
      // upper teeth: a lit strip whose lower edge falls into the cavity — a
      // full-white bar reads as dentures
      X.fillStyle = 'rgba(250,242,238,0.88)';
      X.beginPath(); X.ellipse(0, 8 + mo * 1.0, 4.4, 1.4, 0, 0, TAU); X.fill();
      X.save(); X.globalCompositeOperation = 'multiply';
      shade(0, 9.4 + mo * 1.0, 4.4, 0.8, 'rgba(148,90,90,0.30)', 0);
      X.restore();
    }
    X.fillStyle = lipDk;
    X.beginPath(); X.ellipse(0, 8.4, mw - 0.4, 1.8 + mo * 1.2, 0, Math.PI, 0); X.fill();
    X.fillStyle = `rgba(${hexToRgb(skLight(lipCol, 0.06)).join(',')},0.82)`;
    X.beginPath(); X.ellipse(0, 12 + mo * 3.5, 5.2, 2.2, 0, 0, TAU); X.fill();
  } else {
    // closed: upper lip with a cupid's bow, lower lip fuller and catching the lamp
    X.fillStyle = lipDk;
    X.beginPath();
    X.moveTo(-8.4, 10.6);
    X.quadraticCurveTo(-4.4, 8.4, -1.7, 9.6);
    X.quadraticCurveTo(0, 10.2, 1.7, 9.6);
    X.quadraticCurveTo(4.4, 8.4, 8.4, 10.6);
    X.quadraticCurveTo(4.2, 12.0, 0, 12.0);
    X.quadraticCurveTo(-4.2, 12.0, -8.4, 10.6);
    X.closePath(); X.fill();
    const lipG = X.createLinearGradient(0, 10.4, 0, 14.4);
    lipG.addColorStop(0, skLight(lipCol, 0.16));
    lipG.addColorStop(0.55, lipCol);
    lipG.addColorStop(1, lipDk);
    X.fillStyle = lipG;
    X.beginPath();
    X.moveTo(-7.6, 11.8);
    X.quadraticCurveTo(-4.0, 14.8, 0, 14.8);
    X.quadraticCurveTo(4.0, 14.8, 7.6, 11.8);
    X.quadraticCurveTo(4.0, 12.6, 0, 12.6);
    X.quadraticCurveTo(-4.0, 12.6, -7.6, 11.8);
    X.closePath(); X.fill();
    // the lip line itself
    skArc(-6.8, 11.5, 0, 13.0, 6.8, 11.5, 2.2, '104,40,50', 0.32, { n: 9, thin: 0.55 });
  }
  X.save(); X.globalCompositeOperation = 'screen';
  shade(2.0, 13.0 + mo * 3, 3.0, 1.3, 'rgba(255,250,250,0.5)', 0);
  X.restore();

  // temple locks falling past the jaw, over the cheek edge
  X.save(); X.globalAlpha = 0.9;
  tressS(-31, -12, -40, 2, -40, 12, -34, 24, 3.0, 1.2, hDk);
  tressS(31, -12, 40, 2, 40, 12, 34, 24, 3.0, 1.2, hairCol);
  X.restore();
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
  // He kneels inclined over her, so his trunk axis runs nearly ALONG the lamp
  // direction: the shoulder end is lamp-side and the pelvis end falls into
  // shadow. That ramp was always right — what made him a flat slab was that
  // nothing modelled the cylinder: no spine furrow, no chest-side terminator,
  // no scapula, no iliac crest.
  skForm(torso, T, [shld[0] - 16, shld[1] - 30], [pel[0] + 18, pel[1] + 26],
    { edge: 0.24, rim: 0.16, cool: 0.10 });
  skClipIn(torso, () => {
    // chest-side terminator: the whole under surface turns away from the lamp
    fSh(mid[0] - 10, mid[1] + 20, 96, 16, 'rgba(84,40,24,0.24)', 0.30);
    // latissimus edge sweeping from the armpit down to the hip
    skArc(shld[0] - 4, shld[1] + 6,
          mid[0] - 6, mid[1] + 10,
          pel[0] - 8, pel[1] + 6,
          12, '110,54,32', 0.14, { n: 12, rot: 0.32 });
    // spine furrow along the back, with the erector ridge beside it
    skArc(shld[0] + 2, shld[1] - 10,
          mid[0] + 12, mid[1] - 4,
          pel[0] + 10, pel[1] - 10,
          9, '96,46,26', 0.11, { n: 13, rot: -0.30 });
    // iliac crest and glute fold at the pelvis
    skArc(pel[0] + 6, pel[1] - 14, pel[0] + 14, pel[1] - 6, pel[0] + 16, pel[1] + 6,
          8, '104,50,28', 0.12, { n: 8, rot: 0.3 });
    fHi(shld[0] + 6, shld[1] - 18, 40, 8, 'rgba(255,232,204,0.18)', 0.32);
    fAO(shld[0], shld[1], 18, 12, 0.20);
    fAO(pel[0], pel[1] + 6, 22, 10, 0.16);
  });
  skLine(torso, T, 1.25, 0.18);
  // shadow he casts on her beneath his pelvis/torso
  fAO(pel[0] - 6, pel[1] + 26, 40, 12, 0.34, 0.3);

  // ---- near leg kneeling on bed ----
  limbS([pel[0] + 16, pel[1] + 12], [800, 556], 28, 18, T, { belly: 1.14, aoA: 0.32 });
  limbS([800, 556], [884, 564], 17, 10, T, { belly: 1.2 });
  footS(890, 566, 0.28, 1.0, T);

  // ---- neck + head leaning over her ----
  // His head gets the same form treatment as hers: a real skull silhouette
  // (brow ledge, hollow temples, jaw plane), directional lamp shading, and
  // distinct brow/nose/mouth forms — three strokes on an oval read as a mask.
  neckS([head[0] + 8, head[1] + 12], [shld[0] + 2, shld[1] - 4], 15, T);
  X.save();
  X.translate(head[0], head[1]);
  X.rotate(0.45 + d * 0.06);
  // skull: crown, brow ledge, sunken temple, cheek, jaw corner, chin
  const skull = () => {
    X.moveTo(-20, -22);
    X.bezierCurveTo(-24, -40, 10, -42, 24, -34);
    X.bezierCurveTo(33, -29, 34, -18, 29, -12);
    X.bezierCurveTo(26, -4, 27, 6, 21, 14);
    X.bezierCurveTo(14, 23, 2, 26, -9, 22);
    X.bezierCurveTo(-20, 22, -27, 15, -28, 4);
    X.bezierCurveTo(-29, -7, -27, -16, -20, -22);
    X.closePath();
  };
  X.beginPath(); skull();
  const hg = X.createRadialGradient(-9, -15, 3, -2, 0, 42);
  hg.addColorStop(0, T.hi); hg.addColorStop(0.42, T.b); hg.addColorStop(0.74, T.s); hg.addColorStop(1, T.d);
  X.fillStyle = hg; X.fill();
  // ear wedge tucked behind the jaw
  X.beginPath();
  X.moveTo(-12, 16); X.bezierCurveTo(-24, 24, -33, 16, -33, 5);
  X.bezierCurveTo(-37, -3, -27, -8, -20, -11); X.closePath();
  X.fillStyle = skDark(T.b, 0.12); X.fill();
  skClipIn(skull, () => {
    // form: lamp is up-left, he leans away from it — brow ridge lit, sockets
    // and under-cheek hollow turn away, jaw goes shadowed
    fHi(-6, -20, 17, 7, 'rgba(255,232,200,0.20)', -0.18);        // brow ridge light
    fSh(-4, -7, 7, 5, 'rgba(90,45,30,0.20)', 0.25);              // eye socket
    fSh(14, -2, 10, 9, 'rgba(140,72,52,0.16)', -0.20);           // near temple/cheek turn
    fSh(-18, 2, 8, 10, 'rgba(120,58,42,0.14)', 0.30);            // far temple hollow
    fSh(2, 16, 13, 6, 'rgba(130,66,48,0.16)', -0.08);            // under-cheek
    fSh(16, 20, 10, 5, 'rgba(110,50,38,0.18)', 0.10);            // jaw underside
    fHi(6, 8, 6, 3.4, 'rgba(255,238,214,0.14)', 0.1);            // cheekbone catch
    fSSS(-10, 10, 8, 5, 0.3);                                    // ear/jaw SSS
  });
  // deep-set downcast eye: socket shadow, heavy lid, lash line — no white
  X.save(); X.globalCompositeOperation = 'multiply';
  X.strokeStyle = 'rgba(58,28,18,0.62)'; X.lineWidth = 2.4; X.lineCap = 'round';
  X.beginPath(); X.moveTo(-15, -6.5); X.quadraticCurveTo(-10, -3.5, -4.5, -6); X.stroke();
  // brow: single tapered mass, not a wire
  X.strokeStyle = 'rgba(42,22,12,0.55)'; X.lineWidth = 3.0;
  X.beginPath(); X.moveTo(-17, -13); X.quadraticCurveTo(-10, -11.5, -4, -14); X.stroke();
  // nose: bridge plane + nostril shadow + lit underside
  X.strokeStyle = 'rgba(130,68,42,0.40)'; X.lineWidth = 1.6;
  X.beginPath(); X.moveTo(-13, -3); X.quadraticCurveTo(-17, 5, -14.5, 10.5); X.stroke();
  X.restore();
  X.save(); X.globalCompositeOperation = 'screen';
  shade(-13.5, 12.5, 4.2, 1.7, 'rgba(255,226,196,0.30)', 0.1);   // nose underside light
  X.restore();
  // mouth: parted lip line with lower-lip light, no flat stroke
  X.save(); X.globalCompositeOperation = 'multiply';
  X.strokeStyle = 'rgba(112,48,38,0.55)'; X.lineWidth = 2.0;
  X.beginPath(); X.moveTo(-14.5, 16.5); X.quadraticCurveTo(-10, 18.5, -5.5, 17); X.stroke();
  X.restore();
  X.save(); X.globalCompositeOperation = 'screen';
  shade(-9.5, 20, 4.6, 1.3, 'rgba(255,232,208,0.20)', 0);        // lower-lip light
  X.restore();
  // jaw + cheek stubble: blue-brown shade only over the lower third
  X.save(); X.beginPath(); skull(); X.clip();
  X.globalCompositeOperation = 'multiply';
  shade(4, 20, 22, 11, 'rgba(70,45,40,0.16)', 0.05);
  X.restore();
  // short tousled hair with layered mass + sheen
  hairMassS(2, -22, 27, 17, -0.15, '#1c100a');
  tressS(-14, -30, -6, -38, 6, -38, 14, -27, 8, 2, '#1c100a', 'rgba(255,210,170,0.16)');
  tressS(-22, -16, -28, -6, -28, 4, -24, 10, 6, 2, '#1c100a');
  tressS(-20, -28, -26, -18, -28, -10, -26, -4, 6, 2, '#1c100a');
  // profile nose tip beyond the silhouette
  X.fillStyle = T.b; X.beginPath(); X.ellipse(9, 4, 5.5, 7.5, 0.1, 0, TAU); X.fill();
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
  const K = [702 + wrap * 6, 540 + wrap * 4];
  const A = [766 + wrap * 8, 550 + wrap * 4];

  // contact shadow under the thigh resting on the mattress
  fAO(lerp(hip[0], K[0], 0.5), 552, 58, 10, 0.35);
  limbS(hip, K, 26, 16.5, T, { belly: 1.14, aoA: 0.3 });
  fHi(K[0] + 2, K[1] - 6, 9, 11, 'rgba(255,240,225,0.34)', 0.2);
  limbS(K, A, 14.5, 8, T, { belly: 1.2 });
  footS(A[0] + 4, A[1] + 2, 0.30 - wrap * 0.22, 1.05, T);

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
