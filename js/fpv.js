// Afterglow — module: fpv (loaded by index.html)
// Ultra-High-Fidelity First-Person Perspective Engine
'use strict';

/* ============================================================
   FIRST-PERSON KINEMATICS & SENSORY SHIVER
   ============================================================ */
function fpvTremor(){
  const org = G.state === 'orgasm' ? Math.sin(Math.PI * clamp((G.orgT || 0) / 5.2, 0, 1)) : 0;
  const highAr = clamp(((G.ar || 0) - 60) / 40, 0, 1);
  const shiver = org * (Math.sin(G.t * 38) * 0.65 + Math.sin(G.t * 54) * 0.35 + Math.cos(G.t * 19) * 0.2) * 5.2;
  const arousalTremor = highAr * (Math.sin(G.t * 22) * 0.15 + Math.cos(G.t * 31) * 0.1) * 2.0;
  return shiver + arousalTremor;
}

/* ============================================================
   ENVIRONMENT: CINEMATIC BOUDOIR & RECEDING PERSPECTIVE BED
   ============================================================ */
function drawFPVRoom(){
  // Atmospheric background: deep midnight burgundy & ambient shadows
  X.fillStyle = '#0f090d';
  X.fillRect(0, 0, W, H);

  // Far wall & ceiling with soft perspective corner shadow
  const wallG = X.createLinearGradient(0, 0, 0, 300);
  wallG.addColorStop(0, '#1a1016');
  wallG.addColorStop(0.7, '#130c11');
  wallG.addColorStop(1, '#0c070a');
  X.fillStyle = wallG;
  X.fillRect(0, 0, W, 280);

  // Far window above headboard spilling cool moonlight
  const wx = 960, wy = 24, ww = 180, wh = 140;
  X.fillStyle = 'rgba(120,145,170,0.06)';
  X.fillRect(wx, wy, ww, wh);
  const moonG = X.createRadialGradient(wx + 130, wy + 35, 6, wx + 130, wy + 35, 65);
  moonG.addColorStop(0, 'rgba(230,242,255,0.22)');
  moonG.addColorStop(0.4, 'rgba(180,210,240,0.06)');
  moonG.addColorStop(1, 'rgba(0,0,0,0)');
  X.save(); X.globalCompositeOperation = 'screen';
  X.fillStyle = moonG;
  X.beginPath(); X.arc(wx + 130, wy + 35, 65, 0, TAU); X.fill();
  X.fillStyle = 'rgba(235,245,255,0.85)';
  X.beginPath(); X.arc(wx + 130, wy + 35, 16, 0, TAU); X.fill();
  X.restore();

  // Window mullions
  X.strokeStyle = 'rgba(25,16,22,0.85)';
  X.lineWidth = 4;
  X.strokeRect(wx, wy, ww, wh);
  X.beginPath();
  X.moveTo(wx + ww / 2, wy); X.lineTo(wx + ww / 2, wy + wh);
  X.moveTo(wx, wy + wh * 0.45); X.lineTo(wx + ww, wy + wh * 0.45);
  X.stroke();

  // Bedside lamp (warm left illumination casting directional golden light)
  const lx = 180, ly = 110;
  const lampG = X.createRadialGradient(lx, ly, 8, lx, ly, 460);
  lampG.addColorStop(0, 'rgba(255,195,125,0.28)');
  lampG.addColorStop(0.35, 'rgba(255,165,95,0.12)');
  lampG.addColorStop(0.7, 'rgba(255,140,80,0.03)');
  lampG.addColorStop(1, 'rgba(0,0,0,0)');
  X.save(); X.globalCompositeOperation = 'screen';
  X.fillStyle = lampG;
  X.beginPath(); X.arc(lx, ly, 460, 0, TAU); X.fill();
  X.restore();

  // Lamp fixture silhouette
  X.fillStyle = '#2d1820';
  X.beginPath();
  X.moveTo(148, 134); X.lineTo(212, 134); X.lineTo(198, 98); X.lineTo(162, 98);
  X.closePath(); X.fill();
  X.fillStyle = 'rgba(255,215,145,0.7)';
  X.beginPath();
  X.moveTo(162, 100); X.lineTo(198, 100); X.lineTo(206, 132); X.lineTo(154, 132);
  X.closePath(); X.fill();

  // Mattress & crumpled satin sheets in dramatic perspective receding from camera
  const sheetG = X.createLinearGradient(0, 260, 0, H);
  sheetG.addColorStop(0, '#421d28');
  sheetG.addColorStop(0.35, '#2e131b');
  sheetG.addColorStop(0.75, '#1e0a11');
  sheetG.addColorStop(1, '#11050a');
  X.fillStyle = sheetG;
  X.fillRect(0, 250, W, H - 250);

  // Dynamic tension wrinkles radiating from her hips, back, and thighs
  const bounce = (G.depth || 0) * 8 + (G.impact || 0) * 6;
  X.save();
  X.globalCompositeOperation = 'multiply';
  X.strokeStyle = 'rgba(12,3,6,0.52)';
  X.lineWidth = 3.2;
  for(let i = 0; i < 7; i++){
    const y = 380 + i * 46;
    X.beginPath();
    X.moveTo(70 + ((i * 61) % 120), y);
    X.bezierCurveTo(460, y - 26 + ((i * 29) % 24) + bounce * 0.4, 820, y + 16 - ((i * 19) % 20), 1210 - ((i * 53) % 140), y);
    X.stroke();
  }
  X.restore();

  // Satin highlight luster catching bedside lamp
  X.save();
  X.globalCompositeOperation = 'soft-light';
  X.strokeStyle = 'rgba(255,220,195,0.22)';
  X.lineWidth = 4.5;
  for(let i = 0; i < 5; i++){
    const y = 390 + i * 50;
    X.beginPath();
    X.moveTo(100 + ((i * 53) % 90), y - 6);
    X.bezierCurveTo(520, y - 28 + ((i * 23) % 20), 760, y + 8, 1160, y - 4);
    X.stroke();
  }
  X.restore();

  // Pillow under her head (dim bedding in lamplight, not a glow)
  const pilG = X.createLinearGradient(640 - 170, 120, 640 + 170, 214);
  pilG.addColorStop(0, '#43332a');
  pilG.addColorStop(0.35, '#382b22');
  pilG.addColorStop(0.75, '#281e17');
  pilG.addColorStop(1, '#1d1611');
  X.fillStyle = pilG;
  X.beginPath();
  X.ellipse(640, 170, 170, 44, -0.01, 0, TAU);
  X.fill();

  // Head depression shadow
  X.save();
  X.globalCompositeOperation = 'multiply';
  const headIndent = X.createRadialGradient(640, 158, 6, 640, 158, 44);
  headIndent.addColorStop(0, 'rgba(80,55,40,0.35)');
  headIndent.addColorStop(0.65, 'rgba(90,60,45,0.14)');
  headIndent.addColorStop(1, 'rgba(0,0,0,0)');
  X.fillStyle = headIndent;
  X.beginPath();
  X.ellipse(640, 158, 70, 22, 0, 0, TAU);
  X.fill();
  X.restore();

  // Soft body contact ambient occlusion on sheets
  X.save();
  X.globalCompositeOperation = 'multiply';
  shade(640, 485 + bounce * 0.4, 210, 140, 'rgba(10,3,7,0.58)', 0);
  X.restore();
}

