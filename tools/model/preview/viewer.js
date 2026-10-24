import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

const root = new URL('../../../', import.meta.url);
const manifestUrl = new URL('native/assets/characters/models.json', root);
const assetUrl = (slot) => new URL(`native/assets/characters/${slot}`, root).href;
const variantSelect = document.querySelector('#variant');
const shotSelect = document.querySelector('#shot');
const resetButton = document.querySelector('#reset');
const auditButton = document.createElement('button');
auditButton.id = 'audit';
auditButton.textContent = 'Run pose audit';
resetButton.after(auditButton);
const readout = document.querySelector('#readout');
const host = document.querySelector('main');

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x101114);
const camera = new THREE.PerspectiveCamera(31, 1, 0.01, 100);
camera.position.set(2.4, 1.45, 3.4);
const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: false });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.05;
host.append(renderer.domElement);
const controls = new OrbitControls(camera, renderer.domElement);
controls.target.set(0, 0.9, 0);
controls.enableDamping = true;
controls.minDistance = 1.25;
controls.maxDistance = 6;
scene.add(new THREE.HemisphereLight(0xb8c8ff, 0x241922, 1.5));
const key = new THREE.DirectionalLight(0xffd6bd, 2.7);
key.position.set(-2.5, 3.5, 3);
scene.add(key);
const rim = new THREE.DirectionalLight(0x91a9ff, 1.1);
rim.position.set(2, 2.2, -3);
scene.add(rim);
const floor = new THREE.Mesh(
  new THREE.CircleGeometry(2.2, 64),
  new THREE.MeshStandardMaterial({ color: 0x24242a, roughness: 0.78 }),
);
floor.rotation.x = -Math.PI / 2;
scene.add(floor);

let manifest;
let current;
let mixer;
let activeAction;
let activeShot;
let clock = new THREE.Clock();

function resize() {
  const r = host.getBoundingClientRect();
  renderer.setSize(r.width, r.height, false);
  camera.aspect = Math.max(0.1, r.width / Math.max(1, r.height));
  camera.updateProjectionMatrix();
}
window.addEventListener('resize', resize);
resize();

function selectedVariant() {
  return manifest?.variants?.[variantSelect.value];
}

function selectedShot() {
  const id = shotSelect.value;
  return manifest?.shots?.[id] || Object.values(manifest?.shots || {})[0];
}

function meshBounds(mesh) {
  if (mesh.isSkinnedMesh && typeof mesh.computeBoundingBox === 'function') {
    mesh.computeBoundingBox();
    return mesh.boundingBox.clone().applyMatrix4(mesh.matrixWorld);
  }
  return new THREE.Box3().setFromObject(mesh);
}

function sceneBounds(rootObject) {
  const box = new THREE.Box3();
  rootObject.traverse((o) => {
    if (o.isMesh) box.union(meshBounds(o));
  });
  return box;
}

function setMorphs(rootObject, variant, shotId) {
  const sliders = variant.sliders || {};
  const expression = (variant.expressions && variant.expressions[shotId]) || variant.expression || '';
  rootObject.traverse((o) => {
    if (!o.isMesh || !o.morphTargetDictionary || !o.morphTargetInfluences) return;
    for (const [name, index] of Object.entries(o.morphTargetDictionary)) {
      let value = name === expression ? 0.72 : 0;
      for (const [param, raw] of Object.entries(sliders)) {
        const slider = Math.max(-1, Math.min(1, Number(raw) || 0));
        if (name === `${param}_up`) value = Math.max(value, Math.max(slider, 0));
        if (name === `${param}_dn`) value = Math.max(value, Math.max(-slider, 0));
      }
      o.morphTargetInfluences[index] = value;
    }
  });
}

