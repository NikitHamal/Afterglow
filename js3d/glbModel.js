// Afterglow 3D — module: glbModel (External 3D Character Model Manager)
// Loads, scales, positions and animates external .glb characters (Goat-chan,
// Kiyoko, …) in scene3d.
//
// MULTI-MODEL REGISTRY: each GLB preset has one entry in GLB_REGISTRY.
// GLB_MODEL keeps its original shape/API (active/loaded/model/…) and always
// aliases the currently-selected entry, so older Goat-chan code that touches
// GLB_MODEL directly keeps working while the other agent extends her.
//
// POSE FOLLOW: the active girl's root copies the procedural her3.root every
// frame (position + quaternion, pelvis-anchored). All 7 poses, blends,
// thrust, breathing, recoil, tremor, oral and FPV anchors are inherited for
// free — plus tail sway / ear flicks / breath scale on top.
// SKELETAL DRIVE: Goat-chan's 171-joint MMD skeleton is retargeted from the
// live her3 joints every frame (top-down FK copy, damped), so she articulates
// through every pose instead of holding her bind stance. Kiyoko / anime have
// no skeleton and stay root-follow only (they inherit poses via the pelvis).
// Editable per-body-part surface: GLB_PARTS (gain per part, live-tweakable)
// plus glbSetGain / glbSetTweak / glbSetScale / glbGetParts (see below).
'use strict';

/* Per-character tuning. fit = target height (m) after auto-scale.
   exclude = mesh-name pattern skipped when measuring bounds (Kiyoko's tail
   stretches ~1.5 m behind her and must not shrink her body fit).
   yaw = extra facing correction (rad) if a model proves to face backwards. */
const GLB_REGISTRY = {
  goatchan: { path: 'assets/goatchan/goatchan.glb', fit: 1.35, yOff: 0.62, exclude: null, yaw: 0 },
  kiyoko:   { path: 'assets/kiyoko.005.glb',         fit: 1.35, yOff: 0.62, exclude: /tail/i, yaw: 0 },
  anime:    { path: 'assets/free_download_female_anime.glb',          fit: 1.35, yOff: 0.62, exclude: null, yaw: 0 }
};

// Pose-follow scratch (THREE is always loaded before this module in 3d.html)
const _glbQ = new THREE.Quaternion();
const _glbYawQ = new THREE.Quaternion();
const _glbWob = new THREE.Quaternion();
const _glbOff = new THREE.Vector3();
const _glbYAxis = new THREE.Vector3(0, 1, 0);
const _glbXAxis = new THREE.Vector3(1, 0, 0);

// One shared fill light for ALL GLB girls — the scene already carries key +
// rim + counter + fill + lamp lights, and a per-model light per girl stacked
// up until pale skin blew out to white. First loaded model adds it, once.
let GLB_SHARED_LIGHT = null;

// Per-preset load state. entry: { loaded, loading, model, meshList,
// basePos, baseScale, pelvisLocal, tailMeshes, earMeshes, earT, earFlick,
// prevDepth, jiggleY, jiggleVel, charLight }
const GLB_STORE = {};

const GLB_MODEL = {
  active: false,
  loaded: false,
  loading: false,
  model: null,
  meshList: [],
  basePos: null,   // set to Vector3 on first alias (THREE always present here)
  baseScale: 1.0,
  key: null,

  // Dynamics & Spring Physics (mirrored from the active entry each frame)
  prevDepth: 0,
  jiggleY: 0,
  jiggleVel: 0
};
GLB_MODEL.basePos = null; // assigned once THREE is confirmed below

function glbEntryState(key) {
  let e = GLB_STORE[key];
  if (!e) {
    e = GLB_STORE[key] = {
      loaded: false, loading: false, model: null, meshList: [],
      basePos: null, baseScale: 1.0, pelvisLocal: null,
      tailMeshes: [], earMeshes: [], earT: 2 + Math.random() * 3, earFlick: 0,
      prevDepth: 0, jiggleY: 0, jiggleVel: 0,
      charLight: null, rig: null
    };
  }
  return e;
}

// Point the legacy GLB_MODEL alias at a store entry.
function glbAlias(key) {
  const e = GLB_STORE[key];
  GLB_MODEL.key = key;
  GLB_MODEL.loaded = !!(e && e.loaded);
  GLB_MODEL.loading = !!(e && e.loading);
  GLB_MODEL.model = e ? e.model : null;
  GLB_MODEL.meshList = e ? e.meshList : [];
  GLB_MODEL.basePos = e ? e.basePos : null;
  GLB_MODEL.baseScale = e ? e.baseScale : 1.0;
  GLB_MODEL.prevDepth = e ? e.prevDepth : 0;
  GLB_MODEL.jiggleY = e ? e.jiggleY : 0;
  GLB_MODEL.jiggleVel = e ? e.jiggleVel : 0;
}

function glbFitBox(model, exclude) {
  const box = new THREE.Box3();
  const tmp = new THREE.Box3();
  let any = false;
  model.updateWorldMatrix(true, true);
  model.traverse(c => {
    if (!c.isMesh) return;
    if (exclude && exclude.test(c.name || '')) return;
    tmp.setFromObject(c);
    if (tmp.isEmpty()) return;
    if (!any) { box.copy(tmp); any = true; }
    else box.union(tmp);
  });
  if (!any) box.setFromObject(model);
  return box;
}