/* ============================================================
   HER HEAD: LOOKING UP AT PLAYER (FORESHORTENED, DEVOTIONAL)
   ============================================================ */
function drawFPVHead(E){
  const breathe = Math.sin((G.t || 0) * TAU * 0.33) * 1.8;
  const ks = 1 + (G.kiss || 0) * 0.12;
  const hx = 640 + Math.sin((G.t || 0) * 0.5) * 2.2;
  const hy = 148 + breathe * 0.45 + (G.pleasure || 0) * 0.025;
  const hairCol = G.char ? G.char.hairColor : '#231318';

  // Hair hugging the head — layered mass cradling it, no halo ring
  hairMassS(hx, hy + 16, 80, 50, 0, hairCol);
  X.save();
  X.beginPath(); X.ellipse(hx, hy + 16, 80, 50, 0, 0, TAU); X.clip();
  X.strokeStyle = skDark(hairCol, 0.45); X.lineWidth = 2.4; X.lineCap = 'round';
  for(let i = 0; i < 5; i++){
    X.beginPath();
    X.moveTo(hx - 32 + i * 16, hy - 8);
    X.quadraticCurveTo(hx - 52 + i * 26, hy + 24, hx - 70 + i * 35, hy + 58);
    X.stroke();
  }
  X.globalCompositeOperation = 'soft-light';
  shade(hx, hy + 2, 62, 20, 'rgba(255,235,220,0.20)', 0);
  X.restore();

  // Silky locks spilling down onto the pillow beside her neck
  for(let i = 0; i < 6; i++){
    const s = i < 3 ? -1 : 1, k = i % 3;
    const bx = hx + s * (30 + k * 10), by = hy + 26 + k * 6;
    const c1x = hx + s * (48 + k * 12), c1y = by + 26;
    const c2x = hx + s * (56 + k * 14), c2y = by + 54;
    const tx = hx + s * (50 + k * 14), ty = hy + 96 + k * 12;
    tressS(bx, by, c1x, c1y, c2x, c2y, tx, ty, 13 - k * 2.2, 3, hairCol, 'rgba(255,200,210,0.14)');
  }

  X.save();
  X.translate(hx, hy);
  X.scale(ks, ks);
  X.rotate(E.tilt * 0.35 + (G.nod || 0) * 0.1 + Math.sin((G.t || 0) * TAU * 0.33) * 0.015);

  const fpvSk = getSkin();

  // Neck: slim column with a gentle waist, SCM cords, suprasternal notch,
  // and soft trapezius slopes melting into the clavicle (never a solid cone)
  const nkTop = 30;
  const neckPath = () => {
    X.moveTo(-11, nkTop);
    X.bezierCurveTo(-12.5, 44, -13, 56, -16, 68);
    X.bezierCurveTo(-19, 78, -26, 84, -34, 88);
    X.bezierCurveTo(-20, 99, 20, 99, 34, 88);
    X.bezierCurveTo(26, 84, 19, 78, 16, 68);
    X.bezierCurveTo(13, 56, 12.5, 44, 11, nkTop);
    X.closePath();
  };
  X.beginPath(); neckPath();
  const nkg = X.createLinearGradient(0, nkTop, 0, 92);
  nkg.addColorStop(0, fpvSk.herSh); nkg.addColorStop(0.45, fpvSk.her); nkg.addColorStop(1, fpvSk.herSh);
  X.fillStyle = nkg; X.fill();
  skClipIn(neckPath, () => {
    // throat shadow tucked under the jaw
    fAO(0, nkTop + 3, 13, 7, 0.42);
    // sternocleidomastoid cords running down each side
    fSh(-8, 52, 3.2, 16, 'rgba(175,100,80,0.22)', 0.10);
    fSh(8, 52, 3.2, 16, 'rgba(175,100,80,0.22)', -0.10);
    // one slim front highlight instead of a full-column fill
    fHi(0, 58, 4.5, 15, 'rgba(255,240,225,0.22)');
    // suprasternal notch dip at the base
    fAO(0, 90, 6, 4, 0.35);
    // trapezius slopes softening outward into the shoulders
    fSh(-27, 86, 12, 7, 'rgba(175,100,80,0.16)', 0.5);
    fSh(27, 86, 12, 7, 'rgba(175,100,80,0.16)', -0.5);
  });

  // Jawline & chin foreshortened looking upward
  X.fillStyle = sg(-52, 14, fpvSk.her, fpvSk.herSh);
  X.beginPath();
  X.ellipse(0, -2, 44, 49, 0, 0, TAU);
  X.fill();
  X.beginPath();
  X.moveTo(-40, -8);
  X.bezierCurveTo(-36, 35, -12, 46, 0, 47);
  X.bezierCurveTo(12, 46, 36, 35, 40, -8);
  X.closePath();
  X.fill();

  // Cheekbone, jaw hollow, and bridge shading (kept sheer so skin stays luminous)
  X.save();
  X.globalCompositeOperation = 'multiply';
  shade(26, 12, 22, 14, 'rgba(170,95,72,0.17)', 0.38);
  shade(-26, 12, 22, 14, 'rgba(170,95,72,0.17)', -0.38);
  shade(0, 42, 26, 11, 'rgba(170,95,72,0.16)', 0);
  X.restore();

  // Soft light highlight along forehead & chin tip
  X.save();
  X.globalCompositeOperation = 'soft-light';
  shade(0, -28, 18, 28, 'rgba(255,240,225,0.42)', 0);
  shade(0, 40, 10, 6, 'rgba(255,240,225,0.35)', 0);
  X.restore();

  // Eyes: gazing directly up into the player's eyes with moist specular reflections
  const Eo = E.eye, roll = E.rolled;
  const eyeCol = G.char ? G.char.eyeColor : '#4a2c33';
  for(const s of [-1, 1]){
    const ex = s * 17.5, ey = -9;
    const openness = Eo * (1 - roll * 0.5);

    // Eye socket shadow
    X.save();
    X.globalCompositeOperation = 'multiply';
    shade(ex, ey + 1, 12, 8, 'rgba(145,85,75,0.18)', s * 0.08);
    X.restore();

    if(openness > 0.12){
      // Sclera with soft pink vascularity at corners
      X.beginPath();
      X.moveTo(ex - 10.5, ey + 1);
      X.quadraticCurveTo(ex, ey - 7.5 * openness - 2, ex + 10.5, ey + 1);
      X.quadraticCurveTo(ex, ey + 6.5 * openness, ex - 10.5, ey + 1);
      X.closePath();
      X.fillStyle = '#faf6f3';
      X.fill();

      // Caruncle
      X.fillStyle = 'rgba(215,145,140,0.45)';
      X.beginPath();
      X.ellipse(ex - s * 9, ey + 1, 2.2, 1.8, 0, 0, TAU);
      X.fill();

      // Iris looking upward at player
      const irisY = ey - 2.2 * openness - (roll * 3.5);
      const irisX = ex + roll * 3.8;
      const irisR = 5.2 * Math.max(openness, 0.28);
      X.fillStyle = eyeCol;
      X.beginPath();
      X.arc(irisX, irisY, irisR, 0, TAU);
      X.fill();

      // Deep pupil
      X.fillStyle = '#110609';
      X.beginPath();
      X.arc(irisX, irisY, irisR * 0.52, 0, TAU);
      X.fill();

      // Primary & secondary specular catchlights (reflecting bedroom lamp)
      X.fillStyle = 'rgba(255,255,255,0.92)';
      X.beginPath();
      X.arc(irisX + 1.5, irisY - 1.8, 1.5, 0, TAU);
      X.fill();
      X.fillStyle = 'rgba(255,255,255,0.45)';
      X.beginPath();
      X.arc(irisX - 1.6, irisY + 1.2, 0.8, 0, TAU);
      X.fill();

      // Upper eyelid line & dense dark eyelashes
      X.strokeStyle = 'rgba(46,20,26,0.96)';
      X.lineWidth = 2.4;
      X.beginPath();
      X.moveTo(ex - 11, ey + 1);
      X.quadraticCurveTo(ex, ey - 8 * openness - 2, ex + 11, ey + 1);
      X.stroke();

      // Individual lashes
      X.lineWidth = 1.1;
      for(let l = 0; l < 5; l++){
        const t = l / 4;
        const lx = ex + (-9 + t * 18);
        const ly = ey - 6 * openness - Math.sin(t * Math.PI) * 2;
        X.beginPath();
        X.moveTo(lx, ly);
        X.lineTo(lx + s * 1.5, ly - 3.8);
        X.stroke();
      }
    } else {
      // Eyelids closed in ecstasy
      X.strokeStyle = 'rgba(46,20,26,0.96)';
      X.lineWidth = 2.4;
      X.beginPath();
      X.moveTo(ex - 10.5, ey + 1);
      X.quadraticCurveTo(ex, ey + 4.2, ex + 10.5, ey + 1);
      X.stroke();
    }

    // Subtle eyeliner flick
    X.strokeStyle = 'rgba(46,20,26,0.85)';
    X.lineWidth = 1.4;
    X.beginPath();
    X.moveTo(ex + (s > 0 ? 10.5 : -10.5), ey + 1);
    X.lineTo(ex + (s > 0 ? 14 : -14), ey - 2.5);
    X.stroke();

    // Arched soft eyebrow
    X.strokeStyle = hairCol;
    X.lineWidth = 2.1;
    X.beginPath();
    X.moveTo(ex - 9, ey - 14);
    X.quadraticCurveTo(ex + 1, ey - 18 - E.brow * 6, ex + 10, ey - 13 - E.brow * 8);
    X.stroke();
  }

  // Tears of overwhelming pleasure gathering at outer eye corners
  if(G.pleasure > 70 || G.state === 'orgasm'){
    const tearA = clamp(((G.pleasure - 70) / 30), 0, 1) * 0.75;
    X.fillStyle = `rgba(235,248,255,${tearA})`;
    X.beginPath(); X.arc(-22, -6, 2.2, 0, TAU); X.fill();
    X.beginPath(); X.arc(22, -6, 2.2, 0, TAU); X.fill();
  }

  // Flushed cheeks (subsurface capillary vasocongestion)
  const [brR, brG, brB] = hexToRgb(G.char ? G.char.blushColor : '#e86070');
  shade(-27, 9, 17, 11, `rgba(${brR},${brG},${brB},${0.10 + E.blush * 0.26})`, 0.28);
  shade(27, 9, 17, 11, `rgba(${brR},${brG},${brB},${0.10 + E.blush * 0.26})`, -0.28);

  // Cute upturned nose tip with soft nostril wings
  X.strokeStyle = 'rgba(165,95,82,0.48)';
  X.lineWidth = 1.8;
  X.beginPath();
  X.moveTo(-3, 0); X.quadraticCurveTo(-6, 6, -3.5, 9);
  X.quadraticCurveTo(0, 11, 3.5, 9); X.quadraticCurveTo(6, 6, 3, 0);
  X.stroke();
  X.fillStyle = 'rgba(255,240,230,0.35)';
  X.beginPath(); X.ellipse(0, 3, 2.4, 2, 0, 0, TAU); X.fill();

  // Lips: parted with Cupid's bow, wet inner mucosa, teeth, and tongue
  const mo = E.mouth;
  const lipCol = G.char ? G.char.lipColor : '#b3555f';
  if(mo > 0.05){
    // Dark open oral cavity
    X.fillStyle = '#420f18';
    X.beginPath();
    X.ellipse(0, 29, 9 + mo * 4, 3 + mo * 11, 0, 0, TAU);
    X.fill();

    // Subtle translucent pearlescent upper teeth
    if(mo > 0.22){
      X.fillStyle = 'rgba(252,246,244,0.92)';
      X.beginPath();
      X.ellipse(0, 24.5 + mo * 2, 6.5 + mo * 2, 2.6, 0, 0, TAU);
      X.fill();
    }

    // Moist soft pink tongue resting inside
    X.fillStyle = 'rgba(205,100,118,0.95)';
    X.beginPath();
    X.ellipse(0, 32 + mo * 4.5, 7.5 + mo * 2.2, 2.4 + mo * 3.4, 0, 0, TAU);
    X.fill();

    // Upper lip with Cupid's bow
    X.strokeStyle = lipCol;
    X.lineWidth = 2.2;
    X.beginPath();
    X.moveTo(-10, 26.5);
    X.quadraticCurveTo(-4, 23.5 - mo * 1.5, 0, 25.5);
    X.quadraticCurveTo(4, 23.5 - mo * 1.5, 10, 26.5);
    X.stroke();

    // Plump glossy lower lip
    X.strokeStyle = lipCol;
    X.lineWidth = 2.4;
    X.beginPath();
    X.moveTo(-9, 29 + mo * 4);
    X.quadraticCurveTo(0, 34.5 + mo * 9.5, 9, 29 + mo * 4);
    X.stroke();

    // Wet lip gloss specular reflections
    X.fillStyle = 'rgba(255,250,248,0.55)';
    X.beginPath();
    X.ellipse(0, 33 + mo * 7.5, 4.2, 1.6, 0, 0, TAU);
    X.fill();

    // Glistening saliva strand across parted lips when panting
    if(mo > 0.4 && (G.pleasure > 50 || G.oral > 0.3)){
      X.strokeStyle = 'rgba(255,248,244,0.52)';
      X.lineWidth = 1.3;
      X.beginPath();
      X.moveTo(-5, 27);
      X.quadraticCurveTo(-3, 31 + mo * 4, -4, 33 + mo * 6);
      X.stroke();
    }
  } else {
    // Closed or relaxed soft pout
    X.strokeStyle = lipCol;
    X.lineWidth = 2.4;
    X.beginPath();
    X.moveTo(-9, 28);
    X.quadraticCurveTo(0, 30.5, 9, 28);
    X.stroke();
    X.fillStyle = 'rgba(255,245,240,0.45)';
    X.beginPath();
    X.ellipse(0, 29.5, 4.5, 1.6, 0, 0, TAU);
    X.fill();
  }

  // Delicate beauty mark
  X.fillStyle = 'rgba(85,42,45,0.7)';
  X.beginPath(); X.arc(11, 21, 1.3, 0, TAU); X.fill();

  // Bangs: full fringe sitting on the forehead with a soft scalloped hairline
  X.fillStyle = hairCol;
  X.beginPath();
  X.moveTo(-46, -12);
  X.bezierCurveTo(-42, -48, -18, -60, 2, -58);
  X.bezierCurveTo(24, -58, 44, -46, 46, -12);
  X.quadraticCurveTo(30, -22, 14, -24);
  X.quadraticCurveTo(0, -26, -14, -24);
  X.quadraticCurveTo(-30, -22, -46, -12);
  X.closePath();
  X.fill();
  // strand separations in the fringe
  X.strokeStyle = 'rgba(0,0,0,0.35)'; X.lineWidth = 1.4;
  for(const sx of [-24, -8, 8, 24]){
    X.beginPath(); X.moveTo(sx, -52 + Math.abs(sx) * 0.15);
    X.quadraticCurveTo(sx + 1, -38, sx - 1, -27); X.stroke();
  }

  // Hair sheen
  X.save();
  X.globalCompositeOperation = 'screen';
  X.strokeStyle = 'rgba(215,155,165,0.32)';
  X.lineWidth = 2.4;
  X.beginPath();
  X.moveTo(-26, -48);
  X.quadraticCurveTo(-8, -56, 14, -50);
  X.stroke();
  X.restore();

  // Blink eyelid cover
  if((G.blinkPh || 0) > 0){
    X.strokeStyle = 'rgba(65,32,38,0.95)';
    X.lineWidth = 3.2;
    for(const s of [-1, 1]){
      X.beginPath();
      X.moveTo(s * 17.5 - 11, -8);
      X.lineTo(s * 17.5 + 11, -7.5);
      X.stroke();
    }
  }

  X.restore();
}

