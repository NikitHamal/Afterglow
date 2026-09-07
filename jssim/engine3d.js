// Afterglow 3D — module: engine3d (scene, toon shading, camera rig)
// Loaded by 3d.html after the shared game logic. Replaces js/gfx.js.
'use strict';

/* ============================================================
   RENDERER / SCENE / CAMERA
   ============================================================ */
const CV3 = document.getElementById('scene3d');
let renderer3d = null, scene3d = null, camera3d = null;

try {
  renderer3d = new THREE.WebGLRenderer({ canvas: CV3, antialias: true, alpha: false });
} catch (e) {
  const bm = document.getElementById('bootMsg');
  if (bm) bm.textContent = 'webgl unavailable — this mode needs hardware 3d';
}

if (renderer3d) {
  renderer3d.setPixelRatio(Math.min(2, window.devicePixelRatio || 1));
  renderer3d.shadowMap.enabled = true;
  renderer3d.shadowMap.type = THREE.PCFSoftShadowMap;
  renderer3d.outputEncoding = THREE.sRGBEncoding;
  renderer3d.toneMapping = THREE.ACESFilmicToneMapping;
  renderer3d.toneMappingExposure = 1.0;

  scene3d = new THREE.Scene();
  scene3d.background = new THREE.Color(0x0d0709);
  scene3d.fog = new THREE.Fog(0x0d0709, 9, 26);

  camera3d = new THREE.PerspectiveCamera(42, 16 / 9, 0.05, 120);
  camera3d.position.set(0, 2.1, 5.2);
}

/* ============================================================
   CEL / TOON SHADING  (the "hentai" look: flat bands + rim)
   ============================================================ */
function toonRamp(steps) {
  const d = new Uint8Array(steps);
  for (let i = 0; i < steps; i++) {
    // gentle curve so mid-tones stay warm instead of muddy
    d[i] = Math.round(Math.pow(i / (steps - 1), 0.78) * 255);
  }
  const t = new THREE.DataTexture(d, steps, 1, THREE.RedFormat);
  t.minFilter = t.magFilter = THREE.NearestFilter;
  t.generateMipmaps = false;
  t.needsUpdate = true;
  return t;
}
const RAMP3 = toonRamp(4);   // hard cel bands (props, room)
/* soft painted ramp for SKIN: smooth S-curve, lifted shadows — the glossy
   hentai look instead of hard posterized bands that read as facets */
function toonRampSoft(steps) {
  const d = new Uint8Array(steps);
  for (let i = 0; i < steps; i++) {
    const t = i / (steps - 1);
    const s = t * t * (3 - 2 * t);
    d[i] = Math.round((0.32 + 0.68 * s) * 255);
  }
  const t2 = new THREE.DataTexture(d, steps, 1, THREE.RedFormat);
  t2.minFilter = t2.magFilter = THREE.NearestFilter;
  t2.generateMipmaps = false;
  t2.needsUpdate = true;
  return t2;
}
const RAMP5 = toonRampSoft(32);   // softer, for skin

/* ============================================================
   PROCEDURAL SKIN SURFACE
   Fine pores + warm/cool subsurface mottling so skin isn't flat
   plastic. Seeded noise (not per-frame random) keeps it stable.
   ============================================================ */
let _skinTex3 = null;
function makeSkinTex3() {
  if (_skinTex3) return _skinTex3;
  const S = 256;
  const cv = document.createElement('canvas');
  cv.width = cv.height = S;
  const c = cv.getContext('2d');
  c.fillStyle = '#ffffff';
  c.fillRect(0, 0, S, S);
  const img = c.getImageData(0, 0, S, S);
  const d = img.data;
  // deterministic pseudo-random so the texture is identical every load
  let seed = 1337;
  const rnd = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff;
  for (let y = 0; y < S; y++) {
    for (let x = 0; x < S; x++) {
      const i = (y * S + x) * 4;
      const n = Math.sin(x * 0.09) * Math.sin(y * 0.11) * 0.5
        + Math.sin(x * 0.31 + y * 0.17) * 0.3
        + (rnd() - 0.5) * 0.35;
      const pore = rnd() < 0.022 ? 0.93 : 1.0;
      // broad soft blotches: warm where blood pools, cooler in the hollows
      const blotch = Math.sin(x * 0.023 + 1.7) * Math.sin(y * 0.027 + 0.6) * 0.5
        + Math.sin(x * 0.051 + y * 0.043) * 0.3;
      const warm2 = Math.max(0, blotch) * 0.058;
      const cool = Math.max(0, -blotch) * 0.036;
      const g = clamp(1 + n * 0.06, 0.86, 1.12) * pore;
      const warm = Math.max(0, n) * 0.035;      // mottled areas read warmer
      d[i] = clamp(255 * g * (1 - cool * 0.5), 0, 255);
      d[i + 1] = clamp(255 * g * (1 - warm - warm2), 0, 255);
      d[i + 2] = clamp(255 * g * (1 - warm * 1.6 - warm2 * 1.8), 0, 255);
      d[i + 3] = 255;
    }
  }
  c.putImageData(img, 0, 0);
  const t = new THREE.CanvasTexture(cv);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.repeat.set(3, 3);
  t.encoding = THREE.sRGBEncoding;
  _skinTex3 = t;
  return t;
}