function fitShotCamera(shot, clip) {
  if (!shot || !current?.scene) return;
  const fallback = shot.target || [0, 0.9, 0];
  const fallbackTarget = new THREE.Vector3(fallback[0], fallback[1], fallback[2]);
  const box = new THREE.Box3();
  if (mixer && clip && Number.isFinite(clip.duration) && clip.duration > 0) {
    const previousTime = mixer.time;
    const wasPaused = activeAction?.paused || false;
    if (activeAction) activeAction.paused = true;
    for (const sample of [0, 0.25, 0.5, 0.75, 1]) {
      mixer.setTime(clip.duration * sample);
      current.scene.updateMatrixWorld(true);
      box.union(sceneBounds(current.scene));
    }
    mixer.setTime(previousTime);
    if (activeAction) activeAction.paused = wasPaused;
  } else {
    current.scene.updateMatrixWorld(true);
    box.copy(sceneBounds(current.scene));
  }
  if (box.isEmpty() || !finiteBox(box)) {
    controls.target.copy(fallbackTarget);
    const distance = Number(shot.distance) || 3.2;
    camera.position.set(distance * 0.71, fallback[1] + distance * 0.16, distance * 0.71);
    controls.update();
    return;
  }
  const target = box.getCenter(new THREE.Vector3());
  const size = box.getSize(new THREE.Vector3());
  const radius = Math.max(0.5, size.length() * 0.5);
  const fov = THREE.MathUtils.degToRad(camera.fov);
  const distance = THREE.MathUtils.clamp(radius / Math.tan(fov * 0.5) * 1.18, 1.25, 12);
  const direction = camera.position.clone().sub(controls.target);
  if (direction.lengthSq() < 1e-6) direction.set(0.71, 0.16, 0.71);
  direction.normalize();
  controls.target.copy(target);
  camera.position.copy(target).addScaledVector(direction, distance);
  camera.near = Math.max(0.01, distance / 100);
  camera.far = Math.max(30, distance * 8);
  camera.updateProjectionMatrix();
  const damping = controls.enableDamping;
  controls.enableDamping = false;
  controls.update();
  controls.enableDamping = damping;
}

function resetCamera() {
  fitShotCamera(selectedShot(), activeShot && findClip(activeShot.clip));
}

function findClip(name) {
  return current?.animations.find((clip) => clip.name === name)
    || current?.animations.find((clip) => clip.name === 'stand.idle')
    || current?.animations[0];
}

function playShot(id = shotSelect.value) {
  if (!mixer || !current) return;
  const shot = manifest.shots[id] || selectedShot();
  const clip = findClip(shot?.clip);
  if (!clip) return;
  activeShot = shot;
  setMorphs(current.scene, selectedVariant(), id);
  if (activeAction) activeAction.fadeOut(0.18);
  activeAction = mixer.clipAction(clip);
  activeAction.reset().fadeIn(0.18).play();
  fitShotCamera(shot, clip);
  readout.textContent = `${selectedVariant().label}\n${shot.label} · ${clip.name}\npose guard rail · runtime mesh checks armed`;
}

function finiteBox(box) {
  return [box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z].every(Number.isFinite);
}

function inspectPose() {
  if (!current?.scene) return { ok: false, reason: 'scene missing' };
  current.scene.updateMatrixWorld(true);
  let meshes = 0;
  let maxExtent = 0;
  let reason = '';
  current.scene.traverse((o) => {
    if (!o.isMesh || reason) return;
    meshes += 1;
    if (o.matrixWorld.elements.some((v) => !Number.isFinite(v))) {
      reason = `${o.name || 'mesh'} has a non-finite transform`;
      return;
    }
    const box = meshBounds(o);
    if (!finiteBox(box)) {
      reason = `${o.name || 'mesh'} has a non-finite bound`;
      return;
    }
    const size = box.getSize(new THREE.Vector3());
    maxExtent = Math.max(maxExtent, size.x, size.y, size.z);
    const scale = o.getWorldScale(new THREE.Vector3());
    if (scale.toArray().some((value) => !Number.isFinite(value) || value < 0.05 || value > 20)) {
      reason = `${o.name || 'mesh'} has an unstable world scale`;
      return;
    }
    if (maxExtent > 12) reason = `${o.name || 'mesh'} escaped the 12 m guard envelope`;
  });
  if (!reason) current.scene.traverse((o) => {
    if (!o.isBone || reason) return;
    const determinant = new THREE.Matrix3().setFromMatrix4(o.matrixWorld).determinant();
    if (!Number.isFinite(determinant) || Math.abs(determinant) < 1e-5) reason = `${o.name || 'bone'} has a collapsed basis`;
  });
  return reason ? { ok: false, reason } : { ok: meshes > 0, meshes, maxExtent };
}

