# Dump the Goat-chan reference rig's edit-bone frames (head, tail, roll, x/z axes) -> /root/agb/goat_bones.json.
# The MMD skeleton built by rig_export.py copies these frames bone by bone.
# goatchan.glb is no longer in the tree; extract it from history first:
#   git show 1b10cf2:assets/goatchan/goatchan.glb > /root/agb/goatchan.glb
#   blender -b --python dump_goat.py
import bpy, json
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath="/root/agb/goatchan.glb", bone_heuristic="BLENDER")
arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
weighted = set()
for o in bpy.data.objects:
    if o.type == "MESH":
        for vg in o.vertex_groups: weighted.add(vg.name)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
out = [dict(n=b.name, p=b.parent.name if b.parent else None, h=list(b.head), t=list(b.tail), roll=b.roll,
            x=list(b.x_axis), z=list(b.z_axis), w=b.name in weighted) for b in arm.data.edit_bones]
json.dump(out, open("/root/agb/goat_bones.json", "w"), ensure_ascii=False)
print("GOAT_BONES", len(out))
