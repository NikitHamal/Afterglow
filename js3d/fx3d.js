// Afterglow 3D — module: fx3d (particles, fluids, screen effects)
'use strict';

/* ============================================================
   PARTICLE SPRITES (procedural, no image assets)
   ============================================================ */
function makeSprite3(kind) {
  const S = 64;
  const cv = document.createElement('canvas');
  cv.width = cv.height = S;
  const c = cv.getContext('2d');

  if (kind === 'heart') {
    c.fillStyle = '#ff5f8a';
    c.beginPath();
    const s = S * 0.30, cx = S / 2, cy = S * 0.56;
    c.moveTo(cx, cy + s * 0.95);
    c.bezierCurveTo(cx - s * 1.35, cy + s * 0.12, cx - s * 0.85, cy - s * 0.95, cx, cy - s * 0.32);
    c.bezierCurveTo(cx + s * 0.85, cy - s * 0.95, cx + s * 1.35, cy + s * 0.12, cx, cy + s * 0.95);
    c.closePath(); c.fill();
  } else {
    // soft radial bead with a hot specular core
    const g = c.createRadialGradient(S / 2, S / 2, 0, S / 2, S / 2, S / 2);
    if (kind === 'wet') {
      g.addColorStop(0, 'rgba(255,255,255,1)');
      g.addColorStop(0.42, 'rgba(255,246,240,0.85)');
      g.addColorStop(1, 'rgba(255,240,235,0)');
    } else {
      g.addColorStop(0, 'rgba(255,255,255,0.95)');
      g.addColorStop(0.35, 'rgba(255,225,235,0.55)');
      g.addColorStop(1, 'rgba(255,200,215,0)');
    }
    c.fillStyle = g;
    c.beginPath(); c.arc(S / 2, S / 2, S / 2, 0, Math.PI * 2); c.fill();
  }
  const tex = new THREE.CanvasTexture(cv);
  tex.encoding = THREE.sRGBEncoding;
  return tex;
}

/* ============================================================
   PARTICLE POOL — per-particle alpha + size via a tiny shader
   ============================================================ */
function ParticlePool3(max, tex, baseSize, blending) {
  const geo = new THREE.BufferGeometry();
  const pos = new Float32Array(max * 3);
  const alpha = new Float32Array(max);
  const size = new Float32Array(max);
  for (let i = 0; i < max; i++) { pos[i * 3 + 1] = -999; alpha[i] = 0; size[i] = 1; }
  geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  geo.setAttribute('aAlpha', new THREE.BufferAttribute(alpha, 1));
  geo.setAttribute('aSize', new THREE.BufferAttribute(size, 1));

  const mat = new THREE.ShaderMaterial({
    uniforms: { uSize: { value: baseSize || 26 }, uTex: { value: tex } },
    vertexShader:
      'attribute float aAlpha; attribute float aSize; varying float vA; uniform float uSize;\n' +
      'void main(){ vA = aAlpha;' +
      ' vec4 mv = modelViewMatrix * vec4(position,1.0);' +
      ' gl_PointSize = uSize * aSize * (1.0 / max(0.001,-mv.z));' +
      ' gl_Position = projectionMatrix * mv; }',
    fragmentShader:
      'uniform sampler2D uTex; varying float vA;\n' +
      'void main(){ vec4 c = texture2D(uTex, gl_PointCoord);' +
      ' float a = c.a * vA; if(a < 0.01) discard;' +
      ' gl_FragColor = vec4(c.rgb, a); }',
    transparent: true,
    depthWrite: false,
    blending: blending || THREE.NormalBlending
  });

  const pts = new THREE.Points(geo, mat);
  pts.frustumCulled = false;

  const P = [];
  for (let i = 0; i < max; i++) P.push({ live: false, x: 0, y: 0, z: 0, vx: 0, vy: 0, vz: 0, life: 0, max: 1, sz: 1, g: 0, drag: 0 });

  return {
    points: pts,
    spawn(x, y, z, vx, vy, vz, life, sz, g, drag) {
      for (let i = 0; i < max; i++) {
        const p = P[i];
        if (p.live) continue;
        p.live = true;
        p.x = x; p.y = y; p.z = z;
        p.vx = vx; p.vy = vy; p.vz = vz;
        p.life = life; p.max = life; p.sz = sz || 1;
        p.g = g == null ? 0 : g; p.drag = drag == null ? 0.4 : drag;
        return p;
      }
      return null;
    },
    update(dt) {
      const pa = geo.attributes.position.array;
      const aa = geo.attributes.aAlpha.array;
      const sa = geo.attributes.aSize.array;
      for (let i = 0; i < max; i++) {
        const p = P[i];
        if (!p.live) { aa[i] = 0; continue; }
        p.life -= dt;
        if (p.life <= 0) { p.live = false; aa[i] = 0; continue; }
        p.vy -= p.g * dt;
        const d = Math.pow(1 - p.drag, dt * 60 / 60);
        p.vx *= d; p.vz *= d;
        p.x += p.vx * dt; p.y += p.vy * dt; p.z += p.vz * dt;
        pa[i * 3] = p.x; pa[i * 3 + 1] = p.y; pa[i * 3 + 2] = p.z;
        const u = p.life / p.max;
        aa[i] = Math.min(1, u * 2.2);
        sa[i] = p.sz * (0.7 + 0.3 * u);
      }
      geo.attributes.position.needsUpdate = true;
      geo.attributes.aAlpha.needsUpdate = true;
      geo.attributes.aSize.needsUpdate = true;
    },
    clear() { for (let i = 0; i < max; i++) { P[i].live = false; geo.attributes.aAlpha.array[i] = 0; } geo.attributes.aAlpha.needsUpdate = true; }
  };
}