function glbPlaceEntry(key) {
  const e = GLB_STORE[key];
  const cfg = GLB_REGISTRY[key] || {};
  if (!e || !e.model) return;
  const model = e.model;
  const box = glbFitBox(model, cfg.exclude || null);
  const size = new THREE.Vector3();
  box.getSize(size);
  const center = new THREE.Vector3();
  box.getCenter(center);
  const maxDim = Math.max(size.x, size.y, size.z);
  const scale = (cfg.fit || 1.35) / (maxDim || 1);
  e.baseScale = scale;
  model.scale.setScalar(scale);
  e.basePos = new THREE.Vector3(
    -center.x * scale,
    -center.y * scale + (cfg.yOff == null ? 0.62 : cfg.yOff),
    -center.z * scale
  );
  model.position.copy(e.basePos);

  // Pelvis anchor in MODEL-LOCAL units:
  // If the model has an MMD/Mixamo pelvis bone, use its exact local anchor!
  // Otherwise fall back to 52% up the fitted height.
  let pelvisBone = null;
  model.traverse(c => {
    if (!pelvisBone && c.isBone && /^(腰|下半身|hips|pelvis)$/i.test(glbCleanKey(c.name))) {
      pelvisBone = c;
    }
  });
  if (pelvisBone) {
    model.updateMatrixWorld(true);
    const pLoc = new THREE.Vector3();
    pelvisBone.getWorldPosition(pLoc);
    model.worldToLocal(pLoc);
    e.pelvisLocal = pLoc;
  } else {
    e.pelvisLocal = new THREE.Vector3(
      0,
      box.min.y + (box.max.y - box.min.y) * 0.52,
      center.z
    );
  }

  // Secondary-motion parts with their authored base rotations.
  e.tailMeshes.length = 0;
  e.earMeshes.length = 0;
  model.traverse(c => {
    if (!c.isMesh) return;
    const nm = c.name || '';
    if (/tail/i.test(nm)) e.tailMeshes.push({ m: c, bx: c.rotation.x, by: c.rotation.y, bz: c.rotation.z });
    else if (/ear/i.test(nm)) e.earMeshes.push({ m: c, bx: c.rotation.x, by: c.rotation.y, bz: c.rotation.z });
  });
}

/* Kiyoko's sculpted hair strands ship a gradient texture but no UVs, which
   renders black. Generate a simple planar UV from local bounds so the
   gradient actually shows (vertical textures → map V along height). */
function glbEnsureUV(geo) {
  if (!geo || !geo.attributes || geo.attributes.uv || !geo.attributes.position) return;
  geo.computeBoundingBox();
  const bb = geo.boundingBox, pos = geo.attributes.position;
  const sx = (bb.max.x - bb.min.x) || 1, sy = (bb.max.y - bb.min.y) || 1;
  const uv = new Float32Array(pos.count * 2);
  for (let i = 0; i < pos.count; i++) {
    uv[i * 2] = (pos.getX(i) - bb.min.x) / sx;
    uv[i * 2 + 1] = (pos.getY(i) - bb.min.y) / sy;
  }
  geo.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
}

/* Kiyoko's file has corrupt COLOR_0 streams (NaN/Inf) on several small
   meshes — with vertexColors on they render black. Detect and drop the bad
   attribute, giving the mesh its own vertexColors-off material clone so
   meshes sharing the material (e.g. her body) keep their valid colors. */
function glbColorValid(geo) {
  const col = geo && geo.attributes && geo.attributes.color;
  if (!col) return false;
  // full scan with early exit — Kiyoko's file has NaN/Inf runs buried
  // mid-buffer that spot-checks miss; even 70k floats is a one-time cost
  const arr = col.array;
  for (let i = 0; i < arr.length; i++) { if (!isFinite(arr[i])) return false; }
  return true;
}
function glbDropBadColor(c) {
  const geo = c.geometry;
  if (!geo || !geo.attributes || !geo.attributes.color) return;
  if (glbColorValid(geo)) return;
  geo.deleteAttribute('color');
  const fix = mt => {
    if (!mt || !mt.vertexColors) return mt;
    const cl = mt.clone();
    cl.vertexColors = false;
    cl.needsUpdate = true;
    return cl;
  };
  c.material = Array.isArray(c.material) ? c.material.map(fix) : fix(c.material);
}

/* Chun-Li's file ships textures that trip WebGL: color-space data maps tagged
   sRGB, non-RGBA formats (texSubImage2D INVALID_ENUM), and textures whose
   internalformat can't generate mipmaps. Fix every slot before first upload:
   correct encoding per slot, RGBA/UnsignedByte, UV0, then canvas-rebake so
   the pixel data is really RGBA8. Anything un-rebakeable gets mipmap-free
   safe flags so it can never throw. */
const GLB_TEX_SLOTS = ['map', 'roughnessMap', 'metalnessMap', 'normalMap', 'emissiveMap', 'aoMap', 'bumpMap', 'displacementMap'];
function glbSanitizeTextures(e) {
  if (!e || !e.meshList) return;
  const seen = [];
  e.meshList.forEach(c => {
    const mats = Array.isArray(c.material) ? c.material : [c.material];
    mats.forEach(mt => {
      if (!mt || seen.indexOf(mt) >= 0) return;
      seen.push(mt);
      GLB_TEX_SLOTS.forEach(slot => {
        const t = mt[slot];
        if (!t || !t.isTexture) return;
        try {
          // data maps must be linear; only color/emissive are sRGB
          if (typeof THREE !== 'undefined' && THREE.sRGBEncoding != null) {
            t.encoding = (slot === 'map' || slot === 'emissiveMap') ? THREE.sRGBEncoding : THREE.LinearEncoding;
          }
          if (typeof THREE !== 'undefined') {
            // sRGB sampling is only legal as RGBA/UnsignedByte in WebGL
            if (THREE.RGBAFormat != null) t.format = THREE.RGBAFormat;
            if (THREE.UnsignedByteType != null) t.type = THREE.UnsignedByteType;
          }
          if ('channel' in t) t.channel = 0; // single-UV renderer: pin to UV0
        } catch (_) {}
      });
    });
  });
  try { glbRebakeMaps(e); } catch (_) {}
  if (e.rebakeFail) {
    try { console.log('[glb] ' + e.rebakeFail + ' texture(s) kept original upload (rebake unavailable), safe flags applied'); } catch (_) {}
  }
  // Final pass: textures that survived without a canvas rebake (huge images
  // can exceed canvas limits, data textures have no image at all) must not
  // attempt mipmaps or exotic uploads. NEVER touch a color slot's encoding
  // here — linearizing map/emissiveMap washes skin to white. Data slots are
  // the only ones forced linear, and every slot keeps the loader's encoding
  // otherwise (sRGB color stays sRGB).
  seen.forEach(mt => {
    GLB_TEX_SLOTS.forEach(slot => {
      const t = mt[slot];
      if (!t || !t.isTexture || (t.userData && t.userData.rebaked)) return;
      try {
        const isColor = (slot === 'map' || slot === 'emissiveMap');
        if (!isColor && typeof THREE !== 'undefined' && THREE.LinearEncoding != null) {
          t.encoding = THREE.LinearEncoding;
        }
        t.generateMipmaps = false;
        if (typeof THREE !== 'undefined' && THREE.LinearFilter != null) t.minFilter = THREE.LinearFilter;
        t.needsUpdate = true;
      } catch (_) {}
    });
  });
}

