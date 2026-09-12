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
if (typeof window !== 'undefined') { window.GLB_STORE = GLB_STORE; window.GLB_MODEL = GLB_MODEL; }

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
  // Priority matters: 下半身 (crotch, her3.root level) beats 腰 (waist,
  // ~10 cm head-ward) — anchoring the waist parks the whole girl a head
  // width toward her head, so her hands can never reach her own body.
  let pelvisBone = null, pelvisFallback = null;
  model.traverse(c => {
    if (!c.isBone) return;
    const k = glbCleanKey(c.name);
    if (/^(下半身|hips|pelvis)$/i.test(k)) pelvisBone = pelvisBone || c;
    else if (/^腰$/i.test(k)) pelvisFallback = pelvisFallback || c;
  });
  pelvisBone = pelvisBone || pelvisFallback;
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
  arms:   { gain: 0.85, tweak: [0, 0, 0] },
  hands:  { gain: 0.85, tweak: [0, 0, 0] },
  legs:   { gain: 1.0, tweak: [0, 0, 0] },
  feet:   { gain: 0.85, tweak: [0, 0, 0] },
  breast: { gain: 0.0, tweak: [0, 0, 0] }, // preserve sculpted breast shape
  tail:   { gain: 0.0, tweak: [0, 0, 0] } // tail stays on glbSecondary3 sway
};
// Damping rate for the bone copy (1/s). Matches applyRig3's limb ease.
const GLB_DRIVE_RATE = 18;

/* Foot/ankle tuning knobs. Hoisted out of glbDriveRig so headless verify
   runs can sweep them live (window.GLB_FOOT_TUNE.ankle0 = x) instead of
   doing a code edit + reload per candidate. Defaults reproduce the
   previous hardcoded behaviour exactly (ankle0 -1.6 / ankleK 0.85 /
   dangleK 0.7). toeAbs: when true the つま先 bone is driven as an
   ABSOLUTE angle (bind replaced, not added onto) — the MMD toe bind is
   authored flipped (~±pi on Z), so adding onto it double-counts and the
   toe reads as folded back under the sole. */
var GLB_FOOT_TUNE = {
  ankle0: -1.6, ankleK: 0.85, dangleK: 0.7,
  toeK: 0.5, toeAbs: false
};

const GLB_RIG_MAP = [
  { part: 'spine',  from: 'torso',        to: '上半身' },
  { part: 'spine',  from: 'chest',        to: '上半身2' },
  { part: 'head',   from: 'neck',         to: '首' },
  // SIDE SWAP: the procedural rig labels its -X side "L", but the MMD/GLB
  // girls carry anatomical left on +X (verified: GLB 足L joint sits at +X
  // while her3 legL.hip sits at -X). Every paired joint therefore copies
  // from the OPPOSITE her3 chain — unswapped copies rotate each limb across
  // the midline (X-crossed shins). Spine/head are unpaired, unaffected.
  { part: 'arms',   from: 'armR.shoulder', to: '腕.L' },
  { part: 'arms',   from: 'armL.shoulder', to: '腕.R' },
  { part: 'arms',   from: 'armR.elbow',   to: 'ひじ.L' },
  { part: 'arms',   from: 'armL.elbow',   to: 'ひじ.R' },
  { part: 'hands',  from: 'armR.hand',    to: '手首.L' },
  { part: 'hands',  from: 'armL.hand',    to: '手首.R' },
  { part: 'legs',   from: 'legR.hip',     to: '足.L', thigh: true },
  { part: 'legs',   from: 'legL.hip',     to: '足.R', thigh: true },
  { part: 'legs',   from: 'legR.knee',    to: 'ひざ.L' },
  { part: 'legs',   from: 'legL.knee',    to: 'ひざ.R' },
  // Auxiliary leg bones the MMD author left in the skin. They are NOT in the
  // main chain — ひざALT hangs off the THIGH (not the knee) and 足捩 off
  // 下半身 — so unless we drive them they hold their bind rotation while the
  // real leg bends. Any vertex carrying their weight is then pulled between
  // two divergent transforms, which is what was stretching the sock/shin.
  // Give each the same rotation as its real counterpart so the blend is
  // coherent again. (Measured: the sock's shin verts carry ひざALT 5% +
  // 足捩 7%; before this fix the shin cross-section stretched 4.2x.)
  { part: 'legs',   from: 'legR.knee',    to: 'ひざALT.L' },
  { part: 'legs',   from: 'legL.knee',    to: 'ひざALT.R' },
  { part: 'legs',   from: 'legR.hip',     to: '足捩.L', thigh: true },
  { part: 'legs',   from: 'legL.hip',     to: '足捩.R', thigh: true },
  // sm.ankle/toes[0] belongs to procedural legL (= anatomical R = MMD R),
  // so the MMD L bones read index 1 and vice versa.
  { part: 'feet',   from: 'legR.foot',    to: '足首.L', ankle: 1 },
  { part: 'feet',   from: 'legL.foot',    to: '足首.R', ankle: 0 },
  { part: 'feet',   from: 'legR.foot',    to: 'つま先.L', toe: 1 },
  { part: 'feet',   from: 'legL.foot',    to: 'つま先.R', toe: 0 },
  { part: 'breast', from: 'breastL',      to: '乳親.L' },
  { part: 'breast', from: 'breastR',      to: '乳親.R' }
];

