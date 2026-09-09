// Afterglow 3D — module: poses3d (advanced anatomical joint kinematics & biomechanical dynamics)
// Positions: 0 missionary · 1 legs-up · 2 doggy · 3 prone bone · 4 cowgirl · 5 rev cowgirl · 6 spooning · + oral
'use strict';

/* ============================================================
   MATHEMATICAL & ORTHOGONAL RIG ORIENTATION HELPERS
   Rig baseline: Head along +Y, Pelvic front along +Z.
   Gram-Schmidt orthonormalization prevents any non-affine shearing.
   ============================================================ */
const _v3a = new THREE.Vector3(), _v3b = new THREE.Vector3(), _v3c = new THREE.Vector3();
const _m3 = new THREE.Matrix4();
const _qA = new THREE.Quaternion(), _qB = new THREE.Quaternion();

function orientTo3(obj, up, fwd) {
  _v3b.set(up[0], up[1], up[2]).normalize();          // local +Y (Spine axis)
  _v3c.set(fwd[0], fwd[1], fwd[2]);                   // target +Z (Anterior facing)
  // Gram-Schmidt projection: strip parallel component
  _v3c.addScaledVector(_v3b, -_v3b.dot(_v3c));
  if (_v3c.lengthSq() < 1e-8) _v3c.set(0, 0, 1).addScaledVector(_v3b, -_v3b.z);
  _v3c.normalize();                                   // local +Z
  _v3a.crossVectors(_v3b, _v3c).normalize();          // local +X (Lateral axis)
  _m3.makeBasis(_v3a, _v3b, _v3c);
  obj.quaternion.setFromRotationMatrix(_m3);
}

/* Shortest-arc radian interpolation preventing 360-degree limb spin */
function angleLerp(a, b, t) {
  let diff = (b - a) % TAU;
  if (diff < -Math.PI) diff += TAU;
  if (diff > Math.PI) diff -= TAU;
  return a + diff * t;
}

/* ============================================================
   AUTONOMIC BIOMECHANICAL SHIVER & KINEMATICS MODULATION
   Injects involuntary physiological micro-movements:
   respiration heave, pelvic impact recoil, orgasm tremor, toe curling.
   ============================================================ */
function getBiomechanicalState() {
  const t = G.t || 0;
  const p = (G.pleasure || 0) / 100;
  const ar = (G.ar || 0) / 100;
  const d = G.depth || 0;
  const inOrg = G.state === 'orgasm';

  // Breathing wave: accelerates and deepens with arousal
  const brRate = 0.18 + p * 0.45;
  const breathCycle = Math.sin(t * TAU * brRate);
  const breathHeave = breathCycle * (0.012 + p * 0.024);

  // Pelvic thrust displacement & mattress recoil
  const thrustImpact = (G.impact || 0) * 0.05 + d * 0.035;
  const recoilY = -Math.sin(d * Math.PI) * 0.028 * (1 + (G.impact || 0));

  // Climax / high-arousal neuromuscular tremor
  const orgFactor = inOrg ? Math.sin(Math.PI * clamp((G.orgT || 0) / 5.2, 0, 1)) : 0;
  const tremor = (orgFactor * 0.045 + Math.max(0, p - 0.7) * 0.015) * 
                 (Math.sin(t * 42) * 0.65 + Math.cos(t * 58) * 0.35);

  // Toe curl & ankle plantarflexion tension (peaks during climax)
  const toeTension = clamp(p * 0.6 + orgFactor * 0.75 + ar * 0.2, 0, 1.4);

  // Erotic lumbar lordosis arch (back arches deeper with pleasure)
  const lordosis = Math.sin(t * 1.2) * 0.04 + p * 0.18 + (inOrg ? 0.22 : 0);

  return { breathHeave, recoilY, tremor, toeTension, lordosis, d, p, ar, inOrg };
}