/* Re-upload a GLB entry's textures through canvas 2D. Used to route around
   GPU-upload paths that silently produce black textures for some files. */
function glbRebakeMaps(e) {
  if (!e || !e.meshList) return;
  const seen = [];
  e.meshList.forEach(c => {
    const mats = Array.isArray(c.material) ? c.material : [c.material];
    mats.forEach(mt => {
      if (!mt || seen.indexOf(mt) >= 0) return;
      seen.push(mt);
      ['map', 'roughnessMap', 'metalnessMap', 'normalMap', 'emissiveMap', 'aoMap'].forEach(slot => {
        const t = mt[slot];
        if (!t || !t.image || !t.image.width || t.userData.rebaked) return;
        try {
          const img = t.image;
          const cv = document.createElement('canvas');
          cv.width = img.width; cv.height = img.height;
          const ctx = cv.getContext('2d');
          if (!ctx) throw new Error('no 2d context (texture too large?)');
          ctx.drawImage(img, 0, 0);
          const nt = new THREE.CanvasTexture(cv);
          nt.flipY = t.flipY; nt.encoding = t.encoding;
          nt.wrapS = t.wrapS; nt.wrapT = t.wrapT;
          nt.minFilter = t.minFilter; nt.magFilter = t.magFilter;
          nt.anisotropy = t.anisotropy;
          t.userData.rebaked = true;
          mt[slot] = nt;
          mt.needsUpdate = true;
        } catch (err) {
          // keep the original texture — the sanitize final pass will park it
          // on mipmap-free safe flags instead of dropping it to black
          try { e.rebakeFail = (e.rebakeFail || 0) + 1; } catch (_) {}
        }
      });
    });
  });
}

function glbFinishLoad(key, gltf) {  const e = glbEntryState(key);
  e.loaded = true;
  e.loading = false;
  const model = gltf.scene;
  e.model = model;

  // Configure materials and gather meshes.
  // Kiyoko's body is vertex-colored (COLOR_0) with an untextured paint
  // material — force vertexColors on so her skin actually renders.
  model.traverse(c => {
    if (c.isMesh) {
      c.castShadow = true;
      c.receiveShadow = true;
      e.meshList.push(c);
      glbDropBadColor(c); // corrupt COLOR_0 → black blobs; drop before styling
      const mats = Array.isArray(c.material) ? c.material : (c.material ? [c.material] : []);
      const needsUV = mats.some(mt => mt && mt.map);
      if (needsUV) glbEnsureUV(c.geometry);
      mats.forEach(mt => {
        // Only force vertex colors on UNTEXTURED materials (Kiyoko's body
        // paint). Textured parts (hair/tail) keep the loader's own setting —
        // forcing vertexColors there multiplies the map by COLOR_0 (black).
        if (mt && !mt.map && c.geometry && c.geometry.attributes && c.geometry.attributes.color) {
          mt.vertexColors = true;
        }
        if (mt) {
          if ('roughness' in mt) mt.roughness = Math.max(mt.roughness || 0.4, 0.35);
          if ('metalness' in mt) mt.metalness = Math.min(mt.metalness || 0.1, 0.15);
        }
      });
      if (c.material) c.material.needsUpdate = true;
    }
  });

  glbPlaceEntry(key);
  glbBuildRig(e); // resolve MMD bone handles once (no-skeleton → null)
  try { glbApplyCustom(key); } catch (_) {}

  // Sanitize texture encodings/formats BEFORE first GPU upload (exotic files
  // trip texSubImage2D/mipmap GL errors otherwise), then canvas-rebake.
  // dev only: ?nosanitize=1 skips this for A/B comparisons.
  try { if (!/nosanitize=1/.test(location.search)) glbSanitizeTextures(e); } catch (_) {}

  // Gentle shared fill for model clarity (added once no matter how many
  // girls load — see GLB_SHARED_LIGHT above).
  if (!GLB_SHARED_LIGHT) {
    GLB_SHARED_LIGHT = new THREE.DirectionalLight(0xffeff2, 0.55);
    GLB_SHARED_LIGHT.position.set(0.5, 2.2, 1.8);
    scene3d.add(GLB_SHARED_LIGHT);
  }
  e.charLight = GLB_SHARED_LIGHT;

  model.visible = false;
  scene3d.add(model);

  // Re-assert visibility for the current preset
  const isGLB = G.char && (G.char.preset === key || (!!G.char.isGLB && G.char.glbPath === (GLB_REGISTRY[key] || {}).path));
  if (isGLB) setGLBActive(true, G.char.preset);
  // Authoritative alias: the CURRENT preset, not whichever entry happened to
  // finish loading last (staggered lazy loads used to steal the alias, which
  // froze pose-follow on the visible girl).
  glbAlias((G.char && G.char.preset) || GLB_MODEL.key || key);

  // dev only: ?glbdebug=1 dumps per-mesh material state to the console
  try {
    const hm = /hidehair=(plane|nurbs)/.exec(location.search);
    if (hm) {
      const pat = hm[1] === 'plane' ? /hairplane/i : /hairnurb/i;
      e.meshList.forEach(c => { if (pat.test(c.name || '')) c.visible = false; });
    }
    // dev only: ?hidemesh=tail|ears hides any pipe-separated name pattern
    try {
      const hme = /hidemesh=([\w|]+)/.exec(location.search);
      if (hme) {
        const pat = new RegExp(hme[1].split('|').join('|'), 'i');
        e.meshList.forEach(c => { if (pat.test(c.name || '')) c.visible = false; });
      }
    } catch (_) {}
    // dev only: ?noshadow=1 disables self-shadow receive on GLB meshes
    try {
      if (/noshadow=1/.test(location.search)) {
        e.meshList.forEach(c => { c.castShadow = false; c.receiveShadow = false; });
      }
    } catch (_) {}
    // Textures are sanitized + rebaked unconditionally in glbFinishLoad above
    // (glbSanitizeTextures); dev only: ?rebake=1 is a legacy no-op alias.
    try { if (/rebake=1/.test(location.search)) glbRebakeMaps(e); } catch (_) {}
    // dev only: ?flathair=nurbs isolates lighting vs texture trouble
    if (/flathair=nurbs/.test(location.search)) {
      e.meshList.forEach(c => {
        if (!/hairnurb/i.test(c.name || '')) return;
        const mt = Array.isArray(c.material) ? c.material[0] : c.material;
        if (mt) {
          const cl = mt.clone();
          cl.map = null; cl.vertexColors = false;
          cl.color = new THREE.Color('#e85a10');
          cl.needsUpdate = true;
          c.material = cl;
        }
      });
    }
    if (location.search.indexOf('glbdebug=1') >= 0) {
      const rows = [];
      e.meshList.forEach(c => {
        const mt = Array.isArray(c.material) ? c.material[0] : c.material;
        const row = {
          mesh: c.name, mat: mt && mt.name, map: !!(mt && mt.map),
          vcol: !!(mt && mt.vertexColors), colorAttr: !!(c.geometry && c.geometry.attributes.color),
          uv: !!(c.geometry && c.geometry.attributes.uv), verts: c.geometry.attributes.position.count
        };
        rows.push(row);
        console.log('[glbdebug]', key, JSON.stringify(row));
      });
      document.body.insertAdjacentHTML('beforeend',
        '<pre id="glbdbg">GLBDBG-START' + JSON.stringify(rows) + 'GLBDBG-END</pre>');
    }
  } catch (_) {}

  console.log('Loaded 3D model:', (GLB_REGISTRY[key] || {}).path, 'meshes:', e.meshList.length);
}