/* ============================================================
   FX STATE
   ============================================================ */
let fxJets3 = null, fxDrips3 = null, fxSweat3 = null, fxHearts3 = null, fxGlisten3 = null;
let flashEl3 = null;
let _lastSpurts3 = 0, _sweatAccum3 = 0, _glisAccum3 = 0, _heartAccum3 = 0, _dripAccum3 = 0;

function initFX3() {
  const wet = makeSprite3('wet');
  const soft = makeSprite3('soft');
  const heart = makeSprite3('heart');

  fxJets3 = ParticlePool3(220, wet, 30);
  fxDrips3 = ParticlePool3(120, wet, 22);
  fxSweat3 = ParticlePool3(140, soft, 14);
  fxGlisten3 = ParticlePool3(120, wet, 16, THREE.AdditiveBlending);
  fxHearts3 = ParticlePool3(40, heart, 40);

  scene3d.add(fxJets3.points);
  scene3d.add(fxDrips3.points);
  scene3d.add(fxSweat3.points);
  scene3d.add(fxGlisten3.points);
  scene3d.add(fxHearts3.points);

  // full-screen climax bloom (cheap, no post-processing pass needed)
  flashEl3 = document.createElement('div');
  flashEl3.style.cssText =
    'position:absolute;inset:0;pointer-events:none;z-index:4;' +
    'background:#fff2ec;opacity:0;mix-blend-mode:screen;transition:opacity .12s linear';
  const st = document.getElementById('stage');
  if (st) st.appendChild(flashEl3);
}

/* ============================================================
   CLIMAX JETS — ballistic ropes from the shaft tip
   ============================================================ */
function spawnJets3(count, origin, dir) {
  if (!fxJets3) return;
  for (let i = 0; i < count; i++) {
    const spread = 0.42;
    const vx = dir.x * (2.4 + Math.random() * 1.5) + (Math.random() - 0.5) * spread;
    const vy = dir.y * (2.4 + Math.random() * 1.5) + Math.random() * 0.85 + 0.5;
    const vz = dir.z * (2.4 + Math.random() * 1.5) + (Math.random() - 0.5) * spread;
    fxJets3.spawn(
      origin.x + (Math.random() - 0.5) * 0.04,
      origin.y + (Math.random() - 0.5) * 0.04,
      origin.z + (Math.random() - 0.5) * 0.04,
      vx, vy, vz,
      0.85 + Math.random() * 0.5,
      0.7 + Math.random() * 0.6,
      9.0,        // gravity (m/s^2 in scene units — matches the 2D 950px/s^2 feel)
      0.22
    );
  }
}

/* ============================================================
   PER-FRAME UPDATE
   ============================================================ */
const _fxV = new THREE.Vector3();
const _fxD = new THREE.Vector3();