/* ============================================================
   HIGH-FIDELITY 3D POSE DEFINITIONS
   Angles in radians. Full anatomical rig coverage:
   - Spine (thoracic/lumbar pitch, yaw, roll)
   - Chest (ribcage expansion & tilt)
   - Head & Neck (cervical extension/rotation)
   - Hip (flexion [L, R]), HipRot (external rotation/abduction [L, R])
   - Knee (flexion [L, R]), Ankle (plantar/dorsiflexion [L, R]), Toes (curl [L, R])
   - Arms, Elbows, Wrists, Hands & Fingers
   - Penetration alignment vectors and dynamic camera framing
   ============================================================ */

const POSES3 = [
  { // 0 — MISSIONARY: Intimate supine coitus; grounded back and glutes, legs hooked over his hips
    name: 'MISSIONARY',
    her: {
      pos: [0, 0.31, -0.52], up: [0, -0.06, -0.998], fwd: [0, 0.998, -0.06],
      spine: [-0.02, 0, 0], chest: [0.04, 0, 0], head: [-0.22, 0, 0],
      hip: [-1.22, -1.14], hipRot: [0.38, -0.38],
      knee: [1.96, 1.88], ankle: [0.42, 0.38], toes: [0.55, 0.50],
      spread: 0.58,
      arm: [0.18, 0.18], armZ: [0.72, -0.72], elbow: [-0.22, -0.22], wrist: [0.15, -0.15]
    },
    him: {
      pos: [0, 0.50, -0.21], up: [0, -0.10, -0.99], fwd: [0, -0.99, 0.10],
      spine: [0.12, 0, 0], chest: [0.08, 0, 0], head: [-0.46, 0, 0],
      hip: [-1.58, -1.58], hipRot: [0.22, -0.22],
      knee: [1.84, 1.84], ankle: [0.28, 0.28], toes: [0.10, 0.10],
      spread: 0.44,
      arm: [-1.42, -1.42], armZ: [0.82, -0.82], elbow: [-0.18, -0.18], wrist: [-0.10, 0.10],
      shaft: -2.96
    },
    hands: {
      himL: { k: 'herChest', x: 0.26, y: -0.04, z: 0.06, floor: 0.12 },
      himR: { k: 'herChest', x: -0.26, y: -0.04, z: 0.06, floor: 0.12 },
      herL: { k: 'himShoulderL', x: 0.06, y: -0.02, z: 0.04 },
      herR: { k: 'himShoulderR', x: -0.06, y: -0.02, z: 0.04 }
    },
    cam: { yaw: 0.45, pitch: 0.32, dist: 4.2, fov: 42, target: [0, 0.36, -0.42] }
  },

  { // 1 — LEGS-UP / DEEP FORNIX: Thighs hyperflexed to chest, grounded sacrum, maximum depth
    name: 'LEGS-UP / DEEP',
    her: {
      pos: [0, 0.31, -0.60], up: [0, 0.05, -0.998], fwd: [0, 0.998, 0.05],
      spine: [-0.08, 0, 0], chest: [0.06, 0, 0], head: [-0.16, 0, 0],
      hip: [-2.42, -2.34], hipRot: [0.24, -0.24],
      knee: [0.62, 0.56], ankle: [0.72, 0.68], toes: [0.85, 0.80],
      spread: 0.36,
      arm: [0.24, 0.24], armZ: [1.20, -1.20], elbow: [-0.18, -0.18], wrist: [0.25, -0.25]
    },
    him: {
      pos: [0, 0.49, -0.28], up: [0, -0.14, -0.99], fwd: [0, -0.99, 0.14],
      spine: [0.18, 0, 0], chest: [0.12, 0, 0], head: [-0.58, 0, 0],
      hip: [-1.48, -1.48], hipRot: [0.20, -0.20],
      knee: [1.82, 1.82], ankle: [0.32, 0.32], toes: [0.15, 0.15],
      spread: 0.38,
      arm: [-1.46, -1.46], armZ: [0.68, -0.68], elbow: [-0.15, -0.15], wrist: [-0.08, 0.08],
      shaft: -3.02
    },
    hands: {
      himL: { k: 'herKneeL', x: 0.06, y: 0.02, z: 0.04 },
      himR: { k: 'herKneeR', x: -0.06, y: 0.02, z: 0.04 },
      herL: { k: 'herHead', x: -0.14, y: 0.05, z: 0.02 },
      herR: { k: 'herHead', x: 0.14, y: 0.05, z: 0.02 }
    },
    cam: { yaw: 0.32, pitch: 0.28, dist: 3.9, fov: 40, target: [0, 0.38, -0.36] }
  },

  { // 2 — DOGGY (ARCHED): All fours, deep lumbar lordosis sway, hips raised high, head dipping down
    name: 'DOGGY (ARCHED)',
    her: {
      pos: [0, 0.56, -0.28], up: [0, 0.15, -0.99], fwd: [0, -0.99, -0.15],
      spine: [0.28, 0, 0], chest: [-0.18, 0, 0], head: [0.38, 0, 0],
      hip: [-1.34, -1.34], hipRot: [0.18, -0.18],
      knee: [1.12, 1.12], ankle: [0.65, 0.65], toes: [0.45, 0.45],
      spread: 0.32,
      arm: [-1.82, -1.82], armZ: [0.24, -0.24], elbow: [-0.12, -0.12], wrist: [0.08, -0.08]
    },
    him: {
      pos: [0, 0.58, 0.18], up: [0, 0.88, -0.47], fwd: [0, -0.47, -0.88],
      spine: [-0.12, 0, 0], chest: [0.14, 0, 0], head: [-0.14, 0, 0],
      hip: [-0.38, -0.38], hipRot: [0.28, -0.28],
      knee: [1.66, 1.66], ankle: [0.48, 0.48], toes: [0.20, 0.20],
      spread: 0.48,
      arm: [-0.56, -0.56], armZ: [0.46, -0.46], elbow: [-0.26, -0.26], wrist: [-0.15, 0.15],
      shaft: -1.78
    },
    hands: {
      himL: { k: 'herHips', x: -0.14, y: 0.02, z: -0.06 },
      himR: { k: 'herHips', x: 0.14, y: 0.02, z: -0.06 },
      herL: { k: 'herChest', x: -0.16, y: -0.12, z: 0.03, floor: 0.12 },
      herR: { k: 'herChest', x: 0.16, y: -0.12, z: 0.03, floor: 0.12 }
    },
    cam: { yaw: -0.62, pitch: 0.36, dist: 4.4, fov: 44, target: [0, 0.52, 0.02] }
  },

  { // 3 — PRONE BONE: She lies flat, pelvic tilt creates tight anterior wall friction, he covers her
    name: 'PRONE BONE (FLAT)',
    her: {
      pos: [0, 0.24, -0.42], up: [0, -0.22, -0.97], fwd: [0, -0.97, 0.22],
      spine: [0.12, 0, 0], chest: [-0.08, 0, 0], head: [0.22, 0.62, -0.10],
      hip: [0.08, 0.04], hipRot: [0.10, -0.10],
      knee: [0.22, 0.18], ankle: [0.35, 0.30], toes: [0.60, 0.55],
      spread: 0.24,
      arm: [3.05, 3.05], armZ: [1.25, -1.25], elbow: [-0.24, -0.24], wrist: [0.12, -0.12]
    },
    him: {
      pos: [0, 0.46, -0.14], up: [0, -0.06, -0.99], fwd: [0, -0.99, 0.06],
      spine: [0.15, 0, 0], chest: [0.08, 0, 0], head: [-0.34, 0, 0],
      hip: [-0.58, -0.58], hipRot: [0.26, -0.26],
      knee: [0.36, 0.36], ankle: [0.22, 0.22], toes: [0.10, 0.10],
      spread: 0.48,
      arm: [-1.42, -1.42], armZ: [0.68, -0.68], elbow: [-0.14, -0.14], wrist: [-0.08, 0.08],
      shaft: -2.38
    },
    hands: {
      himL: { k: 'herChest', x: -0.28, y: -0.02, z: 0.06, floor: 0.12 },
      himR: { k: 'herChest', x: 0.28, y: -0.02, z: 0.06, floor: 0.12 },
      herL: null, herR: null
    },
    cam: { yaw: 0.68, pitch: 0.42, dist: 4.3, fov: 42, target: [0, 0.38, -0.16] }
  },

  { // 4 — COWGIRL: Upright straddle riding, arched spine, rhythmic vertical bouncing
    name: 'COWGIRL (RIDING)',
    her: {
      pos: [0, 0.30, 0.02], up: [0, 0.98, -0.18], fwd: [0, 0.18, 0.98],
      spine: [-0.16, 0, 0], chest: [0.12, 0, 0], head: [-0.22, 0, 0],
      hip: [-1.12, -1.12], hipRot: [0.48, -0.48],
      knee: [2.54, 2.54], ankle: [0.82, 0.82], toes: [0.45, 0.45],
      spread: 0.68,
      arm: [-0.34, -0.34], armZ: [0.46, -0.46], elbow: [-0.30, -0.30], wrist: [0.10, -0.10]
    },
    him: {
      pos: [0, 0.17, -0.12], up: [0, 0.05, -0.99], fwd: [0, 0.99, 0.05],
      spine: [0.08, 0, 0], chest: [0.06, 0, 0], head: [0.26, 0, 0],
      hip: [-0.26, -0.26], hipRot: [0.24, -0.24],
      knee: [0.38, 0.38], ankle: [0.25, 0.25], toes: [0.10, 0.10],
      spread: 0.46,
      arm: [-0.88, -0.88], armZ: [0.92, -0.92], elbow: [-0.65, -0.65], wrist: [-0.12, 0.12],
      shaft: 0.15
    },
    hands: {
      // he braces on the mattress beside his own shoulders (her body is
      // genuinely out of his reach while she rides high — verified by probe)
      himL: { k: 'himShoulderL', x: -0.25, y: -0.05, z: -0.12, floor: 0.12 },
      himR: { k: 'himShoulderR', x: 0.25, y: -0.05, z: -0.12, floor: 0.12 },
      // she plays with her own hair while riding (his chest is a full
      // arm-span below her hands — verified by probe)
      herL: { k: 'herHead', x: -0.10, y: 0.02, z: 0.06 },
      herR: { k: 'herHead', x: 0.10, y: 0.02, z: 0.06 }
    },
    cam: { yaw: 0.22, pitch: 0.24, dist: 3.8, fov: 42, target: [0, 0.58, -0.02] }
  },

  { // 5 — REVERSE COWGIRL: Turned away, deep pelvic tilt, gluteal focus
    name: 'REV COWGIRL',
    her: {
      pos: [0, 0.38, 0.02], up: [0, 0.96, 0.26], fwd: [0, -0.26, 0.96],
      spine: [0.22, 0, 0], chest: [-0.14, 0, 0], head: [0.14, 0, 0],
      hip: [-1.14, -1.14], hipRot: [0.46, -0.46],
      knee: [2.54, 2.54], ankle: [0.82, 0.82], toes: [0.45, 0.45],
      spread: 0.64,
      arm: [-0.34, -0.34], armZ: [0.34, -0.34], elbow: [-0.28, -0.28], wrist: [0.10, -0.10]
    },
    him: {
      pos: [0, 0.17, 0.11], up: [0, 0.05, -0.99], fwd: [0, 0.99, 0.05],
      spine: [0.08, 0, 0], chest: [0.06, 0, 0], head: [0.22, 0, 0],
      hip: [-0.26, -0.26], hipRot: [0.24, -0.24],
      knee: [0.38, 0.38], ankle: [0.25, 0.25], toes: [0.10, 0.10],
      spread: 0.46,
      arm: [-0.74, -0.74], armZ: [1.02, -1.02], elbow: [-0.82, -0.82], wrist: [-0.12, 0.12],
      shaft: -1.64
    },
    hands: {
      himL: { k: 'herHips', x: -0.14, y: 0.03, z: 0.05 },
      himR: { k: 'herHips', x: 0.14, y: 0.03, z: 0.05 },
      herL: { k: 'herHead', x: -0.10, y: 0.08, z: 0.02 },
      herR: { k: 'herHead', x: 0.10, y: 0.08, z: 0.02 }
    },
    cam: { yaw: -0.35, pitch: 0.20, dist: 3.8, fov: 42, target: [0, 0.60, 0.16] }
  },

  { // 6 — SPOONING: Intimate lateral coitus; legs entwined, synchronized breathing
    name: 'SPOONING',
    her: {
      pos: [0, 0.16, -0.32], up: [0, 0.04, -0.99], fwd: [0.98, 0.16, 0.08],
      spine: [0.06, -0.12, 0.04], chest: [0.04, -0.08, 0], head: [0.12, -0.38, 0.08],
      hip: [-1.12, -0.68], hipRot: [0.22, -0.12],
      knee: [1.62, 1.34], ankle: [0.45, 0.40], toes: [0.52, 0.48],
      spread: 0.22,
      arm: [-0.88, -0.88], armZ: [0.58, -0.58], elbow: [-0.98, -0.98], wrist: [0.15, -0.15]
    },
    him: {
      pos: [-0.14, 0.18, -0.08], up: [0, 0.04, -0.99], fwd: [0.98, 0.12, 0.06],
      spine: [0.08, -0.10, 0], chest: [0.06, -0.06, 0], head: [0.08, -0.34, 0],
      hip: [-0.76, -0.54], hipRot: [0.18, -0.10],
      knee: [1.06, 0.84], ankle: [0.35, 0.30], toes: [0.15, 0.15],
      spread: 0.12,
      arm: [-1.08, -1.08], armZ: [0.52, -0.52], elbow: [-1.08, -1.08], wrist: [-0.10, 0.10],
      shaft: -2.54
    },
    hands: {
      himL: { k: 'herChest', x: 0.08, y: -0.06, z: 0.08 },
      himR: { k: 'herHips', x: 0.04, y: 0.02, z: 0.06 },
      herL: null, herR: null
    },
    cam: { yaw: 1.20, pitch: 0.30, dist: 4.1, fov: 42, target: [0, 0.38, -0.08] }
  }
];

