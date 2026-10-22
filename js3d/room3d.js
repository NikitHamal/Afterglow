// Afterglow 3D — module: room3d (lighting rig, bedroom set, atmosphere)
// Loaded by 3d.html after engine3d.js. Owns buildLights() / buildRoom3()
// (called from initEngine3) and updateRoom3(dt) (called every frame).
'use strict';

/* ============================================================
   SET DIMENSIONS — everything is scaled to the ~1.65-unit-tall rigs.
   The mattress top sits at y = 0.11 (anim3d's BED_FLOOR contract).
   ============================================================ */
const ROOM3 = {
  bedTop: 0.11, floorY: -0.55,
  bedW: 2.3, bedZ0: -1.86, bedZ1: 1.30,    // mattress footprint (x: ±bedW/2)
  backZ: -2.2, leftX: -2.75, rightX: 2.85,
  win: { z: -0.55, y: 1.25, w: 1.35, h: 1.55 },
  candles: [], flames: [], curtains: [], dust: null, beams: [],
  lampLight: null, candleLightL: null, candleLightR: null, shade: null, t: 0
};
let moonLight3 = null;

/* ============================================================
   LIGHTING — warm lamp key, cool moon rim, candles, soft bounce
   ============================================================ */
function buildLights() {
  scene3d.background = new THREE.Color(0x050307);
  scene3d.fog = new THREE.FogExp2(0x0a0508, 0.045);

  scene3d.add(new THREE.AmbientLight(0x2a1420, 0.25));
  scene3d.add(new THREE.HemisphereLight(0xffc8b0, 0x12060a, 0.22));

  // key: the bedside lamp rakes low across the bed from the left
  keyLight3 = new THREE.DirectionalLight(0xffa463, 1.35);
  keyLight3.position.set(-3.4, 2.7, -0.4);
  keyLight3.target.position.set(0, 0.2, -0.3);
  keyLight3.castShadow = true;
  keyLight3.shadow.mapSize.set(2048, 2048);
  keyLight3.shadow.camera.near = 0.5;
  keyLight3.shadow.camera.far = 9;
  const sc = keyLight3.shadow.camera;
  sc.left = -2.4; sc.right = 2.4; sc.top = 2.2; sc.bottom = -2.2;
  keyLight3.shadow.bias = -0.0004;
  keyLight3.shadow.normalBias = 0.02;
  keyLight3.shadow.radius = 4;
  scene3d.add(keyLight3); scene3d.add(keyLight3.target);

  // moonlight through the window: cool, crisp rim on the far flank
  moonLight3 = rimLight3 = new THREE.DirectionalLight(0x86a8ff, 1.1);
  rimLight3.position.set(4.5, 3.0, -2.6);
  rimLight3.target.position.set(0, 0.3, -0.2);
  scene3d.add(rimLight3); scene3d.add(rimLight3.target);

  // camera-side fill keeps the shadows plum instead of black
  fillLight3 = new THREE.DirectionalLight(0xff7f9a, 0.32);
  fillLight3.position.set(1.5, 1.6, 4.5);
  scene3d.add(fillLight3);

  // practical lamp bulb
  lampPoint3 = new THREE.PointLight(0xff9a4a, 1.6, 5.5, 2);
  lampPoint3.position.set(-1.72, 0.92, -1.62);
  scene3d.add(lampPoint3);
  ROOM3.lampLight = lampPoint3;

  // candle clusters (flicker in updateRoom3)
  ROOM3.candleLightR = new THREE.PointLight(0xff8a3a, 1.2, 3.2, 2);
  ROOM3.candleLightR.position.set(1.72, 0.35, -1.55);
  scene3d.add(ROOM3.candleLightR);
  ROOM3.candleLightL = new THREE.PointLight(0xff8a3a, 0.8, 2.6, 2);
  ROOM3.candleLightL.position.set(-2.2, -0.25, 0.9);
  scene3d.add(ROOM3.candleLightL);
}

/* ============================================================
   ROOM SET
   ============================================================ */
