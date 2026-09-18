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
  // near thighs frame the lens
  const drawUp   = 0.28 + ple * 0.0026 + bounce * 0.004;
  const openAmt  = 0.055 + ple * 0.0011;

  const hipHalf = B.hipHalf * bdy, waistHalf = B.waistHalf * bdy;
  const ribHalf = B.ribHalf * bdy, shHalf = B.shHalf * bdy;

  // ---- projected skeleton (screen points + per-point scale) -------------
  const hipL  = fpvProj(-hipHalf,  B.hip,     B.lift);
  const hipR  = fpvProj( hipHalf,  B.hip,     B.lift);
  const mons  = fpvProj(0, B.mons,  B.lift + 0.04);
  const navel = fpvProj(0, B.navel, B.lift + 0.015);
  const waist = fpvProj(0, B.waist, B.lift + 0.004);
  const ribs  = fpvProj(0, B.ribs,  B.lift);
  const stern = fpvProj(0, B.sternum, B.lift + 0.006);
  const shL   = fpvProj( shHalf, B.shoulder, B.lift);
  const shR   = fpvProj(-shHalf, B.shoulder, B.lift);
  const kneeL = fpvProj( (B.kneeSpread + openAmt) * bdy, B.knee, B.lift + drawUp);
  const kneeR = fpvProj(-(B.kneeSpread + openAmt) * bdy, B.knee, B.lift + drawUp);
  const ankL  = fpvProj( B.footSpread * bdy, B.ankle, 0.15);
  const ankR  = fpvProj(-B.footSpread * bdy, B.ankle, 0.15);
  const toeL  = fpvProj( B.footSpread * bdy * 1.08, 0.20, 0.11);
  const toeR  = fpvProj(-B.footSpread * bdy * 1.08, 0.20, 0.11);

  // helper: a screen-space radius scaled for the depth of a limb
  const R = (base, sc) => base * sc * 0.0016;

  // ---- LEGS: near thighs looming wide, foreshortened toward the lens ----
  for(const s of [-1, 1]){
    const hip = s < 0 ? hipL : hipR;
    const knee = s < 0 ? kneeL : kneeR;
    const ank  = s < 0 ? ankL : ankR;
    const toe  = s < 0 ? toeL : toeR;

    // thigh: hip -> knee. It is the biggest form in frame.
    limbS([hip[0], hip[1]], [knee[0], knee[1]],
      R(48, hip[2]), R(40, knee[2]), T, { belly: 1.14, aoA: 0.30 });
    // calf: knee -> ankle, narrower, receding
    limbS([knee[0], knee[1]], [ank[0], ank[1]],
      R(38, knee[2]), R(24, ank[2]), T, { belly: 1.12 });
    // foot/ankle cap fading off the bottom of the frame
    limbS([ank[0], ank[1]], [toe[0], toe[1]], R(24, ank[2]), R(18, toe[2]), T, { belly: 1.05 });

    // knee cap: a continuous tone across the thigh/calf joint
    X.save();
    X.beginPath();
    X.ellipse(knee[0], knee[1], R(42, knee[2]), R(36, knee[2]), s * 0.42, 0, TAU);
    const kg = X.createRadialGradient(knee[0] - s * 10, knee[1] - 12, 3, knee[0], knee[1], R(46, knee[2]));
    kg.addColorStop(0, T.b); kg.addColorStop(0.66, T.b); kg.addColorStop(1, T.s);
    X.fillStyle = kg; X.fill();
    X.restore();
    fHi(knee[0], knee[1] - 12, R(14, knee[2]), R(18, knee[2]), 'rgba(255,238,220,0.09)');

    // vastus medialis teardrop contour down the inner thigh
    X.strokeStyle = 'rgba(185,115,95,0.18)';
    X.lineWidth = Math.max(1.6, R(2.6, knee[2]));
    X.beginPath();
    X.moveTo(hip[0] + s * R(24, hip[2]), hip[1] - R(30, hip[2]));
    X.bezierCurveTo(
      knee[0] - s * R(12, knee[2]), knee[1] - R(70, knee[2]),
      knee[0] + s * R(5, knee[2]),  knee[1] - R(30, knee[2]),
      knee[0] + s * R(3, knee[2]),  knee[1] - R(14, knee[2]));
    X.stroke();

    // inguinal groove where the thigh meets the pelvis
    X.strokeStyle = 'rgba(165,95,75,0.24)';
    X.lineWidth = Math.max(1.4, R(2.2, hip[2]));
    X.beginPath();
    X.moveTo(hip[0] * 0.72 + 640 * 0.28, hip[1] + R(4, hip[2]));
    X.quadraticCurveTo(knee[0] * 0.42 + 640 * 0.58, hip[1] - R(16, hip[2]),
      knee[0], knee[1] + R(46, knee[2]));
    X.stroke();
  }

  // ---- PELVIS BLOCK: ties the thighs to the torso, hides the leg roots ---
  const pelvY = (hipL[1] + hipR[1]) / 2;
  X.save();
  X.beginPath();
  X.moveTo(hipL[0], hipL[1] - R(16, hipL[2]));
  X.bezierCurveTo(hipL[0] - R(10, hipL[2]), mons[1] + R(20, mons[2]), mons[0] - R(56, mons[2]), mons[1] - R(6, mons[2]), mons[0], mons[1] - R(8, mons[2]));
  X.bezierCurveTo(mons[0] + R(56, mons[2]), mons[1] - R(6, mons[2]), hipR[0] + R(10, hipR[2]), mons[1] + R(20, mons[2]), hipR[0], hipR[1] - R(16, hipR[2]));
  X.bezierCurveTo(hipR[0] + R(6, hipR[2]), pelvY + R(34, hipR[2]), hipR[0] - R(20, hipR[2]), pelvY + R(48, hipR[2]), 640, pelvY + R(52, hipR[2]));
  X.bezierCurveTo(hipL[0] + R(20, hipL[2]), pelvY + R(48, hipL[2]), hipL[0] - R(6, hipL[2]), pelvY + R(34, hipL[2]), hipL[0], hipL[1] - R(16, hipL[2]));
  X.closePath();
  const pg = X.createLinearGradient(640, mons[1], 640, pelvY + R(52, hipL[2]));
  pg.addColorStop(0, T.b); pg.addColorStop(0.55, T.b); pg.addColorStop(1, T.s);
  X.fillStyle = pg; X.fill();
  X.restore();
  skClipIn(() => {
    X.moveTo(hipL[0], hipL[1] - R(16, hipL[2]));
    X.bezierCurveTo(hipL[0] - R(10, hipL[2]), mons[1] + R(20, mons[2]), mons[0] - R(56, mons[2]), mons[1] - R(6, mons[2]), mons[0], mons[1] - R(8, mons[2]));
    X.bezierCurveTo(mons[0] + R(56, mons[2]), mons[1] - R(6, mons[2]), hipR[0] + R(10, hipR[2]), mons[1] + R(20, mons[2]), hipR[0], hipR[1] - R(16, hipR[2]));
    X.bezierCurveTo(hipR[0] + R(6, hipR[2]), pelvY + R(34, hipR[2]), hipR[0] - R(20, hipR[2]), pelvY + R(48, hipR[2]), 640, pelvY + R(52, hipR[2]));
    X.bezierCurveTo(hipL[0] + R(20, hipL[2]), pelvY + R(48, hipL[2]), hipL[0] - R(6, hipL[2]), pelvY + R(34, hipL[2]), hipL[0], hipL[1] - R(16, hipL[2]));
    X.closePath();
  }, () => {
    fAO(hipL[0] + R(18, hipL[2]), hipL[1], R(30, hipL[2]), R(34, hipL[2]), 0.26, -0.4);
    fAO(hipR[0] - R(18, hipR[2]), hipR[1], R(30, hipR[2]), R(34, hipR[2]), 0.26,  0.4);
    fHi(640, mons[1] + R(6, mons[2]), R(34, mons[2]), R(20, mons[2]), 'rgba(255,235,215,0.14)');
  });
  skLine(() => {
    X.moveTo(hipL[0], hipL[1] - R(16, hipL[2]));
    X.bezierCurveTo(mons[0] - R(56, mons[2]), mons[1] - R(6, mons[2]), mons[0] + R(56, mons[2]), mons[1] - R(6, mons[2]), hipR[0], hipR[1] - R(16, hipR[2]));
  }, T, 1.4, 0.26);

  // ---- ARMS: drawn under the torso so the shoulder reads connected -------
  const a2 = sm(55, 80, ple);
  const drawArms = () => {
    for(const s of [-1, 1]){
      const sh = s < 0 ? shR : shL;
      // elbow drifts out and down; at rest the forearm lies beside her flank,
      // at high pleasure it lifts to clutch your back
      const elD = lerp(B.shoulder + 0.26, B.shoulder + 0.13, a2);
      const elU = lerp(0.26, 0.40, a2) * s * bdy + s * 0.02;
      const el  = fpvProj(elU, elD, B.lift + lerp(0.02, 0.10, a2));
      const haD = lerp(B.shoulder + 0.42, B.shoulder + 0.02, a2);
      const haU = lerp(0.30, 0.24, a2) * s * bdy;
      const ha  = fpvProj(haU, haD, B.lift + lerp(0.05, 0.24, a2));
      limbS([sh[0], sh[1]], [el[0], el[1]], R(20, sh[2]), R(15, el[2]), T, { belly: 1.08, aoA: 0.3 });
      limbS([el[0], el[1]], [ha[0], ha[1]], R(15, el[2]), R(11, ha[2]), T, { belly: 1.05 });
      const handAng = s < 0 ? (2.2 - a2 * 3.1) : (Math.PI - 2.2 + a2 * 3.1);
      handS(ha[0], ha[1], handAng, lerp(1.05, 1.30, a2) * (ha[2] / 420),
        T, { curl: lerp(0.42, 0.68, a2), spread: 0.22 });
    }
  };
  if(a2 < 0.5) drawArms();

  // ---- TORSO: hourglass silhouette from mons to collarbones --------------
  const ribsc = 1.0;
  const torso = () => {
    X.moveTo(hipL[0], hipL[1] - R(18, hipL[2]));
    // up the left flank: hip -> waist -> ribcage -> axilla
    X.bezierCurveTo(
      hipL[0] - R(2, hipL[2]), hipL[1] - R(58, hipL[2]),
      waist[0] - R(1, waist[2]) - waistHalf * waist[2], waist[1],
      640 - waistHalf * waist[2], waist[1]);
    X.bezierCurveTo(
      640 - (waistHalf * 0.86) * ribs[2], waist[1] - R(30, ribs[2]),
      640 - ribHalf * ribs[2], ribs[1] + R(6, ribs[2]),
      640 - ribHalf * ribsc, ribs[1]);
    X.bezierCurveTo(
      640 - (ribHalf * 0.92) * stern[2], ribs[1] - R(26, stern[2]),
      640 - shHalf * 0.72 * stern[2], stern[1] - R(14, stern[2]),
      640 - shHalf * shL[2] * 0.62, stern[1] - R(4, stern[2]));
    // across the collarbones
    X.bezierCurveTo(640 - 20, stern[1] - R(24, stern[2]), 640 + 20, stern[1] - R(24, stern[2]), 640 + shHalf * shL[2] * 0.62, stern[1] - R(4, stern[2]));
    // down the right flank (mirror)
    X.bezierCurveTo(
      640 + shHalf * 0.72 * stern[2], stern[1] - R(14, stern[2]),
      640 + (ribHalf * 0.92) * stern[2], ribs[1] - R(26, stern[2]),
      640 + ribHalf * ribsc, ribs[1]);
    X.bezierCurveTo(
      640 + ribHalf * ribs[2], ribs[1] + R(6, ribs[2]),
      640 + (waistHalf * 0.86) * ribs[2], waist[1] - R(30, ribs[2]),
      640 + waistHalf * waist[2], waist[1]);
    X.bezierCurveTo(
      hipR[0] + R(2, hipR[2]), hipR[1] - R(58, hipR[2]),
      hipR[0], hipR[1] - R(18, hipR[2]),
      hipR[0], hipR[1] - R(18, hipR[2]));
    X.closePath();
  };
  skFillShape(torso, T, [640 - 60, waist[1], 640 + 46, stern[1] - 30]);
  skClipIn(torso, () => {
    fAO(640 - ribHalf * ribs[2] * 0.94, (waist[1] + ribs[1]) / 2, R(16, ribs[2]), R(96, ribs[2]), 0.28);
    fAO(640 + ribHalf * ribs[2] * 0.94, (waist[1] + ribs[1]) / 2, R(16, ribs[2]), R(96, ribs[2]), 0.28);
    fAO(640 - hipHalf * hipL[2] * 0.94, hipL[1] - R(10, hipL[2]), R(26, hipL[2]), R(34, hipL[2]), 0.16);
    fAO(640 + hipHalf * hipR[2] * 0.94, hipR[1] - R(10, hipR[2]), R(26, hipR[2]), R(34, hipR[2]), 0.16);
    fSh(640 - waistHalf * waist[2] * 0.72, waist[1] - R(40, ribs[2]), R(30, ribs[2]), R(14, ribs[2]), 'rgba(160,90,70,0.22)', 0.18);
    fSh(640 + waistHalf * waist[2] * 0.72, waist[1] - R(40, ribs[2]), R(30, ribs[2]), R(14, ribs[2]), 'rgba(160,90,70,0.22)', -0.18);
    fHi(640, (ribs[1] + stern[1]) / 2, R(24, stern[2]), R(60, stern[2]), 'rgba(255,235,215,0.20)');
    fHi(640, (waist[1] + navel[1]) / 2, R(30, waist[2]), R(48, waist[2]), 'rgba(255,235,215,0.17)');
  });
  skLine(torso, T, 1.4, 0.26);

  // chest flush / vasocongestion
  shade(640, (ribs[1] + stern[1]) / 2, R(60, stern[2]), R(30, stern[2]), `rgba(${brR},${brG},${brB},${0.05 + E.blush * 0.16})`, 0);

  // sex flush patches across sternum and belly
  if((G.ar || 0) > 30){
    const ra = (G.ar - 30) / 70, rc = `rgba(${brR},${brG},${brB},${0.04 + 0.08 * ra})`;
    [[-30, stern[1] + R(10, stern[2]), 26, 16], [30, stern[1] + R(10, stern[2]), 26, 16],
     [-24, navel[1] + R(6, navel[2]), 28, 18], [24, navel[1] + R(6, navel[2]), 28, 18]]
      .forEach(([dx, y, rx, ry]) => shade(640 + dx, y, R(rx, navel[2]), R(ry, navel[2]), rc, 0));
  }

  // navel
  const navR = R(6, navel[2]);
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
  X.moveTo(640, ribs[1] + R(8, ribs[2])); X.lineTo(640, navel[1] - navR * 1.15);
  X.moveTo(640, navel[1] + navR * 1.15); X.lineTo(640, mons[1] - R(14, mons[2]));
  X.stroke();

  // clavicles
  X.strokeStyle = 'rgba(165,95,75,0.38)';
  X.lineWidth = Math.max(1.6, R(2, stern[2]));
  X.beginPath();
  X.moveTo(640 - shHalf * shL[2] * 0.52, stern[1] - R(10, stern[2]));
  X.quadraticCurveTo(640 - shHalf * shL[2] * 0.2, stern[1] - R(2, stern[2]), 640, stern[1] - R(6, stern[2]));
  X.moveTo(640 + shHalf * shR[2] * 0.52, stern[1] - R(10, stern[2]));
  X.quadraticCurveTo(640 + shHalf * shR[2] * 0.2, stern[1] - R(2, stern[2]), 640, stern[1] - R(6, stern[2]));
  X.stroke();

  // arms rise over the torso only when clutching upward in high pleasure
  if(a2 >= 0.5) drawArms();

  // ---- BREASTS: supine, gravity-spread, draped over the ribcage ----------
  const jig = (G.breast ? G.breast.p : 0) * 0.95;
  const squash = sm(0.86, 1.0, G.depth || 0);
  const erect = clamp(0.35 + 0.65 * ((G.ar || 0) / 100), 0, 1);
  const nipCol = G.char ? G.char.nippleColor : '#c25f63';
  const brD = B.sternum - 0.04;

  for(const s of [-1, 1]){
    // apex slightly outboard and below the sternum, dropped by depth of stroke
    const bu = s * (0.145 * bdy) + s * 0.006 * bsz;
    const bh = B.lift + 0.055 + jig * 0.004 - squash * 0.008;
    const bp = fpvProj(bu, brD, bh);
    const bx = bp[0], by = bp[1], bs = bp[2];
    // supine drape: wider than tall, spilling toward the flank
    const baseW = R(40 * bsz * (1 + squash * 0.08) - jig * 0.1, bs);
    const baseH = R(46 * bsz * (1 - squash * 0.12) + jig * 0.15, bs);

    X.save();
    X.translate(bx, by);
    X.rotate(s * 0.13);

    // contact shadow haloing the dome on the chest wall
    X.save();
    X.globalCompositeOperation = 'multiply';
    shade(s * 8, baseH * 0.34, baseW * 1.14, baseH * 1.02, 'rgba(160,92,70,0.26)', s * 0.10);
    X.restore();

    // anatomical supine contour: flattened at the sternum, full at the flank
    const dome = () => {
      X.moveTo(0, -baseH * 0.86);
      X.bezierCurveTo(s * baseW * 0.16 - baseW * 0.55, -baseH * 0.60,
        -baseW * 1.06, -baseH * 0.05, -baseW * 0.88, baseH * 0.46);
      X.bezierCurveTo(-baseW * 0.68, baseH * 0.94, -baseW * 0.15, baseH * 1.04, 0, baseH);
      X.bezierCurveTo(baseW * 0.58, baseH * 0.98, baseW * 1.10, baseH * 0.56, baseW * 0.96, 0);
      X.bezierCurveTo(baseW * 0.84, -baseH * 0.56, s * baseW * 0.14 + baseW * 0.46, -baseH * 0.84, 0, -baseH * 0.86);
      X.closePath();
    };
    X.beginPath(); dome();
    const bg = X.createRadialGradient(-s * baseW * 0.22, -baseH * 0.28, baseW * 0.12, 0, 0, baseW * 1.45);
    bg.addColorStop(0, T.hi); bg.addColorStop(0.5, T.b); bg.addColorStop(1, T.s);
    X.fillStyle = bg; X.fill();
    skClipIn(dome, () => {
      fAO(-s * baseW * 0.15, baseH * 0.86, baseW * 0.8, baseH * 0.24, 0.30);
      fSh(s * baseW * 0.72, baseH * 0.1, baseW * 0.3, baseH * 0.7, 'rgba(160,90,70,0.20)', s * 0.1);
      fSh(-s * baseW * 0.78, baseH * 0.05, baseW * 0.26, baseH * 0.66, 'rgba(150,85,66,0.14)', -s * 0.1);
    });
    fHi(-s * baseW * 0.16, -baseH * 0.2, baseW * 0.5, baseH * 0.44, 'rgba(255,238,220,0.30)');

    // inframammary fold
    X.strokeStyle = 'rgba(155,90,72,0.34)';
    X.lineWidth = Math.max(1.5, baseW * 0.045);
    X.beginPath();
    X.ellipse(s * 2, baseH * 0.72, baseW * 0.46, baseH * 0.22, 0, 0.2, Math.PI - 0.2);
    X.stroke();

    // areola + nipple, tilted anatomically up and out
    const aDistX = s * 3.5, aDistY = -baseH * 0.06;
    const aer = (10.5 + 2.4 * clamp((G.ar || 0) / 100, 0, 1)) * Math.max(bsz, 0.84) * (bs / 420);
    X.fillStyle = 'rgba(206,128,110,0.36)';
    X.beginPath(); X.ellipse(aDistX, aDistY, aer + 2.6, aer * 0.86, s * 0.1, 0, TAU); X.fill();
    X.fillStyle = 'rgba(216,138,118,0.62)';
    X.beginPath(); X.ellipse(aDistX, aDistY, aer, aer * 0.82, s * 0.1, 0, TAU); X.fill();
    X.fillStyle = 'rgba(188,118,102,0.70)';
    for(let i = 0; i < 7; i++){
      const ta = i / 7 * TAU + 0.35;
      X.beginPath();
      X.arc(aDistX + Math.cos(ta) * aer * 0.72, aDistY + Math.sin(ta) * aer * 0.62, aer * 0.1, 0, TAU);
      X.fill();
    }
    const nr = (3.6 + 2.1 * erect) * Math.max(bsz * 0.9, 0.85) * (bs / 420);
    X.fillStyle = nipCol;
    X.beginPath(); X.ellipse(aDistX, aDistY, nr, nr * (1 + 0.18 * erect), s * 0.08, 0, TAU); X.fill();
    X.fillStyle = 'rgba(255,245,245,0.42)';
    X.beginPath(); X.arc(aDistX - nr * 0.32, aDistY - nr * 0.32, nr * 0.30, 0, TAU); X.fill();

    X.restore();
  }

  // cleavage shadow between the domes
  X.save();
  X.globalCompositeOperation = 'multiply';
  shade(640, stern[1] + R(6, stern[2]), R(10, stern[2]), R(38, stern[2]) * bsz, 'rgba(160,92,70,0.24)', 0);
  X.restore();

  // ---- THE SPOT: vulva, anchored on the projected mons -------------------
  const vy = mons[1] + R(6, mons[2]);
  const vs = mons[2];
  const eng = clamp((G.ar || 0) / 100, 0, 1);
  const iopen = 4.5 + 10.5 * (G.depth || 0);

  if((G.ar || 0) > 20){
    X.save();
    X.globalCompositeOperation = 'screen';
    shade(640, vy, R(28 + 12 * eng, vs), R(22, vs), 'rgba(255,160,165,0.15)', 0);
    X.restore();
  }
  X.save();
  X.translate(640, vy);
  X.scale(vs / 420, vs / 420);
  vulvaS(0, 0, iopen, eng, T, { view: 'front' });
  X.restore();
}