/* ============================================================
   FRESNEL SHEEN — the glossy highlight that sells anime skin
   (an additive shell; brighter at grazing angles)
   ============================================================ */
function makeSheenMat3(color, power, strength) {
  return new THREE.ShaderMaterial({
    uniforms: {
      uColor: { value: new THREE.Color(color) },
      uPower: { value: power == null ? 2.6 : power },
      uStrength: { value: strength == null ? 0.30 : strength }
    },
    vertexShader:
      'varying vec3 vN; varying vec3 vV;\n' +
      'void main(){ vec4 mv = modelViewMatrix * vec4(position,1.0);' +
      ' vN = normalize(normalMatrix * normal);' +
      ' vV = normalize(-mv.xyz);' +
      ' gl_Position = projectionMatrix * mv; }',
    fragmentShader:
      'uniform vec3 uColor; uniform float uPower; uniform float uStrength;\n' +
      'varying vec3 vN; varying vec3 vV;\n' +
      'void main(){ float f = pow(1.0 - clamp(dot(normalize(vN), normalize(vV)),0.0,1.0), uPower);' +
      ' gl_FragColor = vec4(uColor, f * uStrength); }',
    transparent: true,
    depthWrite: false,
    blending: THREE.AdditiveBlending
  });
}

/* ============================================================
   SKIN MATERIAL — toon bands + pore texture + glossy sheen shell
   ============================================================ */
function skinMat3(color) {
  const m = toonMat(color, { skin: true });
  try {
    m.map = makeSkinTex3();
    m.roughnessMap = m.map;   // pore-level gloss variation doubles as micro-normal feel
  } catch (e) { /* canvas unavailable */ }
  // warmer + a touch deeper than the flat picker color (painted skin, not clay)
  m.color.offsetHSL(0.004, 0.03, -0.035);
  // subsurface warmth: shadows glow faintly red instead of going dead grey
  m.emissive = new THREE.Color(0x2b0e12);
  m.emissiveIntensity = 0.32;
  return m;
}
/* clone a mesh as an additive sheen shell parented to the original */
function addSheen3(mesh, color, power, strength) {
  const s = new THREE.Mesh(mesh.geometry, makeSheenMat3(color || '#ffd9d0', power, strength));
  s.castShadow = false; s.receiveShadow = false;
  s.renderOrder = 2;
  s.userData.noInk = true;   // never outline a glow shell
  mesh.add(s);
  return s;
}

/* stylized PBR surface — keeps the anime linework (ink shells, face decal) but
   shades like a real surface: smooth gradients, clearcoat speculars, sheen
   fuzz on skin. `soft:false` props go matte; very dark colors (hair, deep
   moist cavities) automatically get a glossy clearcoat. */
function toonMat(color, opts) {
  opts = opts || {};
  const c = new THREE.Color(color);
  const lum = (c.r + c.g + c.b) / 3;
  const darkGloss = lum < 0.16;
  const skin = opts.skin === true;
  const m = new THREE.MeshPhysicalMaterial({
    color: c,
    roughness: skin ? 0.52 : (darkGloss ? 0.38 : (opts.soft === false ? 0.85 : 0.6)),
    metalness: 0.0,
    clearcoat: skin ? 0.35 : (darkGloss ? 0.65 : 0.08),
    clearcoatRoughness: skin ? 0.55 : 0.32,
    emissive: new THREE.Color(opts.emissive || 0x000000),
    emissiveIntensity: opts.emissiveIntensity == null ? 1 : opts.emissiveIntensity
  });
  if (skin) {
    m.sheen = 0.5;
    m.sheenColor = new THREE.Color(0xffc9bd);
    m.sheenRoughness = 0.6;
  }
  if (opts.transparent) { m.transparent = true; m.opacity = opts.opacity == null ? 1 : opts.opacity; }
  return m;
}

/* inverted-hull outline: extrude along normals, render backfaces only.
   Warm dark brown, not pure black — like printed hentai lineart. */
