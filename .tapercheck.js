// Validates taperTube3 geometry: counts, outward normals, sane bounds.
const fs = require('fs');
const vm = require('vm');
const THREE = require('F:\\Afterglow\\vendor\\three.min.js');
global.THREE = THREE;
global.document = {
  createElement: () => ({ width: 1, height: 1, getContext: () => null }),
  getElementById: () => null, addEventListener() {}
};
global.window = { addEventListener() {}, devicePixelRatio: 1 };
global.clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
vm.runInThisContext(fs.readFileSync('F:\\Afterglow\\js3d\\chars3d.js', 'utf8'), { filename: 'chars3d.js' });

const SEGS = 16, RAD = 7;
const m = taperTube3([[0, 0, 0], [0.1, -0.1, 0], [0.2, -0.3, 0.05], [0.25, -0.5, 0.1]], 0.016, 0.003, null, SEGS, RAD);
const g = m.geometry;
const pos = g.getAttribute('position'), nor = g.getAttribute('normal'), idx = g.getIndex();
const expVerts = (SEGS + 1) * (RAD + 1) + 2;
const expIdx = SEGS * RAD * 6 + RAD * 3 * 2;
console.log(`verts=${pos.count} (exp ${expVerts}) idx=${idx.count} (exp ${expIdx})`);

// mid-ring: normals must point away from the ring centre (outward)
let badN = 0;
const ring = 8, cx = [], center = new THREE.Vector3();
for (let j = 0; j <= RAD; j++) {
  const vi = ring * (RAD + 1) + j;
  center.x += pos.getX(vi); center.y += pos.getY(vi); center.z += pos.getZ(vi);
}
center.multiplyScalar(1 / (RAD + 1));
for (let j = 0; j <= RAD; j++) {
  const vi = ring * (RAD + 1) + j;
  const dx = pos.getX(vi) - center.x, dy = pos.getY(vi) - center.y, dz = pos.getZ(vi) - center.z;
  const d = nor.getX(vi) * dx + nor.getY(vi) * dy + nor.getZ(vi) * dz;
  if (d <= 0) badN++;
}
console.log(`inwardNormals=${badN} (must be 0)`);

// triangle winding: every face normal must agree with its vertex normals
let badW = 0;
const A = new THREE.Vector3(), B = new THREE.Vector3(), C = new THREE.Vector3(), e1 = new THREE.Vector3(), e2 = new THREE.Vector3(), fn = new THREE.Vector3(), vn = new THREE.Vector3();
for (let f = 0; f < idx.count; f += 3) {
  const a = idx.getX(f), b = idx.getX(f + 1), c = idx.getX(f + 2);
  A.fromBufferAttribute(pos, a); B.fromBufferAttribute(pos, b); C.fromBufferAttribute(pos, c);
  e1.subVectors(B, A); e2.subVectors(C, A); fn.crossVectors(e1, e2);
  vn.set(nor.getX(a), nor.getY(a), nor.getZ(a));
  if (fn.dot(vn) <= 0) badW++;
}
console.log(`flippedFaces=${badW} (must be 0)`);
g.computeBoundingBox();
const bb = g.boundingBox;
console.log(`bbox x[${bb.min.x.toFixed(3)},${bb.max.x.toFixed(3)}] y[${bb.min.y.toFixed(3)},${bb.max.y.toFixed(3)}]`);
if (pos.count !== expVerts || idx.count !== expIdx || badN > 0 || badW > 0) process.exit(1);
console.log('TAPER TUBE OK');
