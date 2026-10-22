// usage: node inspect_glb.mjs file.glb [goatchan.glb]
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS);
const doc = await io.read(process.argv[2]); const root = doc.getRoot();
console.log('textures:'); for (const t of root.listTextures()) console.log(' ', t.getName() || t.getURI(), t.getMimeType(), (t.getImage().byteLength / 1e6).toFixed(2), 'MB', t.getSize());
console.log('meshes:');
for (const m of root.listMeshes()) {
  let bytes = 0, verts = 0, tg = 0;
  for (const p of m.listPrimitives()) {
    for (const s of p.listSemantics()) bytes += p.getAttribute(s).getByteLength();
    verts += p.getAttribute('POSITION').getCount();
    for (const t of p.listTargets()) { tg++; for (const s of t.listSemantics()) bytes += t.getAttribute(s).getByteLength(); }
    if (p.getIndices()) bytes += p.getIndices().getByteLength();
  }
  console.log(' ', m.getName(), 'verts', verts, 'targets', tg, 'names', JSON.stringify(m.getExtras().targetNames || []), (bytes / 1e6).toFixed(2), 'MB');
}
console.log('materials:'); for (const m of root.listMaterials()) console.log(' ', m.getName(), m.getAlphaMode(), m.getAlphaCutoff(), 'double', m.getDoubleSided(), 'rough', m.getRoughnessFactor(), 'metal', m.getMetallicFactor(), 'base', !!m.getBaseColorTexture(), 'nrm', !!m.getNormalTexture(), 'mr', !!m.getMetallicRoughnessTexture(), 'occ', !!m.getOcclusionTexture());
const sk = root.listSkins()[0]; console.log('joints', sk.listJoints().length);
// world-space joint axes (glTF Y-up) for mapped bones, compared with a reference model
function axes(doc) {
  const out = {};
  for (const j of doc.getRoot().listSkins()[0].listJoints()) {
    const m = j.getWorldMatrix(); const n = j.getName().replace(/[._]/g, '');
    const nx = Math.hypot(m[0], m[1], m[2]), ny = Math.hypot(m[4], m[5], m[6]), nz = Math.hypot(m[8], m[9], m[10]);
    out[n] = { x: [m[0] / nx, m[1] / nx, m[2] / nx], y: [m[4] / ny, m[5] / ny, m[6] / ny], z: [m[8] / nz, m[9] / nz, m[10] / nz], p: [m[12], m[13], m[14]], child: j.listChildren().filter(c => c.getName()).map(c => c.getTranslation()) };
  }
  return out;
}
if (process.argv[3]) {
  const ref = await io.read(process.argv[3]);
  const A = axes(doc), B = axes(ref);
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  console.log('bone         xdot  ydot  zdot   childAlongY');
  for (const k of ['上半身', '上半身2', '首', '頭', '下半身', '肩L', '腕L', 'ひじL', '手首L', '腕R', 'ひじR', '足L', 'ひざL', '足首L', 'つま先L', '足R', 'ひざR', '人指０L', '人指１L', '親指０L', '乳親L']) {
    if (!A[k] || !B[k]) { console.log(k, 'missing', !!A[k], !!B[k]); continue; }
    const c = A[k].child[0]; const along = c ? (c[1] / Math.hypot(...c)).toFixed(2) : '-';
    console.log(k.padEnd(8), dot(A[k].x, B[k].x).toFixed(2), dot(A[k].y, B[k].y).toFixed(2), dot(A[k].z, B[k].z).toFixed(2), '  ', along);
  }
}