const OUTLINE_MAT = new THREE.ShaderMaterial({
  uniforms: { uThick: { value: 0.014 }, uColor: { value: new THREE.Color(0x3a1e26) } },
  vertexShader:
    'uniform float uThick;\n' +
    'void main(){ vec3 p = position + normalize(normal) * uThick;' +
    ' gl_Position = projectionMatrix * modelViewMatrix * vec4(p,1.0); }',
  fragmentShader:
    'uniform vec3 uColor;\n' +
    'void main(){ gl_FragColor = vec4(uColor,1.0); }',
  side: THREE.BackSide
});

/* attach a dark warm-brown ink outline to any mesh */
const INKSHELLS3 = [];   // {mat, base} — rescaled every few frames for constant screen weight
function inkOutline(mesh, thick) {
  const o = new THREE.Mesh(mesh.geometry, OUTLINE_MAT.clone());
  const b = thick == null ? 0.014 : thick;
  o.material.uniforms.uThick.value = b;
  o.castShadow = false; o.receiveShadow = false;
  o.userData.noInk = true;
  INKSHELLS3.push({ mat: o.material, base: b });
  mesh.add(o);
  return o;
}

/* keep ink lines a constant screen width: shrink them on macro zoom,
   let them breathe slightly on wide shots */
let _inkTick3 = 0;
function updateInk3() {
  if (!camera3d || ((_inkTick3++ % 12) !== 0)) return;
  let d = 4.5;
  if (CAM3.mode === 'fpv') d = 1.6;
  else if (CAM3.target) d = camera3d.position.distanceTo(CAM3.target);
  const f = clamp(d / 4.5, 0.30, 1.6);
  for (const r of INKSHELLS3) r.mat.uniforms.uThick.value = r.base * f;
}

/* ink the whole rig: thin dark shells on every physical mass = printed lineart.
   (skips glow shells, outline shells, the painted face and flat decals)
   Thickness scales with mesh size so toes and fingers get hairlines while
   torso and thighs keep bold readable contours. */
function inkRig3(root, thick) {
  const jobs = [];
  root.traverse(o => {
    if (o.isMesh && !o.userData.noInk && o.material && (o.material.isMeshToonMaterial || o.material.isMeshPhysicalMaterial || o.material.isMeshStandardMaterial)) jobs.push(o);
  });
  for (const m of jobs) {
    let r = 0.09;
    try {
      if (!m.geometry.boundingSphere) m.geometry.computeBoundingSphere();
      r = m.geometry.boundingSphere.radius || 0.09;
    } catch (e) { /* keep default */ }
    inkOutline(m, thick * clamp(r / 0.09, 0.35, 1.15));
  }
}

/* ============================================================
   GLOSS DECALS — the oily specular dots that sell wet anime skin.
   Camera-facing additive sprites parented to a bone so they track it
   (depth-tested, so they hide correctly from the far side).
   ============================================================ */
let _glossTex3 = null;
function glossTex3() {
  if (_glossTex3) return _glossTex3;
  const S = 128;
  const cv = document.createElement('canvas');
  cv.width = cv.height = S;
  const c = cv.getContext('2d');
  const g = c.createRadialGradient(S / 2, S / 2, 0, S / 2, S / 2, S / 2);
  g.addColorStop(0, 'rgba(255,244,238,0.95)');
  g.addColorStop(0.35, 'rgba(255,232,224,0.45)');
  g.addColorStop(1, 'rgba(255,225,220,0)');
  c.fillStyle = g;
  c.beginPath(); c.arc(S / 2, S / 2, S / 2, 0, Math.PI * 2); c.fill();
  const t = new THREE.CanvasTexture(cv);
  _glossTex3 = t;
  return t;
}
function addGloss3(parent, x, y, z, s, op) {
  // Retired: physical clearcoat + rim light now produce real specular
  // highlights. Kept as a no-op so existing call sites don't need edits.
  return null;
}

/* ============================================================
   LIGHTING — warm bedside key, cool moon rim, soft bounce
   ============================================================ */
let keyLight3 = null, rimLight3 = null, fillLight3 = null, lampPoint3 = null;

