// Afterglow Simulator — module: glbModel (External 3D Model Loader)
// Loads web-standard .glb 3D assets (e.g. assets/goatchan/goatchan.glb) into the 3D scene.
'use strict';

const GLB_MODEL = {
  enabled: true,            // Active by default in sim.html
  loaded: false,
  loading: false,
  model: null,
  mixer: null,
  meshList: [],
  basePos: new THREE.Vector3(0, 0, 0),
  baseScale: 1.0,

  // Dynamics
  prevDepth: 0,
  jiggleY: 0,
  jiggleVel: 0
};

function initGLBModel() {
  if (typeof THREE.GLTFLoader === 'undefined') {
    console.warn('GLTFLoader not found. Ensure vendor/GLTFLoader.js is loaded.');
    return;
  }

  GLB_MODEL.loading = true;
  const loader = new THREE.GLTFLoader();
  const path = 'assets/goatchan/goatchan.glb';

  loader.load(
    path,
    gltf => {
      GLB_MODEL.loaded = true;
      GLB_MODEL.loading = false;
      const model = gltf.scene;
      GLB_MODEL.model = model;

      // Enable shadows and gather meshes
      model.traverse(c => {
        if (c.isMesh) {
          c.castShadow = true;
          c.receiveShadow = true;
          GLB_MODEL.meshList.push(c);
          if (c.material) {
            c.material.roughness = Math.max(c.material.roughness || 0.4, 0.35);
            c.material.metalness = Math.min(c.material.metalness || 0.1, 0.15);
          }
        }
      });

      // Normalize size and center on bed
      const box = new THREE.Box3().setFromObject(model);
      const size = new THREE.Vector3();
      box.getSize(size);
      const center = new THREE.Vector3();
      box.getCenter(center);

      const maxDim = Math.max(size.x, size.y, size.z);
      const scale = 1.35 / (maxDim || 1);
      GLB_MODEL.baseScale = scale;
      model.scale.setScalar(scale);

      GLB_MODEL.basePos.set(-center.x * scale, -center.y * scale + 0.62, -center.z * scale);
      model.position.copy(GLB_MODEL.basePos);

      // Add extra key light to complement character shaders
      const charLight = new THREE.DirectionalLight(0xffeff2, 1.1);
      charLight.position.set(0.5, 2.2, 1.8);
      scene3d.add(charLight);

      scene3d.add(model);

      // Hide procedural her3 if GLB model is active
      if (GLB_MODEL.enabled) {
        if (her3 && her3.root) her3.root.visible = false;
        model.visible = true;
      }

      const msg = document.getElementById('bootMsg');
      if (msg) msg.style.display = 'none';

      syncGLBButton();
      console.log('Loaded 3D GLB model:', path, 'meshes:', GLB_MODEL.meshList.length);
    },
    progress => {
      if (progress.total) {
        const pct = Math.round((progress.loaded / progress.total) * 100);
        const msg = document.getElementById('bootMsg');
        if (msg) msg.textContent = `loading 3d model ${pct}%…`;
      }
    },
    err => {
      console.error('Failed to load 3D GLB model:', err);
      GLB_MODEL.loading = false;
      const msg = document.getElementById('bootMsg');
      if (msg) msg.style.display = 'none';
    }
  );
}

function updateGLBModel(dt) {
  if (!GLB_MODEL.loaded || !GLB_MODEL.model || !GLB_MODEL.enabled) return;

  const currentDepth = (typeof G !== 'undefined' && G.depth != null) ? G.depth : 0;
  const depthDelta = currentDepth - GLB_MODEL.prevDepth;
  GLB_MODEL.prevDepth = currentDepth;

  // Spring physics for thrust response
  const k = 130;
  const damp = 10;
  const force = -depthDelta * 1.8;
  GLB_MODEL.jiggleVel += (force - k * GLB_MODEL.jiggleY - damp * GLB_MODEL.jiggleVel) * dt;
  GLB_MODEL.jiggleY += GLB_MODEL.jiggleVel * dt;

  // Subtle natural breathing
  const breath = Math.sin((typeof G !== 'undefined' ? G.t : 0) * 2.5) * 0.008;

  const m = GLB_MODEL.model;
  m.position.y = GLB_MODEL.basePos.y + GLB_MODEL.jiggleY * 0.05 + breath;
  m.position.z = GLB_MODEL.basePos.z + (currentDepth * 0.06);
}

function toggleGLBModel() {
  GLB_MODEL.enabled = !GLB_MODEL.enabled;
  if (GLB_MODEL.model) {
    GLB_MODEL.model.visible = GLB_MODEL.enabled;
  }
  if (her3 && her3.root) {
    her3.root.visible = !GLB_MODEL.enabled;
  }
  syncGLBButton();
}

function syncGLBButton() {
  const btn = document.getElementById('btnModelToggle');
  if (!btn) return;
  if (GLB_MODEL.enabled) {
    btn.textContent = '3D GOAT';
    btn.classList.add('on');
    btn.title = 'Current: 3D Goat-chan model (click to switch to Procedural Rig)';
  } else {
    btn.textContent = '3D RIG';
    btn.classList.remove('on');
    btn.title = 'Current: Procedural 3D Rig (click to switch to 3D Goat-chan)';
  }
}
