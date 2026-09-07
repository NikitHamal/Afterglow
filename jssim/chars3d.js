// Afterglow 3D — module: chars3d (ultra-high-fidelity procedural anime characters)
// Sculpted anatomical hentai figures with organic curves, gravity deformation & volumetric skin
'use strict';

/* ============================================================
   ADVANCED GEOMETRY GENERATORS & ANATOMICAL MODIFIERS
   ============================================================ */

/* Anatomically bowed & sculpted limb segment (vastus, gastrocnemius, biceps) */
function sculptedSegMesh(rTop, rBot, len, mat, rMid, midT, lateralBias = 0, posteriorBias = 0) {
  const segsY = 22, segsR = 20;
  const rm = rMid != null ? rMid : (rTop * 0.54 + rBot * 0.46) * 1.08;
  const mt = midT != null ? midT : 0.42;

  const geo = new THREE.CylinderGeometry(rTop, rBot, len, segsR, segsY, true);
  geo.translate(0, -len * 0.5, 0);

  const pos = geo.attributes.position;
  for (let i = 0; i < pos.count; i++) {
    let x = pos.getX(i), y = pos.getY(i), z = pos.getZ(i);
    const t = clamp(-y / len, 0, 1);

    // Anatomical profile expansion
    let rProfile;
    if (t < mt) {
      const u = t / mt;
      rProfile = rTop + (rm - rTop) * Math.sin(u * Math.PI * 0.5);
    } else {
      const u = (t - mt) / (1 - mt);
      rProfile = rm + (rBot - rm) * Math.sin(u * Math.PI * 0.5);
    }

    const currentR = Math.hypot(x, z) || 1e-5;
    const scale = rProfile / currentR;

    // Muscular asymmetry (e.g., calf bulge posterior/lateral, thigh vastus medialis)
    const curveEnv = Math.sin(t * Math.PI);
    x = x * scale + lateralBias * curveEnv * (x > 0 ? 1.2 : 0.7);
    z = z * scale + posteriorBias * curveEnv;

    pos.setXYZ(i, x, y, z);
  }

  geo.computeVertexNormals();
  const m = new THREE.Mesh(geo, mat);
  m.castShadow = true;
  m.receiveShadow = true;
  return m;
}

/* Fallback lathe segment helper */
function segMesh(rTop, rBot, len, mat, rMid, midT) {
  return sculptedSegMesh(rTop, rBot, len, mat, rMid, midT, 0, 0);
}

/* Subsurface airbrushed blush & skin vascularity per-vertex */
function blush3(mesh, fn) {
  if (!mesh) return;
  const mat = mesh.material;
  if (mat && (mat.isMeshToonMaterial || mat.isMeshPhysicalMaterial || mat.isMeshStandardMaterial) && !mat.vertexColors) {
    mesh.material = mat.clone();
    mesh.material.vertexColors = true;
  }
  const g = mesh.geometry, p = g.getAttribute('position'), n = p.count;
  const col = new Float32Array(n * 3);
  const v = new THREE.Vector3();
  for (let i = 0; i < n; i++) {
    v.fromBufferAttribute(p, i);
    const c = fn(v.x, v.y, v.z);
    col[i * 3] = c[0]; col[i * 3 + 1] = c[1]; col[i * 3 + 2] = c[2];
  }
  g.setAttribute('color', new THREE.BufferAttribute(col, 3));
}

function ballMesh(r, mat, squishY, segW = 20, segH = 16) {
  const m = new THREE.Mesh(new THREE.SphereGeometry(r, segW, segH), mat);
  if (squishY) m.scale.y = squishY;
  m.castShadow = true; m.receiveShadow = true;
  return m;
}

/* Tapered, end-capped tube for silky tresses */
function taperTube3(pts, r0, r1, mat, segs = 18, radial = 8) {
  const curve = new THREE.CatmullRomCurve3(pts.map(p => new THREE.Vector3(p[0], p[1], p[2])));
  const frames = curve.computeFrenetFrames(segs, false);
  const positions = [], normals = [], indices = [];
  const P = new THREE.Vector3(), N = new THREE.Vector3();

  for (let i = 0; i <= segs; i++) {
    const t = i / segs;
    curve.getPoint(t, P);
    const fr = Math.min(i, segs - 1);
    const Nrm = frames.normals[fr], Bnr = frames.binormals[fr];
    const r = r0 + (r1 - r0) * Math.pow(t, 0.85);

    for (let j = 0; j <= radial; j++) {
      const a = (j / radial) * Math.PI * 2;
      const cn = Math.cos(a), sn = Math.sin(a);
      N.set(cn * Nrm.x + sn * Bnr.x, cn * Nrm.y + sn * Bnr.y, cn * Nrm.z + sn * Bnr.z);
      positions.push(P.x + N.x * r, P.y + N.y * r, P.z + N.z * r);
      normals.push(N.x, N.y, N.z);
    }
  }

  for (let i = 0; i < segs; i++) {
    for (let j = 0; j < radial; j++) {
      const a = i * (radial + 1) + j, b = a + radial + 1;
      indices.push(a, a + 1, b, b, a + 1, b + 1);
    }
  }

  const cap = (t, flip) => {
    const C = curve.getPoint(t);
    const T = frames.tangents[t >= 1 ? segs - 1 : 0];
    const ci = positions.length / 3;
    positions.push(C.x, C.y, C.z);
    normals.push(T.x * flip, T.y * flip, T.z * flip);
    const ring = (t >= 1 ? segs : 0) * (radial + 1);
    for (let j = 0; j < radial; j++) {
      if (flip < 0) indices.push(ci, ring + j + 1, ring + j);
      else indices.push(ci, ring + j, ring + j + 1);
    }
  };
  cap(0, -1); cap(1, 1);

  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
  g.setAttribute('normal', new THREE.Float32BufferAttribute(normals, 3));
  g.setIndex(indices);
  const m = new THREE.Mesh(g, mat);
  m.castShadow = true;
  return m;
}

/* ============================================================
   ORGANIC PROCEDURAL FEMALE ANATOMY GEOMETRIES
   ============================================================ */

/* Volumetric teardrop breast with natural gravity slope & lateral fullness */
function makeTeardropBreastGeo(radius, scale) {
  const r = radius * scale;
  const geo = new THREE.SphereGeometry(r, 28, 22);
  const pos = geo.attributes.position;

  for (let i = 0; i < pos.count; i++) {
    let x = pos.getX(i), y = pos.getY(i), z = pos.getZ(i);

    if (z > -r * 0.25) {
      const forwardT = (z + r * 0.25) / (r * 1.25);
      const sagWeight = Math.pow(forwardT, 1.3);

      if (y > 0) {
        // Pectoral slope: gradual incline from clavicle
        y *= (1.0 - forwardT * 0.32);
        z += forwardT * r * 0.22;
      } else {
        // Natural gravity sag & inframammary undercut
        y -= (1.0 - forwardT) * r * 0.16;
        z += forwardT * r * 0.36;
        x *= (1.0 + (1.0 - forwardT) * 0.18); // lateral cleavage fullness
      }
    }
    pos.setXYZ(i, x, y, z);
  }

  geo.computeVertexNormals();
  return geo;
}