function buildLights() {
  // deep boudoir ambient
  scene3d.add(new THREE.AmbientLight(0x3a2030, 0.22));

  // soft sky/ground bounce so skin gradients stay creamy, never chalky
  scene3d.add(new THREE.HemisphereLight(0xffd9c8, 0x2a1218, 0.35));

  // warm bedside lamp — low side key so it rakes across curves (buttocks,
  // thighs, ribs) instead of flattening everything from the camera side
  keyLight3 = new THREE.DirectionalLight(0xffb072, 1.15);
  keyLight3.position.set(-5.0, 3.4, 0.8);
  keyLight3.castShadow = true;
  keyLight3.shadow.mapSize.set(1024, 1024);
  keyLight3.shadow.camera.near = 0.5;
  keyLight3.shadow.camera.far = 22;
  const sc = keyLight3.shadow.camera;
  sc.left = -6; sc.right = 6; sc.top = 6; sc.bottom = -6;
  keyLight3.shadow.bias = -0.0012;
  scene3d.add(keyLight3);

  // cool moon rim from the window — separates silhouette from the dark room
  rimLight3 = new THREE.DirectionalLight(0x8fb6ff, 1.10);
  rimLight3.position.set(5.5, 3.2, -4.5);
  scene3d.add(rimLight3);

  // warm counter-rim from the opposite side so curves read on both flanks
  const counterRim = new THREE.DirectionalLight(0xff9d76, 0.30);
  counterRim.position.set(-5.0, 2.6, 3.8);
  scene3d.add(counterRim);

  // gentle fill so shadows don't go pure black
  fillLight3 = new THREE.DirectionalLight(0xff8fa8, 0.25);
  fillLight3.position.set(2.0, 1.4, 5.0);
  scene3d.add(fillLight3);

  // practical lamp bulb glow
  lampPoint3 = new THREE.PointLight(0xffa860, 1.1, 12, 2);
  lampPoint3.position.set(-2.9, 1.35, -1.4);
  scene3d.add(lampPoint3);
}

/* ============================================================
   ENVIRONMENT — bedroom set dressing
   ============================================================ */
function buildRoom3() {
  const g = new THREE.Group();

  // floor
  const floor = new THREE.Mesh(
    new THREE.PlaneGeometry(40, 40),
    toonMat(0x2a161c, { soft: false })
  );
  floor.rotation.x = -Math.PI / 2;
  floor.position.y = -0.62;
  floor.receiveShadow = true;
  g.add(floor);

  // back wall + side wall
  const wallMat = toonMat(0x241319, { soft: false });
  const back = new THREE.Mesh(new THREE.PlaneGeometry(40, 12), wallMat);
  back.position.set(0, 4, -7);
  g.add(back);
  const side = new THREE.Mesh(new THREE.PlaneGeometry(24, 12), wallMat);
  side.rotation.y = Math.PI / 2;
  side.position.set(-9, 4, 0);
  g.add(side);

  // ---- bed ----
  const bedFrame = new THREE.Mesh(
    new THREE.BoxGeometry(4.6, 0.42, 6.4),
    toonMat(0x3d1b25, { soft: false })
  );
  bedFrame.position.set(0, -0.42, -0.4);
  bedFrame.receiveShadow = true; bedFrame.castShadow = true;
  g.add(bedFrame);

  // mattress (top surface sits at y = 0)
  const mattress = new THREE.Mesh(
    new THREE.BoxGeometry(4.4, 0.34, 6.2),
    toonMat(0x512833, { soft: false })
  );
  mattress.position.set(0, -0.06, -0.4);
  mattress.receiveShadow = true;
  g.add(mattress);

  // duvet ruffle pooled around the lower bed
  const duvet = new THREE.Mesh(
    new THREE.BoxGeometry(4.5, 0.16, 3.0),
    toonMat(0x3a2028, { soft: false })
  );
  duvet.position.set(0, 0.14, 2.4);
  duvet.receiveShadow = true;
  g.add(duvet);

  // pillows at the head of the bed
  for (let i = 0; i < 2; i++) {
    const p = new THREE.Mesh(
      new THREE.BoxGeometry(1.7, 0.28, 0.95),
      toonMat(0xc7ad96, { soft: false })
    );
    p.position.set(-0.55 + i * 1.1, 0.24, -2.75);
    p.rotation.z = i ? 0.05 : -0.04;
    p.castShadow = true; p.receiveShadow = true;
    g.add(p);
  }

  // ---- nightstand + lamp ----
  const stand = new THREE.Mesh(
    new THREE.BoxGeometry(1.15, 1.1, 1.0),
    toonMat(0x1e1015, { soft: false })
  );
  stand.position.set(-3.1, -0.07, -1.5);
  stand.castShadow = true; stand.receiveShadow = true;
  g.add(stand);

  const stem = new THREE.Mesh(
    new THREE.CylinderGeometry(0.045, 0.055, 0.55, 10),
    toonMat(0x8a6a34, { soft: false })
  );
  stem.position.set(-3.1, 0.75, -1.5);
  g.add(stem);

  const shade = new THREE.Mesh(
    new THREE.CylinderGeometry(0.34, 0.46, 0.5, 14, 1, true),
    new THREE.MeshBasicMaterial({ color: 0xffb877, side: THREE.DoubleSide, transparent: true, opacity: 0.92 })
  );
  shade.position.set(-3.1, 1.15, -1.5);
  g.add(shade);
  const bulb = new THREE.Mesh(
    new THREE.SphereGeometry(0.13, 12, 10),
    new THREE.MeshBasicMaterial({ color: 0xffd9a0 })
  );
  bulb.position.set(-3.1, 1.12, -1.5);
  g.add(bulb);

  // ---- window with moon ----
  const winFrame = new THREE.Mesh(
    new THREE.BoxGeometry(0.12, 2.6, 3.4),
    toonMat(0x1a0e14, { soft: false })
  );
  winFrame.position.set(6.4, 2.2, -1.0);
  g.add(winFrame);
  const glass = new THREE.Mesh(
    new THREE.PlaneGeometry(3.2, 2.4),
    new THREE.MeshBasicMaterial({ color: 0x35506e, transparent: true, opacity: 0.34 })
  );
  glass.rotation.y = -Math.PI / 2;
  glass.position.set(6.33, 2.2, -1.0);
  g.add(glass);
  const moon = new THREE.Mesh(
    new THREE.CircleGeometry(0.42, 24),
    new THREE.MeshBasicMaterial({ color: 0xdfe9f7 })
  );
  moon.rotation.y = -Math.PI / 2;
  moon.position.set(6.3, 2.9, -0.4);
  g.add(moon);

  scene3d.add(g);
  return g;
}