/* ============================================================
   ORAL 3D RIG: Cunnilingus (Lick) & Fellatio (Blowjob) Precision Alignments
   ============================================================ */
const ORAL_LICK3 = {
  name: 'ORAL (LICK)',
  her: {
    pos: [0, 0.30, -0.48], up: [0, -0.06, -0.998], fwd: [0, 0.998, -0.06],
    spine: [-0.02, 0, 0], chest: [0.06, 0, 0], head: [-0.34, 0, 0],
    hip: [-1.34, -1.22], hipRot: [0.55, -0.55],
    knee: [2.10, 2.00], ankle: [0.55, 0.50], toes: [0.72, 0.68],
    spread: 0.82,
    arm: [-0.68, -0.68], armZ: [1.08, -1.08], elbow: [-0.98, -0.98], wrist: [0.20, -0.20]
  },
  him: {
    pos: [0, 0.53, 0.24], up: [0, -0.18, -0.98], fwd: [0, -0.98, 0.18],
    spine: [0.10, 0, 0], chest: [0.16, 0, 0], head: [-0.45, 0, 0],
    hip: [-0.64, -0.64], hipRot: [0.32, -0.32],
    knee: [1.22, 1.22], ankle: [0.38, 0.38], toes: [0.15, 0.15],
    spread: 0.56,
    arm: [-1.92, -1.92], armZ: [0.68, -0.68], elbow: [-0.74, -0.74], wrist: [-0.12, 0.12],
    shaft: 0.0
  },
  hands: {
    himL: { k: 'herThighL', x: 0.06, y: 0.04, z: 0.06 },
    himR: { k: 'herThighR', x: -0.06, y: 0.04, z: 0.06 },
    herL: { k: 'herHead', x: -0.10, y: 0.04, z: 0.02 },
    herR: { k: 'herHead', x: 0.10, y: 0.04, z: 0.02 }
  },
  cam: { yaw: 0.15, pitch: 0.36, dist: 3.6, fov: 38, target: [0, 0.36, -0.08] }
};