function buildRoom3() {
  const R3 = ROOM3, g = new THREE.Group();
  g.name = 'room3';
  const fy = R3.floorY;

  /* ---- floor + rug ---- */
  const floor = new THREE.Mesh(new THREE.PlaneGeometry(14, 14),
    _mat3({ map: woodFloorTex3(), roughness: 0.42, clearcoat: 0.35, clearcoatRoughness: 0.35, color: 0xb59a8c }));
  floor.material.map.repeat.set(4, 4);
  floor.rotation.x = -Math.PI / 2; floor.position.y = fy;
  floor.receiveShadow = true; g.add(floor);

  const rug = new THREE.Mesh(new THREE.PlaneGeometry(3.4, 4.25),
    _mat3({ map: rugTex3(), roughness: 0.95, sheen: 0.6, sheenColor: new THREE.Color(0xff9a8a), sheenRoughness: 0.8 }));
  rug.rotation.x = -Math.PI / 2; rug.position.set(0, fy + 0.006, 0.05);
  rug.receiveShadow = true; g.add(rug);

  /* ---- walls: single-sided so the orbit camera can look in from outside ---- */
  const wpTex = wallpaperTex3();
  const wallMat = (rx, ry) => {
    const t = wpTex.clone(); t.needsUpdate = true; t.repeat.set(rx, ry);
    return _mat3({ map: t, roughness: 0.92 });
  };
  const wsTex = wainscotTex3();
  const wainMat = rx => { const t = wsTex.clone(); t.needsUpdate = true; t.repeat.set(rx, 1); return _mat3({ map: t, roughness: 0.6, clearcoat: 0.2 }); };
  const wallH = 3.6, wainH = 0.85;
  const addWall = (len, cx, cz, rotY, holes) => {
    const wg = new THREE.Group();
    wg.position.set(cx, fy, cz); wg.rotation.y = rotY;
    const upper = wallH - wainH;
    const pieces = holes ? holes : [[-len / 2, len / 2, wainH, wallH]];
    for (const [x0, x1, y0, y1] of pieces) {
      const w = x1 - x0, h = y1 - y0;
      const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), wallMat(w / 1.1, h / 1.1));
      m.position.set((x0 + x1) / 2, (y0 + y1) / 2, 0); m.receiveShadow = true; wg.add(m);
    }
    const wain = new THREE.Mesh(new THREE.PlaneGeometry(len, wainH), wainMat(len / 1.3));
    wain.position.set(0, wainH / 2, 0.012); wain.receiveShadow = true; wg.add(wain);
    // chair rail + skirting
    const trim = _mat3({ color: 0x2a121a, roughness: 0.45, clearcoat: 0.5 });
    const rail = new THREE.Mesh(roundedBox3(len, 0.06, 0.05, 0.015, 2), trim);
    rail.position.set(0, wainH, 0.025); _shadow3(rail, false); wg.add(rail);
    const skirt = new THREE.Mesh(roundedBox3(len, 0.14, 0.04, 0.01, 2), trim);
    skirt.position.set(0, 0.07, 0.03); wg.add(skirt);
    void upper;
    g.add(wg);
    return wg;
  };
  addWall(6.4, 0.05, R3.backZ, 0);
  addWall(6.0, R3.leftX, 0.6, Math.PI / 2);
  // right wall with a window hole (wall-local x runs toward -z because of the rotation)
  const W = R3.win, wy0 = W.y - fy - W.h / 2, wy1 = W.y - fy + W.h / 2;
  const wx = W.z - 0.6;                                     // window centre in wall-local x
  addWall(6.0, R3.rightX, 0.6, -Math.PI / 2, [
    [-3, wx - W.w / 2, wainH, wallH], [wx + W.w / 2, 3, wainH, wallH],
    [wx - W.w / 2, wx + W.w / 2, wy1, wallH], [wx - W.w / 2, wx + W.w / 2, wainH, wy0]
  ]);

  /* ---- window: night sky, frame, mullions, sill ---- */
  const sky = new THREE.Mesh(new THREE.PlaneGeometry(4.2, 3.2),
    new THREE.MeshBasicMaterial({ map: nightSkyTex3(), fog: false }));
  sky.material.color.setScalar(1.6);
  sky.rotation.y = -Math.PI / 2; sky.position.set(R3.rightX + 1.2, W.y + 0.2, W.z); g.add(sky);
  const frameMat = _mat3({ color: 0x1a0c12, roughness: 0.4, clearcoat: 0.6 });
  const fb = (sx, sy, sz, x, y, z) => { const m = new THREE.Mesh(roundedBox3(sx, sy, sz, Math.min(sx, sy, sz) * 0.3, 1), frameMat); m.position.set(x, y, z); g.add(m); };
  const fx = R3.rightX - 0.02;
  fb(0.12, 0.07, W.w + 0.14, fx, W.y + W.h / 2 + 0.03, W.z);
  fb(0.12, 0.07, W.w + 0.14, fx, W.y - W.h / 2 - 0.03, W.z);
  fb(0.12, W.h + 0.12, 0.07, fx, W.y, W.z - W.w / 2 - 0.03);
  fb(0.12, W.h + 0.12, 0.07, fx, W.y, W.z + W.w / 2 + 0.03);
  fb(0.06, W.h, 0.04, fx, W.y, W.z);
  fb(0.06, 0.04, W.w, fx, W.y + 0.12, W.z);
  fb(0.26, 0.05, W.w + 0.34, fx - 0.08, W.y - W.h / 2 - 0.07, W.z);   // sill
  const glass = new THREE.Mesh(new THREE.PlaneGeometry(W.w, W.h),
    _mat3({ color: 0x8aa0c8, roughness: 0.05, transmission: 0, transparent: true, opacity: 0.12, clearcoat: 1, depthWrite: false }));
  glass.rotation.y = -Math.PI / 2; glass.position.set(R3.rightX + 0.01, W.y, W.z); g.add(glass);

  /* ---- curtains: heavy velvet, folded, gently breathing ---- */
  const curtainMat = _mat3({ color: 0x4a0f1e, roughness: 0.85, sheen: 1.0, sheenColor: new THREE.Color(0xff6a7a), sheenRoughness: 0.45, side: THREE.DoubleSide });
  for (const side of [-1, 1]) {
    const cw = 0.75, ch = W.y + W.h / 2 + 0.35 - fy;
    const geo = new THREE.PlaneGeometry(cw, ch, 36, 30);
    const m = new THREE.Mesh(geo, curtainMat);
    m.rotation.y = -Math.PI / 2;
    const cz = W.z + side * (W.w / 2 + cw / 2 - 0.12);
    m.position.set(R3.rightX - 0.12, fy + ch / 2, cz);
    _shadow3(m, true, true);
    g.add(m);
    R3.curtains.push({ mesh: m, base: Float32Array.from(geo.attributes.position.array), side, ph: side * 1.7 });
  }
  const rod = new THREE.Mesh(new THREE.CylinderGeometry(0.018, 0.018, W.w + 1.9, 10), _mat3({ color: 0xb58a4a, metalness: 0.9, roughness: 0.35 }));
  rod.rotation.x = Math.PI / 2; rod.position.set(R3.rightX - 0.13, W.y + W.h / 2 + 0.35, W.z); g.add(rod);

  /* ---- bed ---- */
  buildBed3(g);
  /* ---- nightstands, lamp, candles ---- */
  buildFurniture3(g);
  /* ---- light shafts + dust ---- */
  buildAtmosphere3(g);

  scene3d.add(g);
  R3.group = g;
  return g;
}