/* ============================================================
   CAMERA RIG — orbit / side / top / first-person
   ============================================================ */
const CAM3 = {
  mode: 'orbit',          // orbit | side | top | fpv
  look: false,            // LOOK mode: left-drag orbits instead of thrusting
  yaw: 0.0,               // orbit horizontal
  pitch: 0.30,            // orbit vertical
  dist: 5.4,              // orbit distance
  target: new THREE.Vector3(0, 0.55, 0),
  fpvYaw: 0, fpvPitch: -0.12, fpvDist: 1,
  shake: 0,
  _yaw: 0, _pitch: 0.30, _dist: 5.4,
  _px: 0, _py: 2.1, _pz: 5.2
};

/* smoothed camera state so mode switches glide instead of snapping */
function updateCamera3(dt, G) {
  if (!camera3d) return;
  const k = 1 - Math.pow(0.0016, dt);   // frame-rate independent lerp
  updateCamKeys3(dt);

  // wider lens in first person for immersion, adjusted per focus mode
  const focus = (typeof G !== 'undefined' && G.fpvFocus) ? G.fpvFocus : 'full';
  let wantFov = 42;
  if (CAM3.mode === 'fpv') {
    if (focus === 'hips') wantFov = 38;      // Sweet spot closeup
    else if (focus === 'breasts') wantFov = 40;  // Breasts closeup
    else if (focus === 'face') wantFov = 34;     // Face closeup
    else wantFov = 64;                           // Full body 2D-style perspective
  }
  if (Math.abs(camera3d.fov - wantFov) > 0.05) {
    camera3d.fov = lerp(camera3d.fov, wantFov, k);
    camera3d.updateProjectionMatrix();
  }

  if (CAM3.mode === 'fpv') {
    // first-person: ride HIS head — gaze targets selected focus mode (full, hips, breasts, face)
    const bob = Math.sin((G.t || 0) * 9.5) * 0.012 * (G.depth || 0);
    const rigs = (typeof rigs3 === 'function') ? rigs3() : null;
    let eye = null, tgt = null;
    if (rigs && rigs.him && rigs.him.head && rigs.her) {
      try {
        const V = vulvaWorld3(rigs.her);
        const C = new THREE.Vector3();
        if (rigs.her.chest) {
          rigs.her.chest.updateWorldMatrix(true, false);
          rigs.her.chest.getWorldPosition(C);
        } else {
          C.copy(V).add(new THREE.Vector3(0, 0.15, -0.35));
        }

        const H = new THREE.Vector3();
        if (rigs.her.head) {
          rigs.her.head.updateWorldMatrix(true, false);
          rigs.her.head.getWorldPosition(H);
        } else {
          H.copy(C).add(new THREE.Vector3(0, 0.05, -0.25));
        }

        const pos = (typeof G !== 'undefined' && G.pos != null) ? (G.pos | 0) : 0;
        const isOral = (typeof G !== 'undefined' && (G.oral || 0) > 0.03);

        tgt = new THREE.Vector3();

        if (pos === 0 || pos === 1) {
          // 2D-style frontal FPV perspective:
          // Player kneels between her thighs, looking along her body towards her face
          if (focus === 'hips') {
            // Sweet spot closeup: shaft driving into her wet vulva / introitus
            tgt.set(0, 0.28, -0.48);
            eye = new THREE.Vector3(0, 0.44, +0.10);
            wantFov = 38;
          } else if (focus === 'breasts') {
            // Breasts closeup: cleavage, bouncing breasts, flushed areolas from above
            tgt.set(0, 0.30, -0.85);
            eye = new THREE.Vector3(0, 0.68, -0.55);
            wantFov = 38;
          } else if (focus === 'face') {
            // Face closeup: elevated over her upper chest looking down into her face right-side up
            tgt.set(0, 0.28, -1.18);
            eye = new THREE.Vector3(0, 0.72, -0.75);
            wantFov = 34;
          } else {
            // 'full' (Normal view): 2D composition showing shaft entering vulva
            // at the bottom, bouncing breasts in center, face at the top!
            tgt.set(0, 0.30, -0.72);
            eye = new THREE.Vector3(0, 0.54, +0.28);
            wantFov = 58;
          }
        } else if (pos === 4 || pos === 5) {
          // Cowgirl: lying on back looking up at her
          eye = rigs.him.head.localToWorld(new THREE.Vector3(0, 0.04, 0.12));
          if (focus === 'hips') { tgt.copy(V); tgt.y += 0.04; wantFov = 38; }
          else if (focus === 'face') { tgt.copy(H); tgt.y -= 0.02; wantFov = 34; }
          else if (focus === 'breasts') { tgt.copy(C); tgt.y += 0.02; wantFov = 40; }
          else { tgt.addVectors(V, C).multiplyScalar(0.5); wantFov = 62; }
        } else if (pos === 2) {
          // Doggy: kneeling behind her, looking down her arched spine
          eye = rigs.him.head.localToWorld(new THREE.Vector3(0, 0.02, 0.12));
          if (focus === 'hips') { tgt.copy(V); tgt.y += 0.03; wantFov = 38; }
          else if (focus === 'face') { tgt.copy(H); tgt.y -= 0.02; wantFov = 34; }
          else if (focus === 'breasts') { tgt.copy(C); tgt.y -= 0.05; wantFov = 40; }
          else { tgt.addVectors(V, C).multiplyScalar(0.5); wantFov = 60; }
        } else {
          eye = rigs.him.head.localToWorld(new THREE.Vector3(0, 0.02, 0.12));
          if (focus === 'hips') { tgt.copy(V); wantFov = 38; }
          else if (focus === 'face') { tgt.copy(H); wantFov = 34; }
          else if (focus === 'breasts') { tgt.copy(C); wantFov = 40; }
          else { tgt.addVectors(V, C).multiplyScalar(0.5); wantFov = 62; }
        }

        const fp = CAM3.fpvDist || 1;
        if (fp !== 1) {
          const gaze = new THREE.Vector3().subVectors(tgt, eye).normalize();
          eye.addScaledVector(gaze, -(fp - 1) * 0.20);
        }
      } catch (e) { eye = null; tgt = null; }
    }
    if (eye && tgt) {
      const tx = eye.x, ty = eye.y + bob * 0.5, tz = eye.z;
      const isShot = (typeof SHOT3 !== 'undefined' && SHOT3.on);
      if (!CAM3._wasFpv || isShot) {
        CAM3._px = tx; CAM3._py = ty; CAM3._pz = tz;
        camera3d.fov = wantFov;
        camera3d.updateProjectionMatrix();
      }
      CAM3._wasFpv = true;
      CAM3._px = lerp(CAM3._px, tx, k);
      CAM3._py = lerp(CAM3._py, ty, k);
      CAM3._pz = lerp(CAM3._pz, tz, k);
      camera3d.position.set(CAM3._px, CAM3._py, CAM3._pz);

      const lookTgt = tgt.clone();
      if (CAM3.fpvYaw || CAM3.fpvPitch) {
        const d = eye.distanceTo(tgt);
        lookTgt.x += Math.sin(CAM3.fpvYaw || 0) * d * 0.8;
        lookTgt.y += Math.sin(CAM3.fpvPitch || 0) * d * 0.8;
      }
      camera3d.lookAt(lookTgt);
    } else {
      // fallback before the rigs exist
      const fp = CAM3.fpvDist || 1;
      const tx = -0.02, ty = 1.52 + bob, tz = -0.30 + (1 - fp) * 0.55;
      CAM3._px = lerp(CAM3._px, tx, k);
      CAM3._py = lerp(CAM3._py, ty, k);
      CAM3._pz = lerp(CAM3._pz, tz, k);
      camera3d.position.set(CAM3._px, CAM3._py, CAM3._pz);
      const yaw = CAM3.fpvYaw;
      const pitch = CAM3.fpvPitch;
      const dir = new THREE.Vector3(
        Math.sin(yaw) * Math.cos(pitch),
        Math.sin(pitch),
        -Math.cos(yaw) * Math.cos(pitch)
      );
      camera3d.lookAt(
        CAM3._px + dir.x,
        CAM3._py + dir.y,
        CAM3._pz + dir.z
      );
    }
  } else {
    CAM3._wasFpv = false;
    let ty = CAM3.yaw, tp = CAM3.pitch, td = CAM3.dist;
    if (CAM3.mode === 'side') { ty = Math.PI * 0.5; tp = 0.10; td = 5.6; }
    else if (CAM3.mode === 'top') { ty = 0.18; tp = 0.95; td = 4.6; }

    CAM3._yaw = lerp(CAM3._yaw, ty, k);
    CAM3._pitch = lerp(CAM3._pitch, tp, k);
    CAM3._dist = lerp(CAM3._dist, td, k);

    const cp = Math.cos(CAM3._pitch), sp = Math.sin(CAM3._pitch);
    let x = CAM3.target.x + Math.sin(CAM3._yaw) * cp * CAM3._dist;
    let y = CAM3.target.y + sp * CAM3._dist;
    let z = CAM3.target.z + Math.cos(CAM3._yaw) * cp * CAM3._dist;

    // impact / climax shake
    if (CAM3.shake > 0.0005) {
      const s = CAM3.shake;
      x += (Math.random() - 0.5) * s;
      y += (Math.random() - 0.5) * s;
      z += (Math.random() - 0.5) * s;
      CAM3.shake *= Math.pow(0.0001, dt);
      if (CAM3.shake < 0.0005) CAM3.shake = 0;
    }
    camera3d.position.set(x, y, z);
    camera3d.lookAt(CAM3.target);
  }
  updateInk3();
}