// Per-hand task state for finger simulation, written every frame by anim3d
// (which knows each hand's job: rubbing, cupping, weight-bearing, resting).
// Keys are MMD/anatomical sides. curl 0 = flat open, 1 = full fist;
// spread 0 = fingers together, 1 = fully fanned. Relaxed default has natural
// resting curl so hands never read as flat thin paddles.
var GLB_HAND3 = {
  L: { curl: 0.50, spread: 0.12, land: null },
  R: { curl: 0.50, spread: 0.12, land: null }
};
// Static surface offsets (world) from landmark bones to touch points.
const GLB_LAND_OFF = {
  breast: [0, 0.11, 0],
  mons: [0, 0.09, 0.06]
};
const _glbT1 = (typeof THREE !== 'undefined' && THREE.Vector3) ? new THREE.Vector3() : null;
const _glbT2 = (typeof THREE !== 'undefined' && THREE.Vector3) ? new THREE.Vector3() : null;

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
  const byClean = {};
  e.model.traverse(c => {
    if (c.isBone && c.name) byClean[glbCleanKey(c.name)] = c;
  });
  for (let i = 0; i < GLB_RIG_MAP.length; i++) {
    const m = GLB_RIG_MAP[i];
    const dst = byName[m.to] || byName[glbCleanKey(m.to)] || null;
    if (!dst) continue;
    bones.push({
      part: m.part, path: m.from, dst,
      ankle: m.ankle, toe: m.toe, thigh: m.thigh,
      bx: dst.rotation.x, by: dst.rotation.y, bz: dst.rotation.z,
      sx: dst.scale.x, sy: dst.scale.y, sz: dst.scale.z
    });
  }
  // Finger chains: each MMD finger base (stem + ０ + side, e.g. 中指０L)
  // parents 3 segments (stem + １２３). Resolve the full chain so curl
  // propagates down the finger with decay instead of freezing at the base.
  // side is the MMD/anatomical side (matches GLB_HAND3 keys); order spreads
  // the fan from index (0) to pinky (3), thumb handled separately.
  const fingers = [];
  const fingerOrder = { '人指': 0, '中指': 1, '薬指': 2, '小指': 3 };
  e.model.traverse(c => {
    if (!c.name || !c.isBone) return;
    const m = /^(.+?)０([LR])$/i.exec(glbCleanKey(c.name));
    if (!m) return;
    const stem = m[1], side = m[2].toUpperCase();
    if (!(stem in fingerOrder) && stem !== '親指') return;
    const full = ['０', '１', '２', '３'].map(function (d) {
      const n = byClean[stem + d + side];
      if (!n) return null;
      return { dst: n, bx: n.rotation.x, by: n.rotation.y, bz: n.rotation.z };
    });
    if (!full[0]) return;
    fingers.push({
      base: full[0], segs: full.slice(1), side: side,
      order: (stem in fingerOrder) ? fingerOrder[stem] : -1,
      thumb: stem === '親指'
    });
  });
  e.rig = (bones.length || fingers.length) ? { bones, fingers } : null;
  if (e.rig) {
    // Arm IK handles: shoulder/elbow/wrist refs + segment lengths (measured
    // in bind, model units) so the MMD arms can run the same two-bone IK as
    // the procedural rig instead of euler-copying across different rests.
    // MMD arm chains run +Y (measured offsets), unlike procedural -Y.
    e.rig.arms = {};
    ['L', 'R'].forEach(function (s) {
      const sh = byClean['腕' + s], el = byClean['ひじ' + s], wr = byClean['手首' + s];
      if (sh && el && wr) {
        e.rig.arms[s] = {
          sh: sh, el: el, wr: wr,
          upper: el.position.length(), fore: wr.position.length()
        };
      }
    });
    // Touch landmarks: her3-anchored IK targets above the pelvis miss the
    // short-torsoed MMD body by ~20 cm, so targets on her own torso/body get
    // re-anchored onto these bones (plus GLB_LAND_OFF) every frame.
    e.rig.land = {
      pelvis: byClean['下半身'] || null,
      breastL: byClean['乳親L'] || null,
      breastR: byClean['乳親R'] || null
    };
  }
}