const ORAL_BLOW3 = {
  name: 'ORAL (BLOW)',
  him: {
    pos: [0, 0.44, -0.26], up: [0, 0.45, -0.89], fwd: [0, 0.89, 0.45],
    spine: [0.08, 0, 0], chest: [0.06, 0, 0], head: [-0.30, 0, 0],
    hip: [-1.45, -1.45], hipRot: [0.42, -0.42],
    knee: [1.70, 1.70], ankle: [0.32, 0.32], toes: [0.10, 0.10],
    spread: 0.62,
    arm: [-0.95, -0.95], armZ: [0.72, -0.72], elbow: [-0.35, -0.35], wrist: [0.10, -0.10],
    shaft: -1.25
  },
  her: {
    pos: [0, 0.30, 0.18], up: [0, 0.58, -0.81], fwd: [0, -0.81, -0.58],
    spine: [0.18, 0, 0], chest: [0.14, 0, 0], head: [-0.22, 0, 0],
    hip: [-1.88, -1.88], hipRot: [0.22, -0.22],
    knee: [2.35, 2.35], ankle: [0.55, 0.55], toes: [0.45, 0.45],
    spread: 0.34,
    arm: [-0.85, -0.85], armZ: [0.38, -0.38], elbow: [-0.55, -0.55], wrist: [0.15, -0.15]
  },
  hands: {
    herL: { k: 'himThighL', x: 0.08, y: 0.06, z: 0.06 },
    herR: { k: 'himThighR', x: -0.08, y: 0.06, z: 0.06 },
    himL: { k: 'herHead', x: 0.12, y: 0.06, z: 0.04 },
    himR: { k: 'herShoulderR', x: 0.04, y: 0.02, z: 0.04 }
  },
  cam: { yaw: 0.65, pitch: 0.28, dist: 2.3, fov: 38, target: [0, 0.38, -0.06] }
};