function glbLoadEntry(key) {
  const cfg = GLB_REGISTRY[key];
  if (!cfg) return;
  const e = glbEntryState(key);
  if (e.loaded || e.loading) return;
  e.loading = true;
  glbAlias(key);
  const loader = new THREE.GLTFLoader();
  loader.load(
    cfg.path,
    gltf => glbFinishLoad(key, gltf),
    undefined,
    err => {
      console.error('Error loading 3D GLB model:', cfg.path, err);
      e.loading = false;
      glbAlias(key);
    }
  );
}

function initGLBModel() {
  if (typeof THREE === 'undefined' || typeof THREE.GLTFLoader === 'undefined') {
    console.warn('GLTFLoader is not available; external 3D models will not load.');
    return;
  }
  if (GLB_MODEL.basePos === null && THREE.Vector3) {
    GLB_MODEL.basePos = new THREE.Vector3(0, 0, 0);
  }

  // Lazy loading: 70–100 MB files each — load the active girl now, stagger
  // the rest so the night starts instantly instead of fetching ~250 MB at once.
  const activeKey = (G.char && G.char.preset) || 'goatchan';
  if (GLB_REGISTRY[activeKey]) glbLoadEntry(activeKey);
  else if (GLB_REGISTRY.goatchan) glbLoadEntry('goatchan');
  const rest = Object.keys(GLB_REGISTRY).filter(k => {
    const e = GLB_STORE[k];
    return !(e && (e.loaded || e.loading));
  });
  rest.forEach((k, i) => {
    setTimeout(() => glbLoadEntry(k), 2500 + i * 2500);
  });

  // Legacy fallback: a saved character pointing at an unregistered .glb
  const custom = G.char && G.char.glbPath;
  const known = Object.keys(GLB_REGISTRY).some(k => GLB_REGISTRY[k].path === custom);
  if (custom && !known) {
    GLB_REGISTRY.__custom = { path: custom, fit: 1.35, yOff: 0.62, exclude: null };
    glbLoadEntry('__custom');
  }

  glbAlias((G.char && G.char.preset) || 'goatchan');
  const isGLB = !!(G.char && G.char.isGLB);
  setGLBActive(isGLB, G.char && G.char.preset);
}

function glbIsPreset(key) {
  if (!key) return false;
  if (GLB_REGISTRY[key]) return true;
  const custom = G.char && G.char.glbPath;
  return !!(G.char && G.char.isGLB && custom && key === '__custom');
}

function setGLBActive(active, preset) {
  GLB_MODEL.active = !!active;
  const want = preset || (G.char && G.char.preset) || GLB_MODEL.key || 'goatchan';
  // Lazy: selecting an unloaded girl from the picker starts her download now.
  if (GLB_MODEL.active && GLB_REGISTRY[want]) {
    const e0 = GLB_STORE[want];
    if (!e0 || (!e0.loaded && !e0.loading)) glbLoadEntry(want);
  }
  Object.keys(GLB_STORE).forEach(k => {
    const e = GLB_STORE[k];
    if (e && e.model) e.model.visible = GLB_MODEL.active && (k === want || (k === '__custom' && want && !GLB_REGISTRY[want]));
  });
  if (want && GLB_STORE[want]) glbAlias(want);
  else if (want && GLB_REGISTRY[want]) glbAlias(want);
  if (typeof her3 !== 'undefined' && her3 && her3.root) {
    her3.root.visible = !(GLB_MODEL.active && glbIsPreset(want));
  }
  if (GLB_MODEL.active && want) {
    try { glbApplyCustom(want); } catch (_) {}
  }
}

/* ============================================================
   SKELETAL DRIVE — Goat-chan's MMD bones follow the live her3 rig.

   Approach: top-down FK copy with damping. Each mapped bone copies the
   matching her3 joint's LOCAL euler rotation (same semantic joint, e.g.
   procedural shoulder → MMD 腕), damped so gameplay spikes can never snap
   her, scaled by a per-part gain and offset by a per-part local-Euler
   tweak. Copies run top-down (spine → limbs → extremities) so children
   inherit their parents' new transforms within the same frame.

   Why local-euler copy instead of world matching: the bind poses differ
   (MMD A-pose vs procedural T-ish zero), so world quaternions would bake
   a permanent offset error. Matching joints have near-parallel rest axes
   (spine +Y, shoulder X-twist, elbow/knee X-hinge, ankle X-hinge), so a
   damped local copy articulates correctly with zero rest-calibration.
   Twist-heavy MMD leaves (腕捩/足捩/scale bones) are left at rest.

   Models without a skeleton (Kiyoko, anime: 0 skins) never build a rig —
   e.rig stays null and they keep root-only pose-follow (pelvis-anchored),
   which is the most any static mesh can inherit.
   ============================================================ */

