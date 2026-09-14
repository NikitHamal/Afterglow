// Afterglow — module: fpv (loaded by index.html)
// Ultra-High-Fidelity First-Person Perspective Engine
'use strict';

/* ============================================================
   BED-SPACE PERSPECTIVE PROJECTION
   ------------------------------------------------------------
   This is a real pitched pinhole camera, not a stack of offsets.

   The viewer kneels between her spread knees and leans forward over her,
   so his eyes sit at (dOff, eyeH) and the view axis is pitched DOWN by
   `pitch`. Everything she is made of is placed in bed space and projected
   through that camera, which is what produces the foreshortening:

     · her pelvis is nearest  -> largest, low in the frame
     · her torso recedes      -> narrows and rises
     · her face is furthest   -> smallest, near the top
     · her knees are BEHIND the lens plane, so they fall out of frame and
       only the inner thighs graze the bottom corners

     u = lateral offset, metres, +u to screen right
     d = distance along the bed away from her plantar surface (metres)
     h = height above the mattress, metres (0 = flat on the sheet)

   Camera space:  f = d - dOff (forward),  v = h - eyeH (up)
     z  =  f·cos(pitch) - v·sin(pitch)      depth along the view axis
     yc =  f·sin(pitch) + v·cos(pitch)      height above the view axis
   then  x = cx + k·u/z  and  y = cy - k·yc/z.
   ============================================================ */
const FPV_CAM = {
  cx: 640,           // screen x of the lens axis
  cy: 355,           // screen y of the view-axis centre
  k: 960,            // focal length: px of image per metre at 1 m depth
  dOff: 0.60,        // where the viewer's eyes sit along the bed
  eyeH: 0.95,        // eye height above the mattress, metres
  pitch: 0.8727      // 50° downward — leaning over her
};
const FPV_COS = Math.cos(FPV_CAM.pitch), FPV_SIN = Math.sin(FPV_CAM.pitch);
const FPV_ZMIN = 0.12;   // never divide by less than this (keeps behind-lens finite)

function fpvProj(u, d, h){
  const f = d - FPV_CAM.dOff;
  const v = h - FPV_CAM.eyeH;
  const z = Math.max(FPV_ZMIN, f * FPV_COS - v * FPV_SIN);
  const yc = f * FPV_SIN + v * FPV_COS;
  const sc = FPV_CAM.k / z;
  return [FPV_CAM.cx + u * sc, FPV_CAM.cy - yc * sc, sc];
}
// scale only (cheap for sizing strokes / flourishes at a given depth)
function fpvS(d){ return fpvProj(0, d, 0)[2]; }
// true when any of the given screen points is inside the frame (+margin)
function fpvOn(pts, m){
  m = m == null ? 260 : m;
  for(const p of pts){
    if(p[0] > -m && p[0] < W + m && p[1] > -m && p[1] < H + m) return true;
  }
  return false;
}
// her body-scale multiplier for the current character
function fpvBdy(){ return 0.82 + (G.char ? G.char.bodyScale : 0.45) * 0.42; }
// her breast-size multiplier for the current character
function fpvBsz(){ return 0.72 + (G.char ? G.char.breastSize : 0.45) * 0.62; }

/* Skeleton of her supine body in bed space, in metres. Her head is toward
   d≈1.74 and her feet toward the lens at d≈0.34. The values are real
   anatomical landmarks: hip joints ~0.86 m from the soles, shoulder ~1.46,
   crown ~1.74, with half-widths taken from a real female skeleton. */
