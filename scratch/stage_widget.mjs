import * as THREE from "https://esm.sh/three@0.169.0";
import { OrbitControls } from "https://esm.sh/three@0.169.0/examples/jsm/controls/OrbitControls.js";
import { GLTFLoader } from "https://esm.sh/three@0.169.0/examples/jsm/loaders/GLTFLoader.js";
import { RoomEnvironment } from "https://esm.sh/three@0.169.0/examples/jsm/environments/RoomEnvironment.js";

export default function render({ model, el }) {
  el.style.width = "100%";
  el.innerHTML = `
  <style>
    .agw { --ink:#f4eee8; --muted:#a89b92; --line:#332b2c; --panel:#171317; --hot:#e8b274;
      color:var(--ink); font-family:ui-sans-serif,system-ui,-apple-system,"Segoe UI",sans-serif;
      background:linear-gradient(180deg,#171216 0%,#0e0c10 100%); border:1px solid var(--line);
      border-radius:18px; overflow:hidden; box-shadow:0 24px 70px rgba(0,0,0,.45); }
    .agw * { box-sizing:border-box; }
    .agw-head { display:flex; align-items:flex-end; justify-content:space-between; gap:16px;
      padding:16px 20px 13px; border-bottom:1px solid var(--line); background:rgba(255,255,255,.015); }
    .agw-eyebrow { color:var(--hot); font-size:10px; letter-spacing:.22em; font-weight:700; text-transform:uppercase; }
    .agw-title { font-family:Georgia,"Times New Roman",serif; font-size:25px; line-height:1.1; margin-top:3px; }
    .agw-note { color:var(--muted); font-size:11px; line-height:1.45; text-align:right; max-width:370px; }
    .agw-stage { position:relative; height:min(68vh,690px); min-height:430px; background:#09080b; cursor:grab; }
    .agw-stage:active { cursor:grabbing; }
    .agw-stage canvas { display:block; width:100%; height:100%; }
    .agw-load { position:absolute; inset:0; display:flex; align-items:center; justify-content:center; flex-direction:column;
      gap:10px; color:var(--muted); font-size:12px; letter-spacing:.06em; background:radial-gradient(circle at 50% 42%,#1b1519,#09080b 70%); z-index:3; }
    .agw-load b { color:var(--ink); font-family:Georgia,serif; font-weight:400; font-size:17px; letter-spacing:0; }
    .agw-loadbar { width:180px; height:2px; background:#332a2c; overflow:hidden; }
    .agw-loadbar i { display:block; height:100%; width:0; background:var(--hot); transition:width .25s; }
    .agw-hud { position:absolute; left:16px; bottom:14px; z-index:2; color:var(--muted); font-size:10px;
      letter-spacing:.12em; text-transform:uppercase; text-shadow:0 1px 8px #000; pointer-events:none; }
    .agw-read { position:absolute; right:16px; top:14px; z-index:2; padding:6px 9px; border-radius:999px;
      border:1px solid rgba(232,178,116,.28); background:rgba(13,11,14,.68); color:#dcc9b7;
      font-size:10px; letter-spacing:.08em; backdrop-filter:blur(8px); pointer-events:none; }
    .agw-controls { border-top:1px solid var(--line); padding:13px 16px 15px; display:grid; gap:10px; background:#141014; }
    .agw-row { display:flex; align-items:center; gap:9px; flex-wrap:wrap; }
    .agw select, .agw button, .agw .agw-pill { font:inherit; font-size:11px; color:var(--ink); }
    .agw select { appearance:none; border:1px solid var(--line); background:#1c161b url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='6'%3E%3Cpath d='M1 1l4 4 4-4' fill='none' stroke='%23a89b92' stroke-width='1.4'/%3E%3C/svg%3E") no-repeat right 12px center;
      border-radius:9px; padding:8px 31px 8px 11px; outline:none; }
    .agw select:focus-visible, .agw button:focus-visible { outline:2px solid var(--hot); outline-offset:2px; }
    .agw #agwClip { flex:1 1 220px; min-width:170px; font-family:ui-monospace,SFMono-Regular,Consolas,monospace; letter-spacing:.01em; }
    .agw button { border:1px solid var(--line); background:#211a1f; border-radius:9px; padding:8px 12px;
      cursor:pointer; transition:border-color .15s,background .15s,color .15s; white-space:nowrap; }
    .agw button:hover { border-color:#5c4637; background:#281f24; }
    .agw button.on { border-color:#8a6544; background:#2d2118; color:#f6d8b2; }
    .agw #agwPlay { min-width:68px; font-weight:600; }
    .agw .agw-time { flex:1 1 230px; display:flex; align-items:center; gap:10px; min-width:190px; }
    .agw input[type=range] { flex:1; accent-color:var(--hot); height:20px; margin:0; cursor:pointer; }
    .agw .agw-clock { color:var(--muted); font-family:ui-monospace,SFMono-Regular,Consolas,monospace;
      font-size:10px; min-width:116px; text-align:right; }
    .agw .agw-label { color:#887b74; font-size:9px; letter-spacing:.16em; text-transform:uppercase; margin-right:1px; }
    .agw .agw-sep { flex:1; }
    .agw .agw-pill { display:flex; align-items:center; gap:7px; border:1px solid var(--line); border-radius:9px;
      padding:7px 9px; color:#bfb2aa; user-select:none; cursor:pointer; }
    .agw .agw-pill input { accent-color:var(--hot); margin:0; }
    .agw .agw-err { position:absolute; inset:0; z-index:4; padding:26px; background:#160d10; color:#ffc7bd;
      font-family:ui-monospace,Consolas,monospace; font-size:11px; line-height:1.55; overflow:auto; display:none; }
    @media (max-width:700px) {
      .agw-head { align-items:flex-start; flex-direction:column; gap:5px; }
      .agw-note { text-align:left; max-width:none; }
      .agw-stage { height:58vh; min-height:360px; }
      .agw .agw-clock { min-width:90px; }
    }
  </style>
  <div class="agw">
    <div class="agw-head">
      <div><div class="agw-eyebrow">Afterglow native rig</div><div class="agw-title">Live 3D stage</div></div>
      <div class="agw-note">The shipped room and both character GLBs, with every baked clip. Drag to orbit, scroll to zoom. Focus the face or the shaft for contact checks.</div>
    </div>
    <div class="agw-stage">
      <canvas></canvas>
      <div class="agw-load"><b>Building the room</b><span>Loading room and character rigs</span><span class="agw-loadbar"><i></i></span></div>
      <div class="agw-hud">Drag to orbit &nbsp;·&nbsp; Scroll to zoom &nbsp;·&nbsp; Right-drag to pan</div>
      <div class="agw-read">SHIPPED CLIPS</div>
      <div class="agw-err"></div>
    </div>
    <div class="agw-controls">
      <div class="agw-row">
        <span class="agw-label">Clip</span>
        <select id="agwClip" aria-label="Animation clip"></select>
        <button id="agwPlay" type="button">Pause</button>
        <div class="agw-time"><input id="agwTime" type="range" min="0" max="1000" value="0" aria-label="Clip time"><span class="agw-clock" id="agwClock">0.00 s</span></div>
      </div>
      <div class="agw-row">
        <span class="agw-label">Speed</span>
        <select id="agwSpeed" aria-label="Playback speed"><option value="0.25">0.25×</option><option value="0.5">0.5×</option><option value="1" selected>1×</option><option value="1.5">1.5×</option><option value="2">2×</option></select>
        <span class="agw-label">Focus</span>
        <select id="agwFocus" aria-label="Camera focus"><option value="couple">Couple</option><option value="face">Her face</option><option value="tip">Shaft tip</option></select>
        <button type="button" data-view="profile" class="on">Profile</button>
        <button type="button" data-view="front">Front</button>
        <button type="button" data-view="top">Top</button>
        <span class="agw-sep"></span>
        <label class="agw-pill"><input id="agwRoom" type="checkbox" checked> Room</label>
        <label class="agw-pill"><input id="agwLoop" type="checkbox" checked> Loop</label>
      </div>
    </div>
  </div>`;

  const stage = el.querySelector(".agw-stage");
  const canvas = el.querySelector("canvas");
  const loaderEl = el.querySelector(".agw-load");
  const loadbar = el.querySelector(".agw-loadbar i");
  const errEl = el.querySelector(".agw-err");
  const clipSel = el.querySelector("#agwClip");
  const playBtn = el.querySelector("#agwPlay");
  const timeEl = el.querySelector("#agwTime");
  const clockEl = el.querySelector("#agwClock");
  const speedSel = el.querySelector("#agwSpeed");
  const focusSel = el.querySelector("#agwFocus");
  const roomBox = el.querySelector("#agwRoom");
  const loopBox = el.querySelector("#agwLoop");
  const viewBtns = [...el.querySelectorAll("[data-view]")];
  const base = model.get("public_base") || "/public/afterglow/";
  const initialClip = model.get("clip") || "oral.in";

  let disposed = false;
  let playing = true;
  let speed = 1;
  let time = 0;
  let duration = 1;
  let currentClip = initialClip;
  let focus = "couple";
  const clock = new THREE.Clock();

  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: "high-performance" });
  renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.02;
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = THREE.PCFSoftShadowMap;

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x09080b);
  scene.fog = new THREE.Fog(0x09080b, 8, 15);
  const camera = new THREE.PerspectiveCamera(34, 1, 0.02, 60);
  const controls = new OrbitControls(camera, canvas);
  controls.enableDamping = true;
  controls.dampingFactor = 0.075;
  controls.minDistance = 0.22;
  controls.maxDistance = 8;
  controls.maxPolarAngle = Math.PI * 0.92;
  controls.target.set(0, 0.9, -0.9);

  const pmrem = new THREE.PMREMGenerator(renderer);
  scene.environment = pmrem.fromScene(new RoomEnvironment(), 0.05).texture;
  scene.add(new THREE.HemisphereLight(0xffe2c7, 0x24171e, 0.8));
  const key = new THREE.DirectionalLight(0xffd8b4, 2.25);
  key.position.set(2.5, 4.2, 2.0);
  key.castShadow = true;
  key.shadow.mapSize.set(2048, 2048);
  key.shadow.camera.left = -2.8;
  key.shadow.camera.right = 2.8;
  key.shadow.camera.top = 2.8;
  key.shadow.camera.bottom = -2.8;
  key.shadow.camera.near = 0.2;
  key.shadow.camera.far = 9;
  key.shadow.bias = -0.0003;
  scene.add(key);
  const warm = new THREE.PointLight(0xff9a58, 14, 6, 2);
  warm.position.set(-1.7, 2.2, -1.8);
  scene.add(warm);
  const cool = new THREE.PointLight(0x708cff, 5, 7, 2);
  cool.position.set(2.4, 2.9, 2.5);
  scene.add(cool);

  const grid = new THREE.GridHelper(6, 60, 0x6b5748, 0x32282c);
  grid.position.y = 0.0015;
  grid.material.transparent = true;
  grid.material.opacity = 0.28;
  grid.visible = false;
  scene.add(grid);

  const roots = {};
  const mixers = [];
  let room = null;
  const actions = new Map();
  const bones = { vivian: {}, marcus: {} };
  const box = new THREE.Box3();

  function fail(message) {
    errEl.style.display = "block";
    errEl.textContent = message;
    loaderEl.style.display = "none";
    console.error("[Afterglow stage]", message);
  }

  function resize() {
    const w = stage.clientWidth || 900;
    const h = stage.clientHeight || 600;
    renderer.setSize(w, h, false);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
  }
  const ro = new ResizeObserver(resize);
  ro.observe(stage);
  resize();

  function addModel(gltf, who) {
    const root = gltf.scene;
    root.name = who;
    root.traverse((o) => {
      if (o.isMesh) {
        o.castShadow = true;
        o.receiveShadow = true;
        o.frustumCulled = false;
        if (o.material) {
          const mats = Array.isArray(o.material) ? o.material : [o.material];
          for (const m of mats) { m.side = THREE.FrontSide; m.needsUpdate = true; }
        }
      }
      if (o.isBone) {
        if (who === "vivian" || who === "marcus") {
          if (!bones[who][o.name]) bones[who][o.name] = o;
        }
      }
    });
    scene.add(root);
    roots[who] = root;
    if (gltf.animations && gltf.animations.length) {
      const mixer = new THREE.AnimationMixer(root);
      mixers.push(mixer);
      for (const clip of gltf.animations) {
        if (!actions.has(clip.name)) actions.set(clip.name, []);
        actions.get(clip.name).push({ mixer, clip });
      }
    }
    return root;
  }

  function refreshClipList() {
    const names = [...actions.keys()].sort((a, b) => a.localeCompare(b));
    clipSel.innerHTML = names.map((n) => `<option value="${n}">${n}</option>`).join("");
    if (names.includes(initialClip)) clipSel.value = initialClip;
    else if (names.includes("oral.in")) clipSel.value = "oral.in";
    else if (names.length) clipSel.value = names[0];
    currentClip = clipSel.value;
    selectClip(currentClip, false);
  }

  function clearActions() {
    for (const mx of mixers) mx.stopAllAction();
  }

  function selectClip(name, autoplay = true) {
    currentClip = name;
    clearActions();
    const entries = actions.get(name) || [];
    duration = entries.length ? entries[0].clip.duration : 1;
    if (!entries.length) { clockEl.textContent = "no clip"; return; }
    for (const e of entries) {
      e.action.reset();
      e.action.setEffectiveWeight(1);
      e.action.setEffectiveTimeScale(1);
      e.action.clampWhenFinished = !loopBox.checked;
      e.action.play();
    }
    time = 0;
    playing = autoplay;
    playBtn.textContent = playing ? "Pause" : "Play";
    renderAt(0);
  }

  function renderAt(t) {
    if (!actions.size) return;
    time = Math.max(0, Math.min(duration, t));
    for (const entries of actions.values()) {
      for (const e of entries) {
        if (e.clip.name !== currentClip) continue;
        e.action.time = time;
        e.action.paused = true;
      }
    }
    for (const mx of mixers) mx.update(0);
    const u = duration > 0 ? time / duration : 0;
    timeEl.value = String(Math.round(u * 1000));
    const depth = currentClip.endsWith(".in") ? ` · depth ${u.toFixed(2)}` : "";
    clockEl.textContent = `${time.toFixed(2)} / ${duration.toFixed(2)} s${depth}`;
    syncFocus();
    if (room) room.visible = roomBox.checked;
    grid.visible = !roomBox.checked;
  }

  function tick() {
    if (disposed) return;
    requestAnimationFrame(tick);
    const dt = Math.min(clock.getDelta(), 0.05);
    if (playing) {
      let next = time + dt * speed;
      if (next >= duration) {
        if (loopBox.checked) next = next % Math.max(duration, 1e-4);
        else { next = duration; playing = false; playBtn.textContent = "Play"; }
      }
      renderAt(next);
    }
    controls.update();
    renderer.render(scene, camera);
  }

  function bone(who, name) { return bones[who] && bones[who][name]; }
  function bonePos(who, name) {
    const b = bone(who, name);
    if (!b) return null;
    const p = new THREE.Vector3();
    b.getWorldPosition(p);
    return p;
  }

  function focusPoint(which) {
    if (which === "face") {
      const p = bonePos("vivian", "頭");
      if (p) { p.y += 0.035; return p; }
    }
    if (which === "tip") {
      const p = bonePos("marcus", "竿端") || bonePos("marcus", "竿先");
      if (p) return p;
    }
    const a = bonePos("vivian", "下半身");
    const b = bonePos("marcus", "下半身");
    if (a && b) return a.clone().add(b).multiplyScalar(0.5).add(new THREE.Vector3(0, 0.08, 0));
    return new THREE.Vector3(0, 0.9, -0.9);
  }

  function syncFocus() {
    if (!controls) return;
    controls.target.copy(focusPoint(focus));
  }

  function setView(which) {
    focus = focusSel.value;
    const target = focusPoint(focus);
    const radius = focus === "face" ? 0.62 : focus === "tip" ? 0.46 : 3.05;
    let az = Math.PI * 0.5, el = 0.18;
    if (which === "front") { az = Math.PI * 0.04; el = 0.16; }
    if (which === "top") { az = Math.PI * 0.5; el = 1.24; }
    if (which === "back") { az = Math.PI * 0.98; el = 0.18; }
    const y = Math.sin(el) * radius;
    const flat = Math.cos(el) * radius;
    camera.position.set(target.x + Math.sin(az) * flat, target.y + y, target.z + Math.cos(az) * flat);
    controls.target.copy(target);
    controls.update();
    viewBtns.forEach((b) => b.classList.toggle("on", b.dataset.view === which));
  }

  function onTimeInput() {
    playing = false;
    playBtn.textContent = "Play";
    renderAt((Number(timeEl.value) / 1000) * duration);
  }

  function onPlay() {
    if (time >= duration - 1e-4) time = 0;
    playing = !playing;
    playBtn.textContent = playing ? "Pause" : "Play";
  }

  const abort = new AbortController();
  const signal = abort.signal;
  clipSel.addEventListener("change", () => selectClip(clipSel.value, true), { signal });
  playBtn.addEventListener("click", onPlay, { signal });
  timeEl.addEventListener("input", onTimeInput, { signal });
  speedSel.addEventListener("change", () => { speed = Number(speedSel.value) || 1; }, { signal });
  focusSel.addEventListener("change", () => { focus = focusSel.value; syncFocus(); setView("profile"); }, { signal });
  roomBox.addEventListener("change", () => { if (room) room.visible = roomBox.checked; grid.visible = !roomBox.checked; }, { signal });
  loopBox.addEventListener("change", () => { for (const entries of actions.values()) for (const e of entries) e.action.clampWhenFinished = !loopBox.checked; }, { signal });
  viewBtns.forEach((b) => b.addEventListener("click", () => setView(b.dataset.view), { signal }));

  const manager = new THREE.LoadingManager();
  manager.onProgress = (_url, loaded, total) => {
    const pct = total ? Math.min(100, Math.round((loaded / total) * 100)) : 20;
    loadbar.style.width = `${pct}%`;
  };
  const loader = new GLTFLoader(manager);

  async function boot() {
    try {
      loaderEl.querySelector("span").textContent = "Loading room";
      const roomGltf = await loader.loadAsync(base + "room.glb");
      room = roomGltf.scene;
      room.traverse((o) => { if (o.isMesh) { o.receiveShadow = true; o.castShadow = false; } });
      scene.add(room);
      loaderEl.querySelector("span").textContent = "Loading Vivian";
      const vGltf = await loader.loadAsync(base + "vivian.glb");
      addModel(vGltf, "vivian");
      loaderEl.querySelector("span").textContent = "Loading Marcus";
      const mGltf = await loader.loadAsync(base + "marcus.glb");
      addModel(mGltf, "marcus");
      refreshClipList();
      loaderEl.style.display = "none";
      resize();
      // one settle frame puts every skeleton at the first pose before the camera frames it
      renderAt(0);
      requestAnimationFrame(() => {
        setView("profile");
        tick();
      });
    } catch (e) {
      fail("Could not load the stage assets.\n" + (e && e.message ? e.message : String(e)) +
        "\n\nAssets: " + base + "\nThe notebook's /public service worker must be active for this page.");
    }
  }

  boot();

  return () => {
    disposed = true;
    abort.abort();
    ro.disconnect();
    cancelAnimationFrame(0);
    controls.dispose();
    pmrem.dispose();
    renderer.dispose();
  };
}