/* ============================================================
   HER SUPINE BODY: GRAVITY BREASTS, BELLY, THIGHS & THE SPOT
   ============================================================ */
function drawFPVBody(E, br){
  const trem = fpvTremor();
  const bounce = (G.depth || 0) * 8 + (G.impact || 0) * 6;
  const cx = 640, topY = 206 + bounce * 0.3;
  const sk = getSkin();
  const [brR, brG, brB] = hexToRgb(G.char ? G.char.blushColor : '#e86070');
  const bsz = 0.72 + (G.char ? G.char.breastSize : 0.45) * 0.62;
  const bdy = 0.82 + (G.char ? G.char.bodyScale : 0.45) * 0.42;

  // ---- LEGS: SPREAD TOWARD CAMERA, FORESHORTENED IN PERSPECTIVE ----
  const T = herT();
  const spread = (1.0 + (G.pleasure || 0) * 0.0016) * bdy;
  for(const s of [-1, 1]){
    const hip = [cx + s * 64 * bdy, 556 + bounce];
    const knee = [cx + s * (204 * spread) + trem * s, 642 + bounce * 0.6];
    const foot = [cx + s * (274 * spread), 758];

    limbS(knee, foot, 34 * bdy, 22 * bdy, T, { belly: 1.16 });
    limbS(hip, knee, 46 * bdy, 37 * bdy, T, { belly: 1.1, aoA: 0.32 });

    // Knee cap blend: continuous tone across the thigh/calf joint
    X.save();
    X.beginPath(); X.ellipse(knee[0], knee[1], 34 * bdy, 28 * bdy, s * 0.42, 0, TAU);
    const kg = X.createRadialGradient(knee[0] - s * 8, knee[1] - 10, 4, knee[0], knee[1], 36 * bdy);
    kg.addColorStop(0, T.b); kg.addColorStop(0.7, T.b); kg.addColorStop(1, T.s);
    X.fillStyle = kg; X.fill();
    X.restore();
    fHi(knee[0], knee[1] - 10, 13 * bdy, 15, 'rgba(255,238,220,0.08)');

    // Vastus medialis inner thigh teardrop contour
    X.strokeStyle = 'rgba(185,115,95,0.16)';
    X.lineWidth = 2.4;
    X.beginPath();
    X.moveTo(hip[0] + s * 22, hip[1] - 28);
    X.bezierCurveTo(knee[0] - s * 10, knee[1] - 62, knee[0] + s * 4, knee[1] - 26, knee[0] + s * 2, knee[1] - 12);
    X.stroke();

    // Inguinal groove (crease where thigh meets pelvis)
    X.strokeStyle = 'rgba(165,95,75,0.22)';
    X.lineWidth = 2.2;
    X.beginPath();
    X.moveTo(cx + s * 44 * bdy, 528 + bounce);
    X.quadraticCurveTo(cx + s * 115 * bdy, 566 + bounce, cx + s * 158 * bdy, 620);
    X.stroke();
  }

  // ---- ARMS: drawn under the torso at rest so the shoulder joint reads connected ----
  const a2 = sm(55, 80, G.pleasure || 0);
  const drawArms = () => {
    for(const s of [-1, 1]){
      const sh = [cx + s * 58 * bdy, 248 + bounce * 0.3];
      const el = [cx + s * 90 * bdy, lerp(352, 300, a2)];
      const ha = [lerp(cx + s * 116 * bdy, cx + s * 64 * bdy, a2), lerp(462, 190, a2)];
      limbS(sh, el, 17, 13, T, { belly: 1.08, aoA: 0.3 });
      limbS(el, ha, 13, 10, T, { belly: 1.05 });
      const A = lerp(2.2, -0.9, a2);
      handS(ha[0], ha[1], s < 0 ? A : Math.PI - A, 1.0, T, { curl: lerp(0.5, 0.65, a2), spread: 0.35 });
    }
  };
  if(a2 < 0.5) drawArms();

  // ---- TORSO: HOURGLASS, ELEVATED RIBCAGE, TAUT ABDOMEN & SOFT BELLY ----
  const torso = () => {
    X.moveTo(cx - 24 * bdy, topY);
    X.bezierCurveTo(cx - 70 * bdy, 258, cx - 78 * bdy, 308, cx - 74 * bdy, 335); // Upper chest / axilla
    X.bezierCurveTo(cx - 72 * bdy, 372, cx - 56 * bdy, 408, cx - 54 * bdy, 435); // Ribcage tapering to waist
    X.bezierCurveTo(cx - 52 * bdy, 465, cx - 84 * bdy, 532, cx - 82 * bdy, 546); // Waist flaring into smooth hips
    X.bezierCurveTo(cx - 44 * bdy, 570, cx - 18 * bdy, 572, cx, 572);             // Lower pelvis
    X.bezierCurveTo(cx + 18 * bdy, 572, cx + 44 * bdy, 570, cx + 82 * bdy, 546); // Right lower hip
    X.bezierCurveTo(cx + 84 * bdy, 532, cx + 52 * bdy, 465, cx + 54 * bdy, 435); // Right waist
    X.bezierCurveTo(cx + 56 * bdy, 408, cx + 72 * bdy, 372, cx + 74 * bdy, 335); // Right ribcage
    X.bezierCurveTo(cx + 78 * bdy, 308, cx + 70 * bdy, 258, cx + 24 * bdy, topY); // Right chest
    X.closePath();
  };
  skFillShape(torso, T, [cx - 60, topY - 30, cx + 40, 580]);
  skClipIn(torso, () => {
    fAO(cx - 66 * bdy, 400, 16, 90, 0.30);
    fAO(cx + 66 * bdy, 400, 16, 90, 0.30);
    fAO(cx - 74 * bdy, 540, 26, 34, 0.15);
    fAO(cx + 74 * bdy, 540, 26, 34, 0.15);
    fSh(cx - 44 * bdy, 372, 30, 12, 'rgba(160,90,70,0.22)', 0.18);
    fSh(cx + 44 * bdy, 372, 30, 12, 'rgba(160,90,70,0.22)', -0.18);
    fHi(cx, 290, 24 * bdy, 95, 'rgba(255,235,215,0.20)');
    fHi(cx, 476, 30 * bdy, 52, 'rgba(255,235,215,0.18)');
  });
  skLine(torso, T, 1.4, 0.28);

  // Chest flush / vasocongestion (soft-edged tint)
  shade(cx, 314, 60 * bdy, 30, `rgba(${brR},${brG},${brB},${0.05 + E.blush * 0.16})`, 0);

  // Sex flush rubor patches across sternum and belly
  if((G.ar || 0) > 30){
    const ra = (G.ar - 30) / 70;
    const rc = `rgba(${brR},${brG},${brB},${0.04 + 0.08 * ra})`;
    [
      [cx - 30, 318, 26, 16],
      [cx + 30, 318, 26, 16],
      [cx - 24, 418, 28, 18],
      [cx + 24, 418, 28, 18]
    ].forEach(([x, y, rx, ry]) => shade(x, y, rx, ry, rc, 0));
  }

  // Navel (realistic shadow depth and soft upper fold)
  const navelY = 450 + bounce * 0.5;
  X.save();
  X.globalCompositeOperation = 'multiply';
  shade(cx, navelY, 5.5, 7.5, 'rgba(145,85,65,0.48)', 0);
  X.restore();
  X.strokeStyle = 'rgba(155,95,75,0.52)';
  X.lineWidth = 1.8;
  X.beginPath();
  X.ellipse(cx, navelY, 4.4, 6.2, 0, 0, TAU);
  X.stroke();

  // Linea alba (subtle abdominal midline)
  X.strokeStyle = 'rgba(155,95,75,0.12)';
  X.lineWidth = 1.6;
  X.beginPath();
  X.moveTo(cx, 410 + bounce * 0.4); X.lineTo(cx, navelY - 7);
  X.moveTo(cx, navelY + 7); X.lineTo(cx, 486 + bounce * 0.5);
  X.stroke();

  // Clavicle ridges (collarbones rising and falling with breathing)
  X.strokeStyle = 'rgba(165,95,75,0.38)';
  X.lineWidth = 2.0;
  X.beginPath();
  X.moveTo(cx - 38, 230); X.quadraticCurveTo(cx - 14, 238, cx + 2, 234);
  X.moveTo(cx + 38, 230); X.quadraticCurveTo(cx + 14, 238, cx - 2, 234);
  X.stroke();

  // Arms rise over the torso only when clutching upward in high pleasure
  if(a2 >= 0.5) drawArms();

  // ---- BREASTS: SUPINE GRAVITY SPREAD, LATERAL DRAPE, Montgomery Glands & ERECT NIPPLES ----
  const jig = (G.breast ? G.breast.p : 0) * 0.95;
  const squash = sm(0.86, 1.0, G.depth || 0);
  const erect = clamp(0.35 + 0.65 * ((G.ar || 0) / 100), 0, 1);
  const nipCol = G.char ? G.char.nippleColor : '#c25f63';

  for(const s of [-1, 1]){
    // Supine drape: settled closer to the sternum so the pair reads as one chest
    const bx = cx + s * (30 * bdy + bsz * 4.5);
    const by = 336 + br * 0.65 + jig * 0.58 + bounce * 0.4;
    const baseW = 34 * bsz * (1 + squash * 0.08) - jig * 0.1;
    const baseH = 40 * bsz * (1 - squash * 0.12) + jig * 0.15;

    X.save();
    X.translate(bx, by);
    X.rotate(s * 0.10);

    // Contact shadow haloing the dome on the chest wall
    X.save();
    X.globalCompositeOperation = 'multiply';
    shade(s * 8, 16, baseW * 1.12, baseH * 1.0, 'rgba(160,92,70,0.26)', s * 0.10);
    X.restore();

    // Anatomical supine contour (flattened slope at sternum, full lateral curve at flank)
    const dome = () => {
      X.moveTo(0, -baseH * 0.88);
      X.bezierCurveTo(s * 6 - baseW * 0.55, -baseH * 0.6, -baseW * 1.08, -baseH * 0.05, -baseW * 0.88, baseH * 0.46);
      X.bezierCurveTo(-baseW * 0.68, baseH * 0.94, -baseW * 0.15, baseH * 1.02, 0, baseH);
      X.bezierCurveTo(baseW * 0.58, baseH * 0.96, baseW * 1.10, baseH * 0.56, baseW * 0.96, 0);
      X.bezierCurveTo(baseW * 0.84, -baseH * 0.56, s * 8 + baseW * 0.46, -baseH * 0.84, 0, -baseH * 0.88);
      X.closePath();
    };
    X.beginPath(); dome();
    const bg = X.createRadialGradient(-s * baseW * 0.22, -baseH * 0.28, baseW * 0.12, 0, 0, baseW * 1.45);
    bg.addColorStop(0, T.hi); bg.addColorStop(0.5, T.b); bg.addColorStop(1, T.s);
    X.fillStyle = bg; X.fill();
    skClipIn(dome, () => {
      fAO(-s * baseW * 0.15, baseH * 0.86, baseW * 0.8, baseH * 0.24, 0.30);
      fSh(s * baseW * 0.72, baseH * 0.1, baseW * 0.3, baseH * 0.7, 'rgba(160,90,70,0.20)', s * 0.1);
    });

    // Soft spherical dome highlight on upper fullness
    fHi(-s * 5, -baseH * 0.16, baseW * 0.5, baseH * 0.42, 'rgba(255,238,220,0.30)');

    // Inframammary fold crease
    X.strokeStyle = 'rgba(155,90,72,0.34)';
    X.lineWidth = 1.9;
    X.beginPath();
    X.ellipse(s * 2, baseH * 0.74, baseW * 0.46, baseH * 0.22, 0, 0.2, Math.PI - 0.2);
    X.stroke();

    // Areola: tilted anatomically upward and outward, reacting to arousal
    const aDistX = s * 3.5, aDistY = -baseH * 0.06;
    const aer = (9.0 + 2.2 * clamp((G.ar || 0) / 100, 0, 1)) * Math.max(bsz, 0.84);

    // Areolar halo with micro-wrinkles from turgor (kept sheer, never glowing)
    X.fillStyle = 'rgba(206,128,110,0.36)';
    X.beginPath(); X.ellipse(aDistX, aDistY, aer + 2.4, aer * 0.86, s * 0.1, 0, TAU); X.fill();
    X.fillStyle = 'rgba(216,138,118,0.62)';
    X.beginPath(); X.ellipse(aDistX, aDistY, aer, aer * 0.82, s * 0.1, 0, TAU); X.fill();

    // Montgomery tubercles (circumferential glands around areola)
    X.fillStyle = 'rgba(188,118,102,0.70)';
    for(let i = 0; i < 7; i++){
      const ta = i / 7 * TAU + 0.35;
      X.beginPath();
      X.arc(aDistX + Math.cos(ta) * aer * 0.72, aDistY + Math.sin(ta) * aer * 0.62, 0.85, 0, TAU);
      X.fill();
    }

    // Erect nipple (protrudes forward, swollen coronal margin)
    const nr = (3.4 + 2.0 * erect) * Math.max(bsz * 0.9, 0.85);
    X.fillStyle = nipCol;
    X.beginPath();
    X.ellipse(aDistX, aDistY, nr, nr * (1 + 0.18 * erect), s * 0.08, 0, TAU);
    X.fill();

    // High specular moist catchlight on nipple tip (small, never a white dot)
    X.fillStyle = 'rgba(255,245,245,0.42)';
    X.beginPath();
    X.arc(aDistX - 1.1, aDistY - 1.1, nr * 0.30, 0, TAU);
    X.fill();

    X.restore();
  }

  // Cleavage shadow between the domes
  X.save();
  X.globalCompositeOperation = 'multiply';
  shade(cx, 342 + br * 0.65 + jig * 0.58 + bounce * 0.4, 10, 36 * bsz, 'rgba(160,92,70,0.22)', 0);
  X.restore();

  // ---- THE SPOT: VULVA (shared front-view renderer) ----
  const vy = 554 + bounce;
  const eng = clamp((G.ar || 0) / 100, 0, 1);
  const iopen = 4.5 + 10.5 * (G.depth || 0);

  // Arousal hyperemic glow (deep vasocongestion warmth, kept sheer)
  if((G.ar || 0) > 20){
    X.save();
    X.globalCompositeOperation = 'screen';
    shade(cx, vy, 28 + 12 * eng, 22, 'rgba(255,160,165,0.15)', 0);
    X.restore();
  }

  vulvaS(cx, vy, iopen, eng, T, { view: 'front' });
}

