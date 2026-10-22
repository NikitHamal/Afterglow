// Vivian GLB post-process: game material modes, compact textures, quantized geometry, sparse morphs.
// usage: node optimize.mjs in.glb out.glb texdir
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import { prune, dedup, quantize } from '@gltf-transform/functions';
import fs from 'fs';
const [inp, out, texdir] = process.argv.slice(2);
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS);
const doc = await io.read(inp); const root = doc.getRoot();
const SWAP = {
  vivian_skin_normal: 'vivian_skin_normal_2k.jpg', vivian_skin_orm: 'vivian_skin_orm_2k.jpg',
  teeth: 'vivian_teeth_1k.jpg', tongue01_diffuse: 'vivian_tongue_512.jpg',
};
for (const t of root.listTextures()) {
  const f = SWAP[t.getName()];
  if (!f) continue;
  t.setImage(fs.readFileSync(`${texdir}/${f}`)).setMimeType('image/jpeg').setURI(f);
}
// vertex-colour attributes carried paint masks only; GLTFLoader would multiply them into the albedo
for (const mesh of root.listMeshes()) for (const p of mesh.listPrimitives())
  for (const sem of p.listSemantics()) if (sem.startsWith('COLOR_')) p.setAttribute(sem, null);
for (const m of root.listMaterials()) {
  const n = m.getName();
  if (n === 'vivian_hair') m.setAlphaMode('MASK').setAlphaCutoff(0.42).setDoubleSided(true);
  else if (n === 'vivian_brow' || n === 'vivian_lash') m.setAlphaMode('MASK').setAlphaCutoff(0.3).setDoubleSided(true);
  else if (n === 'vivian_eye') m.setAlphaMode('MASK').setAlphaCutoff(0.5).setDoubleSided(false);
  else m.setAlphaMode('OPAQUE').setDoubleSided(false);
  m.setMetallicFactor(0);
}
await doc.transform(
  dedup(), prune(),
  quantize({ quantizePosition: 14, quantizeNormal: 10, quantizeTexcoord: 12, quantizeWeight: 8 }),
);
await io.write(out, doc);
console.log('wrote', out, (fs.statSync(out).size / 1e6).toFixed(2), 'MB');
