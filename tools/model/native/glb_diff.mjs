// Compare two character GLBs on everything but animations: meshes (every attribute + morph target array),
// skins (joints + inverse binds), nodes (names, rest TRS, extras), materials and textures (bytes).
//   node glb_diff.mjs a.glb b.glb      -> prints differences, exit 1 if any
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import crypto from 'crypto';
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS);
const [pa, pb] = process.argv.slice(2);
const A = (await io.read(pa)).getRoot(), B = (await io.read(pb)).getRoot();
const h = (x) => crypto.createHash('md5').update(Buffer.from(x.buffer, x.byteOffset, x.byteLength)).digest('hex');
const sig = (R) => {
  const out = [];
  for (const m of R.listMeshes()) {
    out.push(`mesh ${m.getName()} extras ${JSON.stringify(m.getExtras())} weights ${m.getWeights().length}`);
    m.listPrimitives().forEach((p, pi) => {
      for (const s of p.listSemantics()) out.push(`  ${m.getName()}#${pi} ${s} ${h(p.getAttribute(s).getArray())}`);
      if (p.getIndices()) out.push(`  ${m.getName()}#${pi} idx ${h(p.getIndices().getArray())}`);
      p.listTargets().forEach((t, ti) => { for (const s of t.listSemantics()) out.push(`  ${m.getName()}#${pi} T${ti} ${s} ${h(t.getAttribute(s).getArray())}`); });
      out.push(`  ${m.getName()}#${pi} mat ${p.getMaterial()?.getName()}`);
    });
  }
  for (const s of R.listSkins()) out.push(`skin ${s.listJoints().map(j => j.getName()).join(',')} ibm ${h(s.getInverseBindMatrices().getArray())}`);
  for (const n of R.listNodes()) out.push(`node ${n.getName()} ${n.getTranslation().map(v => v.toFixed(6))} ${n.getRotation().map(v => v.toFixed(6))} ${n.getScale()} ${JSON.stringify(n.getExtras())}`);
  for (const m of R.listMaterials()) out.push(`mat ${m.getName()} ${m.getAlphaMode()} ${m.getBaseColorFactor()} ${m.getDoubleSided()}`);
  for (const t of R.listTextures()) out.push(`tex ${t.getName()} ${t.getMimeType()} ${h(t.getImage())}`);
  return out;
};
const sa = sig(A), sb = sig(B);
const setB = new Set(sb), setA = new Set(sa);
const onlyA = sa.filter(x => !setB.has(x)), onlyB = sb.filter(x => !setA.has(x));
console.log(`compared ${sa.length} / ${sb.length} signatures: ${onlyA.length} only in A, ${onlyB.length} only in B`);
for (const x of onlyA.slice(0, 20)) console.log('  A:', x.slice(0, 200));
for (const x of onlyB.slice(0, 20)) console.log('  B:', x.slice(0, 200));
process.exit(onlyA.length || onlyB.length ? 1 : 0);