/* ============================================================
   HIS FOREARMS & HANDS: PHYSICAL INTERACTION & BREAST KNEADING
   ============================================================ */
let fpvSmL = { x: 0, y: 0, v: '', ok: false };
let fpvSmR = { x: 0, y: 0, v: '', ok: false };

function fpvSmTo(smObj, tx, ty){
  if(!smObj.ok || smObj.v !== G.view){
    smObj.x = tx; smObj.y = ty; smObj.v = G.view; smObj.ok = true;
  } else {
    smObj.x = lerp(smObj.x, tx, 0.22);
    smObj.y = lerp(smObj.y, ty, 0.22);
  }
  return [smObj.x, smObj.y];
}

function drawFPVKnead(hx, hy, rub, small){
  // Fingertip fan + circular motion streaks + tissue compression waves
  const nf = small ? 2 : 3;
  X.fillStyle = '#b88255';
  for(let i = 0; i < nf; i++){
    const a = (small ? 0.6 : 0.52) + i * (small ? 0.7 : 0.52) + Math.sin(G.t * 9 + i) * 0.16;
    X.beginPath();
    X.ellipse(
      hx + Math.cos(a) * (small ? 14 : 19),
      hy + Math.sin(a) * (small ? 11 : 15),
      small ? 5.5 : 7.5,
      small ? 3.8 : 4.8,
      a, 0, TAU
    );
    X.fill();
    // Fingernails
    X.fillStyle = 'rgba(255,235,225,0.7)';
    X.beginPath();
    X.arc(hx + Math.cos(a) * (small ? 15 : 20), hy + Math.sin(a) * (small ? 12 : 16), 1.2, 0, TAU);
    X.fill();
    X.fillStyle = '#b88255';
  }

  // Pressure arc highlights
  X.strokeStyle = `rgba(255,225,205,${0.34 * rub})`;
  X.lineWidth = 2.6;
  X.beginPath();
  X.ellipse(hx, hy, small ? 24 : 42, small ? 22 : 40, 0, G.t * 9, G.t * 9 + 1.8);
  X.stroke();

  // Arousal ripple waves expanding from point of contact
  for(let i = 0; i < 2; i++){
    const u = ((G.t * 1.6 + i * 0.5) % 1);
    X.strokeStyle = `rgba(255,145,170,${(1 - u) * 0.35 * rub})`;
    X.lineWidth = 2.0;
    X.beginPath();
    X.ellipse(hx, hy, (small ? 11 : 18) + u * (small ? 28 : 46), (small ? 10 : 16) + u * (small ? 26 : 42), 0, 0, TAU);
    X.stroke();
  }
}