const FPV_BODY = {
  footFwd:  0.34,   // her feet (nearest the lens)
  ankle:    0.42,
  knee:     0.60,
  thighMid: 0.72,
  hip:      0.86,
  mons:     0.92,
  navel:    1.05,
  waist:    1.09,
  ribs:     1.27,
  sternum:  1.33,
  shoulder: 1.46,
  neck:     1.54,
  chin:     1.60,
  brow:     1.66,
  crown:    1.74,
  hipHalf:  0.150,  // half-width of the pelvis  (real ~0.30 m across)
  waistHalf:0.114,
  ribHalf:  0.132,
  shHalf:   0.168,
  headHalf: 0.072,
  kneeSpread: 0.235, // half-spread of the knees — thighs still frame the
                     // lens but stay anatomically continuous with the hips
  footSpread: 0.205,
  lift:     0.15    // default body-plane height above the sheet
};

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

  // The viewer is pitched down over her, so almost the whole frame is the
  // mattress. The far wall only intrudes as a dim band along the very top,
  // beyond her crown.
  const farY = 62;
  const wallG = X.createLinearGradient(0, 0, 0, farY + 40);
  wallG.addColorStop(0, '#1a1016');
  wallG.addColorStop(0.7, '#130c11');
  wallG.addColorStop(1, '#0c070a');
  X.fillStyle = wallG;
  X.fillRect(0, 0, W, farY + 40);

  // Far window above the headboard spilling cool moonlight (mostly cropped)
  const wx = 992, wy = -60, ww = 160, wh = 116;
  X.fillStyle = 'rgba(120,145,170,0.06)';
  X.fillRect(wx, wy, ww, wh);
  const moonG = X.createRadialGradient(wx + 112, wy + 30, 6, wx + 112, wy + 30, 56);
  moonG.addColorStop(0, 'rgba(230,242,255,0.22)');
  moonG.addColorStop(0.4, 'rgba(180,210,240,0.06)');
  moonG.addColorStop(1, 'rgba(0,0,0,0)');
  X.save(); X.globalCompositeOperation = 'screen';
  X.fillStyle = moonG;
  X.beginPath(); X.arc(wx + 112, wy + 30, 56, 0, TAU); X.fill();
  X.fillStyle = 'rgba(235,245,255,0.85)';
  X.beginPath(); X.arc(wx + 112, wy + 30, 12, 0, TAU); X.fill();
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
  const lx = 140, ly = -30;
  const lampG = X.createRadialGradient(lx, ly, 8, lx, ly, 480);
  lampG.addColorStop(0, 'rgba(255,195,125,0.30)');
  lampG.addColorStop(0.35, 'rgba(255,165,95,0.13)');
  lampG.addColorStop(0.7, 'rgba(255,140,80,0.03)');
  lampG.addColorStop(1, 'rgba(0,0,0,0)');
  X.save(); X.globalCompositeOperation = 'screen';
  X.fillStyle = lampG;
  X.beginPath(); X.arc(lx, ly, 480, 0, TAU); X.fill();
  X.restore();

  // Lamp fixture silhouette
  X.fillStyle = '#2d1820';
  X.beginPath();
  X.moveTo(108, -4); X.lineTo(172, -4); X.lineTo(158, -40); X.lineTo(122, -40);
  X.closePath(); X.fill();
  X.fillStyle = 'rgba(255,215,145,0.7)';
  X.beginPath();
  X.moveTo(122, -38); X.lineTo(158, -38); X.lineTo(166, -6); X.lineTo(114, -6);
  X.closePath(); X.fill();

  // Mattress & crumpled satin sheets: the near sheet fills almost the whole
  // frame because the camera looks straight along the bed.
  const sheetG = X.createLinearGradient(0, farY + 40, 0, H);
  sheetG.addColorStop(0, '#3a1a24');
  sheetG.addColorStop(0.16, '#421d28');
  sheetG.addColorStop(0.45, '#2e131b');
  sheetG.addColorStop(0.8, '#1e0a11');
  sheetG.addColorStop(1, '#11050a');
  X.fillStyle = sheetG;
  X.fillRect(0, farY + 40, W, H - farY - 40);

  // Dynamic tension wrinkles radiating from her hips, back, and thighs.
  // They are compressed near the far edge and stretch toward the lens.
  const bounce = (G.depth || 0) * 8 + (G.impact || 0) * 6;
  X.save();
  X.globalCompositeOperation = 'multiply';
  X.strokeStyle = 'rgba(12,3,6,0.52)';
  X.lineWidth = 3.2;
  for(let i = 0; i < 7; i++){
    const u = i / 6;
    const y = lerp(farY + 74, 700, u * u * 0.85 + u * 0.15);
    X.beginPath();
    X.moveTo(70 + ((i * 61) % 120), y);
    X.bezierCurveTo(460, y - 14 + ((i * 29) % 24) + bounce * 0.4, 820, y + 10 - ((i * 19) % 20), 1210 - ((i * 53) % 140), y);
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

  // Pillow under her head. Kept very low-contrast — a bright bolster behind
  // her crown reads as a halo, which is the last thing we want.
  const pilY = 40, pilRx = 78, pilRy = 19;
  const pilG = X.createLinearGradient(640 - pilRx, pilY - pilRy, 640 + pilRx, pilY + pilRy);
  pilG.addColorStop(0, '#2b2019');
  pilG.addColorStop(0.35, '#241b15');
  pilG.addColorStop(0.75, '#1a130f');
  pilG.addColorStop(1, '#130d0a');
  X.fillStyle = pilG;
  X.beginPath();
  X.ellipse(640, pilY, pilRx, pilRy, -0.01, 0, TAU);
  X.fill();

  // Head depression shadow
  X.save();
  X.globalCompositeOperation = 'multiply';
  const headIndent = X.createRadialGradient(640, pilY - 6, 4, 640, pilY - 6, 24);  headIndent.addColorStop(0, 'rgba(80,55,40,0.32)');
  headIndent.addColorStop(0.65, 'rgba(90,60,45,0.12)');
  headIndent.addColorStop(1, 'rgba(0,0,0,0)');
  X.fillStyle = headIndent;
  X.beginPath();
  X.ellipse(640, pilY - 6, 36, 11, 0, 0, TAU);
  X.fill();
  X.restore();

  // Soft body contact ambient occlusion pooled under her hips and thighs —
  // this is the anchor shadow that plants her on the sheet
  X.save();
  X.globalCompositeOperation = 'multiply';
  shade(640, 772 + bounce * 0.4, 300, 180, 'rgba(10,3,7,0.58)', 0);
  X.restore();
}

/* ============================================================
   HER HEAD: LOOKING UP AT PLAYER (FORESHORTENED, DEVOTIONAL)
   ============================================================ */