function updateFX3(dt) {
  if (!fxJets3) return;
  const t = G.t || 0;
  const ar = clamp((G.ar || 0) / 100, 0, 1);
  const p = clamp((G.pleasure || 0) / 100, 0, 1);

  /* ---- climax: spawn a burst whenever the spurt counter ticks ---- */
  if ((G.spurts || 0) !== _lastSpurts3) {
    _lastSpurts3 = G.spurts || 0;
    if (her3 && him3) {
      const org = shaftTipWorld3(him3);
      // fire along his shaft axis, outward
      _fxD.set(0, 0.35, 1).normalize();
      him3.shaftRoot.getWorldDirection(_fxV);
      spawnJets3(26, org, _fxD);
      flash3(0.42);
      CAM3.shake = Math.max(CAM3.shake, 0.05);
    }
  }
  if (G.state !== 'climax' && G.state !== 'finish') {
    if ((G.spurts || 0) === 0) _lastSpurts3 = 0;
  }

  /* ---- drips: slow beads sliding from her vulva ---- */
  if ((G.drips || []).length && her3) {
    const org = vulvaWorld3(her3);
    _dripAccum3 = (_dripAccum3 || 0) + dt;
    if (_dripAccum3 > 0.09) {
      _dripAccum3 = 0;
      fxDrips3.spawn(
        org.x + (Math.random() - 0.5) * 0.05, org.y, org.z + (Math.random() - 0.5) * 0.05,
        (Math.random() - 0.5) * 0.12, -0.15, (Math.random() - 0.5) * 0.12,
        1.6 + Math.random(), 0.75, 2.2, 0.05
      );
    }
  }

  /* ---- sweat: beads flying off skin when she works hard ---- */
  _sweatAccum3 += dt;
  const sweatRate = 0.55 - p * 0.42;
  if (_sweatAccum3 > sweatRate && p > 0.28 && her3) {
    _sweatAccum3 = 0;
    const src = breastWorld3(her3, Math.random() < 0.5 ? -1 : 1);
    fxSweat3.spawn(
      src.x + (Math.random() - 0.5) * 0.22, src.y + Math.random() * 0.3, src.z + (Math.random() - 0.5) * 0.2,
      (Math.random() - 0.5) * 0.7, Math.random() * 0.55 + 0.15, (Math.random() - 0.5) * 0.7,
      0.7 + Math.random() * 0.5, 0.55, 3.4, 0.5
    );
  }

  /* ---- glisten: wet specular specks around the contact point ---- */
  _glisAccum3 += dt;
  if (_glisAccum3 > 0.14 && ar > 0.24 && her3) {
    _glisAccum3 = 0;
    const org = vulvaWorld3(her3);
    fxGlisten3.spawn(
      org.x + (Math.random() - 0.5) * 0.14, org.y + Math.random() * 0.08, org.z + (Math.random() - 0.5) * 0.12,
      0, 0.08 + Math.random() * 0.1, 0,
      0.5 + Math.random() * 0.4, 0.5, 0, 0.9
    );
  }

  /* ---- euphoric hearts on orgasm ---- */
  if (G.state === 'orgasm' && her3) {
    _heartAccum3 += dt;
    if (_heartAccum3 > 0.22) {
      _heartAccum3 = 0;
      const h = her3.head.getWorldPosition(_fxV);
      fxHearts3.spawn(
        h.x + (Math.random() - 0.5) * 0.6, h.y + 0.1, h.z + (Math.random() - 0.5) * 0.3,
        (Math.random() - 0.5) * 0.25, 0.45 + Math.random() * 0.25, (Math.random() - 0.5) * 0.2,
        1.8 + Math.random(), 0.9 + Math.random() * 0.5, -0.35, 0.15
      );
    }
  }

  /* ---- integrate everything ---- */
  fxJets3.update(dt);
  fxDrips3.update(dt);
  fxSweat3.update(dt);
  fxGlisten3.update(dt);
  fxHearts3.update(dt);

  /* ---- screen bloom + camera shake follow the game state ---- */
  const bloom = (G.bloom || 0);
  if (flashEl3) {
    const target = bloom * 0.45;
    flashEl3.style.opacity = String(clamp(target, 0, 0.8));
  }
  if ((G.shake || 0) > 0.001) CAM3.shake = Math.max(CAM3.shake, G.shake * 0.035);
}

/* one-shot white flash */
function flash3(amount) {
  if (!flashEl3) return;
  flashEl3.style.transition = 'none';
  flashEl3.style.opacity = String(clamp(amount, 0, 1));
  // next frame, fade back out
  requestAnimationFrame(() => {
    if (!flashEl3) return;
    flashEl3.style.transition = 'opacity .5s ease-out';
    flashEl3.style.opacity = '0';
  });
}

/* wipe all particles (used on restart / round change) */
function clearFX3() {
  if (fxJets3) fxJets3.clear();
  if (fxDrips3) fxDrips3.clear();
  if (fxSweat3) fxSweat3.clear();
  if (fxGlisten3) fxGlisten3.clear();
  if (fxHearts3) fxHearts3.clear();
  _lastSpurts3 = 0;
}