function drawFPVHands(br){
  const rub = G.rub || 0, kiss = G.kiss || 0, zone = (G.rubZone || 0) | 0;
  const jig = (G.breast ? G.breast.p : 0) * 0.95;
  const bounce = (G.depth || 0) * 8 + (G.impact || 0) * 6;
  const bsz = 0.72 + (G.char ? G.char.breastSize : 0.45) * 0.62;
  const bdy = 0.82 + (G.char ? G.char.bodyScale : 0.45) * 0.42;

  const LB = [640 - (30 * bdy + bsz * 4.5), 336 + br * 0.65 + jig * 0.58 + bounce * 0.4];
  const RB = [640 + (30 * bdy + bsz * 4.5), 336 + br * 0.65 + jig * 0.58 + bounce * 0.4];
  const LOW = [640, 544 + bounce];

  const lWork = zone === 0 || zone === 1;
  const rWork = zone === 0 || zone === 2;
  const lowWork = zone === 3;
  const embracing = kiss >= 0.5;
  const active = rub > 0.12 && !embracing;

  // Left Forearm entering from bottom left corner
  const lBase = [170, 755], lEl = [370, 626];
  let lT = [495, 536 + (G.depth || 0) * 6];
  if(active && lWork){
    lT = [LB[0] + Math.sin(G.t * 9 + Math.PI) * 12 * rub, LB[1] + Math.cos(G.t * 7.4 + Math.PI) * 10 * rub];
  }
  let lHand = fpvSmTo(fpvSmL, lT[0], lT[1]);
  lHand = [lerp(lHand[0], 568, kiss), lerp(lHand[1], 248, kiss)];

  const HT = himT();
  limbS(lBase, lEl, 30, 24, HT, { belly: 1.08, aoA: 0.3 });
  limbS(lEl, lHand, 22, 17, HT, { belly: 1.05 });
  handS(lHand[0], lHand[1], Math.PI - 0.6, 1.32, HT, { curl: 0.62, spread: 0.3 });

  // Right Forearm entering from bottom right corner
  const rBase = [1110, 755], rEl = [910, 626];
  let rT = [785, 536 + (G.depth || 0) * 6];
  if(active){
    if(rWork) rT = [RB[0] + Math.sin(G.t * 9) * 12 * rub, RB[1] + Math.cos(G.t * 7.4) * 10 * rub];
    else if(lowWork) rT = [LOW[0] + Math.sin(G.t * 11) * 7.5 * rub, LOW[1] + Math.cos(G.t * 9) * 6.5 * rub];
  }
  let rHand = fpvSmTo(fpvSmR, rT[0], rT[1]);
  rHand = [lerp(rHand[0], 712, kiss), lerp(rHand[1], 248, kiss)];

  limbS(rBase, rEl, 30, 24, HT, { belly: 1.08, aoA: 0.3 });
  limbS(rEl, rHand, 22, 17, HT, { belly: 1.05 });
  handS(rHand[0], rHand[1], Math.PI + 0.6, 1.32, HT, { curl: 0.62, spread: 0.3 });

  if(!active) return;

  // Active touch FX & breast displacement
  X.save();
  X.globalAlpha = clamp(rub * 1.4, 0, 1);
  if(lWork){
    drawFPVKnead(lHand[0], lHand[1], rub, false);
    if(R() < 0.025 * rub) G.hearts.push({ x: 592 + rr(-15, 15), y: 460 + rr(-8, 8), vy: -rr(20, 36), ph: R() * TAU, life: 1.2, s: rr(0.4, 0.7) });
  }
  if(rWork){
    drawFPVKnead(rHand[0], rHand[1], rub, false);
    if(R() < 0.025 * rub) G.hearts.push({ x: 688 + rr(-15, 15), y: 460 + rr(-8, 8), vy: -rr(20, 36), ph: R() * TAU, life: 1.2, s: rr(0.4, 0.7) });
  }
  if(lowWork){
    drawFPVKnead(rHand[0], rHand[1], rub, true);
    if(R() < 0.035 * rub) G.hearts.push({ x: 640 + rr(-12, 12), y: 695 + rr(-8, 8), vy: -rr(20, 36), ph: R() * TAU, life: 1.2, s: rr(0.4, 0.7) });
  }
  X.restore();
}