function buildBed3(g) {
  const R3 = ROOM3, top = R3.bedTop, fy = R3.floorY;
  const bw = R3.bedW, bl = R3.bedZ1 - R3.bedZ0, bz = (R3.bedZ0 + R3.bedZ1) / 2;
  const woodMat = _mat3({ color: 0x2b130c, roughness: 0.38, clearcoat: 0.7, clearcoatRoughness: 0.25 });
  const sheetMat = _mat3({ color: 0x3a0f1a, roughness: 0.5, sheen: 0.6, sheenColor: new THREE.Color(0x8a3a4a), sheenRoughness: 0.4, map: weaveTex3('#c8c0bc', 3), clearcoat: 0.08 });

  // frame + legs
  const frameH = 0.22, frameTop = top - 0.24;
  const frame = new THREE.Mesh(roundedBox3(bw + 0.16, frameH, bl + 0.12, 0.04, 3), woodMat);
  frame.position.set(0, frameTop - frameH / 2, bz); _shadow3(frame); g.add(frame);
  for (const sx of [-1, 1]) for (const sz of [0, 1]) {
    const leg = new THREE.Mesh(new THREE.CylinderGeometry(0.045, 0.035, frameTop - frameH - fy, 12), woodMat);
    leg.position.set(sx * (bw / 2 - 0.02), (frameTop - frameH + fy) / 2, sz ? R3.bedZ1 - 0.02 : R3.bedZ0 + 0.02);
    _shadow3(leg); g.add(leg);
  }

  // mattress (dressed in the fitted sheet)
  const mh = 0.26;
  const mat = new THREE.Mesh(roundedBox3(bw, mh, bl, 0.07, 4), sheetMat);
  mat.position.set(0, top - mh / 2, bz); _shadow3(mat, false, true); g.add(mat);

  // rumpled top sheet: a dense plane that wrinkles at the margins and spills
  // over the sides into hanging folds (the middle stays flat under the bodies)
  const ow = 0.28, sw = bw + ow * 2, sl = bl + ow - 0.35;
  const sg = new THREE.PlaneGeometry(sw, sl, 96, 120);        // ~23k tris (was 36k) — folds still read
  sg.rotateX(-Math.PI / 2);
  const p = sg.attributes.position, rnd = _rng3(31);
  const blobs = [];
  for (let i = 0; i < 14; i++) blobs.push([(rnd() - 0.5) * bw, (rnd() - 0.5) * sl, 0.08 + rnd() * 0.2, rnd() * TAU]);
  for (let i = 0; i < p.count; i++) {
    const x = p.getX(i), z = p.getZ(i);
    const ax = Math.abs(x) - bw / 2, edge = Math.max(0, Math.min(1, (Math.abs(x) - 0.45) / 0.55));
    const footEdge = Math.max(0, Math.min(1, (z - (sl / 2 - 0.9)) / 0.6));
    let y = 0.006;
    // wrinkles grow toward the margins, radiating from a few pinch points
    let wr = 0;
    // creases fan out from pinch points: parallel folds across a direction,
    // dying off along it — fabric, not water ripples
    for (const [bx, bzz, r, ph] of blobs) {
      const dx = x - bx, dz = z - bzz, ca = Math.cos(ph), sa = Math.sin(ph);
      const along = dx * ca + dz * sa, perp = -dx * sa + dz * ca;
      const spread = r * (0.35 + Math.abs(along) * 0.6);
      wr += Math.pow(Math.abs(Math.sin(perp / spread * 4.5)), 3) * Math.exp(-Math.abs(along) / (r * 2.2)) * Math.exp(-perp * perp / (spread * spread * 2.5)) * 1.4;
    }
    wr += Math.sin(x * 5 + z * 2.1) * 0.12 + Math.sin(z * 7 - x * 3) * 0.1;
    y += wr * 0.011 * Math.max(edge, footEdge * 0.9);
    if (ax > 0) {                                   // spill over the side
      const drop = Math.min(ax, ow) / ow;
      y -= Math.sin(drop * Math.PI / 2) * 0.33;
      const fold = Math.sin(z * 15 + rnd() * 0.2) * 0.022 * drop;
      p.setX(i, Math.sign(x) * (bw / 2 + Math.sin(drop * Math.PI / 2) * 0.05 + 0.012 + fold));
    }
    p.setY(i, top + y);
    p.setZ(i, z + bz + 0.175);
  }
  sg.computeVertexNormals();
  const sheet = new THREE.Mesh(sg, sheetMat.clone());
  sheet.material.side = THREE.DoubleSide;
  sheet.material.map = sheetMat.map;
  _shadow3(sheet, false, true); g.add(sheet);

  // headboard: tall tufted velvet panel in a wood surround
  const hbW = bw + 0.3, hbH = 1.25, hbZ = R3.bedZ0 - 0.12;
  const hbg = roundedBox3(hbW, hbH, 0.14, 0.05, 8);
  // re-segment the front with a tufting field: dimples on a diamond grid
  const hbDense = new THREE.PlaneGeometry(hbW - 0.16, hbH - 0.16, 104, 60);   // ~12k tris (was 22k)
  const hp = hbDense.attributes.position;
  const sx = 0.24, sy = 0.21;
  for (let i = 0; i < hp.count; i++) {
    const x = hp.getX(i), y = hp.getY(i);
    const u = x / sx, v = y / sy;
    const du = u - Math.round(u), dv = v - Math.round(v);
    const du2 = u + 0.5 - Math.round(u + 0.5), dv2 = v + 0.5 - Math.round(v + 0.5);
    const d1 = Math.hypot(du, dv), d2 = Math.hypot(du2, dv2);
    const d = Math.min(d1, d2);
    const pillow = 0.05 * (1 - Math.exp(-d * d * 18));
    const ex = 1 - Math.pow(Math.abs(x) / ((hbW - 0.16) / 2), 6), ey = 1 - Math.pow(Math.abs(y) / ((hbH - 0.16) / 2), 6);
    hp.setZ(i, pillow * Math.max(0, ex) * Math.max(0, ey));
  }
  hbDense.computeVertexNormals();
  const velvet = _mat3({ color: 0x3e1226, roughness: 0.9, sheen: 1, sheenColor: new THREE.Color(0xff7a9a), sheenRoughness: 0.35 });
  const hbBack = new THREE.Mesh(hbg, woodMat);
  const hbY = top - 0.12 + hbH / 2;
  hbBack.position.set(0, hbY, hbZ); _shadow3(hbBack); g.add(hbBack);
  const tuft = new THREE.Mesh(hbDense, velvet);
  tuft.position.set(0, hbY, hbZ + 0.071); _shadow3(tuft, false, true); g.add(tuft);
  const btnMat = _mat3({ color: 0x2a0a18, roughness: 0.3, clearcoat: 1 });
  const btnGeo = new THREE.SphereGeometry(0.011, 8, 6);
  for (let ix = -6; ix <= 6; ix++) for (let iy = -3; iy <= 3; iy++) {
    for (const o of [0, 0.5]) {
      const x = (ix + o) * sx, y = (iy + o) * sy;
      if (Math.abs(x) > (hbW - 0.3) / 2 || Math.abs(y) > (hbH - 0.3) / 2) continue;
      const b = new THREE.Mesh(btnGeo, btnMat);
      b.position.set(x, hbY + y, hbZ + 0.075); g.add(b);
    }
  }

  // pillows: puffed rounded boxes, slightly squashed where heads rest
  const pillowMat = _mat3({ color: 0xb59a92, roughness: 0.65, sheen: 0.5, sheenColor: new THREE.Color(0xd8c0b8), sheenRoughness: 0.4, map: weaveTex3('#ffffff', 11) });
  const mkPillow = (w, h, d, x, y, z, rx, ry, rz, mat) => {
    const geo = roundedBox3(w, h, d, Math.min(h * 0.48, 0.06), 6);
    const pp = geo.attributes.position;
    for (let i = 0; i < pp.count; i++) {
      const px = pp.getX(i) / (w / 2), pz = pp.getZ(i) / (d / 2);
      const puff = (1 - px * px * 0.75) * (1 - pz * pz * 0.75);
      pp.setY(i, pp.getY(i) * (0.35 + 0.75 * puff));
      // wavy seam pull at the corners
      pp.setX(i, pp.getX(i) * (1 - 0.06 * Math.abs(pz) * Math.abs(pz)));
    }
    geo.computeVertexNormals();
    const m = new THREE.Mesh(geo, mat || pillowMat);
    m.position.set(x, y, z); m.rotation.set(rx, ry, rz); _shadow3(m); g.add(m);
    return m;
  };
  mkPillow(0.78, 0.16, 0.46, -0.5, top + 0.05, R3.bedZ0 + 0.3, 0.05, 0.06, -0.03);
  mkPillow(0.78, 0.16, 0.46, 0.5, top + 0.05, R3.bedZ0 + 0.28, 0.04, -0.05, 0.03);
}

