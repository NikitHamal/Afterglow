import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

const root = new URL('../../../', import.meta.url);
const manifestUrl = new URL('native/assets/characters/models.json', root);
const assetUrl = (slot) => new URL(`native/assets/characters/${slot}`, root).href;
const variantSelect = document.querySelector('#variant');
const poseSelect = document.querySelector('#pose');
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
controls.minDistance = 1.4;
controls.maxDistance = 6;
scene.add(new THREE.HemisphereLight(0xb8c8ff, 0x241922, 1.5));
const key = new THREE.DirectionalLight(0xffd6bd, 2.7); key.position.set(-2.5, 3.5, 3); scene.add(key);
const rim = new THREE.DirectionalLight(0x91a9ff, 1.1); rim.position.set(2, 2.2, -3); scene.add(rim);
const floor = new THREE.Mesh(new THREE.CircleGeometry(2.2, 64), new THREE.MeshStandardMaterial({ color: 0x24242a, roughness: 0.78 }));
floor.rotation.x = -Math.PI / 2; floor.position.y = 0; scene.add(floor);

let manifest;
let current;
let mixer;
let activeAction;
let clock = new THREE.Clock();

function resize() {
  const r = host.getBoundingClientRect();
  renderer.setSize(r.width, r.height, false);
  camera.aspect = Math.max(0.1, r.width / Math.max(1, r.height));
  camera.updateProjectionMatrix();
}
window.addEventListener('resize', resize);
resize();

function setMorphs(rootObject, variant) {
  const sliders = variant.sliders || {};
  const expression = variant.expression || '';
  rootObject.traverse((o) => {
    if (!o.isMesh || !o.morphTargetDictionary || !o.morphTargetInfluences) return;
    for (const [name, index] of Object.entries(o.morphTargetDictionary)) {
      let v = 0;
      if (name === expression) v = 0.72;
      for (const [param, raw] of Object.entries(sliders)) {
        const value = Math.max(-1, Math.min(1, Number(raw) || 0));
        if (name === `${param}_up`) v = Math.max(v, Math.max(value, 0));
        if (name === `${param}_dn`) v = Math.max(v, Math.max(-value, 0));
      }
      o.morphTargetInfluences[index] = v;
    }
  });
}

function resetCamera() {
  camera.position.set(2.4, 1.45, 3.4);
  controls.target.set(0, 0.9, 0);
  controls.update();
}

function playPose(name) {
  if (!mixer || !current) return;
  const clip = current.animations.find((a) => a.name === name) || current.animations.find((a) => a.name === 'stand.idle') || current.animations[0];
  if (!clip) return;
  if (activeAction) activeAction.fadeOut(0.18);
  activeAction = mixer.clipAction(clip);
  activeAction.reset().fadeIn(0.18).play();
  readout.textContent = `${variantSelect.selectedOptions[0]?.textContent || ''}\n${clip.name}\n${current.scene.userData.modelStats || ''}`;
}

async function loadVariant(keyName) {
  const variant = manifest.variants[keyName];
  const slot = manifest.slots[variant.slot];
  if (!variant || !slot) return;
  if (current?.scene) scene.remove(current.scene);
  if (mixer) mixer.stopAllAction();
  const loader = new GLTFLoader();
  readout.textContent = `Loading ${variant.label}…`;
  current = await loader.loadAsync(assetUrl(slot.asset));
  current.scene.traverse((o) => {
    if (!o.isMesh) return;
    o.castShadow = true; o.receiveShadow = true;
    if (o.material) o.material.envMapIntensity = 0.75;
  });
  setMorphs(current.scene, variant);
  scene.add(current.scene);
  mixer = new THREE.AnimationMixer(current.scene);
  playPose(poseSelect.value);
}

async function boot() {
  manifest = await fetch(manifestUrl).then((r) => r.json());
  for (const [keyName, variant] of Object.entries(manifest.variants || {})) {
    const option = document.createElement('option'); option.value = keyName; option.textContent = variant.label;
    variantSelect.append(option);
  }
  const query = new URLSearchParams(location.search);
  const initial = query.get('variant') || Object.keys(manifest.variants)[0];
  variantSelect.value = manifest.variants[initial] ? initial : Object.keys(manifest.variants)[0];
  await loadVariant(variantSelect.value);
}

variantSelect.addEventListener('change', () => loadVariant(variantSelect.value));
poseSelect.addEventListener('change', () => playPose(poseSelect.value));
document.querySelector('#reset').addEventListener('click', resetCamera);

function frame() {
  requestAnimationFrame(frame);
  const dt = Math.min(clock.getDelta(), 0.05);
  if (mixer) mixer.update(dt);
  controls.update();
  renderer.render(scene, camera);
}
boot().catch((error) => { readout.textContent = `Preview failed: ${error.message}`; console.error(error); });
frame();