/* ============================================================
   THE SHAFT: TOWERING FORESHORTENED PERSPECTIVE & ENTRY SEAL
   ============================================================ */
function drawFPVShaft(){
  const d = G.depth || 0;
  const sway = Math.sin(G.t * 1.7) * 3.2 + chaos(3) * 0.35;
  let tipY, tipX = 640 + sway;

  if(G.state === 'climax' || G.state === 'finish'){
    tipY = 542 + Math.sin(G.t * 30) * 2;
  } else {
    // Tip meets the introitus early; deeper thrusts sink it just past the seal
    tipY = lerp(652, 552, clamp(d / 0.35, 0, 1)) - 6 * clamp((d - 0.35) / 0.65, 0, 1);
  }

  const wb = 27, wt = 17, baseY = 794;
  const pu = 1 + 0.36 * (G.shaftPulse || 0);

  // Testicles with realistic skin folds & weighted sag when pulled back
  if(d < 0.55 && G.state !== 'finish'){
    const bs = 1 - d * 0.88;
    X.fillStyle = sg(695, 775, '#c58a66', '#a56840');
    X.beginPath(); X.ellipse(604, 762, 25 * bs, 21 * bs, 0.22, 0, TAU); X.fill();
    X.fillStyle = sg(695, 775, '#ce8f69', '#a56840');
    X.beginPath(); X.ellipse(676, 762, 23 * bs, 20 * bs, -0.22, 0, TAU); X.fill();

    // Scrotal median raphe
    X.strokeStyle = 'rgba(85,48,28,0.38)';
    X.lineWidth = 2.0;
    X.beginPath();
    X.moveTo(640, 752); X.quadraticCurveTo(640, 768, 640, 780);
    X.stroke();
  }

  // His lower abdomen entering the frame so the shaft connects to a body
  X.fillStyle = sg(640, 716, '#d8976a', '#9a5f3a');
  X.beginPath(); X.ellipse(640, 762, 72, 48, 0, 0, TAU); X.fill();
  X.save();
  X.globalCompositeOperation = 'multiply';
  shade(640, 742, 46, 14, 'rgba(85,45,24,0.30)', 0);
  X.restore();

  // Foreshortened cylindrical shaft body with realistic skin tones
  const cg = X.createLinearGradient(640 - wb, 0, 640 + wb, 0);
  cg.addColorStop(0, '#a56840');
  cg.addColorStop(0.24, '#d18f60');
  cg.addColorStop(0.48, '#eeb488');
  cg.addColorStop(0.72, '#d69364');
  cg.addColorStop(1, '#925c38');

  X.beginPath();
  X.moveTo(640 - wb, baseY);
  X.quadraticCurveTo(640 - wb + 6, (baseY + tipY) / 2, tipX - wt * pu, tipY + 8);
  X.quadraticCurveTo(tipX, tipY - 2, tipX + wt * pu, tipY + 8);
  X.quadraticCurveTo(640 + wb - 6, (baseY + tipY) / 2, 640 + wb, baseY);
  X.closePath();
  X.fillStyle = cg;
  X.fill();

  // Cylindrical shading: ambient occlusion underside & soft highlight
  X.save();
  X.globalCompositeOperation = 'multiply';
  shade(640 - 17, (baseY + tipY) / 2, 12, Math.abs(baseY - tipY) / 2, 'rgba(85,45,24,0.25)', 0);
  X.restore();

  X.save();
  X.globalCompositeOperation = 'soft-light';
  shade(640 - 4, (baseY + tipY) / 2, 14, Math.abs(baseY - tipY) / 2, 'rgba(255,230,200,0.38)', 0);
  X.restore();

  // Tight fleshy contact ring sealing the shaft at the introitus
  const contactA = 0.18 + 0.26 * clamp(d, 0, 1);
  X.save();
  X.globalCompositeOperation = 'multiply';
  shade(640, 556, 30 - 8 * clamp(d, 0, 1), 10, `rgba(22,6,10,${contactA})`, 0);
  X.restore();

  // Lubrication meniscus glinting at the entrance
  if((G.ar || 0) > 35){
    X.strokeStyle = `rgba(255,245,240,${0.10 + 0.14 * ((G.ar || 0) / 100)})`;
    X.lineWidth = 2.4;
    X.beginPath();
    X.ellipse(640, 556, 24, 8, 0, 0, TAU);
    X.stroke();
  }

  // Prominent branched dorsal veins pulsing with arousal
  X.strokeStyle = 'rgba(135,75,55,0.40)';
  X.lineWidth = 3.2;
  X.lineCap = 'round';
  X.beginPath();
  X.moveTo(632, 765);
  X.quadraticCurveTo(626, (765 + tipY) / 2 + 18, 635, tipY + 48);
  X.stroke();

  X.strokeStyle = 'rgba(135,75,55,0.28)';
  X.lineWidth = 2.1;
  X.beginPath();
  X.moveTo(630, 705);
  X.quadraticCurveTo(642, 682, 648, 658);
  X.stroke();

  // Glans, coronal ridge & urethral slit (visible during withdrawal or climax)
  if(tipY > 580 || G.state === 'climax'){
    // Coronal ridge flare
    X.fillStyle = '#c7846f';
    X.beginPath();
    X.ellipse(tipX, tipY + 8, (wt + 3.0) * pu, 7.5 * pu, 0, 0, TAU);
    X.fill();

    // Glans body
    const gg = X.createLinearGradient(tipX - wt, 0, tipX + wt, 0);
    gg.addColorStop(0, '#c7846f');
    gg.addColorStop(0.45, '#e9aa8f');
    gg.addColorStop(1, '#b67562');
    X.fillStyle = gg;
    X.beginPath();
    X.ellipse(tipX, tipY - 4, (wt - 0.5) * pu, 16 * pu, 0, 0, TAU);
    X.fill();

    // Urethral meatus
    X.strokeStyle = 'rgba(115,55,50,0.65)';
    X.lineWidth = 2.2;
    X.beginPath();
    X.moveTo(tipX, tipY - 17);
    X.lineTo(tipX, tipY - 8);
    X.stroke();

    // Specular highlight on moist glans apex
    X.fillStyle = 'rgba(255,255,255,0.35)';
    X.beginPath();
    X.ellipse(tipX - 6, tipY - 8, 3.8, 7.5, -0.2, 0, TAU);
    X.fill();
  }

  // Glistening viscous wet coating along exposed shaft
  if((G.ar || 0) > 26){
    X.save();
    X.globalCompositeOperation = 'screen';
    X.strokeStyle = 'rgba(255,255,255,0.32)';
    X.lineWidth = 4.2;
    X.beginPath();
    X.moveTo(622, 745);
    X.quadraticCurveTo(627, (745 + tipY) / 2, tipX - 10, tipY + 12);
    X.stroke();
    X.restore();
  }
}