function buildFurniture3(g) {
  const R3 = ROOM3, fy = R3.floorY;
  const woodMat = _mat3({ color: 0x2b130c, roughness: 0.36, clearcoat: 0.75, clearcoatRoughness: 0.22 });
  const brass = _mat3({ color: 0xc9964a, metalness: 1, roughness: 0.3 });

  const stand = (x, z) => {
    const sw = 0.58, sh = 0.62, sd = 0.48;
    const body = new THREE.Mesh(roundedBox3(sw, sh, sd, 0.025, 2), woodMat);
    body.position.set(x, fy + sh / 2 + 0.06, z); _shadow3(body); g.add(body);
    const drawer = new THREE.Mesh(roundedBox3(sw - 0.08, 0.2, 0.02, 0.008, 2), woodMat);
    drawer.position.set(x, fy + sh - 0.1, z + sd / 2 + 0.006); g.add(drawer);
    const drawer2 = drawer.clone(); drawer2.position.y -= 0.24; g.add(drawer2);
    for (const d of [drawer, drawer2]) {
      const k = new THREE.Mesh(new THREE.SphereGeometry(0.018, 12, 8), brass);
      k.position.set(x, d.position.y, z + sd / 2 + 0.03); g.add(k);
    }
    for (const sx of [-1, 1]) for (const sz of [-1, 1]) {
      const leg = new THREE.Mesh(new THREE.CylinderGeometry(0.02, 0.014, 0.08, 8), woodMat);
      leg.position.set(x + sx * (sw / 2 - 0.05), fy + 0.04, z + sz * (sd / 2 - 0.05)); g.add(leg);
    }
    return fy + sh + 0.06;
  };

  // left stand + lamp
  const lx = -1.72, lz = -1.62, topL = stand(lx, lz);
  const basePts = [];
  for (let i = 0; i <= 16; i++) {
    const t = i / 16;
    basePts.push(new THREE.Vector2(0.03 + Math.sin(t * Math.PI) * 0.085 + (t < 0.08 ? 0.04 : 0), t * 0.3));
  }
  const lampBase = new THREE.Mesh(new THREE.LatheGeometry(basePts, 32),
    _mat3({ color: 0x1b2a2a, roughness: 0.18, clearcoat: 1, metalness: 0.1 }));
  lampBase.position.set(lx, topL, lz); _shadow3(lampBase); g.add(lampBase);
  const neck = new THREE.Mesh(new THREE.CylinderGeometry(0.008, 0.008, 0.12, 8), brass);
  neck.position.set(lx, topL + 0.36, lz); g.add(neck);
  const shadeMat = new THREE.MeshPhysicalMaterial({
    color: 0xf4d6b0, emissive: 0xff9442, emissiveIntensity: 1.25, roughness: 0.9,
    side: THREE.DoubleSide, transparent: true, opacity: 0.94
  });
  const shade = new THREE.Mesh(new THREE.CylinderGeometry(0.15, 0.24, 0.26, 40, 1, true), shadeMat);
  shade.position.set(lx, topL + 0.48, lz); g.add(shade);
  ROOM3.shade = shade;
  lampPoint3.position.set(lx, topL + 0.45, lz);
  // hot spill rings top & bottom of the shade
  const bulbGlow = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowSprite3('rgba(255,236,200,1)', 'rgba(255,150,70,0.35)'), blending: THREE.AdditiveBlending, depthWrite: false, transparent: true, fog: false }));
  bulbGlow.scale.set(0.9, 0.9, 1); bulbGlow.position.set(lx, topL + 0.47, lz); g.add(bulbGlow);
  ROOM3.bulbGlow = bulbGlow;

  // book stack + glass on the left stand
  const bookCols = [0x4a1a22, 0x1e2a3a, 0x6a4a22];
  for (let i = 0; i < 3; i++) {
    const b = new THREE.Mesh(roundedBox3(0.2 - i * 0.015, 0.035, 0.14, 0.006, 1), _mat3({ color: bookCols[i], roughness: 0.7 }));
    b.position.set(lx + 0.13, topL + 0.018 + i * 0.036, lz + 0.1); b.rotation.y = 0.3 + i * 0.25; _shadow3(b); g.add(b);
  }

  // right stand + candle cluster
  const rx = 1.72, rz = -1.6, topR = stand(rx, rz);
  addCandles3(g, rx, topR, rz, [[0, 0, 0.16], [0.11, 0.06, 0.11], [-0.1, 0.07, 0.08]]);
  ROOM3.candleLightR.position.set(rx, topR + 0.3, rz + 0.05);
  // floor candles in a brass tray by the foot of the bed (left)
  const tray = new THREE.Mesh(new THREE.CylinderGeometry(0.2, 0.2, 0.015, 32), brass);
  tray.position.set(-2.1, fy + 0.008, 0.95); g.add(tray);
  addCandles3(g, -2.1, fy + 0.016, 0.95, [[0, 0, 0.22], [0.1, 0.07, 0.14], [-0.08, 0.09, 0.1], [0.04, -0.11, 0.07]]);
  ROOM3.candleLightL.position.set(-2.1, fy + 0.38, 0.95);
}