/* Sculpted hourglass torso: ribcage expansion, iliac crest flare, navel & spinal groove */
function makeSculptedTorsoGeo(cR, wR, hR, height) {
  const geo = new THREE.CylinderGeometry(cR, hR, height, 30, 24, true);
  geo.translate(0, height * 0.5, 0);

  const pos = geo.attributes.position;
  for (let i = 0; i < pos.count; i++) {
    let x = pos.getX(i), y = pos.getY(i), z = pos.getZ(i);
    const t = clamp(y / height, 0, 1);

    // Anatomical waist constriction at t = 0.38
    let waistMod = 1.0;
    if (t > 0.12 && t < 0.78) {
      const u = (t - 0.12) / 0.66;
      waistMod = 1.0 - Math.sin(u * Math.PI) * 0.22;
    }

    // Oval feminine cross-section (wide coronal, slender sagittal)
    let wX = waistMod * 1.08;
    let wZ = waistMod * 0.80;

    // Ribcage arch upper flare (t > 0.55)
    if (t > 0.55) {
      const ribT = (t - 0.55) / 0.45;
      wX *= (1.0 + Math.sin(ribT * Math.PI) * 0.08);
      wZ *= (1.0 + Math.sin(ribT * Math.PI) * 0.04);
    }

    // Soft hypogastric belly curve in front (z > 0, t: 0.15 - 0.48)
    if (z > 0 && t > 0.14 && t < 0.50) {
      const bCurve = Math.sin(((t - 0.14) / 0.36) * Math.PI);
      const xAtten = Math.cos(clamp(x / (cR * wX), -1, 1) * Math.PI * 0.5);
      z += bCurve * xAtten * 0.020;
    }

    // Navel indentation (t around 0.34, center front)
    if (z > 0 && Math.abs(t - 0.34) < 0.05 && Math.abs(x) < 0.028) {
      const nd = 1.0 - Math.hypot(x / 0.028, (t - 0.34) / 0.05);
      if (nd > 0) z -= nd * nd * 0.012;
    }

    // Erector spinae back groove (z < 0, midline)
    if (z < 0 && Math.abs(x) < 0.038) {
      const spD = 1.0 - Math.abs(x) / 0.038;
      z += spD * 0.010; // indentation inward
    }

    // Dimples of Venus (sacral dimples at lower back, t = 0.18, x = +/- 0.04)
    if (z < 0 && Math.abs(t - 0.18) < 0.045 && Math.abs(Math.abs(x) - 0.042) < 0.022) {
      z += 0.007;
    }

    pos.setXYZ(i, x * wX, y, z * wZ);
  }

  geo.computeVertexNormals();
  return geo;
}

/* Anatomically sculpted voluptuous gluteal cheek with heart-shelf and crease */
function makeGluteCheekGeo(radius, side) {
  const geo = new THREE.SphereGeometry(radius, 24, 20);
  const pos = geo.attributes.position;

  for (let i = 0; i < pos.count; i++) {
    let x = pos.getX(i), y = pos.getY(i), z = pos.getZ(i);

    // Posterior projection (buttock shelf)
    if (z < 0) {
      const pT = -z / radius;
      z *= (1.0 + pT * 0.28);
      y *= (1.0 + pT * 0.14);
      x *= (1.0 + pT * 0.18);
    }

    // Infragluteal under-crease flattening
    if (y < -radius * 0.35 && z < 0) {
      const foldT = (-y - radius * 0.35) / (radius * 0.65);
      y += foldT * 0.028 * Math.abs(z / radius);
    }

    // Medial cleft flattening toward midline
    if (side * x < 0) {
      x *= 0.78;
    }

    pos.setXYZ(i, x, y, z);
  }

  geo.computeVertexNormals();
  return geo;
}

/* ============================================================
   SKIN PALETTES
   ============================================================ */
function herSkin3() {
  const sk = (typeof getSkin === 'function') ? getSkin() : null;
  return {
    base: (sk && sk.her) || '#f2c8b8',
    shade: (sk && sk.herSh) || '#dca290',
    dark: (sk && sk.herDk) || '#b87c6c'
  };
}
const HIS_SKIN3 = { base: '#dfa878', shade: '#a86c44', dark: '#7a4224' };

/* ============================================================
   ANIME FACE — High-DPI Canvas Painted Expression Texture
   ============================================================ */
const FACE_PX = 1024;
function makeFaceTexture3() {
  const cv = document.createElement('canvas');
  cv.width = cv.height = FACE_PX;
  const tex = new THREE.CanvasTexture(cv);
  tex.encoding = THREE.sRGBEncoding;
  tex.anisotropy = 4;
  return { tex, cv, ctx: cv.getContext('2d') };
}