/* ============================================================
   PARTICLES: RECTILINEAR VISCOUS DRIPS, SWEAT & CLIMAX JETS
   ============================================================ */
function drawFPVFluids(){
  // Drips running down sheets from her hips
  (G.drips || []).forEach(d => {
    const y = lerp(560, 655, d.p);
    const x = 640 + (d.x - 648) * 0.5 + Math.sin(d.p * 9.5 + (d.j || 0)) * 1.6;

    X.fillStyle = 'rgba(255,248,244,0.72)';
    X.beginPath(); X.ellipse(x, y, 2.6, 4.2, 0, 0, TAU); X.fill();
    X.fillStyle = 'rgba(255,255,255,0.95)';
    X.beginPath(); X.arc(x - 0.7, y - 1.2, 1.0, 0, TAU); X.fill();

    // Trailing thread
    X.strokeStyle = 'rgba(255,248,244,0.32)';
    X.lineWidth = 1.3;
    X.beginPath();
    X.moveTo(x, y - 4); X.lineTo(x, y - 10);
    X.stroke();
  });

  // Wetness glisten specks clustered near entrance
  (G.glisten || []).forEach(g => {
    X.fillStyle = `rgba(255,250,245,${g.a * 0.65})`;
    X.beginPath();
    X.ellipse(640 + (g.x - 640) * 0.3, 556 + (g.y - 546) * 0.4, 2.8, 1.8, 0, 0, TAU);
    X.fill();
  });

  // Ballistic climax jets
  (G.jets || []).forEach(j => {
    if(j.view !== 'fpv') return;
    const spd = Math.hypot(j.vx, j.vy);
    const a = Math.atan2(j.vy, j.vx);
    const al = clamp(j.life * 2.4, 0, 1);

    X.fillStyle = `rgba(255,250,246,${0.85 * al})`;
    X.beginPath(); X.ellipse(j.x, j.y, 3.5 + spd * 0.007, 2.2, a, 0, TAU); X.fill();
    X.fillStyle = `rgba(255,250,246,${0.35 * al})`;
    X.beginPath(); X.ellipse(j.x - Math.cos(a) * 8, j.y - Math.sin(a) * 8, 2.2, 1.5, a, 0, TAU); X.fill();
  });

  // Perspiration beads on face and chest
  (G.sweat || []).forEach((s, i) => {
    const a = 0.45 * s.life;
    X.fillStyle = `rgba(235,248,255,${a})`;
    if(i % 2){
      X.beginPath(); X.ellipse(640 + (s.x - 318) * 1.5, 140 + (s.y - 462) * 0.5, 1.5, 2.4, 0, 0, TAU); X.fill();
    } else {
      X.beginPath(); X.ellipse(640 + (s.x - 318) * 2.2, 300 + (s.y - 462) * 1.2, 1.5, 2.4, 0, 0, TAU); X.fill();
    }
  });

  // Euphoric floating hearts
  (G.hearts || []).forEach(h => {
    X.fillStyle = `rgba(255,105,140,${0.58 * Math.min(1, h.life)})`;
    heartPath(640 + (h.x - 655) + Math.sin(h.ph) * 8, 340 + (h.y - 485) * 0.8, h.s);
    X.fill();
  });
}

