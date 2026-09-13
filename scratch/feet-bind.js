(function(){GLB_PARTS.feet.gain=0;var r=GLB_STORE[GLB_MODEL.key].rig;r.bones.forEach(function(b){if(b.part==="feet")b.dst.rotation.set(b.bx,b.by,b.bz);});return "feet frozen at bind";})()