function drawFPVHead(E){
  const B = FPV_BODY;
  const breathe = Math.sin((G.t || 0) * TAU * 0.33) * 1.8;
  const hairCol = G.char ? G.char.hairColor : '#231318';

  // ---- project the head into bed space -----------------------------------
  // Her face is seen from between her breasts looking up the bed: the chin is
  // the nearest point of the head, the crown the furthest, so the skull is
  // squashed vertically while its width holds. Everything is sized in metres
  // against the local projection scale, so the head is correctly SMALL and
  // DISTANT relative to the looming pelvis.
  const hp = fpvProj(0, B.chin + 0.02, B.lift + 0.055);
  const hx = hp[0] + Math.sin((G.t || 0) * 0.5) * 1.6;
  const hy = hp[1] + breathe * 0.3;
  // The face/hair art below is authored at a ~430 px reference scale, where
  // the face spans 88 px and the hair mass 160 px. Her head projects at
  // sc≈760, so hs ≈ 0.88 makes the drawn face ~78 px wide — matching a real
  // 0.11 m face seen from the far end of the bed.
  const ks = (hp[2] / 430) * 0.56 * (1 + 0.03 * (G.kiss || 0));
  const hs = ks;
  const squashY = clamp(0.88 - (G.nod || 0) * 0.05 - (G.pleasure || 0) * 0.0004, 0.68, 0.95);

  // Hair hugging the head — layered mass cradling it, no halo ring
  X.save();
  X.translate(hx, hy);
  X.scale(hs, hs * squashY);

  hairMassS(0, 16, 80, 50, 0, hairCol);
  X.save();
  X.beginPath(); X.ellipse(0, 16, 80, 50, 0, 0, TAU); X.clip();
  X.strokeStyle = skDark(hairCol, 0.45); X.lineWidth = 2.4; X.lineCap = 'round';
  for(let i = 0; i < 5; i++){
    X.beginPath();
    X.moveTo(-32 + i * 16, -8);
    X.quadraticCurveTo(-52 + i * 26, 24, -70 + i * 35, 58);
    X.stroke();
  }
  X.globalCompositeOperation = 'soft-light';
  shade(0, 2, 62, 20, 'rgba(255,235,220,0.20)', 0);
  X.restore();

  // Silky locks spilling down onto the pillow beside her neck
  for(let i = 0; i < 6; i++){
    const s = i < 3 ? -1 : 1, k = i % 3;
    const bx = s * (30 + k * 10), by = 26 + k * 6;
    const c1x = s * (48 + k * 12), c1y = by + 26;
    const c2x = s * (56 + k * 14), c2y = by + 54;
    const tx = s * (50 + k * 14), ty = 96 + k * 12;
    tressS(bx, by, c1x, c1y, c2x, c2y, tx, ty, 13 - k * 2.2, 3, hairCol, 'rgba(255,200,210,0.14)');
  }
  X.restore();

  X.save();
  X.translate(hx, hy);
  X.scale(hs, hs * squashY);
  X.rotate(E.tilt * 0.35 + (G.nod || 0) * 0.1 + Math.sin((G.t || 0) * TAU * 0.33) * 0.015);

  const fpvSk = getSkin();

  // The neck must actually reach the shoulders the torso drew, so undo the
  // head transform to find where they land and rebuild the neck in local px.
  const _sy = hs * squashY, _sx = hs;
  const shLp = fpvProj( B.shHalf * fpvBdy(), B.shoulder, B.lift );
  const shRp = fpvProj(-B.shHalf * fpvBdy(), B.shoulder, B.lift );
  const nkBaseY = ((shLp[1] + shRp[1]) / 2 - hy) / _sy;
  // A real neck is ~0.084 m across — noticeably narrower than the head, which
  // is what stops it reading as a lampshade under the jaw.
  const nkHalfPx = 0.038 * fpvS(B.neck) * fpvBdy();
  const nkHalfN  = nkHalfPx / _sx;

  // Neck: a slim column that only starts to flare where the trapezius meets
  // the collarbone line the torso already drew. The flare is gentle (1.3x) so
  // the neck never becomes a second trunk.
  const nkTop = 14;
  const nkBot = clamp(nkBaseY, nkTop + 30, nkTop + 260);
  const nkW = nkHalfN;
  const neckPath = () => {
    X.moveTo(-nkW * 1.00, nkTop);
    X.bezierCurveTo(-nkW * 1.06, lerp(nkTop, nkBot, 0.34),
                    -nkW * 1.10, lerp(nkTop, nkBot, 0.60),
                    -nkW * 1.16, lerp(nkTop, nkBot, 0.78));
    X.bezierCurveTo(-nkW * 1.24, lerp(nkTop, nkBot, 0.89),
                    -nkW * 1.29, lerp(nkTop, nkBot, 0.96),
                    -nkW * 1.32, nkBot);
    X.lineTo(nkW * 1.32, nkBot);
    X.bezierCurveTo( nkW * 1.29, lerp(nkTop, nkBot, 0.96),
                     nkW * 1.24, lerp(nkTop, nkBot, 0.89),
                     nkW * 1.16, lerp(nkTop, nkBot, 0.78));
    X.bezierCurveTo( nkW * 1.10, lerp(nkTop, nkBot, 0.60),
                     nkW * 1.06, lerp(nkTop, nkBot, 0.34),
                     nkW * 1.00, nkTop);
    X.closePath();
  };
  // Fill with the same directional light the torso uses, so the neck and trunk
  // read as one continuous surface instead of two pasted tones.
  const nkT = skTone(fpvSk.her);
  skFillShape(neckPath, nkT, [0 + nkW * 0.9, nkTop, 0 - nkW * 0.9, nkBot]);
  skClipIn(neckPath, () => {
    // Throat shadow — it must sit BELOW the chin line, otherwise the face
    // paints over it and the jaw melts into the neck like a snout.
    fAO(0, 54, nkW * 1.25, nkW * 0.80, 0.40);
    // sternocleidomastoid cords running down each side
    fSh(-nkW * 0.58, lerp(nkTop, nkBot, 0.42), nkW * 0.30, (nkBot - nkTop) * 0.28,
      'rgba(175,100,80,0.18)', 0.10);
    fSh( nkW * 0.58, lerp(nkTop, nkBot, 0.42), nkW * 0.30, (nkBot - nkTop) * 0.28,
      'rgba(175,100,80,0.18)', -0.10);
    // one slim front highlight
    fHi(0, lerp(nkTop, nkBot, 0.55), nkW * 0.40, (nkBot - nkTop) * 0.26,
      'rgba(255,240,225,0.16)');
    // suprasternal notch dip at the base
    fAO(0, nkBot - nkW * 0.35, nkW * 0.6, nkW * 0.4, 0.32);
  });

  // Jawline & chin foreshortened looking upward. Filled with the SAME tone
  // range as the trunk (highlight -> shadow), otherwise the face reads a
  // full stop darker than the body it belongs to.
  X.fillStyle = sg(-52, 14, nkT.hi, nkT.s);
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
  // Under-jaw shadow: a dark crescent along the mandible. Without it the face
  // ellipse simply continues into the neck and she reads as snouted.
  shade(0, 52, 34, 13, 'rgba(120,58,46,0.34)', 0);
  shade(0, 47, 40, 6,  'rgba(120,58,46,0.20)', 0);
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
  const B = FPV_BODY;
  const T = herT();
  const [brR, brG, brB] = hexToRgb(G.char ? G.char.blushColor : '#e86070');
  const bsz = 0.72 + (G.char ? G.char.breastSize : 0.45) * 0.62;
  const bdy = 0.82 + (G.char ? G.char.bodyScale : 0.45) * 0.42;
  const ple = G.pleasure || 0;

  // ---- pose in bed space ------------------------------------------------
  // knees draw up and fall open with pleasure; feet stay planted wide so the
  // near thighs frame the lens. The base lift is small — at rest her knees
  // are only slightly raised, and the projection amplifies every centimetre.
  const drawUp   = 0.14 + ple * 0.0022 + bounce * 0.003;
  const openAmt  = 0.050 + ple * 0.0011;

  const hipHalf = B.hipHalf * bdy, waistHalf = B.waistHalf * bdy;
  const ribHalf = B.ribHalf * bdy, shHalf = B.shHalf * bdy;

  // ---- projected skeleton (screen points + per-point scale) -------------
  // hipL/kneeL/ankL all use u < 0 so they stay on the same (screen-left) side;
  // mixing the signs makes the two legs cross in an X over her pelvis.
  const hipL  = fpvProj(-hipHalf,  B.hip,     B.lift);
  const hipR  = fpvProj( hipHalf,  B.hip,     B.lift);
  const mons  = fpvProj(0, B.mons,  B.lift + 0.04);
  const navel = fpvProj(0, B.navel, B.lift + 0.015);
  const waist = fpvProj(0, B.waist, B.lift + 0.004);
  const ribs  = fpvProj(0, B.ribs,  B.lift);
  const stern = fpvProj(0, B.sternum, B.lift + 0.006);
  const shL   = fpvProj( shHalf, B.shoulder, B.lift);
  const shR   = fpvProj(-shHalf, B.shoulder, B.lift);
  const kneeL = fpvProj(-(B.kneeSpread + openAmt) * bdy, B.knee, B.lift + drawUp);
  const kneeR = fpvProj( (B.kneeSpread + openAmt) * bdy, B.knee, B.lift + drawUp);
  const ankL  = fpvProj(-B.footSpread * bdy, B.ankle, 0.15);
  const ankR  = fpvProj( B.footSpread * bdy, B.ankle, 0.15);
  const toeL  = fpvProj(-B.footSpread * bdy * 1.08, 0.20, 0.11);
  const toeR  = fpvProj( B.footSpread * bdy * 1.08, 0.20, 0.11);

  // helper: metres -> screen px at a given projected scale. The reference
  // scale is the pelvis (sc≈445), where 1 m spans about 445 px.
  const px = (m, sc) => m * sc;

  // ---- LEGS: near thighs looming wide, foreshortened toward the lens ----
  for(const s of [-1, 1]){
    const hip = s < 0 ? hipL : hipR;
    const knee = s < 0 ? kneeL : kneeR;
    const ank  = s < 0 ? ankL : ankR;
    const toe  = s < 0 ? toeL : toeR;

    // Her knees sit almost exactly at the lens plane, so the thighs rake down
    // out of frame and the shins/feet are behind the camera entirely. Skip
    // anything that projects wholly off-frame — it also avoids allocating
    // enormous gradients for shapes nobody can see.
    if(!fpvOn([[hip[0], hip[1]], [knee[0], knee[1]]], 400)) continue;
    const shinVisible = fpvOn([[knee[0], knee[1]], [ank[0], ank[1]]], 200);

    // thigh: hip -> knee. Real upper-thigh radius ~0.085 m, tapering to ~0.062
    // at the knee. belly>1 gives the quadriceps its outward fullness.
    limbS([hip[0], hip[1]], [knee[0], knee[1]],
      px(0.084 * bdy, hip[2]), px(0.062 * bdy, knee[2]), T, { belly: 1.10, aoA: 0.26 });
    if(shinVisible){
      // calf: knee -> ankle — soleus swell then a slim ankle
      limbS([knee[0], knee[1]], [ank[0], ank[1]],
        px(0.058 * bdy, knee[2]), px(0.036 * bdy, ank[2]), T, { belly: 1.12 });
      // foot/ankle cap fading off the bottom of the frame
      limbS([ank[0], ank[1]], [toe[0], toe[1]], px(0.036 * bdy, ank[2]), px(0.028 * bdy, toe[2]), T, { belly: 1.05 });
    }
    if(shinVisible){
      // Patella: a soft, slightly lighter disc that merges the two limb fills.
      // Kept inside the limb radius so it never reads as a bolted-on ball.
      X.save();
      X.beginPath();
      X.ellipse(knee[0], knee[1], px(0.060 * bdy, knee[2]), px(0.052 * bdy, knee[2]), s * 0.42, 0, TAU);
      const kg = X.createRadialGradient(knee[0] - s * 8, knee[1] - 9, 2, knee[0], knee[1], px(0.068 * bdy, knee[2]));
      kg.addColorStop(0, T.b); kg.addColorStop(0.68, T.b); kg.addColorStop(1, T.s);
      X.fillStyle = kg; X.fill();
      X.restore();
      // tiny retracted-patella highlight, not a specular ball
      fHi(knee[0], knee[1] - 9, px(0.020 * bdy, knee[2]), px(0.026 * bdy, knee[2]), 'rgba(255,238,220,0.07)');

      // median patellar crease so the knee reads as a joint
      X.strokeStyle = 'rgba(178,108,90,0.20)';
      X.lineWidth = Math.max(1.3, px(0.0035, knee[2]));
      X.beginPath();
      X.arc(knee[0], knee[1] - px(0.006, knee[2]), px(0.030 * bdy, knee[2]), Math.PI * 0.22, Math.PI * 0.78);
      X.stroke();

      // kneecap AO where the shin turns away
      fAO(ank[0], ank[1], px(0.048 * bdy, ank[2]), px(0.030 * bdy, ank[2]), 0.16);
    }

    // vastus medialis teardrop riding the inner thigh — drawn for the thigh
    // even when the shin is off-frame, since this is the muscle that reads
    // on the big near limb.
    X.strokeStyle = 'rgba(185,115,95,0.16)';
    X.lineWidth = Math.max(1.5, px(0.0045, knee[2]));
    X.beginPath();
    X.moveTo(hip[0] + s * px(0.040, hip[2]), hip[1] - px(0.052, hip[2]));
    X.bezierCurveTo(
      knee[0] - s * px(0.020, knee[2]), knee[1] - px(0.120, knee[2]),
      knee[0] + s * px(0.008, knee[2]),  knee[1] - px(0.050, knee[2]),
      knee[0] + s * px(0.004, knee[2]),  knee[1] - px(0.024, knee[2]));
    X.stroke();

    // inguinal groove where the thigh meets the pelvis
    X.strokeStyle = 'rgba(165,95,75,0.22)';
    X.lineWidth = Math.max(1.4, px(0.0038, hip[2]));
    X.beginPath();
    X.moveTo(hip[0] * 0.72 + 640 * 0.28, hip[1] + px(0.007, hip[2]));
    X.quadraticCurveTo(knee[0] * 0.42 + 640 * 0.58, hip[1] - px(0.028, hip[2]),
      knee[0], knee[1] + px(0.080, knee[2]));
    X.stroke();
  }

  // ---- PELVIS BLOCK: ties the thighs to the torso, hides the leg roots ---
  const pelvY = (hipL[1] + hipR[1]) / 2;
  X.save();
  X.beginPath();
  X.moveTo(hipL[0], hipL[1] - px(0.0372, hipL[2]));
  X.bezierCurveTo(hipL[0] - px(0.0233, hipL[2]), mons[1] + px(0.0465, mons[2]), mons[0] - px(0.1302, mons[2]), mons[1] - px(0.0140, mons[2]), mons[0], mons[1] - px(0.0186, mons[2]));
  X.bezierCurveTo(mons[0] + px(0.1302, mons[2]), mons[1] - px(0.0140, mons[2]), hipR[0] + px(0.0233, hipR[2]), mons[1] + px(0.0465, mons[2]), hipR[0], hipR[1] - px(0.0372, hipR[2]));
  X.bezierCurveTo(hipR[0] + px(0.0140, hipR[2]), pelvY + px(0.0791, hipR[2]), hipR[0] - px(0.0465, hipR[2]), pelvY + px(0.1116, hipR[2]), 640, pelvY + px(0.1209, hipR[2]));
  X.bezierCurveTo(hipL[0] + px(0.0465, hipL[2]), pelvY + px(0.1116, hipL[2]), hipL[0] - px(0.0140, hipL[2]), pelvY + px(0.0791, hipL[2]), hipL[0], hipL[1] - px(0.0372, hipL[2]));
  X.closePath();
  const pg = X.createLinearGradient(640, mons[1], 640, pelvY + px(0.1209, hipL[2]));
  pg.addColorStop(0, T.b); pg.addColorStop(0.55, T.b); pg.addColorStop(1, T.s);
  X.fillStyle = pg; X.fill();
  X.restore();
  skClipIn(() => {
    X.moveTo(hipL[0], hipL[1] - px(0.0372, hipL[2]));
    X.bezierCurveTo(hipL[0] - px(0.0233, hipL[2]), mons[1] + px(0.0465, mons[2]), mons[0] - px(0.1302, mons[2]), mons[1] - px(0.0140, mons[2]), mons[0], mons[1] - px(0.0186, mons[2]));
    X.bezierCurveTo(mons[0] + px(0.1302, mons[2]), mons[1] - px(0.0140, mons[2]), hipR[0] + px(0.0233, hipR[2]), mons[1] + px(0.0465, mons[2]), hipR[0], hipR[1] - px(0.0372, hipR[2]));
    X.bezierCurveTo(hipR[0] + px(0.0140, hipR[2]), pelvY + px(0.0791, hipR[2]), hipR[0] - px(0.0465, hipR[2]), pelvY + px(0.1116, hipR[2]), 640, pelvY + px(0.1209, hipR[2]));
    X.bezierCurveTo(hipL[0] + px(0.0465, hipL[2]), pelvY + px(0.1116, hipL[2]), hipL[0] - px(0.0140, hipL[2]), pelvY + px(0.0791, hipL[2]), hipL[0], hipL[1] - px(0.0372, hipL[2]));
    X.closePath();
  }, () => {
    fAO(hipL[0] + px(0.0419, hipL[2]), hipL[1], px(0.0698, hipL[2]), px(0.0791, hipL[2]), 0.26, -0.4);
    fAO(hipR[0] - px(0.0419, hipR[2]), hipR[1], px(0.0698, hipR[2]), px(0.0791, hipR[2]), 0.26,  0.4);
    fHi(640, mons[1] + px(0.0140, mons[2]), px(0.0791, mons[2]), px(0.0465, mons[2]), 'rgba(255,235,215,0.14)');
  });
  skLine(() => {
    X.moveTo(hipL[0], hipL[1] - px(0.0372, hipL[2]));
    X.bezierCurveTo(mons[0] - px(0.1302, mons[2]), mons[1] - px(0.0140, mons[2]), mons[0] + px(0.1302, mons[2]), mons[1] - px(0.0140, mons[2]), hipR[0], hipR[1] - px(0.0372, hipR[2]));
  }, T, 1.4, 0.26);

  // ---- ARMS: drawn under the torso so the shoulder reads connected -------
  // At rest her upper arms lie on the sheet beside her ribs, running from the
  // shoulder TOWARD the lens (down the length of her body) — so the elbow and
  // wrist depths are always SMALLER than the shoulder's. Only at high
  // pleasure do they lift off the sheet to clutch at your back.
  const a2 = sm(62, 88, ple);
  const drawArms = () => {
    for(const s of [-1, 1]){
      const sh = s < 0 ? shR : shL;
      // The arm's root sits slightly INBOARD, further away and lower than the
      // shoulder joint, so the deltoid stays tucked under the torso silhouette
      // instead of ballooning above it as a shoulder pad.
      const shA = fpvProj(s * shHalf * 0.86, B.shoulder + 0.03, B.lift - 0.085);
      const elU = lerp(0.232, 0.300, a2) * s * bdy;
      const elD = lerp(B.shoulder - 0.30, B.shoulder - 0.06, a2);
      const elH = B.lift + lerp(-0.075, 0.10, a2);
      const el  = fpvProj(elU, elD, elH);
      const haU = lerp(0.212, 0.200, a2) * s * bdy;
      const haD = lerp(B.shoulder - 0.56, B.shoulder - 0.26, a2);
      const haH = B.lift + lerp(-0.065, 0.30, a2);
      const ha  = fpvProj(haU, haD, haH);

      // upper arm: real radius ~0.044 m -> 0.034 at the elbow
      limbS([shA[0], shA[1]], [el[0], el[1]],
        px(0.0445 * bdy, shA[2]), px(0.0335 * bdy, el[2]), T,
        { belly: 1.05, aoA: 0.24 });
      // forearm: ~0.033 -> 0.024, belly gives the brachioradialis swell
      limbS([el[0], el[1]], [ha[0], ha[1]],
        px(0.0325 * bdy, el[2]), px(0.0235 * bdy, ha[2]), T,
        { belly: 1.06 });
      // a soft AO pool in the crook of the elbow, NOT a stroked ring — a
      // circle outline here reads as a doll's ball joint
      fAO(el[0] + s * px(0.010, el[2]), el[1] + px(0.004, el[2]),
          px(0.020 * bdy, el[2]), px(0.016 * bdy, el[2]), 0.20);

      const handAng = s < 0 ? (1.55 - a2 * 2.3) : (Math.PI - 1.55 + a2 * 2.3);
      handS(ha[0], ha[1], handAng,
        lerp(0.44, 0.60, a2) * (ha[2] / 430),
        T, { curl: lerp(0.34, 0.66, a2), spread: 0.20 });
    }
  };
  if(a2 < 0.5) drawArms();

  // ---- TORSO: hourglass silhouette from mons to the collarbone line ------
  // The top corners ride on the projected shoulder joints, and the crest of
  // the chest sits between them, so the trunk meets the neck with no seam.
  const clavY = (shL[1] + shR[1]) / 2 + px(0.012, stern[2]);   // collarbone line
  // NOTE: shL is the u>0 shoulder (screen RIGHT) and shR the u<0 one (screen
  // LEFT), because they are named for her body, not the viewer. hipL/hipR are
  // the opposite way round. The armpits must follow the VIEWER's sides so the
  // silhouette path never jumps across the trunk and self-intersects.
  const axilL = 640 - (shHalf * 1.02) * shR[2];   // screen-left armpit
  const axilR = 640 + (shHalf * 1.02) * shL[2];   // screen-right armpit
  const torso = () => {
    X.moveTo(hipL[0], hipL[1] - px(0.0419, hipL[2]));
    // up the left flank: hip -> waist -> ribcage -> axilla
    X.bezierCurveTo(
      hipL[0] - px(0.0047, hipL[2]), hipL[1] - px(0.1349, hipL[2]),
      640 - waistHalf * waist[2], waist[1] + px(0.0186, waist[2]),
      640 - waistHalf * waist[2], waist[1]);
    X.bezierCurveTo(
      640 - (waistHalf * 0.80) * ribs[2], lerp(waist[1], ribs[1], 0.55),
      640 - ribHalf * ribs[2], ribs[1] + px(0.0140, ribs[2]),
      640 - ribHalf * ribs[2], ribs[1]);
    X.bezierCurveTo(
      640 - (ribHalf * 0.96) * stern[2], lerp(ribs[1], clavY, 0.55),
      axilL + px(0.0140, shR[2]),       lerp(ribs[1], clavY, 0.86),
      axilL,                            clavY);
    // Shoulder line: a trapezius slope that climbs from the outer shoulder up
    // to the base of the neck. A flat bar across the top reads as a garment
    // collar; the slope is what makes it read as muscle.
    X.bezierCurveTo(
      axilL - px(0.0090, shR[2]),       clavY - px(0.0180, shR[2]),
      640 - shHalf * 0.52 * shR[2],     clavY - px(0.0330, shR[2]),
      640 - shHalf * 0.17 * shR[2],     clavY - px(0.0420, shR[2]));
    X.bezierCurveTo(
      640 - shHalf * 0.06 * shL[2],     clavY - px(0.0450, shL[2]),
      640 + shHalf * 0.06 * shL[2],     clavY - px(0.0450, shL[2]),
      640 + shHalf * 0.17 * shL[2],     clavY - px(0.0420, shL[2]));
    X.bezierCurveTo(
      640 + shHalf * 0.52 * shL[2],     clavY - px(0.0330, shL[2]),
      axilR + px(0.0090, shL[2]),       clavY - px(0.0180, shL[2]),
      axilR,                            clavY);
    // down the right flank (mirror of the left)
    X.bezierCurveTo(
      axilR - px(0.0140, shL[2]),       lerp(ribs[1], clavY, 0.86),
      640 + (ribHalf * 0.96) * stern[2], lerp(ribs[1], clavY, 0.55),
      640 + ribHalf * ribs[2],          ribs[1]);
    X.bezierCurveTo(
      640 + ribHalf * ribs[2],          ribs[1] + px(0.0140, ribs[2]),
      640 + (waistHalf * 0.80) * ribs[2], lerp(waist[1], ribs[1], 0.55),
      640 + waistHalf * waist[2],       waist[1]);
    X.bezierCurveTo(
      640 + waistHalf * waist[2],       waist[1] + px(0.0186, waist[2]),
      hipR[0] + px(0.0047, hipR[2]),    hipR[1] - px(0.1349, hipR[2]),
      hipR[0],                          hipR[1] - px(0.0419, hipR[2]));
    X.closePath();
  };
  const torsoBBox = () => {
    X.moveTo(hipL[0], hipL[1] - px(0.0419, hipL[2]));
    X.lineTo(640 - ribHalf * ribs[2], ribs[1]);
    X.lineTo(axilL, clavY);
    X.lineTo(axilR, clavY);
    X.lineTo(hipR[0], hipR[1] - px(0.0419, hipR[2]));
    X.closePath();
  };
  // The light axis must START ABOVE the shoulder line and END BELOW the hips,
  // otherwise the gradient clamps to its lightest stop across the whole upper
  // chest and the shoulders read as a flat bright collar.
  skFillShape(torso, T, [640 - 56, clavY - 120, 640 + 50, hipL[1] + 130]);
  skClipIn(torso, () => {
    // Flank shadows hug the ribcage — they must stay SHORT, or their upper
    // tips rise past the collarbones and carve a dark bowtie out of the chest.
    fAO(640 - ribHalf * ribs[2] * 0.92, (waist[1] + ribs[1]) / 2 + px(0.028, ribs[2]), px(0.028, ribs[2]), px(0.098, ribs[2]), 0.22);
    fAO(640 + ribHalf * ribs[2] * 0.92, (waist[1] + ribs[1]) / 2 + px(0.028, ribs[2]), px(0.028, ribs[2]), px(0.098, ribs[2]), 0.22);
    fAO(640 - hipHalf * hipL[2] * 0.94, hipL[1] - px(0.0233, hipL[2]), px(0.0605, hipL[2]), px(0.0791, hipL[2]), 0.16);
    fAO(640 + hipHalf * hipR[2] * 0.94, hipR[1] - px(0.0233, hipR[2]), px(0.0605, hipR[2]), px(0.0791, hipR[2]), 0.16);
    fSh(640 - waistHalf * waist[2] * 0.72, waist[1] - px(0.0930, ribs[2]), px(0.0698, ribs[2]), px(0.0326, ribs[2]), 'rgba(160,90,70,0.22)', 0.18);
    fSh(640 + waistHalf * waist[2] * 0.72, waist[1] - px(0.0930, ribs[2]), px(0.0698, ribs[2]), px(0.0326, ribs[2]), 'rgba(160,90,70,0.22)', -0.18);
    // diffuse chest light — kept broad and low so the collarbones never read
    // as a bright hard crescent across the top of the trunk
    fHi(640, (ribs[1] + stern[1]) / 2 + px(0.028, stern[2]), px(0.062, stern[2]), px(0.115, stern[2]), 'rgba(255,235,215,0.13)');
    fHi(640, (waist[1] + navel[1]) / 2, px(0.0698, waist[2]), px(0.1116, waist[2]), 'rgba(255,235,215,0.14)');
    // soft abdominal midline groove
    X.save(); X.globalCompositeOperation = 'multiply';
    shade(640, lerp(waist[1], navel[1], 0.5), px(0.020, waist[2]), px(0.090, waist[2]), 'rgba(158,92,70,0.16)', 0);
    X.restore();
  });
  skLine(torso, T, 1.4, 0.26);

  // chest flush / vasocongestion
  shade(640, (ribs[1] + stern[1]) / 2, px(0.1395, stern[2]), px(0.0698, stern[2]), `rgba(${brR},${brG},${brB},${0.05 + E.blush * 0.16})`, 0);

  // sex flush patches across sternum and belly
  if((G.ar || 0) > 30){
    const ra = (G.ar - 30) / 70, rc = `rgba(${brR},${brG},${brB},${0.04 + 0.08 * ra})`;
    [[-30, stern[1] + px(0.0233, stern[2]), 26, 16], [30, stern[1] + px(0.0233, stern[2]), 26, 16],
     [-24, navel[1] + px(0.0140, navel[2]), 28, 18], [24, navel[1] + px(0.0140, navel[2]), 28, 18]]
      .forEach(([dx, y, rx, ry]) => shade(640 + dx, y, px(rx, navel[2]), px(ry, navel[2]), rc, 0));
  }

  // navel
  const navR = px(0.0140, navel[2]);
  X.save();
  X.globalCompositeOperation = 'multiply';
  shade(navel[0], navel[1], navR * 0.92, navR * 1.25, 'rgba(145,85,65,0.48)', 0);
  X.restore();
  X.strokeStyle = 'rgba(155,95,75,0.52)';
  X.lineWidth = Math.max(1.4, navR * 0.3);
  X.beginPath(); X.ellipse(navel[0], navel[1], navR * 0.74, navR, 0, 0, TAU); X.stroke();

  // linea alba (subtle midline)
  X.strokeStyle = 'rgba(155,95,75,0.12)';
  X.lineWidth = Math.max(1.2, navR * 0.26);
  X.beginPath();
  X.moveTo(640, ribs[1] + px(0.0186, ribs[2])); X.lineTo(640, navel[1] - navR * 1.15);
  X.moveTo(640, navel[1] + navR * 1.15); X.lineTo(640, mons[1] - px(0.0326, mons[2]));
  X.stroke();

  // clavicles — ridges from the shoulder joint sweeping in to the sternum
  X.strokeStyle = 'rgba(165,95,75,0.38)';
  X.lineWidth = Math.max(1.6, px(0.0047, stern[2]));
  X.beginPath();
  X.moveTo(axilL, clavY + px(0.0047, shR[2]));
  X.quadraticCurveTo(640 - shHalf * 0.34 * shR[2], clavY - px(0.0116, shR[2]), 640, clavY - px(0.0069, shR[2]));
  X.moveTo(axilR, clavY + px(0.0047, shL[2]));
  X.quadraticCurveTo(640 + shHalf * 0.34 * shL[2], clavY - px(0.0116, shL[2]), 640, clavY - px(0.0069, shL[2]));
  X.stroke();

  // arms rise over the torso only when clutching upward in high pleasure
  if(a2 >= 0.5) drawArms();

  // ---- BREASTS: supine, gravity-spread, merged into the ribcage ----------
  // Key fix: supine breasts are NOT spheres. Lying on her back, the gland
  // settles laterally and flattens against the chest wall, so each breast is
  // drawn as a soft dome whose medial edge dissolves into the sternum and
  // whose lateral edge rolls over the flank. No closed outline — the dome is
  // blended with a radial fill plus an interior shadow, so no seam shows.
  const jig = (G.breast ? G.breast.p : 0) * 0.95;
  const squash = sm(0.86, 1.0, G.depth || 0);
  const erect = clamp(0.35 + 0.65 * ((G.ar || 0) / 100), 0, 1);
  const nipCol = G.char ? G.char.nippleColor : '#c25f63';
  const brD = B.sternum - 0.03;
  // Nipple-to-nipple is ~0.19 m, so each dome is centred ~0.09 m off the
  // midline — well inside the ribcage, not hanging off its edges.
  const brU = 0.092 * bdy;
  const brH = B.lift + 0.030 + jig * 0.003;      // apex barely proud of the chest

  for(const s of [-1, 1]){
    const bp = fpvProj(s * brU, brD, brH);
    const bx = bp[0], by = bp[1], bs = bp[2];
    // realistic supine breast: ~0.062 m wide, ~0.058 m tall before size mult.
    // (breastSize 0.45 -> bsz ~1.0, so this is the nominal figure.)
    const baseW = px((0.060 + 0.018 * (bsz - 1)) * (1 + squash * 0.10) - jig * 0.0002, bs);
    const baseH = px((0.056 + 0.016 * (bsz - 1)) * (1 - squash * 0.16) + jig * 0.0003, bs);

    X.save();
    X.translate(bx, by);
    X.rotate(s * 0.10);

    // A shadow tide on the chest wall UNDER the dome — this is what makes the
    // dome read as a mass standing off the ribs rather than a pasted circle.
    // Kept tight so it never carves a dark wedge out of the sternum.
    X.save();
    X.globalCompositeOperation = 'multiply';
    shade(-s * baseW * 0.10, baseH * 0.26, baseW * 0.72, baseH * 0.62,
      'rgba(142,76,60,0.22)', s * 0.06);
    X.restore();

    // Supine contour: full and low outboard (where the gland pools), tapering
    // to a soft medial slope that vanishes into the sternum. The two lower
    // control points push the silhouette down onto the ribcage.
    const dome = () => {
      X.moveTo(s * baseW * 0.55, -baseH * 0.72);              // upper medial
      X.bezierCurveTo(
        -s * baseW * 0.30, -baseH * 0.94,
        -baseW * 1.30,     -baseH * 0.34,
        -baseW * 1.10,      baseH * 0.34);                    // lateral bulge
      X.bezierCurveTo(
        -baseW * 0.90,      baseH * 0.92,
        -baseW * 0.16,      baseH * 1.02,
         s * baseW * 0.30,  baseH * 0.86);                    // lower fold
      X.bezierCurveTo(
         baseW * 0.86,      baseH * 0.60,
         baseW * 1.06,      baseH * 0.02,
         s * baseW * 0.55, -baseH * 0.72);                    // medial slope
      X.closePath();
    };
    X.beginPath(); dome();
    // The rim fades to transparent so the dome melts into the chest wall
    // instead of sitting on it as a pasted oval with a visible edge.
    const bg = X.createRadialGradient(
      -s * baseW * 0.34, -baseH * 0.30, baseW * 0.06,
       s * baseW * 0.06,  baseH * 0.06, baseW * 1.30);
    bg.addColorStop(0,    T.hi);
    bg.addColorStop(0.28, T.b);
    bg.addColorStop(0.62, T.b);
    bg.addColorStop(0.86, `rgba(${hexToRgb(T.s).join(',')},0.92)`);
    bg.addColorStop(1,    `rgba(${hexToRgb(T.s).join(',')},0)`);
    X.fillStyle = bg; X.fill();

    skClipIn(dome, () => {
      fAO(-s * baseW * 0.30, baseH * 0.80, baseW * 0.94, baseH * 0.30, 0.28);   // under-fold
      fSh( s * baseW * 0.80, baseH * 0.06, baseW * 0.34, baseH * 0.74, 'rgba(150,80,62,0.20)',  s * 0.10);
      fSh(-s * baseW * 0.86, baseH * 0.02, baseW * 0.28, baseH * 0.70, 'rgba(140,74,58,0.14)', -s * 0.10);
      fHi(-s * baseW * 0.40, -baseH * 0.26, baseW * 0.54, baseH * 0.44, 'rgba(255,238,220,0.24)');
    });

    // Inframammary fold: a soft crease, not a stroked arc
    X.save();
    X.globalCompositeOperation = 'multiply';
    shade(0, baseH * 0.74, baseW * 0.72, baseH * 0.16, 'rgba(148,80,62,0.26)', 0);
    X.restore();

    // Areola + nipple. Seen from above and between her legs, the areola sits
    // on the upper-medial face of the dome and is squashed to an ellipse.
    const aDistX = -s * baseW * 0.20, aDistY = -baseH * 0.22;
    const aer = px((0.018 + 0.0050 * clamp((G.ar || 0) / 100, 0, 1)) *
      Math.max(bsz, 0.84), bs);
    // soft areola halo — no hard rim, so it melts into the breast
    const ag = X.createRadialGradient(aDistX, aDistY, aer * 0.2, aDistX, aDistY, aer * 1.5);
    ag.addColorStop(0,   'rgba(206,128,110,0.42)');
    ag.addColorStop(0.62,'rgba(206,128,110,0.26)');
    ag.addColorStop(1,   'rgba(206,128,110,0)');
    X.fillStyle = ag;
    X.beginPath(); X.ellipse(aDistX, aDistY, aer * 1.5, aer * 1.2, s * 0.12, 0, TAU); X.fill();
    X.fillStyle = 'rgba(212,132,112,0.52)';
    X.beginPath(); X.ellipse(aDistX, aDistY, aer, aer * 0.80, s * 0.12, 0, TAU); X.fill();
    const nr = px((0.0068 + 0.0038 * erect) * Math.max(bsz * 0.9, 0.85), bs);
    X.fillStyle = nipCol;
    X.beginPath();
    X.ellipse(aDistX, aDistY, nr, nr * (1 + 0.20 * erect), s * 0.10, 0, TAU);
    X.fill();
    X.fillStyle = 'rgba(255,240,240,0.34)';
    X.beginPath();
    X.arc(aDistX - nr * 0.30, aDistY - nr * 0.30, nr * 0.30, 0, TAU);
    X.fill();

    X.restore();
  }

  // Sternum valley between the domes — a soft, low-contrast dip, not a dark
  // wedge. Anything stronger reads as a bowtie carved out of her chest.
  X.save();
  X.globalCompositeOperation = 'multiply';
  shade(640, (stern[1] + ribs[1]) / 2 + px(0.006, stern[2]),
    px(0.026, stern[2]), px(0.052, stern[2]) * bsz, 'rgba(158,92,72,0.16)', 0);
  X.restore();

  // ---- THE SPOT: vulva, anchored on the projected mons -------------------
  const vy = mons[1] + px(0.0140, mons[2]);
  const vs = mons[2];
  const eng = clamp((G.ar || 0) / 100, 0, 1);
  const iopen = 4.5 + 10.5 * (G.depth || 0);

  if((G.ar || 0) > 20){
    X.save();
    X.globalCompositeOperation = 'screen';
    shade(640, vy, px(0.065 + 0.028 * eng, vs), px(0.0512, vs), 'rgba(255,160,165,0.15)', 0);
    X.restore();
  }
  X.save();
  X.translate(640, vy);
  // vulvaS is authored at a ~430px scale, so scale it to this depth
  X.scale(vs / 430, vs / 430);
  vulvaS(0, 0, iopen, eng, T, { view: 'front' });
  X.restore();
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
  const sway = Math.sin(G.t * 1.7) * 3.2;
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