function addCandles3(g, x, y, z, list) {
  const wax = _mat3({ color: 0xf0e2c8, roughness: 0.55, transmission: 0, sheen: 0.4, sheenColor: new THREE.Color(0xffd0a0), emissive: 0xff7a2a, emissiveIntensity: 0.06 });
  const flameTex = glowSprite3('rgba(255,250,220,1)', 'rgba(255,140,40,0.55)');
  for (const [dx, dz, h] of list) {
    const c = new THREE.Mesh(new THREE.CylinderGeometry(0.032, 0.034, h, 20), wax);
    c.position.set(x + dx, y + h / 2, z + dz); _shadow3(c); g.add(c);
    // melted lip
    const lip = new THREE.Mesh(new THREE.TorusGeometry(0.028, 0.006, 6, 20), wax);
    lip.rotation.x = Math.PI / 2; lip.position.set(x + dx, y + h - 0.002, z + dz); g.add(lip);
    const flame = new THREE.Sprite(new THREE.SpriteMaterial({ map: flameTex, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true, fog: false }));
    flame.position.set(x + dx, y + h + 0.03, z + dz);
    flame.scale.set(0.05, 0.09, 1);
    g.add(flame);
    const halo = new THREE.Sprite(new THREE.SpriteMaterial({ map: flameTex, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true, opacity: 0.25, fog: false }));
    halo.position.copy(flame.position); halo.scale.set(0.42, 0.42, 1); g.add(halo);
    ROOM3.flames.push({ flame, halo, ph: Math.random() * 100, y: flame.position.y });
  }
}