const ORAL3 = ORAL_LICK3;

/* ============================================================
   SOLO POSE: She lies alone on her back in a relaxed, sensual repose.
   Arms free for IK-driven solo masturbation; no partner joints needed.
   pos/up/fwd place her supine, facing camera, centred on the duvet.
   ============================================================ */
const SOLO_POSE3 = {
  name: 'SOLO',
  her: {
    // Supine: lying gracefully on the bed, head at -Z end
    pos: [0, 0.20, -0.22], up: [0, -0.04, -0.999], fwd: [0, 0.999, -0.04],
    // Natural sensual spinal relaxation
    spine: [0.04, 0, 0], chest: [0.02, 0, 0], head: [-0.12, 0.04, 0],
    // Legs: rest naturally on the mattress with soft relaxed knee bend
    hip: [0.06, 0.06], hipRot: [0.10, -0.10],
    knee: [0.20, 0.20], ankle: [0.15, 0.15], toes: [0.12, 0.12],
    spread: 0.26,
    // Arms: relaxed beside torso when not IK-placed
    arm: [0.14, 0.14], armZ: [0.36, -0.36], elbow: [0.18, 0.18], wrist: [0.04, -0.04]
  },
  him: {
    // Him hidden in solo; give a neutral out-of-view placement
    pos: [0, -1.5, 0], up: [0, 1, 0], fwd: [0, 0, 1],
    spine: [0, 0, 0], chest: [0, 0, 0], head: [0, 0, 0],
    hip: [0, 0], hipRot: [0, 0], knee: [0, 0], ankle: [0, 0], toes: [0, 0],
    spread: 0, arm: [0, 0], armZ: [0, 0], elbow: [0, 0], wrist: [0, 0],
    shaft: -Math.PI / 2
  },
  hands: {
    himL: null, himR: null,
    herL: { k: 'herChest', x: -0.08, y: 0.04, z: 0.09 },
    herR: { k: 'herHips', x: 0.04, y: -0.03, z: 0.12 }
  },
  cam: { yaw: 0.50, pitch: 0.30, dist: 3.4, fov: 40, target: [0, 0.35, -0.20] }
};

