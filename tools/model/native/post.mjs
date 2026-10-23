// Native character GLB post-process (gltf-transform):
//   node post.mjs in_raw.glb out.glb who texdir
// - animation names: strip the authoring prefixes (VIV| / MAR|)
// - material modes for a plain PBR renderer: skin/teeth/tongue OPAQUE single-sided, hair/brows/lashes MASK
//   double-sided, eyes MASK, metallic 0 everywhere (no metal skin -> no black man)
// - texture budget: normal/ORM swapped for the 2K versions in texdir (albedo stays 4K)
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import { prune, dedup } from '@gltf-transform/functions';
import fs from 'fs';
const [inp, out, who, texdir] = process.argv.slice(2);
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS);
const doc = await io.read(inp); const root = doc.getRoot();
const seen = new Set();
for (const a of root.listAnimations()) {
  const nm = a.getName().replace(/^(VIV|MAR)\|/, '');
  if (seen.has(nm)) { a.dispose(); continue; }
  seen.add(nm); a.setName(nm);
}
// morph normal deltas only where shading needs them (faces, openings, aroused); body-shape and hair carries
// are POSITION-only (a missing NORMAL delta means zero) — keeps the files inside the budget
for (const mesh of root.listMeshes()) {
  const names = (mesh.getExtras() && mesh.getExtras().targetNames) || [];
  const isHair = /hair/.test(mesh.getName());
  for (const p of mesh.listPrimitives()) p.listTargets().forEach((tg, i) => {
    const nm = names[i] || '';
    if (isHair) { const n = tg.getAttribute('NORMAL'); if (n) tg.setAttribute('NORMAL', null); }
  });
}
for (const m of root.listMaterials()) {
  const n = m.getName().toLowerCase();
  m.setMetallicFactor(0);
  if (/hair/.test(n)) m.setAlphaMode('MASK').setAlphaCutoff(0.42).setDoubleSided(true);
  else if (/brow|lash/.test(n)) m.setAlphaMode('MASK').setAlphaCutoff(0.3).setDoubleSided(true);
  else if (/eye/.test(n)) m.setAlphaMode('MASK').setAlphaCutoff(0.5).setDoubleSided(false);
  else m.setAlphaMode('OPAQUE').setDoubleSided(false);
}
const swaps = fs.existsSync(`${texdir}/swaps.json`) ? JSON.parse(fs.readFileSync(`${texdir}/swaps.json`)) : {};
for (const t of root.listTextures()) {
  const key = Object.keys(swaps).find(k => (t.getName() || t.getURI() || '').includes(k));
  if (!key) continue;
  t.setImage(fs.readFileSync(`${texdir}/${swaps[key]}`)).setMimeType('image/jpeg').setURI(swaps[key]);
}
// vertex colours only where they carry data (Marcus' hairline fringe alpha); never on skin
for (const mesh of root.listMeshes()) {
  const keep = /hair/.test(mesh.getName());
  for (const p of mesh.listPrimitives()) for (const sem of p.listSemantics())
    if (sem.startsWith('COLOR_') && !keep) p.setAttribute(sem, null);
}
root.getAsset().extras = { afterglow: { character: who, units: 'm', up: '+Y', version: 1 } };
await doc.transform(dedup(), prune({ keepLeaves: true }));
await io.write(out, doc);
const anims = root.listAnimations().map(a => a.getName());
console.log('wrote', out, (fs.statSync(out).size / 1e6).toFixed(2), 'MB', 'animations:', anims.length, anims.join(','));