function runPoseAudit() {
  if (!mixer || !current) return;
  const shotEntries = Object.entries(manifest.shots || {});
  const failures = [];
  for (const [id, shot] of shotEntries) {
    const clip = current.animations.find((candidate) => candidate.name === shot.clip);
    if (!clip) {
      failures.push(`${id}: clip missing`);
      continue;
    }
    setMorphs(current.scene, selectedVariant(), id);
    if (activeAction) activeAction.stop();
    activeAction = mixer.clipAction(clip).reset().play();
    activeAction.paused = true;
    for (const t of [0, 0.25, 0.5, 0.75, 1]) {
      mixer.setTime(clip.duration * t);
      const check = inspectPose();
      if (!check.ok) failures.push(`${id}@${t}: ${check.reason || 'empty scene'}`);
    }
    activeAction.paused = false;
  }
  const label = selectedVariant().label;
  const result = failures.length
    ? `${label}\npose audit FAILED\n${failures.join('\n')}`
    : `${label}\npose audit passed · ${shotEntries.length} shots × 5 samples\nfinite transforms · bounded meshes`;
  playShot(shotSelect.value);
  readout.textContent = result;
}

async function loadVariant(keyName) {
  const variant = manifest.variants[keyName];
  if (!variant) return;
  const slot = manifest.slots[variant.slot];
  if (!slot) return;
  if (current?.scene) scene.remove(current.scene);
  if (mixer) mixer.stopAllAction();
  const loader = new GLTFLoader();
  readout.textContent = `Loading ${variant.label}…`;
  current = await loader.loadAsync(assetUrl(slot.asset));
  const morphNames = new Set();
  current.scene.traverse((o) => {
    if (!o.morphTargetDictionary) return;
    Object.keys(o.morphTargetDictionary).forEach((name) => morphNames.add(name));
  });
  const missingExpressions = (slot.expressionSets || []).filter((name) => !morphNames.has(name));
  const missingShapes = (slot.shapeSets || []).filter((name) => !morphNames.has(`${name}_up`) && !morphNames.has(`${name}_dn`) && !morphNames.has(name));
  if (missingExpressions.length || missingShapes.length) {
    throw new Error(`rig contract missing morphs: ${[...missingExpressions, ...missingShapes].join(', ')}`);
  }
  const badClips = current.animations.filter((clip) => !Number.isFinite(clip.duration) || clip.duration <= 0 || clip.tracks.some((track) => track.values.some((value) => !Number.isFinite(value))));
  if (badClips.length) throw new Error(`animation contract failed: ${badClips.map((clip) => clip.name || 'unnamed').join(', ')}`);
  current.scene.traverse((o) => {
    if (!o.isMesh) return;
    o.castShadow = true;
    o.receiveShadow = true;
    if (o.material) o.material.envMapIntensity = 0.75;
  });
  scene.add(current.scene);
  mixer = new THREE.AnimationMixer(current.scene);
  playShot(shotSelect.value);
}

function populateShots() {
  shotSelect.replaceChildren();
  for (const [id, shot] of Object.entries(manifest.shots || {})) {
    const option = document.createElement('option');
    option.value = id;
    option.textContent = shot.label;
    shotSelect.append(option);
  }
}

async function boot() {
  manifest = await fetch(manifestUrl).then((r) => r.json());
  populateShots();
  for (const [keyName, variant] of Object.entries(manifest.variants || {})) {
    const option = document.createElement('option');
    option.value = keyName;
    option.textContent = variant.label;
    variantSelect.append(option);
  }
  const query = new URLSearchParams(location.search);
  const initialVariant = query.get('variant') || Object.keys(manifest.variants)[0];
  const initialShot = query.get('shot') || Object.keys(manifest.shots)[0];
  variantSelect.value = manifest.variants[initialVariant] ? initialVariant : Object.keys(manifest.variants)[0];
  shotSelect.value = manifest.shots[initialShot] ? initialShot : Object.keys(manifest.shots)[0];
  await loadVariant(variantSelect.value);
}

variantSelect.addEventListener('change', () => loadVariant(variantSelect.value));
shotSelect.addEventListener('change', () => playShot(shotSelect.value));
resetButton.addEventListener('click', resetCamera);
auditButton.addEventListener('click', runPoseAudit);

function frame() {
  requestAnimationFrame(frame);
  const dt = Math.min(clock.getDelta(), 0.05);
  if (mixer) mixer.update(dt);
  controls.update();
  renderer.render(scene, camera);
}
boot().catch((error) => { readout.textContent = `Preview failed: ${error.message}`; console.error(error); });
frame();
