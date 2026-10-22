// Marcus GLB post-process (game): material modes, compact textures, strip COLOR_*, quantize, sparse morphs.
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import { prune, dedup, quantize, sparse } from '@gltf-transform/functions';
import fs from 'fs';
const [inp, out, texdir] = process.argv.slice(2);
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS);
const doc = await io.read(inp); const root = doc.getRoot();
const SWAP = { marcus_skin_caramel: 'skin_caramel.jpg', marcus_skin_normal: 'marcus_skin_normal_2k.jpg', marcus_skin_orm: 'marcus_skin_orm_2k.jpg', teeth: 'marcus_teeth_1k.jpg', tongue01_diffuse: 'marcus_tongue_512.jpg' };
for (const t of root.listTextures()) { const f = SWAP[t.getName()]; if (f) t.setImage(fs.readFileSync(`${texdir}/${f}`)).setMimeType('image/jpeg').setURI(f); }
for (const mesh of root.listMeshes()) for (const p of mesh.listPrimitives())
  for (const sem of p.listSemantics()) if (sem.startsWith('COLOR_')) p.setAttribute(sem, null);
for (const m of root.listMaterials()) {
  let n = m.getName(); if (n.startsWith('marcus_genital')) { n = 'marcus_genital'; m.setName(n); }
  if (n === 'marcus_hair') m.setAlphaMode('MASK').setAlphaCutoff(0.4).setDoubleSided(true);
  else if (n === 'marcus_brow' || n === 'marcus_lash') m.setAlphaMode('MASK').setAlphaCutoff(0.3).setDoubleSided(true);
  else if (n === 'marcus_eye') m.setAlphaMode('MASK').setAlphaCutoff(0.5).setDoubleSided(false);
  else m.setAlphaMode('OPAQUE').setDoubleSided(false);
  m.setMetallicFactor(0);
}
// no sparse(): all-zero morph accessors become bufferView-less, which three r150 GLTFLoader turns into null morph attributes
await doc.transform(dedup(), prune(), quantize({ quantizePosition: 14, quantizeNormal: 10, quantizeTexcoord: 12, quantizeWeight: 8 }));
await io.write(out, doc);
console.log('wrote', out, (fs.statSync(out).size / 1e6).toFixed(2), 'MB');
