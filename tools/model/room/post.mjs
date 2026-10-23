// Room post-process (gltf-transform 4.x, run from a dir with @gltf-transform/{core,extensions,functions}):
//   node post.mjs room_raw.glb room.json out_dir
// -> out_dir/room.glb + out_dir/room.json. Scene extras get {"afterglow": <room.json>} (engine: bedTop, bedCenter, lamp);
// lit materials keep the baked lightmap as emissiveTexture on texCoord 1 (engine: room colour = base x emissive(uv1)).
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import { dedup, prune } from '@gltf-transform/functions';
import fs from 'fs';
import path from 'path';

const [raw, metaPath, outDir] = process.argv.slice(2);
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS);
const doc = await io.read(raw);
const root = doc.getRoot();
const meta = JSON.parse(fs.readFileSync(metaPath, 'utf8'));
if (meta.bed && !meta.bed.size) meta.bed.size = [meta.bed.width, meta.bed.length];   // engine reads bed.size [w, l]

const scene = root.getDefaultScene() || root.listScenes()[0];
scene.setExtras({ ...(scene.getExtras() || {}), afterglow: meta });

let baked = 0, plain = 0, wrongUV = 0;
for (const m of root.listMaterials()) {
  m.setMetallicFactor(0);
  const t = m.getEmissiveTexture();
  if (!t) { plain++; continue; }
  const uv = m.getEmissiveTextureInfo().getTexCoord();
  if (t.getName() === 'room_lightmap' || /lightmap/i.test(t.getURI() || t.getName() || '')) {
    if (uv !== 1) wrongUV++;
    m.setEmissiveFactor([1, 1, 1]);
    baked++;
  }
}
await doc.transform(dedup(), prune());

const lights = (root.getExtension('KHR_lights_punctual')?.listProperties() || [])
  .map((l) => `${l.getName()}:${l.getType()}:${l.getIntensity().toFixed(2)}`);
fs.mkdirSync(outDir, { recursive: true });
await io.write(path.join(outDir, 'room.glb'), doc);
fs.writeFileSync(path.join(outDir, 'room.json'), JSON.stringify(meta, null, 1));
const mb = fs.statSync(path.join(outDir, 'room.glb')).size / 1e6;
console.log(`room.glb ${mb.toFixed(2)} MB | baked materials ${baked} (not on UV1: ${wrongUV}), unbaked ${plain} | ` +
  `meshes ${root.listMeshes().length}, textures ${root.listTextures().length} | lights ${lights.join(' ')}`);
if (wrongUV) process.exit(2);