/* ============================================================
   MAIN FPV COMPOSITOR & CINEMATIC CAMERA SYSTEM
   ============================================================ */
function drawFPV(){
  drawFPVRoom();

  const E = herExpression();
  const br = Math.sin((G.t || 0) * TAU * (0.16 + (G.pleasure || 0) * 0.004)) * 2.2;

  // Camera targets: full (1.0x), breasts (1.65x), hips/the spot (1.78x), face (1.85x)
  let tZoom = 1.0, tPanX = 640, tPanY = 360;
  if(G.fpvFocus === 'breasts'){ tZoom = 1.65; tPanX = 640; tPanY = 348; }
  else if(G.fpvFocus === 'hips'){ tZoom = 1.78; tPanX = 640; tPanY = 548; }
  else if(G.fpvFocus === 'face'){ tZoom = 1.85; tPanX = 640; tPanY = 172; }

  G.fpvZoom = lerp(G.fpvZoom || 1.0, tZoom, 0.08);
  G.fpvPanX = lerp(G.fpvPanX !== undefined ? G.fpvPanX : 640, tPanX, 0.08);
  G.fpvPanY = lerp(G.fpvPanY !== undefined ? G.fpvPanY : 360, tPanY, 0.08);

  const curZ = G.fpvZoom, px = G.fpvPanX, py = G.fpvPanY;

  // POV Camera Dynamics: respiration sway + physical thrust tremor + tachycardia pulse
  const swayX = Math.sin((G.t || 0) * 0.9) * 2.6 + (G.tired ? Math.sin((G.t || 0) * 7.5) * 1.3 : 0);
  const swayY = Math.sin((G.t || 0) * TAU * 0.16) * 2.1 + (G.kiss || 0) * 10 - (G.depth || 0) * 4.2;

  let beat = 0;
  if((G.pleasure || 0) > 72){
    const bpm = 74 + (G.pleasure - 72) * 1.25 + (G.orgasms || 0) * 8.5;
    beat = Math.max(0, Math.sin((G.t || 0) * bpm * Math.PI / 30)) * 0.95;
  }

  X.save();
  // Apply zoom centered on focus point with visceral motion
  X.translate(640, 360);
  X.scale(curZ, curZ);
  X.translate(-px + swayX + beat * 0.75, -py + swayY + beat * 0.55);

  drawFPVBody(E, br);
  drawFPVHands(br);
  drawFPVShaft();
  drawFPVHead(E);
  drawFPVFluids();

  X.restore();

  // Out-of-focus blurred foreground framing: your broad muscular shoulders leaning in
  X.save();
  X.fillStyle = 'rgba(145,85,55,0.18)';
  X.beginPath(); X.ellipse(60, 762, 230, 125, 0.32, 0, TAU); X.fill();
  X.beginPath(); X.ellipse(1220, 762, 230, 125, -0.32, 0, TAU); X.fill();

  X.fillStyle = 'rgba(10,3,6,0.38)';
  X.beginPath(); X.ellipse(38, 785, 210, 95, 0.32, 0, TAU); X.fill();
  X.beginPath(); X.ellipse(1242, 785, 210, 95, -0.32, 0, TAU); X.fill();
  X.restore();
}