// Per-part drive config. gain scales the copied rotation (0 = locked in
// bind pose, 1 = full articulation). tweak adds a fixed local-Euler
// offset (radians) on top — e.g. to relax her splayed hands. Both are
// live-tweakable via glbSetGain/glbSetTweak below (also from the browser
// console: GLB_PARTS.arms.gain = 0.5).
const GLB_PARTS = {
  spine:  { gain: 1.0, tweak: [0, 0, 0] },
  head:   { gain: 1.0, tweak: [0, 0, 0] },
  arms:   { gain: 1.0, tweak: [0, 0, 0] },
  hands:  { gain: 1.0, tweak: [0, 0, 0] },
  legs:   { gain: 1.0, tweak: [0, 0, 0] },
  feet:   { gain: 1.0, tweak: [0, 0, 0] },
  breast: { gain: 0.35, tweak: [0, 0, 0] },
  tail:   { gain: 0.0, tweak: [0, 0, 0] } // tail stays on glbSecondary3 sway
};
// Damping rate for the bone copy (1/s). Matches applyRig3's limb ease.
const GLB_DRIVE_RATE = 18;

/* Bone map: her3 joint → Goat-chan MMD node name. Plain entries copy the
   named her3 joint's local rotation; ankle/toe entries resolve the
   smoothed pose channels instead (see glbDriveRig) since the procedural
   rig has no ankle/toe joints. */
const GLB_RIG_MAP = [
  { part: 'spine',  from: 'torso',        to: '上半身' },
  { part: 'spine',  from: 'chest',        to: '上半身2' },
  { part: 'head',   from: 'neck',         to: '首' },
  { part: 'arms',   from: 'armL.shoulder', to: '腕.L' },
  { part: 'arms',   from: 'armR.shoulder', to: '腕.R' },
  { part: 'arms',   from: 'armL.elbow',   to: 'ひじ.L' },
  { part: 'arms',   from: 'armR.elbow',   to: 'ひじ.R' },
  { part: 'hands',  from: 'armL.hand',    to: '手首.L' },
  { part: 'hands',  from: 'armR.hand',    to: '手首.R' },
  { part: 'legs',   from: 'legL.hip',     to: '足.L' },
  { part: 'legs',   from: 'legR.hip',     to: '足.R' },
  { part: 'legs',   from: 'legL.knee',    to: 'ひざ.L' },
  { part: 'legs',   from: 'legR.knee',    to: 'ひざ.R' },
  { part: 'feet',   from: 'legL.foot',    to: '足首.L', ankle: 0 },
  { part: 'feet',   from: 'legR.foot',    to: '足首.R', ankle: 1 },
  // Toes: the procedural rig has no toe joints (see glbDriveRig), so they
  // ride the same-side ankle at half rate, X-only — their rest frame is
  // ~180° Y-twisted, full-euler copy would corkscrew.
  { part: 'feet',   from: 'legL.foot',    to: 'つま先.L', toe: 0 },
  { part: 'feet',   from: 'legR.foot',    to: 'つま先.R', toe: 1 },
  { part: 'breast', from: 'breastL',      to: '乳親.L' },
  { part: 'breast', from: 'breastR',      to: '乳親.R' }
];

// Read a her3 joint by dotted path ('armL.shoulder' → her3.armL.shoulder).
function glbSrcJoint(path) {
  if (typeof her3 === 'undefined' || !her3) return null;
  let o = her3;
  const bits = path.split('.');
  for (let i = 0; i < bits.length; i++) {
    o = o[bits[i]];
    if (!o) return null;
  }
  return o;
}
function glbCleanKey(s) {
  return (s || '').replace(/[._]/g, '');
}

/* Walk the loaded scene once, resolve every mapped MMD node by exact or
   dot-stripped name (Three.js GLTFLoader sanitizes dots out of node names),
   and snapshot its bind-pose local euler. Cheap per-frame drive after that.
   Skips IK/control leaves (腕IK/足ＩＫ/指IK/捩 bones): driving FK chains is
   enough, the IK leaves have no skinned weight worth chasing. */
function glbBuildRig(e) {
  if (!e || !e.model || e.rig) return;
  const byName = {};
  e.model.traverse(c => {
    if (c.name) {
      if (byName[c.name] === undefined) byName[c.name] = c;
      const ck = glbCleanKey(c.name);
      if (byName[ck] === undefined) byName[ck] = c;
    }
  });
  const bones = [];
  for (let i = 0; i < GLB_RIG_MAP.length; i++) {
    const m = GLB_RIG_MAP[i];
    const dst = byName[m.to] || byName[glbCleanKey(m.to)] || null;
    if (!dst) continue;
    bones.push({
      part: m.part, path: m.from, dst,
      ankle: m.ankle, toe: m.toe,
      bx: dst.rotation.x, by: dst.rotation.y, bz: dst.rotation.z,
      sx: dst.scale.x, sy: dst.scale.y, sz: dst.scale.z
    });
  }
  // Finger chains: each MMD finger base lives under 手首.L/R or 手首L/R.
  // Curl the base segment toward a loose fist scaled by the her3 hand's roll.
  const fingers = [];
  e.model.traverse(c => {
    if (!c.name) return;
    if (/(指０|親指０)/.test(c.name) && /[._]?[LR]$/i.test(c.name)) {
      fingers.push({
        dst: c, side: /L$/i.test(glbCleanKey(c.name)) ? 'armL.hand' : 'armR.hand',
        bx: c.rotation.x, by: c.rotation.y, bz: c.rotation.z
      });
    }
  });
  e.rig = (bones.length || fingers.length) ? { bones, fingers } : null;
}

/* Drive the mapped skeleton from the live her3 joints. Runs AFTER
   glbFollowHer3 (root placement) inside updateGLBModel — which itself runs
   after updateAnim3, so every her3 joint is final for this frame. */