/* ============================================================
   CAMERA CONTROLS
   · LOOK mode ON  → left-drag orbits (click the LOOK button or press L)
   · LOOK mode OFF → left-drag thrusts (gameplay, see js/input.js)
   · ALWAYS        → right-drag / middle-drag / shift+drag / alt+drag orbit
   · wheel         → zoom      · arrow keys → orbit
   · WASD          → move the camera through the room
   · R             → reset the camera
   ============================================================ */
const CAMKEY3 = { f: 0, b: 0, l: 0, r: 0, u: 0, d: 0, yl: 0, yr: 0, pu: 0, pd: 0 };

function initCamControls3() {
  let orbiting = false, lx = 0, ly = 0;

  /* does this pointer event mean "orbit"? */
  const wantsOrbit = e =>
    e.button === 2 ||            // right
    e.button === 1 ||            // middle
    e.shiftKey || e.altKey ||    // modifiers
    CAM3.look;                   // explicit LOOK mode

  const begin = e => {
    if (!wantsOrbit(e)) return;              // plain left-drag → thrust
    orbiting = true; lx = e.clientX; ly = e.clientY;
    if (e.button === 2 || e.button === 1) e.preventDefault();
    // stop js/input.js from reading this as a thrust stroke
    if (CAM3.look && e.button === 0) { e.stopPropagation(); G.dragOn = false; }
  };
  const move = e => {
    if (!orbiting) return;
    const s = CAM3.mode === 'fpv' ? 0.0032 : 0.006;
    if (CAM3.mode === 'fpv') {
      CAM3.fpvYaw -= (e.clientX - lx) * s;
      CAM3.fpvPitch = clamp(CAM3.fpvPitch - (e.clientY - ly) * s, -0.9, 0.7);
    } else {
      CAM3.yaw -= (e.clientX - lx) * s;
      CAM3.pitch = clamp(CAM3.pitch + (e.clientY - ly) * 0.005, -0.25, 1.28);
    }
    lx = e.clientX; ly = e.clientY;
  };
  const end = () => { orbiting = false; };

  // capture-phase so we see the event before js/input.js does
  CV3.addEventListener('pointerdown', begin, true);
  window.addEventListener('pointermove', move);
  window.addEventListener('pointerup', end);
  window.addEventListener('pointercancel', end);
  CV3.addEventListener('contextmenu', e => e.preventDefault());

  CV3.addEventListener('wheel', e => {
    e.preventDefault();
    if (CAM3.mode === 'fpv') CAM3.fpvDist = clamp((CAM3.fpvDist || 1) + Math.sign(e.deltaY) * 0.12, 0.15, 3.2);
    else {
      // fine steps up close for macro inspection, coarse steps far away
      const st = CAM3.dist > 3 ? 0.42 : 0.13;
      CAM3.dist = clamp(CAM3.dist + Math.sign(e.deltaY) * st, 0.6, 11);
    }
  }, { passive: false });

  /* ---- keyboard ---- */
  const set = (code, v, ev) => {
    switch (code) {
      case 'KeyW': CAMKEY3.f = v; break;
      case 'KeyS': CAMKEY3.b = v; break;
      case 'KeyA': CAMKEY3.l = v; break;
      case 'KeyD': CAMKEY3.r = v; break;
      case 'KeyR': if (v) resetCam3(); break;
      case 'KeyL': if (v) { CAM3.look = !CAM3.look; syncCamButtons3(); } break;
      case 'ArrowLeft':  CAMKEY3.yl = v; break;
      case 'ArrowRight': CAMKEY3.yr = v; break;
      case 'ArrowUp':    CAMKEY3.pu = v; break;
      case 'ArrowDown':  CAMKEY3.pd = v; break;
      default: return;
    }
    if (ev && v) ev.preventDefault();
  };
  window.addEventListener('keydown', e => { if (!e.repeat) set(e.code, 1, e); });
  window.addEventListener('keyup', e => set(e.code, 0, e));
  window.addEventListener('blur', () => {
    for (const k in CAMKEY3) CAMKEY3[k] = 0;
  });

  const bind = (id, fn) => {
    const b = document.getElementById(id);
    if (b) b.onclick = ev => { ev.stopPropagation(); fn(); syncCamButtons3(); };
  };
  const exitFpv = () => {
    if (typeof G !== 'undefined' && G.view === 'fpv') {
      G.view = 'side';
      const bv = document.getElementById('btnView');
      if (bv) bv.classList.remove('on');
    }
  };
  bind('btnCamOrbit', () => { exitFpv(); CAM3.mode = 'orbit'; });
  bind('btnCamSide', () => { exitFpv(); CAM3.mode = 'side'; });
  bind('btnCamTop', () => { exitFpv(); CAM3.mode = 'top'; });
  bind('btnCamLook', () => { CAM3.look = !CAM3.look; });
  bind('btnCamReset', () => { resetCam3(); });
}

