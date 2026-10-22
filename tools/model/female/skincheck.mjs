import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
const io = new NodeIO().registerExtensions(ALL_EXTENSIONS);
for (const f of process.argv.slice(2)) {
  const doc = await io.read(f); const root = doc.getRoot();
  console.log(f, 'skins', root.listSkins().length, root.listSkins().map(s => s.listJoints().length));
  for (const n of root.listNodes()) if (n.getMesh()) console.log('  node', n.getName(), 'mesh', n.getMesh().getName(), 'skin', n.getSkin() ? n.getSkin().listJoints().length : null,
    'attrs', n.getMesh().listPrimitives()[0].listSemantics().join(','), 'type', n.getMesh().listPrimitives()[0].getAttribute('POSITION').getComponentType());
}