const _gdZero = [0, 0, 0];
function glbDriveRig(e, dt) {
  if (!e || !e.rig || (!e.rig.bones.length && !e.rig.fingers.length)) return;
  const rate = (dt > 0) ? (1 - Math.exp(-GLB_DRIVE_RATE * dt)) : 1;
  const rig = e.rig;
  const sm = (typeof _smPose3 !== 'undefined' && _smPose3.her) || null;

  for (let i = 0; i < rig.bones.length; i++) {
    const b = rig.bones[i];
    const src = glbSrcJoint(b.path);
    if (!src) continue;
    const P = GLB_PARTS[b.part] || GLB_PARTS.spine;
    const g = (P.gain == null) ? 1 : P.gain;
    if (g === 0) continue;
    const tw = P.tweak || _gdZero;

    let sx = src.rotation.x, sy = src.rotation.y, sz = src.rotation.z;
    if (b.ankle === 0 || b.ankle === 1) {
      sx = (sm && sm.ankle && (sm.ankle[b.ankle] || 0)) || 0; sy = 0; sz = 0;
    } else if (b.toe === 0 || b.toe === 1) {
      const a = (sm && sm.toes && (sm.toes[b.toe] || 0)) || 0;
      sx = a * 0.5; sy = 0; sz = 0;
    } else if (b.path === 'armL.hand' || b.path === 'armR.hand') {
      const elb = (b.path === 'armL.hand') ? 'armL.elbow' : 'armR.elbow';
      const es = glbSrcJoint(elb);
      sx = es ? es.rotation.x * 0.5 : 0; sy = 0;
    } else if (b.path === 'armL.elbow' || b.path === 'armR.elbow') {
      // Elbow hinge flexion
      sx = src.rotation.x; sy = 0; sz = 0;
    } else if (b.path === 'legL.knee' || b.path === 'legR.knee') {
      // Knee flexion: positive X bends backward naturally in MMD
      sx = src.rotation.x; sy = 0; sz = 0;
    }

    // Breasts: applyRig3 never rotates them (jiggle owns them) — reuse the
    // smoothed chest pitch at low gain so cleavage rises/falls with the pose.
    if ((b.path === 'breastL' || b.path === 'breastR') && sm && sm.chest) {
      sx = (sm.chest[0] || 0); sy = 0; sz = 0;
    }

    const tx = b.bx + sx * g + (tw[0] || 0);
    const ty = b.by + sy * g + (tw[1] || 0);
    const tz = b.bz + sz * g + (tw[2] || 0);
    const r = b.dst.rotation;

    // dt<=0 (paused/headless probe) snaps — still deterministic, no NaN risk.
    r.x = (rate >= 1) ? tx : r.x + (tx - r.x) * rate;
    r.y = (rate >= 1) ? ty : r.y + (ty - r.y) * rate;
    r.z = (rate >= 1) ? tz : r.z + (tz - r.z) * rate;

    // Optional per-part bone scale (e.g. breast size); always written so
    // resetting to 1 restores the authored bind scale instead of sticking.
    const ps = P.scale || 1;
    b.dst.scale.set(b.sx * ps, b.sy * ps, b.sz * ps);
  }

  // fingers: loose curl follows the her3 wrist roll (hands read as fists
  // when she grips; open when the wrist is neutral). Gain-gated by hands.
  const PH = GLB_PARTS.hands;
  if (PH && PH.gain !== 0 && rig.fingers.length) {
    for (let i = 0; i < rig.fingers.length; i++) {
      const f = rig.fingers[i];
      const src = glbSrcJoint(f.side);
      const curl = src ? clamp(Math.abs(src.rotation.z) * 1.2, 0, 0.9) : 0;
      const tw = PH.tweak || _gdZero;
      const tx = f.bx + curl * PH.gain * 0.9 + (tw[0] || 0);
      const r = f.dst.rotation;
      r.x = (rate >= 1) ? tx : r.x + (tx - r.x) * rate;
    }
  }
}

/* ---- editable per-part surface (live from console or UI) ---- */
// Set a part's drive gain (0 locks it in bind pose, 1 = full articulation).
function glbSetGain(part, gain) {
  if (!GLB_PARTS[part]) return false;
  GLB_PARTS[part].gain = clamp(+gain, 0, 1.5);
  return true;
}
// Add a fixed local-Euler offset (radians) on top of the copied motion.
function glbSetTweak(part, x, y, z) {
  if (!GLB_PARTS[part]) return false;
  GLB_PARTS[part].tweak = [+x || 0, +y || 0, +z || 0];
  return true;
}
// Scale a whole part's bones (e.g. breast size); 1 = authored.
function glbSetScale(part, s) {
  if (!GLB_PARTS[part]) return false;
  GLB_PARTS[part].scale = clamp(+s, 0.3, 2.5);
  return true;
}
// Snapshot the current per-part config (for UI panels / debugging).
function glbGetParts() {
  const out = {};
  for (const k in GLB_PARTS) out[k] = { gain: GLB_PARTS[k].gain, tweak: GLB_PARTS[k].tweak.slice(), scale: GLB_PARTS[k].scale || 1 };
  return out;
}

/* ============================================================
   DEEP GLB CUSTOMIZATION SYSTEM
   Live toggling of meshes, material color tinting, and part scaling.
   Supports Goat-chan (tail, horns, ears, socks, pasties, accessories,
   runes, genitals, skin tone, hair color, breast scale) and other GLB models.
   ============================================================ */

const GLB_PARTS_CONFIG = {
  goatchan: {
    meshes: {
      tail:        { label: 'Tail',         default: true,  match: /^tail$/i },
      horns:       { label: 'Horns',        default: true,  match: /horn/i },
      ears:        { label: 'Ears',         default: true,  match: /^ear$/i },
      socks:       { label: 'Thigh Socks',  default: true,  match: /平面010_2|socks/i },
      accessories: { label: 'Accessories',  default: true,  match: /平面010_1|acce/i },
      nippless:    { label: 'Pasties',      default: true,  match: /nippless/i },
      runes:       { label: 'Body Runes',   default: true,  match: /平面010_3|pattern/i },
      genital:     { label: 'Genitals',     default: true,  match: /genital/i }
    },
    materials: {
      skin:        { label: 'Skin Tint',    mats: ['body', 'face', 'face_nosp'] },
      hair:        { label: 'Hair Color',   mats: ['hair'] },
      runes:       { label: 'Runes Color',  mats: ['pattern'] },
      socks:       { label: 'Socks Color',  mats: ['body_socks'] }
    }
  }
};

// Set mesh visibility by part key
function glbSetPartVisible(preset, partKey, visible) {
  const e = GLB_STORE[preset];
  if (!e || !e.model) return false;
  const cfg = GLB_PARTS_CONFIG[preset];
  const meshDef = cfg && cfg.meshes && cfg.meshes[partKey];
  const pat = meshDef ? meshDef.match : new RegExp(partKey, 'i');

  e.model.traverse(c => {
    if (c.isMesh && pat.test(c.name || '')) {
      c.visible = !!visible;
    }
  });

  if (typeof G !== 'undefined' && G.char) {
    if (!G.char.glbCustom) G.char.glbCustom = {};
    if (!G.char.glbCustom.parts) G.char.glbCustom.parts = {};
    G.char.glbCustom.parts[partKey] = !!visible;
    if (typeof lsSaveChar === 'function') lsSaveChar();
  }
  return true;
}