/* ============================================================
   ATMOSPHERE — light shafts + drifting dust
   ============================================================ */
function shaftMat3(color, strength) {
  return new THREE.ShaderMaterial({
    uniforms: { uColor: { value: new THREE.Color(color) }, uStr: { value: strength }, uTime: { value: 0 } },
    vertexShader:
      'varying vec2 vUv; varying vec3 vN; varying vec3 vV;\n' +
      'void main(){ vUv = uv; vec4 mv = modelViewMatrix*vec4(position,1.0);' +
      ' vN = normalize(normalMatrix*normal); vV = normalize(-mv.xyz); gl_Position = projectionMatrix*mv; }',
    fragmentShader:
      'uniform vec3 uColor; uniform float uStr; uniform float uTime; varying vec2 vUv; varying vec3 vN; varying vec3 vV;\n' +
      'void main(){ float along = smoothstep(0.0,0.25,vUv.y)*smoothstep(1.0,0.55,vUv.y);' +
      ' float facing = pow(abs(dot(normalize(vN),normalize(vV))), 3.0);' +
      ' float n = 0.75 + 0.25*sin(vUv.x*40.0 + uTime*0.3)*sin(vUv.y*9.0 - uTime*0.2);' +
      ' gl_FragColor = vec4(uColor * uStr * along * facing * n, 1.0); }',
    transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide, fog: false
  });
}