function paintFace3(ctx, E, char) {
  const S = FACE_PX;
  ctx.clearRect(0, 0, S, S);

  const eyeCol = char.eyeColor || '#4a2c33';
  const blushHex = char.blushColor || '#e86070';
  const lipHex = char.lipColor || '#b3555f';

  const eyeY = S * 0.478;
  const eyeDX = S * 0.160;
  const open = clamp(E.eye * (1.0 - E.rolled * 0.58), 0, 1);

  for (const s of [-1, 1]) {
    const ex = S * 0.5 + s * eyeDX;

    // Soft orbital socket shadow
    ctx.save();
    ctx.globalAlpha = 0.16 + E.blush * 0.14;
    ctx.fillStyle = '#9e524a';
    ctx.beginPath();
    ctx.ellipse(ex, eyeY + S * 0.014, S * 0.086, S * 0.058, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();

    if (open > 0.12) {
      const eh = S * 0.058 * open;
      const ew = S * 0.068;

      // Sclera with soft pink corner gradient
      ctx.fillStyle = '#faf5f2';
      ctx.beginPath();
      ctx.ellipse(ex, eyeY, ew, eh, 0, 0, Math.PI * 2);
      ctx.fill();

      // Caruncle pink corner
      ctx.fillStyle = 'rgba(220,145,140,0.55)';
      ctx.beginPath();
      ctx.ellipse(ex - s * ew * 0.88, eyeY, ew * 0.18, eh * 0.35, 0, 0, Math.PI * 2);
      ctx.fill();

      // Iris: Large, glassy, rich hentai stroma
      const ir = eh * 0.95;
      const iy = eyeY + eh * 0.16 - E.rolled * eh * 0.55;
      const grad = ctx.createRadialGradient(ex, iy - ir * 0.2, ir * 0.1, ex, iy, ir);
      grad.addColorStop(0, eyeCol);
      grad.addColorStop(0.55, eyeCol);
      grad.addColorStop(0.85, '#2e121a');
      grad.addColorStop(1, '#14060a');
      ctx.fillStyle = grad;
      ctx.beginPath(); ctx.arc(ex, iy, ir, 0, Math.PI * 2); ctx.fill();

      // Iris fiber ring
      ctx.strokeStyle = 'rgba(255,255,255,0.18)';
      ctx.lineWidth = S * 0.003;
      for (let r = 0; r < 8; r++) {
        const ra = (r / 8) * Math.PI * 2;
        ctx.beginPath();
        ctx.moveTo(ex + Math.cos(ra) * ir * 0.35, iy + Math.sin(ra) * ir * 0.35);
        ctx.lineTo(ex + Math.cos(ra) * ir * 0.82, iy + Math.sin(ra) * ir * 0.82);
        ctx.stroke();
      }

      // Deep dilated pupil
      ctx.fillStyle = '#0f0407';
      ctx.beginPath(); ctx.arc(ex, iy, ir * 0.46, 0, Math.PI * 2); ctx.fill();

      // Specular catchlights (Key light + warm fill reflection)
      ctx.fillStyle = 'rgba(255,255,255,0.96)';
      ctx.beginPath(); ctx.arc(ex - ir * 0.36, iy - ir * 0.40, ir * 0.32, 0, Math.PI * 2); ctx.fill();
      ctx.fillStyle = 'rgba(255,255,255,0.60)';
      ctx.beginPath(); ctx.arc(ex + ir * 0.38, iy + ir * 0.26, ir * 0.18, 0, Math.PI * 2); ctx.fill();

      // Upper lash line: Thick, winged, feathered
      ctx.strokeStyle = '#260e15';
      ctx.lineWidth = S * 0.021;
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(ex - ew * 1.05, eyeY - eh * 0.10);
      ctx.quadraticCurveTo(ex, eyeY - eh * 1.35, ex + ew * 1.05, eyeY - eh * 0.10);
      ctx.stroke();

      // Wing flick
      ctx.lineWidth = S * 0.014;
      ctx.beginPath();
      ctx.moveTo(ex + s * ew * 0.94, eyeY - eh * 0.18);
      ctx.lineTo(ex + s * ew * 1.38, eyeY - eh * 0.78);
      ctx.stroke();

      // Fine lower lashes
      ctx.strokeStyle = 'rgba(65,28,38,0.65)';
      ctx.lineWidth = S * 0.007;
      ctx.beginPath();
      ctx.moveTo(ex - ew * 0.78, eyeY + eh * 0.76);
      ctx.quadraticCurveTo(ex, eyeY + eh * 1.10, ex + ew * 0.78, eyeY + eh * 0.76);
      ctx.stroke();
    } else {
      // Ecstatic closed eyes
      ctx.strokeStyle = '#260e15';
      ctx.lineWidth = S * 0.023;
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(ex - S * 0.075, eyeY);
      ctx.quadraticCurveTo(ex, eyeY + S * 0.034, ex + S * 0.075, eyeY);
      ctx.stroke();
    }

    // Eyebrows (lifted and trembling with arousal)
    ctx.strokeStyle = char.hairColor || '#231318';
    ctx.lineWidth = S * 0.015;
    ctx.lineCap = 'round';
    ctx.beginPath();
    const by = eyeY - S * 0.102 - E.brow * S * 0.024;
    ctx.moveTo(ex - S * 0.065, by + S * 0.014);
    ctx.quadraticCurveTo(ex, by - S * 0.024 - E.brow * S * 0.018, ex + S * 0.065, by);
    ctx.stroke();
  }

  // Capillary vasocongestion flush across nose bridge & cheeks
  const rgb = hexToRgb(blushHex);
  ctx.save();
  ctx.globalAlpha = 0.22 + E.blush * 0.52;
  for (const s of [-1, 1]) {
    const bg = ctx.createRadialGradient(S * 0.5 + s * S * 0.21, S * 0.575, 4, S * 0.5 + s * S * 0.21, S * 0.575, S * 0.115);
    bg.addColorStop(0, `rgba(${rgb[0]},${rgb[1]},${rgb[2]},0.92)`);
    bg.addColorStop(1, `rgba(${rgb[0]},${rgb[1]},${rgb[2]},0)`);
    ctx.fillStyle = bg;
    ctx.beginPath();
    ctx.ellipse(S * 0.5 + s * S * 0.21, S * 0.575, S * 0.115, S * 0.056, 0, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.restore();

  // Delicate upturned nose hint (geometry now carries the bridge — paint stays sheer)
  ctx.strokeStyle = 'rgba(155,85,75,0.30)';
  ctx.lineWidth = S * 0.009;
  ctx.lineCap = 'round';
  ctx.beginPath();
  ctx.moveTo(S * 0.5, S * 0.542);
  ctx.quadraticCurveTo(S * 0.484, S * 0.578, S * 0.5, S * 0.584);
  ctx.stroke();

  // Parted luscious lips with wet mucosa, teeth & tongue
  const mo = clamp(E.mouth, 0, 1);
  const my = S * 0.668;

  if (mo > 0.06) {
    const mh = S * 0.022 + mo * S * 0.062;
    const mw = S * 0.054 + mo * S * 0.024;

    // Oral cavity
    ctx.fillStyle = '#4c101a';
    ctx.beginPath(); ctx.ellipse(S * 0.5, my, mw, mh, 0, 0, Math.PI * 2); ctx.fill();

    // Upper pearlescent teeth
    if (mo > 0.18) {
      ctx.fillStyle = 'rgba(255,248,245,0.96)';
      ctx.beginPath();
      ctx.ellipse(S * 0.5, my - mh * 0.44, mw * 0.72, mh * 0.26, 0, 0, Math.PI * 2);
      ctx.fill();
    }

    // Wet tongue
    if (mo > 0.22) {
      ctx.fillStyle = 'rgba(216,92,112,0.96)';
      ctx.beginPath();
      ctx.ellipse(S * 0.5, my + mh * 0.44, mw * 0.68, mh * 0.46, 0, 0, Math.PI * 2);
      ctx.fill();
    }

    // Vermilion borders
    ctx.strokeStyle = lipHex;
    ctx.lineWidth = S * 0.013;
    ctx.beginPath();
    ctx.moveTo(S * 0.5 - mw, my);
    ctx.quadraticCurveTo(S * 0.5, my + mh * 0.58, S * 0.5 + mw, my);
    ctx.stroke();

    // Lip gloss shine
    ctx.fillStyle = 'rgba(255,255,255,0.55)';
    ctx.beginPath();
    ctx.ellipse(S * 0.5, my + mh * 0.65, mw * 0.42, mh * 0.22, 0, 0, Math.PI * 2);
    ctx.fill();
  } else {
    // Soft closed pout
    ctx.strokeStyle = lipHex;
    ctx.lineWidth = S * 0.014;
    ctx.beginPath();
    ctx.moveTo(S * 0.5 - S * 0.042, my);
    ctx.quadraticCurveTo(S * 0.5, my + S * 0.016, S * 0.5 + S * 0.042, my);
    ctx.stroke();
  }

  // Ecstasy tear drops at outer eye corners
  if (E.blush > 0.68) {
    ctx.fillStyle = 'rgba(235,248,255,0.76)';
    for (const s of [-1, 1]) {
      ctx.beginPath();
      ctx.ellipse(S * 0.5 + s * S * 0.292, eyeY + S * 0.034, S * 0.014, S * 0.022, 0, 0, Math.PI * 2);
      ctx.fill();
    }
  }
}

/* ============================================================
   HAIR ASSEMBLE
   ============================================================ */
function buildHair3(headR, hairMat, style) {
  const g = new THREE.Group();

  // Fitted scalp cap
  const cap = new THREE.Mesh(
    new THREE.SphereGeometry(headR * 1.10, 24, 18, 0, Math.PI * 2, 0, Math.PI * 0.64),
    hairMat
  );
  cap.position.y = headR * 0.10;
  cap.castShadow = true;
  g.add(cap);
  addSheen3(cap, '#ffd0c0', 3.0, 0.14);

  // Full back volume
  const back = new THREE.Mesh(
    new THREE.SphereGeometry(headR * 1.08, 20, 16, 0, Math.PI * 2, Math.PI * 0.28, Math.PI * 0.72),
    hairMat
  );
  back.position.set(0, -headR * 0.12, -headR * 0.22);
  back.scale.set(1.0, 1.28, 0.88);
  back.castShadow = true;
  g.add(back);

  // Soft sweeping bangs
  const bang = new THREE.Mesh(
    new THREE.SphereGeometry(headR * 1.14, 22, 16, Math.PI * 0.70, Math.PI * 1.60, Math.PI * 0.06, Math.PI * 0.42),
    hairMat
  );
  bang.position.set(0, headR * 0.06, headR * 0.02);
  bang.castShadow = true;
  g.add(bang);

  // Temple wisps & forehead baby-hairs breaking the wig hairline
  const wisp = (x0, z0, dx, len) => {
    const wsp = taperTube3(
      [[x0, headR * 0.42, z0],
       [x0 + dx, headR * 0.10, z0 + headR * 0.06],
       [x0 + dx * 1.4, -headR * 0.18, z0 + headR * 0.10],
       [x0 + dx * 1.2, -headR * 0.18 - len, z0 + headR * 0.08]],
      headR * 0.020, headR * 0.006, hairMat, 10, 6);
    wsp.userData.noInk = true;
    g.add(wsp);
  };
  [-1, 1].forEach(s => {
    wisp(s * headR * 0.72, headR * 0.62, s * headR * 0.10, headR * 0.22);
    wisp(s * headR * 0.45, headR * 0.78, s * headR * 0.05, headR * 0.16);
    wisp(s * headR * 0.90, headR * 0.30, s * headR * 0.14, headR * 0.30);
  });
  // Crown flyaways catching the key light
  for (let i = 0; i < 5; i++) {
    const a = (i / 4 - 0.5) * 1.6;
    const fly = taperTube3(
      [[Math.sin(a) * headR * 0.5, headR * 0.95, -Math.cos(a) * headR * 0.5],
       [Math.sin(a) * headR * 0.8, headR * 1.15, -Math.cos(a) * headR * 0.8],
       [Math.sin(a) * headR * 1.0, headR * 1.18, -Math.cos(a) * headR * 1.0]],
      headR * 0.012, headR * 0.004, hairMat, 8, 6);
    fly.userData.noInk = true;
    g.add(fly);
  }

  // Layered flowing tresses
  if (style !== 'short') {
    const long = style === 'long';
    const len = long ? 1.0 : 0.52;
    const rRoot = headR * (long ? 0.088 : 0.076), rTip = headR * 0.014;

    const fan = (count, pool) => {
      for (let i = 0; i < count; i++) {
        const a = (count === 1 ? 0.5 : i / (count - 1) - 0.5) * Math.PI * 1.34;
        const r0 = headR * 0.96;
        const x0 = Math.sin(a) * r0, z0 = -Math.cos(a) * r0 * 0.88;
        let pts;
        if (!pool) {
          pts = [
            [x0, headR * 0.25, z0],
            [x0 * 1.32, -headR * 0.70, z0 * 1.16],
            [x0 * 1.42, -headR * 1.32 - len * 0.12, z0 * 1.22],
            [x0 * 1.28, -headR * 1.74 - len * 0.22, z0 * 1.16]
          ];
        } else {
          pts = [
            [x0, headR * 0.12, -headR * 0.65],
            [x0 * 1.62, -headR * 0.02, -headR * 0.86 - len * headR * 0.03],
            [x0 * 1.84, -headR * 0.06, -headR * 1.02 - len * headR * 0.05],
            [x0 * 1.62, -headR * 0.02, -headR * 1.08 - len * headR * 0.08]
          ];
        }
        const tr = taperTube3(pts, rRoot * (1 - (i % 3) * 0.12), rTip, hairMat, 16, 8);
        tr.userData.noInk = true; // thin strands must never get fat outline shells
        g.add(tr);
      }
    };
    if (long) { fan(10, false); fan(8, true); }
    else { fan(6, false); fan(5, true); }
  }

  return g;
}

/* ============================================================
   HER — Sculpted Hentai Masterpiece Rig
   ============================================================ */
function buildHer3() {
  const S = herSkin3();
  const ch = G.char || {};
  const skinMat = skinMat3(S.base);
  const skinShade = toonMat(S.shade);
  const breastScale = 0.64 + (ch.breastSize == null ? 0.45 : ch.breastSize) * 0.88;

  const root = new THREE.Group();
  // ---- SINGLE BODY PROFILE: every measurement derives from one canon ----
  // 7.25-head figure (H=1.70). Shoulders narrower than hips; hand ≈ 0.75 HU,
  // foot ≈ 0.96 HU. Change values here and the whole figure follows.
  const BODY = {
    H: 1.70, heads: 7.25,
    headR: 0.115,            // skull sphere radius (head height ≈ 2.04 × headR)
    neckTopR: 0.042, neckBaseR: 0.052, neckLen: 0.082,
    shoulderX: 0.148,        // glenohumeral joint |x|
    chestW: 0.150, waistW: 0.118, hipW: 0.158,
    thighLen: 0.430, shinLen: 0.410,
    thighTopR: 0.084, kneeR: 0.052, ankleR: 0.030,
    upperArmLen: 0.250, foreArmLen: 0.232,
    armTopR: 0.047, elbowR: 0.039, wristR: 0.027,
    deltR: 0.050,
    palmR: 0.046, fingerLen: 0.040,
    footFrontZ: 0.052, toeZ: 0.138
  };
  const R = {
    headR: BODY.headR,
    neck: 0.072,
    chestR: BODY.chestW,
    waistR: BODY.waistW,
    hipR: BODY.hipW,
    upperArm: BODY.upperArmLen, foreArm: BODY.foreArmLen,
    thigh: BODY.thighLen, shin: BODY.shinLen
  };

  // ---- HIPS & PELVIS ----
  const hips = new THREE.Group();
  root.add(hips);

  const pelvis = ballMesh(R.hipR, skinMat, 0.84);
  pelvis.scale.set(1.14, 0.84, 0.94);
  hips.add(pelvis);

  // Soft rounded mons pubis mound
  const mons = ballMesh(0.088, skinMat, 0.64);
  mons.position.set(0, -0.046, 0.118);
  hips.add(mons);

  // Voluptuous anatomical buttocks with distinct intergluteal cleft
  [-1, 1].forEach(s => {
    const cheekGeo = makeGluteCheekGeo(0.114, s);
    const chx = new THREE.Mesh(cheekGeo, skinMat);
    chx.position.set(s * 0.106, -0.052, -0.096);
    hips.add(chx);

    // Warm airbrushed peach-red flush on gluteal fullness
    blush3(chx, (x, y, z) => {
      const w = clamp(0.38 - y * 3.4, 0, 1) * 0.72 + clamp((Math.abs(x) - 0.03) * 5.2, 0, 1) * 0.28;
      return [1, 1 - 0.15 * w, 1 - 0.24 * w];
    });
    addGloss3(chx, s * 0.052, 0.048, -0.052, 0.090, 0.44);
  });

  // Intergluteal cleft (crack shadow)
  const cleft = new THREE.Mesh(
    new THREE.BoxGeometry(0.016, 0.19, 0.026),
    toonMat('#561e1b', { soft: false })
  );
  cleft.position.set(0, -0.018, -0.156);
  cleft.userData.noInk = true;
  hips.add(cleft);

  // Infragluteal crease lines (banana roll folds)
  [-1, 1].forEach(s => {
    const f = new THREE.Mesh(
      new THREE.CylinderGeometry(0.007, 0.007, 0.135, 10),
      toonMat('#863e32', { soft: false })
    );
    f.rotation.z = Math.PI / 2;
    f.position.set(s * 0.090, -0.102, -0.058);
    f.userData.noInk = true;
    hips.add(f);
  });

  // Pubic hair options: 'bare', 'trim', 'full'
  const ph = ch.pubicHair || 'trim';
  if (ph !== 'bare') {
    const full = ph === 'full';
    const pb = ballMesh(full ? 0.058 : 0.044, toonMat(ch.hairColor || '#231318'), 0.58);
    pb.position.set(0, -0.006, 0.150);
    pb.scale.set(1.18, 0.46, 0.72);
    pb.userData.noInk = true;
    hips.add(pb);
  }

  // ---- VULVA (Articulated Hentai Genital Assembly) ----
  const vulva = new THREE.Group();
  vulva.position.set(0, -0.045, 0.118);
  hips.add(vulva);

  // Labia Majora (twin soft outer lips)
  const majoraMat = toonMat(S.shade);
  [-1, 1].forEach(s => {
    const mj = ballMesh(0.054, majoraMat, 0.92);
    mj.scale.set(0.56, 1.18, 0.72);
    mj.position.set(s * 0.033, -0.032, 0.014);
    vulva.add(mj);
    blush3(mj, (x, y, z) => {
      const w = clamp(0.55 - s * x * 10, 0, 1);
      return [1, 1 - 0.13 * w, 1 - 0.20 * w];
    });
  });

  // Labia Minora (delicate inner rose petals framing vestibule)
  const minoraMat = toonMat('#db7680');
  [-1, 1].forEach(s => {
    const mn = ballMesh(0.032, minoraMat, 0.92);
    mn.scale.set(0.48, 1.28, 0.62);
    mn.position.set(s * 0.015, -0.044, 0.028);
    mn.userData.noInk = true;
    vulva.add(mn);
  });

  // Pudendal cleft interior shadow
  const vcl = new THREE.Mesh(
    new THREE.BoxGeometry(0.011, 0.090, 0.022),
    toonMat('#52181a', { soft: false })
  );
  vcl.position.set(0, -0.042, 0.022);
  vcl.userData.noInk = true;
  vulva.add(vcl);

  // Clitoral hood & sensitive swollen glans
  const hood = ballMesh(0.015, skinMat, 0.88);
  hood.position.set(0, 0.009, 0.030);
  hood.userData.noInk = true;
  vulva.add(hood);

  const clitGlans = ballMesh(0.0075, toonMat('#e85a70'), 0.9);
  clitGlans.position.set(0, 0.006, 0.038);
  clitGlans.userData.noInk = true;
  vulva.add(clitGlans);

  // Vaginal introitus (the penetration target ring)
  const intro = new THREE.Mesh(
    new THREE.CircleGeometry(0.020, 22),
    toonMat('#441014', { soft: false })
  );
  intro.scale.set(0.82, 1.18, 1);
  intro.position.set(0, -0.058, 0.030);
  intro.userData.noInk = true;
  vulva.add(intro);

  // Vermilion mucosal ring
  const verm = new THREE.Mesh(
    new THREE.TorusGeometry(0.019, 0.0065, 8, 22),
    toonMat('#cc646e')
  );
  verm.scale.set(0.82, 1.18, 1);
  verm.position.set(0, -0.058, 0.030);
  verm.userData.noInk = true;
  vulva.add(verm);

  // Perineum & delicate anal rosebud sphincter
  const anus = new THREE.Mesh(
    new THREE.CircleGeometry(0.008, 14),
    toonMat('#662826', { soft: false })
  );
  anus.position.set(0, -0.100, 0.004);
  anus.rotation.x = 0.52;
  anus.userData.noInk = true;
  vulva.add(anus);

  addGloss3(vulva, 0.022, -0.022, 0.036, 0.048, 0.44);

  // ---- SCULPTED HOURGLASS TORSO & ABDOMEN ----
  const torso = new THREE.Group();
  torso.position.y = 0.02; // sunk into the pelvis so the waist grows out of the hips
  hips.add(torso);

  const torsoGeo = makeSculptedTorsoGeo(R.chestR, R.waistR, R.hipR * 0.96, 0.48);
  const belly = new THREE.Mesh(torsoGeo, skinMat);
  belly.castShadow = true; belly.receiveShadow = true;
  addSheen3(belly, '#ffd2c6', 3.0, 0.28);
  torso.add(belly);

  // Painted AO & vascular warmth: under-bust fold, navel, groin, lumbar
  blush3(belly, (x, y, z) => {
    let r = 1, g = 1, b = 1;
    const t = clamp(y / 0.48, 0, 1);
    if (z > 0.04 && t > 0.78 && t < 0.92) {           // under-bust crease shadow
      const w = Math.sin((t - 0.78) / 0.14 * Math.PI) * clamp((z - 0.04) * 8, 0, 1);
      const k = 0.16 * w;
      r -= k * 0.6; g -= k; b -= k * 0.9;
    }
    if (z > 0.05 && Math.abs(t - 0.34) < 0.07 && Math.abs(x) < 0.05) { // navel warmth
      const w = (1 - Math.abs(t - 0.34) / 0.07) * (1 - Math.abs(x) / 0.05);
      g -= 0.06 * w; b -= 0.10 * w;
    }
    if (z > 0 && t < 0.16) {                          // groin shadow
      const w = (1 - t / 0.16) * clamp(z * 6, 0, 1);
      const k = 0.14 * w;
      r -= k * 0.5; g -= k; b -= k * 0.9;
    }
    if (z < -0.03 && t > 0.08 && t < 0.34) {          // lumbar warmth
      const w = Math.sin((t - 0.08) / 0.26 * Math.PI);
      g -= 0.05 * w; b -= 0.09 * w;
    }
    return [r, g, b];
  });

  const chest = new THREE.Group();
  chest.position.y = 0.42;
  torso.add(chest);

  // Ribcage mass: fills the hollow upper chest so clavicles, breasts and
  // neck all socket into one continuous torso (no floating parts)
  const ribcage = ballMesh(0.146, skinMat, 0.78);
  ribcage.position.set(0, 0.055, 0.008);
  ribcage.scale.set(1.0, 0.78, 0.82);
  ribcage.receiveShadow = true;
  chest.add(ribcage);

  // ---- VOLUMETRIC TEARDROP BREASTS ----
  const bMat = toonMat(S.base);
  const nipMat = toonMat(ch.nippleColor || '#c25f63');
  const areolaMat = toonMat('#c98676');

  const breastL = new THREE.Group(), breastR = new THREE.Group();
  breastL.position.set(-0.088, 0.048, 0.088);
  breastR.position.set(0.088, 0.048, 0.088);

  [breastL, breastR].forEach((bg, idx) => {
    const s = idx === 0 ? -1 : 1;
    const bGeo = makeTeardropBreastGeo(0.084, breastScale);
    const b = new THREE.Mesh(bGeo, bMat);
    b.rotation.z = s * 0.08; // subtle natural outward flare
    b.position.y = -0.028;
    bg.add(b);

    blush3(b, (x, y, z) => {
      const w = clamp(0.32 - y * 4.8, 0, 1);
      return [1, 1 - 0.14 * w, 1 - 0.22 * w];
    });
    addGloss3(bg, s * 0.015, 0.032, 0.048, 0.070, 0.40);

    // Areola with soft feathering
    const arR = 0.028 * breastScale;
    const arGeo = new THREE.CircleGeometry(arR, 22);
    const arM = areolaMat.clone();
    arM.vertexColors = true;
    {
      const skRGB = hexToRgb(S.base), an = [201, 134, 118];
      const p = arGeo.getAttribute('position'), n2 = p.count;
      const col = new Float32Array(n2 * 3);
      for (let vi = 0; vi < n2; vi++) {
        const rl = Math.hypot(p.getX(vi), p.getY(vi)) / arR;
        let e = clamp((rl - 0.52) / 0.48, 0, 1);
        e = e * e * (3 - 2 * e);
        col[vi * 3] = (an[0] + (skRGB[0] - an[0]) * e) / 255;
        col[vi * 3 + 1] = (an[1] + (skRGB[1] - an[1]) * e) / 255;
        col[vi * 3 + 2] = (an[2] + (skRGB[2] - an[2]) * e) / 255;
      }
      arGeo.setAttribute('color', new THREE.BufferAttribute(col, 3));
    }

    const ar = new THREE.Mesh(arGeo, arM);
    ar.position.set(s * 0.012, -0.050, 0.076 * breastScale);
    ar.rotation.x = -0.32;
    ar.rotation.y = s * 0.14;
    ar.userData.noInk = true;
    bg.add(ar);

    // Protruding erect nipple with tip catchlight
    const np = ballMesh(0.0118, nipMat, 0.72);
    np.position.set(s * 0.012, -0.050, 0.082 * breastScale);
    np.userData.noInk = true;
    bg.add(np);

    chest.add(bg);
  });

  // Inframammary crease lines hugging lower breast curve
  [-1, 1].forEach(s => {
    const u = new THREE.Mesh(
      new THREE.CylinderGeometry(0.0055, 0.0055, 0.086, 8),
      toonMat('#944a3a', { soft: false })
    );
    u.rotation.z = Math.PI / 2;
    u.position.set(s * 0.088, -0.074, 0.058);
    u.userData.noInk = true;
    chest.add(u);
  });

  // Clavicle bone ridges (slim, skin-toned so they sit IN the chest, not on it)
  [-1, 1].forEach(s => {
    const clav = new THREE.Mesh(
      new THREE.CylinderGeometry(0.0042, 0.0032, 0.11, 8),
      skinShade
    );
    clav.rotation.z = s * 1.42;
    clav.position.set(s * 0.068, 0.148, 0.048);
    clav.userData.noInk = true;
    chest.add(clav);
  });

  // ---- NECK & HEAD ----
  const neck = new THREE.Group();
  neck.position.y = 0.178;
  chest.add(neck);

  const neckM = segMesh(0.042, 0.052, 0.082, skinMat);
  neckM.rotation.x = Math.PI;
  neck.add(neckM);

  // Trapezius slopes blending neck into shoulders (kills the pipe-on-ball joint)
  [-1, 1].forEach(s => {
    const trap = ballMesh(0.052, skinMat, 0.62);
    trap.position.set(s * 0.112, -0.048, -0.005);
    trap.scale.set(1.8, 0.5, 1.0);
    trap.rotation.z = s * -0.35;
    trap.userData.noInk = true;
    neck.add(trap);
  });

  // Suprasternal notch dip between the collarbones
  const notch = ballMesh(0.014, skinShade, 0.6);
  notch.position.set(0, -0.012, 0.050);
  notch.scale.set(1.2, 0.6, 0.5);
  notch.userData.noInk = true;
  neck.add(notch);

  const head = new THREE.Group();
  head.position.y = 0.090;
  neck.add(head);

  const skull = ballMesh(R.headR, skinMat, 1.02);
  skull.scale.set(0.92, 1.02, 0.96);
  addSheen3(skull, '#ffd8cc', 3.2, 0.25);
  head.add(skull);

  // Narrower realistic jaw tapering to a soft chin
  const chin = ballMesh(R.headR * 0.52, skinMat, 0.84);
  chin.position.set(0, -R.headR * 0.68, R.headR * 0.10);
  chin.scale.set(0.78, 0.74, 0.88);
  head.add(chin);

  // Nose bridge with a defined tip (reads in profile & 3/4 views)
  const nose = ballMesh(R.headR * 0.13, skinMat, 1.5);
  nose.position.set(0, -R.headR * 0.06, R.headR * 0.94);
  nose.scale.set(0.55, 1.35, 0.80);
  nose.userData.noInk = true;
  head.add(nose);
  const noseTip = ballMesh(R.headR * 0.075, skinMat, 0.9);
  noseTip.position.set(0, -R.headR * 0.185, R.headR * 1.0);
  noseTip.scale.set(0.85, 0.70, 0.85);
  noseTip.userData.noInk = true;
  head.add(noseTip);

  // Lower-lip volume under the painted mouth (catches light in close-ups)
  const lowLip = ballMesh(R.headR * 0.16, skinShade, 0.55);
  lowLip.position.set(0, -R.headR * 0.42, R.headR * 0.86);
  lowLip.scale.set(1.15, 0.42, 0.55);
  lowLip.userData.noInk = true;
  head.add(lowLip);

  // High-res facial texture decal
  const face = makeFaceTexture3();
  const faceGeo = new THREE.SphereGeometry(
    R.headR * 1.014, 32, 26,
    Math.PI * 0.72, Math.PI * 1.56,
    Math.PI * 0.18, Math.PI * 0.52
  );
  const faceMesh = new THREE.Mesh(faceGeo, new THREE.MeshBasicMaterial({
    map: face.tex, transparent: true, depthWrite: false
  }));
  faceMesh.rotation.y = Math.PI;
  faceMesh.userData.noInk = true;
  head.add(faceMesh);

  // Ears
  [-1, 1].forEach(s => {
    const ear = ballMesh(R.headR * 0.20, skinShade, 1.22);
    ear.position.set(s * R.headR * 0.92, 0, 0);
    ear.scale.set(0.42, 1.15, 0.72);
    head.add(ear);
  });

  // Hair
  const hairMat = toonMat(ch.hairColor || '#231318');
  const hair = buildHair3(R.headR, hairMat, ch.hairStyle || 'long');
  head.add(hair);

  // ---- ARMS & DELICATE HANDS ----
  function makeArm(side) {
    const sh = new THREE.Group();
    sh.position.set(side * BODY.shoulderX, 0.138, 0);
    chest.add(sh);

    // Deltoid cap matched to the upper-arm radius (no ball-on-stick crease)
    sh.add(ballMesh(BODY.deltR, skinMat));
    const up = segMesh(BODY.armTopR, 0.038, R.upperArm, skinMat, 0.049, 0.40);
    sh.add(up);

    const el = new THREE.Group();
    el.position.y = -R.upperArm;
    sh.add(el);
    // Elbow condyle: squashed along the arm so it reads as a joint, not a bead
    const elB = ballMesh(BODY.elbowR, skinMat);
    elB.scale.set(0.95, 0.82, 0.95);
    el.add(elB);

    // Forearm with brachioradialis fullness
    const fo = sculptedSegMesh(0.039, BODY.wristR, R.foreArm, skinMat, 0.042, 0.32, side * 0.006, 0.005);
    el.add(fo);

    const hand = new THREE.Group();
    hand.position.y = -R.foreArm;
    el.add(hand);

    // Palm (lifelike volume — fingers anchor along its base edge)
    const hm = ballMesh(BODY.palmR, skinMat, 1.28);
    hm.scale.set(0.62, 1.30, 0.52);
    hand.add(hm);

    // Four articulated fingers with knuckle joints and a natural relaxed curl
    for (let f = 0; f < 4; f++) {
      const fx = (f - 1.5) * 0.016;
      const fl = BODY.fingerLen - Math.abs(f - 1.5) * 0.004;
      const fg = new THREE.Group();
      fg.position.set(fx, -0.052, 0.004);
      fg.rotation.x = 0.18 + f * 0.02;
      hand.add(fg);
      const seg1 = new THREE.Mesh(new THREE.CylinderGeometry(0.0062, 0.0054, fl, 8), skinMat);
      seg1.geometry.translate(0, -fl / 2, 0);
      seg1.castShadow = true;
      seg1.userData.noInk = true;
      fg.add(seg1);
      const tip = new THREE.Group();
      tip.position.y = -fl;
      tip.rotation.x = 0.38;
      fg.add(tip);
      const seg2 = new THREE.Mesh(new THREE.CylinderGeometry(0.0052, 0.0042, fl * 0.8, 8), skinMat);
      seg2.geometry.translate(0, -fl * 0.4, 0);
      seg2.castShadow = true;
      seg2.userData.noInk = true;
      tip.add(seg2);
    }

    // Opposable thumb with two segments
    const thumbG = new THREE.Group();
    thumbG.position.set(-side * 0.024, -0.008, 0.008);
    thumbG.rotation.set(0.5, 0, side * 0.7);
    hand.add(thumbG);
    const th1 = new THREE.Mesh(new THREE.CylinderGeometry(0.007, 0.006, 0.030, 8), skinMat);
    th1.geometry.translate(0, -0.015, 0);
    th1.castShadow = true;
    th1.userData.noInk = true;
    thumbG.add(th1);

    return { shoulder: sh, elbow: el, hand: hand };
  }
  const armL = makeArm(-1), armR = makeArm(1);

  // ---- LEGS: SCULPTED THIGHS, KNEES & ARTICULATED TOES ----
  function makeLeg(side) {
    const hp = new THREE.Group();
    hp.position.set(side * R.hipR * 0.58, -0.044, 0);
    hips.add(hp);
    hp.add(ballMesh(0.084, skinMat)); // buried in the thigh head — no waist gap

    // Thigh with vastus medialis & lateralis contours
    const th = sculptedSegMesh(
      BODY.thighTopR, 0.057, R.thigh, skinMat,
      0.086, 0.38,
      side * 0.014, // lateral sweep
      0.010         // hamstring curve
    );
    hp.add(th);
    hp.userData.thighM = th;

    const kn = new THREE.Group();
    kn.position.y = -R.thigh;
    hp.add(kn);

    // Sculpted knee joint with patella prominence
    kn.add(ballMesh(0.054, skinMat));
    const patella = ballMesh(0.024, skinShade, 1.2);
    patella.position.set(0, 0.004, 0.048);
    patella.userData.noInk = true;
    kn.add(patella);

    // Calf with gastrocnemius & slender Achilles tendon
    const shn = sculptedSegMesh(
      0.053, 0.034, R.shin, skinMat,
      0.057, 0.30,
      side * 0.006, // outer calf head
      -0.016        // posterior calf belly bulge
    );
    kn.add(shn);

    // Medial & lateral malleoli (ankle bones grounding the shin into the foot)
    [-1, 1].forEach(s => {
      const mal = ballMesh(0.013, skinShade, 1.0);
      mal.position.set(s * 0.030, -R.shin + 0.045, 0.004);
      mal.userData.noInk = true;
      kn.add(mal);
    });

    const ft = new THREE.Group();
    ft.position.y = -R.shin;
    kn.add(ft);

    // Foot with heel, raised arch and tapering forefoot
    const heel = ballMesh(0.034, skinMat, 0.85);
    heel.position.set(0, -0.012, -0.028);
    heel.scale.set(0.72, 0.80, 0.85);
    heel.userData.noInk = true;
    ft.add(heel);
    const fm = ballMesh(0.041, skinMat, 0.68);
    fm.scale.set(0.58, 0.52, 2.35);
    fm.position.set(0, -0.004, BODY.footFrontZ);
    ft.add(fm);

    // Five articulated toes with natural fan and glossy nail polish
    for (let t = -2; t <= 2; t++) {
      const toeR = 0.0135 - Math.abs(t) * 0.0016;
      const toe = ballMesh(toeR, skinMat, 0.82);
      const tz = BODY.toeZ - Math.abs(t) * 0.007;
      toe.position.set(t * 0.0165 + (side > 0 ? 0.004 : -0.004), -0.012, tz);
      toe.userData.noInk = true;
      ft.add(toe);

      // Glossy toenail
      const nail = new THREE.Mesh(
        new THREE.PlaneGeometry(0.0055, 0.0055),
        toonMat('#f6d2cc')
      );
      nail.position.set(toe.position.x, toe.position.y + 0.008, toe.position.z + 0.004);
      nail.rotation.x = -Math.PI * 0.35;
      nail.userData.noInk = true;
      ft.add(nail);
    }

    return { hip: hp, knee: kn, foot: ft };
  }

  const legL = makeLeg(-1), legR = makeLeg(1);

  // Subsurface thigh blush & lateral sheen
  [legL, legR].forEach(lg => {
    const th = lg.hip.userData.thighM;
    blush3(th, (x, y, z) => {
      const w = clamp(-y * 2.4, 0, 1) * 0.58 + 0.16;
      return [1, 1 - 0.08 * w, 1 - 0.12 * w];
    });
    addGloss3(lg.hip, 0, -0.20, 0.065, 0.11, 0.35);
  });

  inkRig3(root, 0.0064);

  return {
    root, hips, torso, chest, neck, head, skull,
    face, faceMesh, hair, hairMat,
    breastL, breastR, armL, armR, legL, legR,
    mons, vulva, mat: skinMat, R
  };
}

/* ============================================================
   HIM — Muscular Masculine Rig
   ============================================================ */
function buildHim3() {
  const S = HIS_SKIN3;
  const skinMat = skinMat3(S.base);
  const skinShade = toonMat(S.shade);

  const root = new THREE.Group();
  const R = {
    headR: 0.134, chestR: 0.188, waistR: 0.142, hipR: 0.138,
    upperArm: 0.272, foreArm: 0.252, thigh: 0.418, shin: 0.402
  };

  const hips = new THREE.Group();
  root.add(hips);
  const pelvis = ballMesh(R.hipR, skinMat, 0.78);
  pelvis.scale.set(1.08, 0.78, 0.92);
  hips.add(pelvis);

  [-1, 1].forEach(s => {
    const chx = ballMesh(0.090, skinMat, 0.95);
    chx.position.set(s * 0.088, -0.050, -0.086);
    chx.scale.set(0.92, 1.0, 0.86);
    hips.add(chx);
    addGloss3(chx, s * 0.046, 0.036, -0.046, 0.072, 0.32);
  });

  const cleftH = new THREE.Mesh(
    new THREE.BoxGeometry(0.014, 0.15, 0.025),
    toonMat('#521f1c', { soft: false })
  );
  cleftH.position.set(0, -0.015, -0.134);
  cleftH.userData.noInk = true;
  hips.add(cleftH);

  const torso = new THREE.Group();
  torso.position.y = 0.10;
  hips.add(torso);

  const belly = new THREE.Mesh(
    new THREE.CylinderGeometry(R.chestR * 0.94, R.waistR * 1.10, 0.28, 18, 1),
    skinMat
  );
  belly.position.y = 0.12;
  belly.castShadow = true;
  torso.add(belly);

  const fpvHide = [belly];
  const chest = new THREE.Group();
  chest.position.y = 0.30;
  torso.add(chest);

  const rib = new THREE.Mesh(
    new THREE.CylinderGeometry(R.chestR * 1.10, R.chestR * 0.98, 0.22, 18, 1),
    skinMat
  );
  rib.position.y = 0.07;
  rib.scale.z = 0.78;
  rib.castShadow = true;
  chest.add(rib);
  fpvHide.push(rib);

  [-1, 1].forEach(s => {
    const pec = ballMesh(0.064, skinMat, 0.56);
    pec.position.set(s * 0.074, 0.032, 0.120);
    pec.scale.set(1.0, 0.62, 0.74);
    chest.add(pec);
    fpvHide.push(pec);
  });

  const neck = new THREE.Group();
  neck.position.y = 0.172;
  chest.add(neck);
  const nm = segMesh(0.058, 0.064, 0.072, skinMat);
  nm.rotation.x = Math.PI;
  neck.add(nm);
  fpvHide.push(nm);

  const head = new THREE.Group();
  head.position.y = 0.084;
  neck.add(head);
  fpvHide.push(head);

  const skull = ballMesh(R.headR, skinMat, 1.04);
  skull.scale.set(0.92, 1.04, 1.0);
  head.add(skull);

  const jaw = ballMesh(R.headR * 0.72, skinMat, 0.82);
  jaw.position.set(0, -R.headR * 0.58, R.headR * 0.06);
  jaw.scale.set(0.94, 0.78, 0.96);
  head.add(jaw);

  [-1, 1].forEach(s => {
    const ear = ballMesh(R.headR * 0.19, skinShade, 1.2);
    ear.position.set(s * R.headR * 0.90, 0, 0);
    ear.scale.set(0.40, 1.12, 0.70);
    head.add(ear);
  });

  const hairMat = toonMat('#1c100a');
  const cap = new THREE.Mesh(
    new THREE.SphereGeometry(R.headR * 1.09, 20, 14, 0, Math.PI * 2, 0, Math.PI * 0.58),
    hairMat
  );
  cap.position.y = R.headR * 0.10;
  cap.castShadow = true;
  head.add(cap);
  addSheen3(cap, '#ffcfc0', 3.0, 0.10);

  // Phallus with veins, coronal ridge & glans
  const shaftRoot = new THREE.Group();
  shaftRoot.position.set(0, -0.045, 0.118);
  hips.add(shaftRoot);

  const shaftMat = toonMat('#cf9a7c');
  const shaft = new THREE.Mesh(new THREE.CylinderGeometry(0.033, 0.038, 0.31, 16, 1), shaftMat);
  shaft.geometry.translate(0, -0.155, 0);
  shaft.castShadow = true;
  shaftRoot.add(shaft);

  blush3(shaft, (x, y, z) => {
    const w = 0.25 + 0.75 * clamp(-y * 3, 0, 1);
    return [1, 1 - 0.10 * w, 1 - 0.16 * w];
  });
  addGloss3(shaft, 0, -0.155, 0.046, 0.058, 0.38);

  [-1, 1].forEach(s => {
    const vn = new THREE.Mesh(
      new THREE.CylinderGeometry(0.005, 0.005, 0.25, 6),
      toonMat('#9a5f4e', { soft: false })
    );
    vn.position.set(s * 0.021, -0.155, 0.033);
    vn.userData.noInk = true;
    shaft.add(vn);
  });

  const ridge = new THREE.Mesh(new THREE.TorusGeometry(0.035, 0.010, 8, 22), toonMat('#b8695e'));
  ridge.rotation.x = Math.PI / 2;
  ridge.position.y = -0.270;
  ridge.userData.noInk = true;
  shaftRoot.add(ridge);

  const glans = ballMesh(0.046, toonMat('#c25e68'), 1.15);
  glans.scale.set(0.96, 1.15, 0.96);
  glans.position.y = -0.31;
  shaftRoot.add(glans);

  const slit = new THREE.Mesh(
    new THREE.BoxGeometry(0.008, 0.016, 0.008),
    toonMat('#571f1e', { soft: false })
  );
  slit.position.set(0, -0.358, 0.028);
  slit.userData.noInk = true;
  shaftRoot.add(slit);

  // Scrotum
  [-1, 1].forEach(s => {
    const sc = ballMesh(0.054, skinMat, 1.0);
    sc.scale.set(0.90, 1.16, 0.90);
    sc.position.set(s * 0.039, -0.108, 0.076);
    hips.add(sc);
  });

  const seam = new THREE.Mesh(
    new THREE.BoxGeometry(0.006, 0.072, 0.010),
    toonMat('#8a4a40', { soft: false })
  );
  seam.position.set(0, -0.122, 0.096);
  seam.userData.noInk = true;
  hips.add(seam);

  function makeArm(side) {
    const sh = new THREE.Group();
    sh.position.set(side * R.chestR * 1.05, 0.132, 0);
    chest.add(sh);
    sh.add(ballMesh(0.063, skinMat));
    sh.add(segMesh(0.059, 0.049, R.upperArm, skinMat));

    const el = new THREE.Group();
    el.position.y = -R.upperArm;
    sh.add(el);
    el.add(ballMesh(0.049, skinMat));
    el.add(segMesh(0.047, 0.035, R.foreArm, skinMat));

    const hand = new THREE.Group();
    hand.position.y = -R.foreArm;
    el.add(hand);

    const hm = ballMesh(0.047, skinMat, 1.3);
    hm.scale.set(0.60, 1.22, 0.50);
    hand.add(hm);

    return { shoulder: sh, elbow: el, hand: hand };
  }

  function makeLeg(side) {
    const hp = new THREE.Group();
    hp.position.set(side * R.hipR * 0.58, -0.042, 0);
    hips.add(hp);
    hp.add(ballMesh(0.084, skinMat));

    const thH = segMesh(0.088, 0.064, R.thigh, skinMat);
    hp.add(thH);
    hp.userData.thighM = thH;

    const kn = new THREE.Group();
    kn.position.y = -R.thigh;
    hp.add(kn);
    kn.add(ballMesh(0.061, skinMat));
    kn.add(segMesh(0.059, 0.039, R.shin, skinMat));

    const ft = new THREE.Group();
    ft.position.y = -R.shin;
    kn.add(ft);

    const fm = ballMesh(0.047, skinMat, 0.70);
    fm.scale.set(0.60, 0.60, 1.56);
    fm.position.z = 0.034;
    ft.add(fm);

    return { hip: hp, knee: kn, foot: ft };
  }

  const himLegL = makeLeg(-1), himLegR = makeLeg(1);
  [himLegL, himLegR].forEach(lg => {
    addGloss3(lg.hip, 0, -0.22, 0.07, 0.11, 0.32);
  });

  inkRig3(root, 0.0075);

  return {
    root, hips, torso, chest, neck, head, skull,
    shaftRoot, shaft, glans,
    armL: makeArm(-1), armR: makeArm(1),
    legL: himLegL, legR: himLegR,
    mat: skinMat, R,
    fpvHide
  };
}