function resetCam3() {
  CAM3.yaw = 0.35; CAM3.pitch = 0.30; CAM3.dist = 5.4;
  CAM3.fpvYaw = 0; CAM3.fpvPitch = 0; CAM3.fpvDist = 1;
  CAM3.target.set(0, 0.55, 0);
  CAM3.shake = 0;
}

function syncCamButtons3() {
  [['btnCamOrbit', 'orbit'], ['btnCamSide', 'side'], ['btnCamTop', 'top']].forEach(([id, m]) => {
    const b = document.getElementById(id);
    if (b) b.classList.toggle('on', CAM3.mode === m);
  });
  const bl = document.getElementById('btnCamLook');
  if (bl) bl.classList.toggle('on', !!CAM3.look);
}

/* keyboard camera motion — integrates WASD + arrows each frame */
function updateCamKeys3(dt) {
  const anyKey = CAMKEY3.f || CAMKEY3.b || CAMKEY3.l || CAMKEY3.r ||
    CAMKEY3.yl || CAMKEY3.yr || CAMKEY3.pu || CAMKEY3.pd;
  if (!anyKey) return;

  if (CAM3.mode === 'fpv') {
    CAM3.fpvYaw += (CAMKEY3.yl - CAMKEY3.yr) * 1.6 * dt;
    CAM3.fpvPitch = clamp(CAM3.fpvPitch + (CAMKEY3.pu - CAMKEY3.pd) * 1.2 * dt, -0.9, 0.7);
    return;
  }
  // orbit: arrows spin, WASD slides the focus point
  CAM3.yaw -= (CAMKEY3.yl - CAMKEY3.yr) * 1.5 * dt;
  CAM3.pitch = clamp(CAM3.pitch + (CAMKEY3.pu - CAMKEY3.pd) * 1.1 * dt, -0.25, 1.28);

  const sp = 2.2 * dt;
  const sy = Math.sin(CAM3.yaw), cy = Math.cos(CAM3.yaw);
  const fx = -sy, fz = -cy;          // forward (from camera toward target)
  const rx = cy, rz = -sy;           // right
  const mv = (CAMKEY3.f - CAMKEY3.b), st = (CAMKEY3.r - CAMKEY3.l);
  CAM3.target.x += (fx * mv + rx * st) * sp;
  CAM3.target.z += (fz * mv + rz * st) * sp;
  CAM3.target.x = clamp(CAM3.target.x, -3.2, 3.2);
  CAM3.target.z = clamp(CAM3.target.z, -4.5, 4.5);
}

/* ============================================================
   RESIZE
   ============================================================ */
function resize3() {
  const st = document.getElementById('stage');
  const w = st ? st.clientWidth : window.innerWidth;
  const h = st ? st.clientHeight : window.innerHeight;
  if (!renderer3d) return;
  renderer3d.setSize(w, h, false);
  camera3d.aspect = (w && h) ? w / h : 16 / 9;
  camera3d.updateProjectionMatrix();
}

/* ============================================================
   BOOTSTRAP
   ============================================================ */
function initEngine3() {
  if (!renderer3d) return false;
  buildLights();
  buildRoom3();
  initCamControls3();
  resize3();
  syncCamButtons3();
  const bm = document.getElementById('bootMsg');
  if (bm) bm.style.display = 'none';
  return true;
}
window.addEventListener('resize', resize3);
