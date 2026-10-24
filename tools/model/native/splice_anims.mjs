// Replace the animations of a shipped character GLB with freshly baked ones, touching nothing else.
//   node splice_anims.mjs <shipped.glb> <raw_export.glb> <out.glb> [clip,clip,...]
// The shipped GLBs carry later work done on the meshes (faces, expression morphs, eyes, hair, identity data):
// re-exporting them from the authoring blend would undo it. Only animations move here: each raw clip (authoring
// prefix VIV| / MAR| stripped) replaces the shipped clip of the same name; channels are bound by node (bone) name.
// Checks before writing: every animated bone exists in the shipped skeleton and its rest transform matches the raw
// export's (else the clip would be posed against another skeleton) — the script fails loudly instead.
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import fs from 'fs';

const [shippedPath, rawPath, outPath, onlyArg] = process.argv.slice(2);
const only = onlyArg ? new Set(onlyArg.split(',')) : null;
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS);
const dst = await io.read(shippedPath);
const src = await io.read(rawPath);
const D = dst.getRoot(), S = src.getRoot();
const byName = new Map(D.listNodes().map(n => [n.getName(), n]));
const buffer = D.listBuffers()[0];

// rest transforms must agree (same skeleton), else the baked rotations mean something else on the shipped rig
let worstT = 0, worstR = 0, worstWho = '';
for (const sn of S.listNodes()) {
  const dn = byName.get(sn.getName());
  if (!dn || !S.listSkins().some(sk => sk.listJoints().includes(sn))) continue;
  const a = sn.getTranslation(), b = dn.getTranslation(), qa = sn.getRotation(), qb = dn.getRotation();
  const dt = Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
  const dq = 1 - Math.abs(qa[0] * qb[0] + qa[1] * qb[1] + qa[2] * qb[2] + qa[3] * qb[3]);
  if (dt > worstT) { worstT = dt; worstWho = sn.getName(); }
  worstR = Math.max(worstR, dq);
}
console.log(`rest check: max joint offset ${(worstT * 1000).toFixed(3)} mm (${worstWho}), max rotation 1-|dot| ${worstR.toExponential(2)}`);
if (worstT > 0.0005 || worstR > 1e-5) { console.error('SKELETON MISMATCH: refusing to splice'); process.exit(2); }

const copyAcc = (a) => {
  const c = dst.createAccessor(a.getName()).setType(a.getType()).setArray(a.getArray().slice()).setBuffer(buffer);
  if (a.getNormalized()) c.setNormalized(true);
  return c;
};
const names = [];
const replaced = new Set();
for (const sa of S.listAnimations()) {
  const nm = sa.getName().replace(/^(VIV|MAR)\|/, '');
  if (only && !only.has(nm)) continue;
  if (replaced.has(nm)) continue;                       // the exporter can repeat a strip: first one wins
  // drop the shipped clip of that name (+ accessors used only by it)
  for (const old of D.listAnimations().filter(a => a.getName() === nm)) {
    const accs = new Set();
    for (const smp of old.listSamplers()) { accs.add(smp.getInput()); accs.add(smp.getOutput()); }
    old.listChannels().forEach(c => c.dispose()); old.listSamplers().forEach(s => s.dispose()); old.dispose();
    for (const a of accs) if (a && a.listParents().every(p => p.propertyType === 'Root')) a.dispose();
  }
  const da = dst.createAnimation(nm);
  const inputs = new Map();
  let skipped = 0;
  for (const ch of sa.listChannels()) {
    const tn = ch.getTargetNode(); const dn = tn && byName.get(tn.getName());
    if (!dn) { skipped++; continue; }
    const smp = ch.getSampler();
    let inp = inputs.get(smp.getInput());
    if (!inp) { inp = copyAcc(smp.getInput()); inputs.set(smp.getInput(), inp); }
    const ds = dst.createAnimationSampler().setInput(inp).setOutput(copyAcc(smp.getOutput())).setInterpolation(smp.getInterpolation());
    da.addSampler(ds);
    da.addChannel(dst.createAnimationChannel().setTargetNode(dn).setTargetPath(ch.getTargetPath()).setSampler(ds));
  }
  if (skipped) console.log(`  ${nm}: ${skipped} channels without a shipped node (skipped)`);
  replaced.add(nm); names.push(nm);
}
await io.write(outPath, dst);
const a0 = fs.statSync(shippedPath).size, a1 = fs.statSync(outPath).size;
console.log(`spliced ${names.length} clips -> ${outPath} (${(a0 / 1e6).toFixed(2)} -> ${(a1 / 1e6).toFixed(2)} MB); clips now: ${D.listAnimations().map(a => a.getName()).sort().join(', ')}`);