/* ============================================================
   ACTIVE POSE EVALUATION & BLENDING
   Applies procedural biological harmonics (respiration, recoil,
   tremors) and slerp-smooth transitions between positions.
   ============================================================ */
function currentPose3() {
  if (G.solo) return SOLO_POSE3;
  if ((G.oral || 0) > 0.03) {
    return (G.oralMode === 'blow' || G.oralT === 2) ? ORAL_BLOW3 : ORAL_LICK3;
  }
  return POSES3[(G.pos | 0) % POSES3.length];
}

function evaluatePoseBio3(pose) {
  const bio = getBiomechanicalState();

  // Clone joint vectors to avoid mutating static authored tables
  const evaluated = JSON.parse(JSON.stringify(pose));

  // 1. Respiration chest expansion & spinal arching
  evaluated.her.pos[1] += bio.recoilY + bio.breathHeave * 0.4;
  evaluated.her.chest[0] += bio.breathHeave * 1.5;
  evaluated.her.spine[0] += bio.lordosis * 0.5;

  // 2. Pelvic thrust impact recoil
  evaluated.her.pos[2] -= (G.depth || 0) * 0.025;
  evaluated.him.pos[2] -= (G.depth || 0) * 0.030;

  // 3. Neuromuscular shiver & toe curls
  evaluated.her.pos[0] += bio.tremor * 0.3;
  evaluated.her.knee[0] += bio.tremor * 1.2;
  evaluated.her.knee[1] -= bio.tremor * 1.2;
  evaluated.her.toes[0] = clamp(evaluated.her.toes[0] + bio.toeTension * 0.45, 0, 1.8);
  evaluated.her.toes[1] = clamp(evaluated.her.toes[1] + bio.toeTension * 0.45, 0, 1.8);

  // 4. Male thrust effort & breathing
  evaluated.him.pos[1] += bio.breathHeave * 0.5;
  evaluated.him.chest[0] += bio.breathHeave * 1.2;

  return evaluated;
}