function buildAtmosphere3(g) {
  const R3 = ROOM3, W = R3.win;
  // moon shaft: a long open box from the window falling across the bed
  const shaftGeo = new THREE.CylinderGeometry(0.55, 0.9, 4.2, 32, 1, true);
  const moon = new THREE.Mesh(shaftGeo, shaftMat3(0x6f8fd8, 0.05));
  const from = new THREE.Vector3(R3.rightX + 0.3, W.y + 0.15, W.z);
  const to = new THREE.Vector3(-0.6, R3.floorY + 0.1, -0.1);
  moon.position.copy(from).lerp(to, 0.5);
  moon.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), from.clone().sub(to).normalize());
  moon.renderOrder = 5; g.add(moon);
  R3.beams.push(moon);

  // lamp: soft cones of light escaping the top and bottom of the shade
  const lp = ROOM3.shade.position;
  for (const dir of [1, -1]) {
    const geo = new THREE.CylinderGeometry(dir > 0 ? 0.55 : 0.75, 0.16, 0.9, 28, 1, true);
    const m = new THREE.Mesh(geo, shaftMat3(0xff9a50, dir > 0 ? 0.06 : 0.04));
    if (dir < 0) m.rotation.x = Math.PI;
    m.position.set(lp.x, lp.y + dir * 0.55, lp.z);
    m.renderOrder = 5; g.add(m);
    R3.beams.push(m);
  }

  // dust motes: drift slowly, flare where the light hits them
  const N = 260, pos = new Float32Array(N * 3), seed = new Float32Array(N);
  const rnd = _rng3(99);
  for (let i = 0; i < N; i++) {
    pos[i * 3] = -2.4 + rnd() * 5.0;
    pos[i * 3 + 1] = R3.floorY + 0.2 + rnd() * 2.4;
    pos[i * 3 + 2] = -2.0 + rnd() * 3.6;
    seed[i] = rnd() * 100;
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  geo.setAttribute('aSeed', new THREE.BufferAttribute(seed, 1));
  const mat = new THREE.ShaderMaterial({
    uniforms: { uTime: { value: 0 }, uLamp: { value: lp.clone() }, uPx: { value: renderer3d ? renderer3d.getPixelRatio() : 1 } },
    vertexShader:
      'attribute float aSeed; uniform float uTime; uniform vec3 uLamp; uniform float uPx; varying float vA;\n' +
      'void main(){ vec3 p = position;' +
      ' p.x += sin(uTime*0.11 + aSeed)*0.25; p.y += sin(uTime*0.07 + aSeed*1.3)*0.18 + mod(uTime*0.012 + aSeed, 1.0)*0.12;' +
      ' p.z += cos(uTime*0.09 + aSeed*0.7)*0.25;' +
      ' float dl = distance(p, uLamp); float lampHit = exp(-dl*dl*0.9);' +
      ' float moonHit = exp(-pow((p.x - p.y*1.1 - 0.4)*1.2, 2.0)) * smoothstep(-1.0, 1.4, p.x);' +
      ' vA = (lampHit*0.8 + moonHit*0.25 + 0.015) * (0.5 + 0.5*sin(uTime*1.7 + aSeed*5.0));' +
      ' vec4 mv = modelViewMatrix*vec4(p,1.0); gl_Position = projectionMatrix*mv;' +
      ' gl_PointSize = uPx * (1.2 + fract(aSeed)*1.8) * (3.0 / -mv.z); }',
    fragmentShader:
      'varying float vA;\n' +
      'void main(){ vec2 c = gl_PointCoord - 0.5; float d = dot(c,c); if (d > 0.25) discard;' +
      ' gl_FragColor = vec4(vec3(1.0,0.82,0.62) * vA * (1.0 - d*4.0) * 1.6, 1.0); }',
    transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, fog: false
  });
  const pts = new THREE.Points(geo, mat);
  pts.frustumCulled = false; pts.renderOrder = 6;
  g.add(pts);
  R3.dust = pts;
}