/* Drive the mapped skeleton from the live her3 joints. Runs AFTER
   glbFollowHer3 (root placement) inside updateGLBModel — which itself runs
   after updateAnim3, so every her3 joint is final for this frame. */
const _gdZero = [0, 0, 0];
// Last-seen IK ages per procedural hand: when anim3d's slewed IK target age
// advances, that arm is IK-owned this frame (rub or pose plant) and the MMD
// arm runs real two-bone IK to the same target instead of euler-copying.
const _glbArmAge = { herL: -1, herR: -1 };
function glbDriveRig(e, dt) {
  if (!e || !e.rig || (!e.rig.bones.length && !e.rig.fingers.length)) return;
  const rate = (dt > 0) ? (1 - Math.exp(-GLB_DRIVE_RATE * dt)) : 1;
  const rig = e.rig;
  const sm = (typeof _smPose3 !== 'undefined' && _smPose3.her) || null;

  // Ankle rest calibration: the MMD ankle bind (-0.75) points the foot
  // steeply down while her3's ankle 0 is ~neutral, so the smoothed pose value
  // maps through an affine rest pose instead of adding onto the bind.
  // Dangle adds passive plantarflexion with knee bend (a bent, unsupported
  // knee lets the foot drop; an extended resting leg keeps toes up).
  // Hoisted to GLB_FOOT_TUNE (module scope) so verify runs can sweep these
  // live instead of round-tripping a code edit per candidate value.
  const GLB_ANKLE0 = GLB_FOOT_TUNE.ankle0,
        GLB_ANKLE_K = GLB_FOOT_TUNE.ankleK,
        GLB_DANGLE_K = GLB_FOOT_TUNE.dangleK;

  // Arms under live IK (rub/pose plant) skip the euler copy — the IK pass
  // below aims them at the same slewed world target as the procedural rig.
  // Ownership is inferred from the target's freshness stamp (anim3d bumps
  // _aimSm3.age on every IK write; updateAnim3 always runs before us).
  // Snapshot once per frame: the bone loop visits two entries per arm.
  const _glbArmIK = { herL: false, herR: false };
  if (typeof _aimSm3 !== 'undefined' && _aimSm3.age) {
    ['herL', 'herR'].forEach(function (hk) {
      const age = _aimSm3.age[hk];
      _glbArmIK[hk] = (age !== undefined && age !== _glbArmAge[hk]);
      _glbArmAge[hk] = age;
    });
  }

  for (let i = 0; i < rig.bones.length; i++) {
    const b = rig.bones[i];
    const src = glbSrcJoint(b.path);
    if (!src) continue;
    const P = GLB_PARTS[b.part] || GLB_PARTS.spine;
    const g = (P.gain == null) ? 1 : P.gain;
    if (g === 0) continue;
    const tw = P.tweak || _gdZero;

    let sx = src.rotation.x, sy = src.rotation.y, sz = src.rotation.z;
    let absolute = false; // absolute replaces bind; default adds onto it
    if (b.ankle === 0 || b.ankle === 1) {
      const smA = (sm && sm.ankle && (sm.ankle[b.ankle] || 0)) || 0;
      const kneeSrc = glbSrcJoint(b.path.replace('.foot', '.knee'));
      const dangle = clamp(((kneeSrc && kneeSrc.rotation.x) || 0) - 0.5, 0, 1.4) * GLB_DANGLE_K;
      sx = GLB_ANKLE0 + smA * GLB_ANKLE_K + dangle; sy = 0; sz = 0;
      absolute = true;
    } else if (b.toe === 0 || b.toe === 1) {
      const a = (sm && sm.toes && (sm.toes[b.toe] || 0)) || 0;
      sx = a * GLB_FOOT_TUNE.toeK; sy = 0; sz = 0;
      // MMD authors つま先 with a flipped bind (Z ≈ ±pi): adding onto it
      // folds the toe under the sole. Replacing the bind (absolute) keeps
      // the toe aligned with the ankle's plantarflexion.
      if (GLB_FOOT_TUNE.toeAbs) absolute = true;
    } else if (b.part === 'hands') {
      // Wrist stays near-neutral: it rides the forearm, so only a whisper of
      // the elbow bend transfers (full coupling used to fold hands back
      // against the forearm, reading as thin flat flaps from above).
      const elb = glbSrcJoint(b.path.replace('.hand', '.elbow'));
      sx = elb ? elb.rotation.x * 0.12 : 0; sy = 0; sz = 0;
    } else if (b.part === 'arms' && /ひじ/.test(glbCleanKey(b.dst.name))) {
      // Elbow hinge flexion
      sx = src.rotation.x; sy = 0; sz = 0;
    } else if (b.part === 'legs' && b.thigh) {
      // Thigh ABSOLUTE + mirrored spread. The src euler is an absolute joint
      // angle while the MMD bind is a different rest pose (A-stance z=±0.135),
      // so adding double-counts rest and overspreads 1.5x. And her3 +Z splays
      // a +X-side limb outward while MMD +Z adducts it (opposite axis
      // conventions, measured on the rig), so spread copies negated.
      sx = src.rotation.x; sy = 0; sz = -src.rotation.z;
      absolute = true;
    } else if (b.part === 'legs') {
      // Knee flexion: positive X bends backward naturally in MMD
      sx = src.rotation.x; sy = 0; sz = 0;
    }

    // Breasts: keep bind pose rotation to avoid pitch distortion
    if (b.path === 'breastL' || b.path === 'breastR') {
      sx = 0; sy = 0; sz = 0;
    }

    const tx = (absolute ? 0 : b.bx) + sx * g + (tw[0] || 0);
    const ty = (absolute ? 0 : b.by) + sy * g + (tw[1] || 0);
    const tz = (absolute ? 0 : b.bz) + sz * g + (tw[2] || 0);
    const r = b.dst.rotation;

    let ikOwned = false;
    if (b.part === 'arms') {
      ikOwned = _glbArmIK[/^armL\./.test(b.path) ? 'herL' : 'herR'];
    }
    if (!ikOwned) {
      // dt<=0 (paused/headless probe) snaps — still deterministic, no NaN risk.
      r.x = (rate >= 1) ? tx : r.x + (tx - r.x) * rate;
      r.y = (rate >= 1) ? ty : r.y + (ty - r.y) * rate;
      r.z = (rate >= 1) ? tz : r.z + (tz - r.z) * rate;
    }

    // Optional per-part bone scale (e.g. breast size); always written so
    // resetting to 1 restores the authored bind scale instead of sticking.
    const ps = P.scale || 1;
    b.dst.scale.set(b.sx * ps, b.sy * ps, b.sz * ps);
  }

  // Fingers: task-driven curl + fan. Curl (local X, the flexion axis found
  // by probing the rig) propagates base→tip with decay so the whole finger
  // bends instead of kinking at the knuckle; fan (local Z) opens the four
  // fingers from index to pinky so the hand reads volumetric, never a flat
  // paddle. The rubbing hand ripples gently with the stroke rhythm.
  const PH = GLB_PARTS.hands;
  if (PH && PH.gain !== 0 && rig.fingers.length) {
    const rubT = (typeof G !== 'undefined' && G.t) || 0;
    for (let i = 0; i < rig.fingers.length; i++) {
      const f = rig.fingers[i];
      if (!f.base) continue;
      const task = (typeof GLB_HAND3 !== 'undefined' && GLB_HAND3[f.side]) || { curl: 0.50, spread: 0.12 };
      let curl = clamp(task.curl || 0, 0, 1);
      if (task.rub) curl += Math.sin(rubT * 12) * 0.08;
      curl = clamp(curl, 0, 1) * (PH.gain == null ? 1 : PH.gain);
      const spread = clamp(task.spread || 0, 0, 1) * (PH.gain == null ? 1 : PH.gain);
      const tw = PH.tweak || _gdZero;
      const curlK = f.thumb ? 0.55 : 1.0;
      const fan = (f.order < 0) ? 0 : (f.order - 1.5) * spread * 0.14;
      const chain = [f.base].concat(f.segs);
      const decay = [0.95, 0.75, 0.55, 0.38];
      for (let s = 0; s < chain.length; s++) {
        const seg = chain[s];
        if (!seg || !seg.dst) continue;
        const k = decay[Math.min(s, decay.length - 1)] * curlK;
        const tx2 = seg.bx + curl * 0.85 * k + (s === 0 ? (tw[0] || 0) : 0);
        const tz2 = seg.bz + (s === 0 ? fan : fan * 0.4) + (s === 0 ? (tw[2] || 0) : 0);
        const r = seg.dst.rotation;
        if (rate >= 1) { r.x = tx2; r.z = tz2; }
        else { r.x = r.x + (tx2 - r.x) * rate; r.z = r.z + (tz2 - r.z) * rate; }
      }
    }
  }

  // Arm IK: aim each IK-owned MMD arm at the same slewed world target the
  // procedural rig is tracking (rub + pose plants). Same two-bone primitive,
  // just with the MMD rest direction (+Y) and measured segment lengths —
  // exact tracking instead of euler-copying across different rest frames.
  // Procedural herL/herR drive MMD R/L (side swap, see RIG_MAP).
  if (rig.arms && typeof _aimSm3 !== 'undefined' && typeof aimArmAt3 === 'function') {
    const pairs = [['herL', 'R'], ['herR', 'L']];
    for (let a = 0; a < pairs.length; a++) {
      const hk = pairs[a][0], ms = pairs[a][1];
      if (!_glbArmIK[hk]) continue;
      const A = rig.arms[ms];
      const tgt = _aimSm3[hk];
      if (!A || !tgt || !tgt.isVector3) continue;
      // Re-anchor torso/body targets onto GLB anatomy: the MMD girl's torso
      // is much shorter than her3's, so her3-anchored breast/mons targets
      // float ~20 cm toward her head. Corrected = target + (GLB landmark −
      // her3 landmark), recomputed live every frame. World/floor targets
      // (bed, his body) stay exact — both rigs share that space.
      let aimTgt = tgt;
      const task = (typeof GLB_HAND3 !== 'undefined' && GLB_HAND3[ms]) || null;
      const land = task && task.land;
      if (land && rig.land && _glbT1 && _glbT2 && typeof her3 !== 'undefined' && her3) {
        let hb = null, mb = null, off = null;
        if (land === 'mons') {
          mb = rig.land.pelvis; off = GLB_LAND_OFF.mons;
          try { _glbT2.copy(clitorisWorld3(her3)); hb = true; } catch (_) { hb = false; }
        } else if (land === 'breast') {
          const leftish = tgt.x < 0;
          mb = leftish ? rig.land.breastR : rig.land.breastL;
          off = GLB_LAND_OFF.breast;
          try { _glbT2.copy(breastWorld3(her3, leftish ? -1 : 1)); hb = true; } catch (_) { hb = false; }
        }
        if (hb && mb) {
          mb.updateWorldMatrix(true, false);
          _glbT1.setFromMatrixPosition(mb.matrixWorld);
          _glbT1.x += off[0]; _glbT1.y += off[1]; _glbT1.z += off[2];
          _glbT1.sub(_glbT2).add(tgt);
          aimTgt = _glbT1;
        }
      }
      const bias = (hk === 'herR') ? 0.22 : -0.22;
      aimArmAt3({ shoulder: A.sh, elbow: A.el }, aimTgt,
        A.upper, A.fore, bias, 30, dt, 20, _glbYAxis);
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
      glbSetScale('breast', preset === 'goatchan' ? 1.0 : (0.65 + G.char.breastSize * 0.9));
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
  if (!m || e.rig) return; // Rigged models have their pelvis anchored to her3.root directly
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