function lerpPose3(a, b, t) {
  if (t <= 0) return a;
  if (t >= 1) return b;

  // Smooth Hermite S-curve interpolation
  const s = t * t * (3 - 2 * t);

  const mix = (x, y) => x + (y - x) * s;
  const mixArr = (p, q) => p.map((v, i) => mix(v, q[i]));
  const mixAngles = (p, q) => p.map((v, i) => angleLerp(v, q[i], s));

  const mixShaft = (p, q) => {
    const sA = p?.shaft ?? null, sB = q?.shaft ?? null;
    if (sA == null) return sB;
    if (sB == null) return sA;
    return angleLerp(sA, sB, s);
  };

  return {
    name: s > 0.5 ? b.name : a.name,
    her: {
      pos: mixArr(a.her.pos, b.her.pos),
      up: mixArr(a.her.up, b.her.up),
      fwd: mixArr(a.her.fwd, b.her.fwd),
      spine: mixAngles(a.her.spine || [0, 0, 0], b.her.spine || [0, 0, 0]),
      chest: mixAngles(a.her.chest || [0, 0, 0], b.her.chest || [0, 0, 0]),
      head: mixAngles(a.her.head, b.her.head),
      hip: mixAngles(a.her.hip, b.her.hip),
      hipRot: mixAngles(a.her.hipRot || [0, 0], b.her.hipRot || [0, 0]),
      knee: mixAngles(a.her.knee, b.her.knee),
      ankle: mixAngles(a.her.ankle || [0, 0], b.her.ankle || [0, 0]),
      toes: mixAngles(a.her.toes || [0, 0], b.her.toes || [0, 0]),
      spread: mix(a.her.spread, b.her.spread),
      arm: mixAngles(a.her.arm, b.her.arm),
      armZ: mixAngles(a.her.armZ, b.her.armZ),
      elbow: mixAngles(a.her.elbow, b.her.elbow),
      wrist: mixAngles(a.her.wrist || [0, 0], b.her.wrist || [0, 0])
    },
    him: {
      pos: mixArr(a.him.pos, b.him.pos),
      up: mixArr(a.him.up, b.him.up),
      fwd: mixArr(a.him.fwd, b.him.fwd),
      spine: mixAngles(a.him.spine || [0, 0, 0], b.him.spine || [0, 0, 0]),
      chest: mixAngles(a.him.chest || [0, 0, 0], b.him.chest || [0, 0, 0]),
      head: mixAngles(a.him.head, b.him.head),
      hip: mixAngles(a.him.hip, b.him.hip),
      hipRot: mixAngles(a.him.hipRot || [0, 0], b.him.hipRot || [0, 0]),
      knee: mixAngles(a.him.knee, b.him.knee),
      ankle: mixAngles(a.him.ankle || [0, 0], b.him.ankle || [0, 0]),
      toes: mixAngles(a.him.toes || [0, 0], b.him.toes || [0, 0]),
      spread: mix(a.him.spread, b.him.spread),
      arm: mixAngles(a.him.arm, b.him.arm),
      armZ: mixAngles(a.him.armZ, b.him.armZ),
      elbow: mixAngles(a.him.elbow, b.him.elbow),
      wrist: mixAngles(a.him.wrist || [0, 0], b.him.wrist || [0, 0]),
      shaft: mixShaft(a.him, b.him)
    },
    hands: s > 0.5 ? b.hands : a.hands,
    cam: {
      yaw: angleLerp(a.cam.yaw, b.cam.yaw, s),
      pitch: mix(a.cam.pitch, b.cam.pitch),
      dist: mix(a.cam.dist, b.cam.dist),
      fov: mix(a.cam.fov || 42, b.cam.fov || 42),
      target: mixArr(a.cam.target, b.cam.target)
    }
  };
}