/* ============================================================
   PER-FRAME: candle flicker, curtain breath, dust drift,
   room light that subtly answers her arousal
   ============================================================ */
function updateRoom3(dt) {
  const R3 = ROOM3;
  if (!R3.group) return;
  const t = (R3.t += dt);
  const heat = (typeof G !== 'undefined') ? clamp((G.pleasure || 0) / 100, 0, 1) : 0;

  // candles: layered noise flicker, never a metronome
  const fl = (ph) => 0.82 + Math.sin(t * 13.1 + ph) * 0.06 + Math.sin(t * 7.3 + ph * 1.7) * 0.07 + Math.sin(t * 23.9 + ph * 0.3) * 0.04;
  let sumR = 0, n = 0;
  for (const f of R3.flames) {
    const k = fl(f.ph);
    f.flame.scale.set(0.05 * (0.9 + (1 - k) * 0.8), 0.095 * k, 1);
    f.flame.position.y = f.y + (k - 0.82) * 0.02;
    f.flame.position.x += Math.sin(t * 3.1 + f.ph) * 0.00012;
    f.halo.material.opacity = 0.18 + k * 0.12;
    sumR += k; n++;
  }
  const avg = n ? sumR / n : 1;
  if (R3.candleLightR) R3.candleLightR.intensity = 1.1 * avg * (0.9 + Math.sin(t * 17.0) * 0.05);
  if (R3.candleLightL) R3.candleLightL.intensity = 0.8 * avg * (0.9 + Math.sin(t * 15.0 + 2.0) * 0.06);

  // lamp warms slightly as things heat up
  if (R3.lampLight) R3.lampLight.intensity = 1.5 + heat * 0.5;
  if (keyLight3) {
    keyLight3.intensity = 1.3 + heat * 0.3;
    keyLight3.color.setRGB(1.0, 0.62 - heat * 0.05, 0.36 - heat * 0.06);
  }

  // curtains: slow breathing sway, more at the hem
  for (const c of R3.curtains) {
    const p = c.mesh.geometry.attributes.position, b = c.base;
    for (let i = 0; i < p.count; i++) {
      const x = b[i * 3], y = b[i * 3 + 1];
      const hang = clamp(0.5 - y / 2.6, 0, 1);
      const pleat = Math.sin(x * 27 + c.ph) * 0.045 + Math.sin(x * 11 + 1.3) * 0.02;
      const sway = Math.sin(t * 0.6 + x * 3 + c.ph) * 0.03 * hang + Math.sin(t * 1.3 + y * 2) * 0.008 * hang;
      p.setZ(i, pleat + sway);
      p.setX(i, x * (1 - 0.25 * (1 - hang) * 0.4));
    }
    p.needsUpdate = true;
    if (((t * 10) | 0) % 3 === 0) c.mesh.geometry.computeVertexNormals();
  }

  if (R3.dust) R3.dust.material.uniforms.uTime.value = t;
  for (const b of R3.beams) b.material.uniforms.uTime.value = t;
}

if (typeof window !== 'undefined') { window.ROOM3 = ROOM3; window.updateRoom3 = updateRoom3; }