// Set material color tint by group
function glbSetPartColor(preset, groupKey, hexColor) {
  const e = GLB_STORE[preset];
  if (!e || !e.model) return false;
  const cfg = GLB_PARTS_CONFIG[preset];
  const matDef = cfg && cfg.materials && cfg.materials[groupKey];
  const targetMats = matDef ? matDef.mats : [groupKey];
  const col = new THREE.Color(hexColor);

  e.model.traverse(c => {
    if (!c.isMesh || !c.material) return;
    const mats = Array.isArray(c.material) ? c.material : [c.material];
    mats.forEach(m => {
      if (m && targetMats.some(tm => (m.name || '').toLowerCase().includes(tm.toLowerCase()))) {
        m.color.copy(col);
        m.needsUpdate = true;
      }
    });
  });

  if (typeof G !== 'undefined' && G.char) {
    if (!G.char.glbCustom) G.char.glbCustom = {};
    if (!G.char.glbCustom.colors) G.char.glbCustom.colors = {};
    G.char.glbCustom.colors[groupKey] = hexColor;
    if (typeof lsSaveChar === 'function') lsSaveChar();
  }
  return true;
}

// Apply full saved customizations to a GLB model
function glbApplyCustom(preset) {
  const e = GLB_STORE[preset];
  if (!e || !e.model) return;
  const custom = (G.char && G.char.glbCustom) || {};

  // Apply part visibilities
  const parts = custom.parts || {};
  const cfg = GLB_PARTS_CONFIG[preset];
  if (cfg && cfg.meshes) {
    for (const pk in cfg.meshes) {
      const vis = parts[pk] !== undefined ? parts[pk] : cfg.meshes[pk].default;
      glbSetPartVisible(preset, pk, vis);
    }
  }

  // Apply colors
  const colors = custom.colors || {};
  if (colors) {
    for (const gk in colors) {
      glbSetPartColor(preset, gk, colors[gk]);
    }
  }

  // Synchronize character skin tone and hair color with GLB if not explicitly overridden
  if (G.char) {
    if (G.char.hairColor && (!colors || !colors.hair)) {
      glbSetPartColor(preset, 'hair', G.char.hairColor);
    }
    if (G.char.skinTone !== undefined && (!colors || !colors.skin)) {
      const sk = typeof getSkin === 'function' ? getSkin(G.char.skinTone) : null;
      if (sk && sk.base) glbSetPartColor(preset, 'skin', sk.base);
    }
    if (G.char.breastSize !== undefined) {
      glbSetScale('breast', 0.65 + G.char.breastSize * 0.9);
    }
    if (G.char.bodyScale !== undefined) {
      const bs = 0.85 + G.char.bodyScale * 0.3;
      if (GLB_REGISTRY[preset]) {
        GLB_REGISTRY[preset].fit = 1.35 * bs;
      }
    }
  }
}

/* Pose follow: pin the girl's pelvis to the procedural her3.root (which
   carries the fully blended/damped 7-pose + thrust + breath + tremor + oral
   motion), then layer secondary life on top. Falls back to the legacy
   standing placement when the procedural rigs are unavailable. */
function glbFollowHer3(e, dt, key) {
  const m = e.model;
  const root = (typeof her3 !== 'undefined' && her3) ? her3.root : null;
  if (!root || !e.pelvisLocal) {
    if (e.basePos) {
      m.position.copy(e.basePos);
      m.position.y += e.jiggleY * 0.05;
    }
    return;
  }
  const cfg = GLB_REGISTRY[key] || {};

  // orientation = her frame (+ optional per-model yaw fix)
  _glbQ.copy(root.quaternion);
  if (cfg.yaw) {
    _glbYawQ.setFromAxisAngle(_glbYAxis, cfg.yaw);
    _glbQ.multiply(_glbYawQ);
  }
  // oral rhythm wobble: her head bobs with his thrusts down there
  const oral = (typeof G !== 'undefined') ? clamp(G.oral || 0, 0, 1) : 0;
  if (oral > 0.03) {
    const t = (typeof G !== 'undefined' ? G.t : 0) || 0;
    const gag = clamp((typeof G !== 'undefined' ? G.oralGag : 0) || 0, 0, 1);
    _glbWob.setFromAxisAngle(_glbXAxis, Math.sin(t * 5.2) * 0.022 * (0.4 + oral + gag));
    _glbQ.multiply(_glbWob);
  }
  m.quaternion.copy(_glbQ);

  // position = her pelvis minus the rotated pelvis offset (+ bounce/breath)
  _glbOff.copy(e.pelvisLocal).multiplyScalar(e.baseScale).applyQuaternion(_glbQ);
  m.position.set(
    root.position.x - _glbOff.x,
    root.position.y - _glbOff.y + e.jiggleY * 0.05,
    root.position.z - _glbOff.z
  );
}

/* Secondary life: breathing scale, tail sway, ear flicks. Purely additive —
   never fights the pose-follow transform above. calm (0..1) scales every
   idle amplitude — at rest she lies nearly still instead of shaking. */
function glbSecondary3(e, dt, calm) {
  const m = e.model;
  if (!m) return;
  calm = (calm == null) ? 1 : calm;
  const t = (typeof G !== 'undefined' ? G.t : 0) || 0;
  const p = (typeof G !== 'undefined') ? clamp((G.pleasure || 0) / 100, 0, 1) : 0;

  // breathing scale (mirrors updateAnim3's chest rhythm)
  const br = Math.sin(t * TAU * (0.16 + p * 0.0045)) * ((2.4 + p * 1.6) / 260) * calm;
  m.scale.setScalar(Math.max(0.5, e.baseScale * (1 + br * 0.6)));

  // tail sway: idle wave + thrust bounce coupling
  const sway = 0.15 + 0.85 * calm;
  for (let i = 0; i < e.tailMeshes.length; i++) {
    const tm = e.tailMeshes[i];
    tm.m.rotation.z = tm.bz + Math.sin(t * 2.1 + i * 1.7) * 0.10 * sway + e.jiggleY * 1.6;
    tm.m.rotation.x = tm.bx + Math.sin(t * 1.6 + i * 0.9) * 0.05 * sway;
  }

  // ear flicks: every few seconds a quick twitch
  e.earT -= dt;
  if (e.earT <= 0) { e.earT = 2.5 + Math.random() * 4; e.earFlick = 0.32; }
  let flick = 0;
  if (e.earFlick > 0) {
    e.earFlick = Math.max(0, e.earFlick - dt);
    const fp = 1 - e.earFlick / 0.32;
    flick = Math.sin(fp * Math.PI * 5) * 0.16 * (1 - fp);
  }
  for (let i = 0; i < e.earMeshes.length; i++) {
    const em = e.earMeshes[i];
    em.m.rotation.x = em.bx + flick;
  }
}