/* ============================================================
   CAMERA PRESET & DEPTH-OF-FIELD COUPLING
   Smoothly directs orbit camera to authored framing on position switch.
   ============================================================ */
let _lastPoseIdx3 = -1;
function applyPoseCam3(force) {
  const idx = ((G.oral || 0) > 0.03) ? -1 : (G.pos | 0);
  if (idx === _lastPoseIdx3 && !force) return;
  _lastPoseIdx3 = idx;
  const p = currentPose3();
  if (!p || !p.cam || !window.CAM3) return;
  if (CAM3.mode !== 'orbit') return;

  CAM3.yaw = p.cam.yaw;
  CAM3.pitch = p.cam.pitch;
  CAM3.dist = p.cam.dist;
  if (p.cam.fov && CAM3.camera) {
    CAM3.camera.fov = p.cam.fov;
    CAM3.camera.updateProjectionMatrix();
  }
  CAM3.target.set(p.cam.target[0], p.cam.target[1], p.cam.target[2]);
}

/* ============================================================
   WORLD ANCHORS & CONTACT DETECTION SOLVERS
   Pinpoints exact 3D coordinates for introitus penetration,
   clitoral stimulation, and breast bounce oscillators.
   ============================================================ */
function vulvaWorld3(her) {
  if (her.vulva) {
    her.vulva.updateWorldMatrix(true, false);
    return her.vulva.localToWorld(new THREE.Vector3(0, -0.058, 0.030));
  }
  const v = new THREE.Vector3(0, -0.046, 0.116);
  her.hips.localToWorld(v);
  return v;
}

function clitorisWorld3(her) {
  if (her.vulva) {
    her.vulva.updateWorldMatrix(true, false);
    return her.vulva.localToWorld(new THREE.Vector3(0, -0.038, 0.044));
  }
  const v = new THREE.Vector3(0, -0.032, 0.130);
  her.hips.localToWorld(v);
  return v;
}

function shaftTipWorld3(him) {
  const v = new THREE.Vector3(0, -0.34 * (1 + (G.shaftPulse || 0) * 0.08), 0);
  him.shaftRoot.localToWorld(v);
  return v;
}

function breastWorld3(her, side) {
  const g = side < 0 ? her.breastL : her.breastR;
  const v = new THREE.Vector3(0, -0.032, 0.054);
  g.localToWorld(v);
  return v;
}