/* Bed-solid collision: the mattress/duvet is a solid slab — no model ends up
   inside it, and none hovers above it either. After pose-follow + secondary
   motion, measure the real bounds: sunk → lift onto the surface, floating
   (beyond a 3 cm deadband) → settle down onto it. Procedural rigs keep
   their own solveContact3 floor; this covers every GLB girl. */
const _glbBedBox = new THREE.Box3();
function glbBedFloor(x, z) {
  if (Math.abs(x) < 2.3 && z > 0.85 && z < 3.95) return 0.24; // duvet mound
  return 0.13; // mattress top (0.11) + skin margin, matches solveContact3
}
function glbCollideBed(e) {
  const m = e && e.model;
  if (!m) return;
  try {
    _glbBedBox.setFromObject(m);
    if (_glbBedBox.isEmpty()) return;
    const cx = (_glbBedBox.min.x + _glbBedBox.max.x) * 0.5;
    const cz = (_glbBedBox.min.z + _glbBedBox.max.z) * 0.5;
    // only collide over the bed slab — figures posed beside it rest on the floor
    if (Math.abs(cx) > 2.6 || cz < -3.8 || cz > 4.2) return;
    const pen = glbBedFloor(cx, cz) - _glbBedBox.min.y;
    if (pen > 0) m.position.y += pen; // sunk into mattress → lift onto surface
  } catch (_) {}
}

function updateGLBModel(dt) {
  if (!GLB_MODEL.active) return;
  // G.char.preset is authoritative — the alias may lag after staggered loads.
  const key = (G.char && G.char.preset) || GLB_MODEL.key;
  const e = (key && GLB_STORE[key]) || null;
  if (!e || !e.loaded || !e.model) return;

  // Idle freeze: before the night starts she holds her placed pose — no
  // breathing, tail sway, ear flicks or jiggle. (Addresses "moving by default".)
  const frozen = (typeof G !== 'undefined') && G.state === 'intro';

  const currentDepth = (typeof G !== 'undefined' && G.depth != null) ? G.depth : 0;
  const depthDelta = currentDepth - e.prevDepth;
  e.prevDepth = currentDepth;

  // Spring physics for thrust response (drives bounce + tail coupling)
  const k = 130;
  const damp = 10;
  if (frozen) {
    e.jiggleVel = 0; e.jiggleY = 0;
  } else {
    const force = -depthDelta * 1.8;
    e.jiggleVel += (force - k * e.jiggleY - damp * e.jiggleVel) * dt;
    e.jiggleY += e.jiggleVel * dt;
  }

  // Subtle breathing bob (updateAnim3's chest swell is inherited via her3)
  const breath = Math.sin((typeof G !== 'undefined' ? G.t : 0) * 2.5) * 0.008;

  // Activity calm: at rest (no stroke, no touch, cool arousal) she lies
  // nearly still — full secondary life only while things are happening.
  // Fixes the "moving/shaking by default" complaint; thrust/pleasure wake her.
  let calm = 0.12;
  if (!frozen && typeof G !== 'undefined') {
    const depthVel = dt > 0 ? Math.abs(depthDelta) / dt : 0;
    const touching = (G.spaceHeld || G.dragOn || (G.rubT | 0) || (G.kissT | 0) || (G.oralT | 0)) ? 0.7 : 0;
    const hot = clamp((G.pleasure || 0) / 100, 0, 1) > 0.4 ? 0.3 : 0;
    const target = clamp(depthVel * 3 + touching + hot, 0, 1);
    e.calm = (e.calm == null) ? target : e.calm + (target - e.calm) * Math.min(1, dt * 3);
    calm = 0.12 + 0.88 * e.calm;
  }

  glbFollowHer3(e, dt, key);
  // Skeletal articulation AFTER root placement (her3 joints are final by
  // now — updateGLBModel runs after updateAnim3 in tick3). No-skeleton
  // models (null rig) skip silently and keep root-follow.
  if (e.rig) glbDriveRig(e, dt);
  if (!frozen) {
    e.model.position.y += breath * calm;
    glbSecondary3(e, dt, calm);
  }
  // Bed-solid collision LAST: the mattress/duvet is solid, nobody rests inside it.
  glbCollideBed(e);

  // dev only: ?showstate=1 overlays live GLB state (for headless debugging)
  try {
    if (location.search.indexOf('showstate=1') >= 0) {
      const bm = document.getElementById('bootMsg');
      if (bm) {
        bm.style.display = 'flex';
        bm.style.fontSize = '16px';
        bm.style.pointerEvents = 'none';
        bm.style.alignItems = 'flex-start';
        bm.style.justifyContent = 'flex-start';
        bm.style.textAlign = 'left';
        bm.style.padding = '60px 20px';
        const hv = (typeof her3 !== 'undefined' && her3 && her3.root) ? her3.root.visible : 'noher3';
        bm.textContent = 'preset=' + ((G && G.char && G.char.preset) || '?') +
          ' active=' + GLB_MODEL.active + ' alias=' + GLB_MODEL.key +
          ' herVis=' + hv + ' kVis=' + e.model.visible +
          ' kPos=' + e.model.position.toArray().map(v => (+v).toFixed(2)).join(',') +
          ' state=' + ((typeof G !== 'undefined' && G.state) || '?');
      }
    }
  } catch (_) {}

  // Keep the legacy alias in sync for Goat-chan-era readers
  GLB_MODEL.prevDepth = e.prevDepth;
  GLB_MODEL.jiggleY = e.jiggleY;
  GLB_MODEL.jiggleVel = e.jiggleVel;
  GLB_MODEL.model = e.model;
  GLB_MODEL.meshList = e.meshList;
  GLB_MODEL.baseScale = e.baseScale;
  GLB_MODEL.loaded = true;
}
