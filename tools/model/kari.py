import argparse
import gzip
import hashlib
import json
import math
import struct
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent / 'hair'))
import hairkit
import hairio
import styles


def checkpoint(path):
    if path.suffix == '.glb':
        packed = path.with_suffix('.glb.gz')
        with path.open('rb') as source, gzip.open(packed, 'wb', compresslevel=4) as target:
            import shutil
            shutil.copyfileobj(source, target)
        path = packed
    manifest_path = path.parent / 'checkpoints.json'
    manifest = json.loads(manifest_path.read_text())
    manifest[path.name] = {'bytes': path.stat().st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    pending = manifest_path.with_suffix('.tmp')
    pending.write_text(json.dumps(manifest))
    pending.replace(manifest_path)
    print('CHECKPOINT', path.name, flush=True)


def coordinates(data):
    out = np.empty(len(data) * 3, dtype=np.float32)
    data.foreach_get('co', out)
    return out.reshape(-1, 3)


def sculpt(points):
    p = points.astype(np.float64)
    d = np.zeros_like(p)

    def field(center, radius):
        return np.exp(-0.5 * np.sum(((p - center) / radius) ** 2, axis=1))

    for side in (-1, 1):
        w = field((side * 0.045, -0.120, 1.535), (0.023, 0.035, 0.025))
        d[:, 0] += side * 0.004 * w
        d[:, 1] -= 0.0035 * w
        w = field((side * 0.042, -0.105, 1.490), (0.025, 0.045, 0.019))
        d[:, 0] += side * 0.002 * w
    w = field((0, -0.116, 1.464), (0.04, 0.05, 0.019))
    d[:, 2] += 0.009 * w
    d[:, 1] += 0.002 * w
    w = field((0, -0.158, 1.540), (0.014, 0.018, 0.025))
    d[:, 0] -= p[:, 0] * 0.07 * w
    d[:, 1] += 0.0015 * w
    w = field((0, -0.143, 1.510), (0.022, 0.019, 0.010))
    d[:, 1] -= 0.001 * w
    return (p + d).astype(np.float32)


def shape(obj, amounts, face=False):
    keys = obj.data.shape_keys
    if not keys:
        return
    base = coordinates(keys.key_blocks[0].data)
    delta = np.zeros_like(base)
    for name, value in amounts.items():
        key = keys.key_blocks.get(name)
        if key:
            delta += value * (coordinates(key.data) - base)
    for key in keys.key_blocks:
        p = coordinates(key.data) + delta
        if face:
            p = sculpt(p)
        key.data.foreach_set('co', p.ravel())
        key.value = 0
    obj.data.vertices.foreach_set('co', coordinates(keys.key_blocks[0].data).ravel())
    obj.data.update()
    if obj.data.has_custom_normals:
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.mesh.customdata_custom_splitnormals_clear()


def set_base_texture(obj, image):
    for material in obj.data.materials:
        if not material or not material.use_nodes:
            continue
        bsdf = next((n for n in material.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
        if not bsdf:
            continue
        links = bsdf.inputs['Base Color'].links
        if links and links[0].from_node.type == 'TEX_IMAGE':
            links[0].from_node.image = image


def material(name, image, color, alpha=True):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bsdf = nodes.get('Principled BSDF')
    tex = nodes.new('ShaderNodeTexImage')
    tex.image = image
    tint = nodes.new('ShaderNodeMix')
    tint.data_type = 'RGBA'
    tint.blend_type = 'MULTIPLY'
    tint.inputs[0].default_value = 1
    tint.inputs[7].default_value = (*color, 1)
    mat.node_tree.links.new(tex.outputs['Color'], tint.inputs[6])
    coverage = nodes.new('ShaderNodeVertexColor')
    coverage.layer_name = 'Color'
    color_mix = nodes.new('ShaderNodeMix')
    color_mix.data_type = 'RGBA'
    color_mix.blend_type = 'MULTIPLY'
    color_mix.inputs[0].default_value = 1
    mat.node_tree.links.new(tint.outputs[2], color_mix.inputs[6])
    mat.node_tree.links.new(coverage.outputs['Color'], color_mix.inputs[7])
    mat.node_tree.links.new(color_mix.outputs[2], bsdf.inputs['Base Color'])
    if alpha:
        opacity = nodes.new('ShaderNodeMath')
        opacity.operation = 'MULTIPLY'
        mat.node_tree.links.new(tex.outputs['Alpha'], opacity.inputs[0])
        mat.node_tree.links.new(coverage.outputs['Alpha'], opacity.inputs[1])
        mat.node_tree.links.new(opacity.outputs[0], bsdf.inputs['Alpha'])
    else:
        mat.node_tree.links.new(coverage.outputs['Alpha'], bsdf.inputs['Alpha'])
    bsdf.inputs['Roughness'].default_value = 0.74
    bsdf.inputs['Metallic'].default_value = 0
    bsdf.inputs['Specular IOR Level'].default_value = 0.08
    mat.diffuse_color = (*color, 1)
    mat.surface_render_method = 'BLENDED'
    mat.use_backface_culling = False
    return mat


def make_hair(body, eyes, rig, out):
    h = hairkit.Head(body.name, eyes.name, rig.name, np.random.default_rng(41))
    builder = styles.long_waves(h, h.rng)
    atlas_path, cap_path = out / 'kari_hair_atlas.png', out / 'kari_hair_cap.png'
    hairio.strand_atlas(np.random.default_rng(42)).save(atlas_path)
    hairio.cap_texture(np.random.default_rng(43)).save(cap_path)
    mats = [material('kari_hair', bpy.data.images.load(str(atlas_path)), (0.075, 0.035, 0.020)),
            material('kari_hair_cap', bpy.data.images.load(str(cap_path)), (0.038, 0.018, 0.012), False)]
    mesh = bpy.data.meshes.new('kari_long_waves')
    mesh.from_pydata(builder.P, [], builder.F)
    mesh.update()
    obj = bpy.data.objects.new('kari_long_waves', mesh)
    bpy.context.collection.objects.link(obj)
    obj.parent = rig
    for mat in mats:
        mesh.materials.append(mat)
    coverage = mesh.color_attributes.new(name='Color', type='FLOAT_COLOR', domain='POINT')
    colors = np.ones((len(builder.P), 4), dtype=np.float32)
    colors[:, 3] = builder.A
    coverage.data.foreach_set('color', colors.ravel())
    uv = mesh.uv_layers.new(name='UVMap')
    for polygon, mat_index in zip(mesh.polygons, builder.FM):
        polygon.material_index = mat_index
        polygon.use_smooth = True
        for loop_index in polygon.loop_indices:
            vertex = mesh.loops[loop_index].vertex_index
            u, v = builder.UV[vertex]
            uv.data[loop_index].uv = (u, v)
    positions = np.asarray(builder.P)
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    heights = np.linspace(h.EC[2] - 0.005, h.CHIN_Z - 0.38 * h.k, 6)
    for chain in range(4):
        side = 1 if chain % 2 == 0 else -1
        front = chain < 2
        region = positions[(positions[:, 0] * side > 0.025) & ((positions[:, 1] < h.Ch[1]) == front)]
        points = []
        for height in heights:
            nearest = region[np.argsort(np.abs(region[:, 2] - height))[:250]]
            point = nearest.mean(axis=0)
            point[2] = height
            points.append(point)
        for index, suffix in enumerate('abcdz'):
            bone = rig.data.edit_bones[f'髪.{chain}.{suffix}']
            bone.use_connect = False
            bone.head, bone.tail = points[index], points[index + 1]
    bpy.ops.object.mode_set(mode='OBJECT')
    groups = {name: obj.vertex_groups.new(name=name) for name in [hairkit.HEAD, '上半身2'] +
              [f'髪.{chain}.{suffix}' for chain in range(4) for suffix in 'abcdz']}
    for index, p in enumerate(positions):
        head_weight = float(hairkit.ss(h.EC[2] - 0.065, h.EC[2] + 0.020, p[2]))
        if head_weight >= 0.999:
            groups[hairkit.HEAD].add([index], 1, 'REPLACE')
            continue
        chain = (0 if p[0] >= 0 else 1) + (0 if p[1] < h.Ch[1] else 2)
        progress = float(np.clip((heights[0] - p[2]) / (heights[0] - heights[4]) * 4, 0, 3.999))
        segment = int(progress)
        fraction = progress - segment
        chest_weight = float(hairkit.ss(h.CHIN_Z - 0.08, h.CHIN_Z - 0.30, p[2])) * 0.30
        loose = 1 - head_weight - chest_weight
        weights = {hairkit.HEAD: head_weight, '上半身2': chest_weight,
                   f'髪.{chain}.{"abcdz"[segment]}': loose * (1 - fraction),
                   f'髪.{chain}.{"abcdz"[segment + 1]}': loose * fraction}
        for name, weight in weights.items():
            if weight > 0:
                groups[name].add([index], weight, 'REPLACE')
    mod = obj.modifiers.new('Kari hair rig', 'ARMATURE')
    mod.object = rig
    obj.shape_key_add(name='Basis')
    for name, delta in zip(('weight_up', 'weight_dn'), h.morph_deltas(np.asarray(builder.P))):
        key = obj.shape_key_add(name=name)
        key.data.foreach_set('co', (np.asarray(builder.P) + delta).astype(np.float32).ravel())
    return obj


def seated_pose(rig, phase):
    for bone in rig.pose.bones:
        bone.matrix_basis.identity()
    root = rig.pose.bones['全ての親']
    root.matrix = Matrix.Translation((0, 0, -0.33)) @ root.bone.matrix_local
    bpy.context.view_layer.update()
    spine = rig.pose.bones['上半身2']
    center = spine.head.copy()
    spine.matrix = Matrix.Translation(center) @ Matrix.Rotation(math.radians(4 + 0.35 * math.sin(phase)), 4, 'X') @ Matrix.Translation(-center) @ spine.matrix
    bpy.context.view_layer.update()

    def aim(name, target):
        bone = rig.pose.bones[name]
        direction = (target - bone.head).normalized()
        rotation = (bone.tail - bone.head).normalized().rotation_difference(direction)
        head = bone.head.copy()
        bone.matrix = Matrix.Translation(head) @ rotation.to_matrix().to_4x4() @ Matrix.Translation(-head) @ bone.matrix
        bpy.context.view_layer.update()

    def limb(upper_name, lower_name, target, pole):
        upper, lower = rig.pose.bones[upper_name], rig.pose.bones[lower_name]
        start = upper.head.copy()
        length_a, length_b = upper.length, lower.length
        direction = (target - start).normalized()
        distance = min((target - start).length, length_a + length_b - 0.002)
        reach = (length_a * length_a - length_b * length_b + distance * distance) / (2 * distance)
        bend = pole - start
        bend = (bend - direction * bend.dot(direction)).normalized()
        elbow = start + direction * reach + bend * math.sqrt(max(0, length_a * length_a - reach * reach))
        aim(upper_name, elbow)
        aim(lower_name, start + direction * distance)

    for side, suffix in ((1, 'L'), (-1, 'R')):
        foot = Vector((side * 0.17, -0.40, 0.077))
        limb(f'足.{suffix}', f'ひざ.{suffix}', foot, Vector((side * 0.15, -1, 0.60)))
        aim(f'足首.{suffix}', foot + Vector((0, -0.111, -0.061)))
        hand = Vector((side * 0.19, -0.26, 0.63))
        limb(f'腕.{suffix}', f'ひじ.{suffix}', hand, Vector((side * 0.40, -0.17, 0.78)))
        aim(f'手首.{suffix}', hand + Vector((0, -0.045, -0.012)))


def refine_neutral_motion(rig):
    animation = rig.animation_data
    animation.use_nla = False
    for track in animation.nla_tracks:
        strip = track.strips[0]
        action = strip.action
        bag = action.layers[0].strips[0].channelbag(strip.action_slot)
        for curve in list(bag.fcurves):
            if curve.data_path.startswith('pose.bones["髪.'):
                bag.fcurves.remove(curve)
        start, end = action.frame_range
        animation.action = action
        animation.action_slot = strip.action_slot
        if track.name == 'sit.bed':
            for curve in list(bag.fcurves):
                bag.fcurves.remove(curve)
            for frame in np.linspace(start, end, 25):
                bpy.context.scene.frame_set(int(frame))
                seated_pose(rig, 2 * math.pi * (frame - start) / (end - start))
                for bone in rig.pose.bones:
                    bone.rotation_mode = 'QUATERNION'
                    for channel in ('location', 'rotation_quaternion', 'scale'):
                        bone.keyframe_insert(channel, frame=frame, group=bone.name)
            for curve in bag.fcurves:
                for point in curve.keyframe_points:
                    point.interpolation = 'LINEAR'
        for chain in range(4):
            for segment, suffix in enumerate('abcd'):
                bone = rig.pose.bones[f'髪.{chain}.{suffix}']
                bone.rotation_mode = 'QUATERNION'
                for frame in np.linspace(start, end, 25):
                    phase = 2 * math.pi * (frame - start) / (end - start) - segment * 0.35 + chain * 0.65
                    angle = math.radians(0.35 + segment * 0.20) * math.sin(phase)
                    bone.rotation_quaternion = Quaternion((1, 0, 0), angle)
                    bone.keyframe_insert('rotation_quaternion', frame=frame, group=bone.name)
        for curve in bag.fcurves:
            if curve.data_path.startswith('pose.bones["髪.'):
                for point in curve.keyframe_points:
                    point.interpolation = 'LINEAR'
    animation.action = None
    animation.use_nla = True
    for bone in rig.pose.bones:
        bone.matrix_basis.identity()


def point_at(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def stage(out, device):
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 48
    scene.cycles.use_denoising = device == 'OPTIX'
    if device == 'OPTIX':
        preferences = bpy.context.preferences.addons['cycles'].preferences
        preferences.compute_device_type = 'OPTIX'
        preferences.refresh_devices()
        for render_device in preferences.devices:
            render_device.use = render_device.type == 'OPTIX'
        scene.cycles.device = 'GPU'
    scene.render.resolution_x = 900
    scene.render.resolution_y = 1100
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.world = bpy.data.worlds.new('Kari studio')
    scene.world.use_nodes = True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (0.16, 0.18, 0.22, 1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = 0.4
    scene.view_settings.view_transform = 'AgX'
    for name, location, power, size, color in (
        ('Key', (-1.5, -2.2, 2.6), 160, 1.6, (1.0, 0.91, 0.83)),
        ('Fill', (1.5, -1.2, 1.7), 85, 1.4, (0.82, 0.89, 1.0)),
        ('Rim', (0.7, 1.0, 2.3), 180, 1.0, (1.0, 0.92, 0.83)),
    ):
        light = bpy.data.lights.new(name, 'AREA')
        light.energy, light.size, light.color = power, size, color
        obj = bpy.data.objects.new(name, light)
        scene.collection.objects.link(obj)
        obj.location = location
        point_at(obj, (0, 0, 1.3))
    camera = bpy.data.objects.new('Kari camera', bpy.data.cameras.new('Kari camera'))
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = 0.52
    camera.location = (0, -2.8, 1.55)
    point_at(camera, (0, -0.06, 1.48))
    scene.render.filepath = str(out / 'kari_front.png')
    return camera


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('root', type=Path)
    parser.add_argument('out', type=Path)
    parser.add_argument('--device', choices=('CPU', 'OPTIX'), default='CPU')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    root, out = args.root, args.out
    out.mkdir(parents=True, exist_ok=True)
    (out / 'checkpoints.json').write_text('{}')
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(root / 'native/assets/characters/vivian.glb'))
    rig = bpy.data.objects['VivianRig']
    body, eyes = bpy.data.objects['vivian_body'], bpy.data.objects['vivian_eyes']
    source_faces = [tuple(face.vertices) for face in body.data.polygons]
    for obj in list(bpy.data.objects):
        if obj not in [rig] and obj.parent != rig:
            bpy.data.objects.remove(obj, do_unlink=True)
    rig.animation_data.action = None
    for track in rig.animation_data.nla_tracks:
        track.mute = True
    for bone in rig.pose.bones:
        bone.custom_shape = None
        bone.matrix_basis.identity()
    bpy.context.view_layer.update()
    original = coordinates(body.data.shape_keys.key_blocks[0].data).copy()
    amounts = {'weight_up': 0.26, 'hips_up': 0.38, 'waist_dn': 0.10, 'muscle_up': 0.05}
    for obj in list(rig.children):
        if obj.type == 'MESH':
            shape(obj, {**amounts, 'smile': 0.18, 'lids_down': 0.14}, obj not in [eyes, bpy.data.objects['vivian_hair']])
    bpy.data.objects.remove(bpy.data.objects['vivian_hair'], do_unlink=True)
    set_base_texture(body, bpy.data.images.load(str(root / 'native/assets/characters/kari_skin_olive.jpg')))
    set_base_texture(eyes, bpy.data.images.load(str(root / 'native/assets/characters/kari_eye.png')))
    make_hair(body, eyes, rig, out)
    keep_keys = {'Basis', 'blink', 'smile', 'smirk', 'pout', 'wink', 'shy', 'lids_down', 'lids_up', 'gasp',
                 'weight_up', 'weight_dn', 'hips_up', 'hips_dn', 'waist_up', 'waist_dn', 'muscle_up', 'muscle_dn', 'belly_up'}
    for obj in rig.children:
        if obj.type != 'MESH':
            continue
        keys = obj.data.shape_keys
        if keys:
            for key in list(keys.key_blocks):
                if key.name not in keep_keys:
                    obj.shape_key_remove(key)
        obj.name = obj.name.replace('vivian', 'kari')
        for polygon in obj.data.polygons:
            polygon.use_smooth = True
    for track in list(rig.animation_data.nla_tracks):
        if track.name not in {'stand.idle', 'sit.bed'}:
            rig.animation_data.nla_tracks.remove(track)
    retained_actions = {strip.action for track in rig.animation_data.nla_tracks for strip in track.strips}
    assert None not in retained_actions and len(retained_actions) == 2, 'Missing neutral actions'
    for action in list(bpy.data.actions):
        if action not in retained_actions:
            bpy.data.actions.remove(action)
    rig.name = 'KariRig'
    refine_neutral_motion(rig)
    camera = stage(out, args.device)
    bpy.context.scene.frame_set(0)
    bpy.context.view_layer.update()
    bpy.ops.outliner.orphans_purge(do_recursive=True)
    bpy.ops.file.pack_all()
    assert [tuple(face.vertices) for face in body.data.polygons] == source_faces, 'Body topology changed'
    assert not any('outer' in obj.name for obj in rig.children), 'Unexpected garment'
    bpy.ops.wm.save_as_mainfile(filepath=str(out / 'kari.blend'), compress=True)
    checkpoint(out / 'kari.blend')
    report = {'source': 'vivian.glb', 'rigBones': len(rig.data.bones),
              'garments': [], 'bodyFaces': len(source_faces), 'bodyTopologyPreserved': True,
              'meshes': {obj.name: len(obj.data.vertices) for obj in rig.children if obj.type == 'MESH'},
              'neutralClips': [t.name for t in rig.animation_data.nla_tracks],
              'bodyMaxDisplacementMeters': float(np.linalg.norm(coordinates(body.data.shape_keys.key_blocks[0].data) - original, axis=1).max()),
              'hairBinding': 'four long-hair chains with shoulder blending and looped secondary sway',
              'likeness': 'unverified prototype; no exact-replica claim'}
    (out / 'authoring.json').write_text(json.dumps(report, indent=2))
    print('AUTHORING_SAVED', json.dumps(report), flush=True)
    for obj in bpy.context.scene.objects:
        obj.select_set(obj == rig or obj.parent == rig)
    bpy.context.view_layer.objects.active = rig
    for track in rig.animation_data.nla_tracks:
        track.mute = False
    bpy.ops.export_scene.gltf(filepath=str(out / 'kari.glb'), export_format='GLB', use_selection=True,
                              export_skins=True, export_morph=True, export_morph_normal=True,
                              export_animations=True, export_animation_mode='NLA_TRACKS', export_nla_strips=True,
                              export_force_sampling=True, export_def_bones=False, export_rest_position_armature=True,
                              export_cameras=False, export_lights=False)
    with (out / 'kari.glb').open('rb') as exported:
        exported.seek(12)
        json_size, chunk_type = struct.unpack('<II', exported.read(8))
        assert chunk_type == 0x4E4F534A
        gltf = json.loads(exported.read(json_size))
    clips = {clip['name']: clip for clip in gltf.get('animations', [])}
    assert set(clips) == {'stand.idle', 'sit.bed'}, f'Unexpected exported clips: {list(clips)}'
    assert all(clip['channels'] and clip['samplers'] for clip in clips.values()), 'Empty exported clip'
    report['neutralClips'] = sorted(clips)
    report['animationChannels'] = {name: len(clip['channels']) for name, clip in clips.items()}
    (out / 'authoring.json').write_text(json.dumps(report, indent=2))
    checkpoint(out / 'kari.glb')
    for track in rig.animation_data.nla_tracks:
        track.mute = True
    for bone in rig.pose.bones:
        bone.matrix_basis.identity()
    bpy.context.view_layer.update()
    for name, location, target, scale in (
        ('front', (0, -2.8, 1.58), (0, -0.06, 1.565), 0.31),
        ('three', (1.7, -2.4, 1.58), (0, -0.02, 1.565), 0.31),
    ):
        camera.location = location
        camera.data.ortho_scale = scale
        point_at(camera, target)
        bpy.context.scene.render.filepath = str(out / f'kari_{name}.png')
        bpy.ops.render.render(write_still=True)
        checkpoint(out / f'kari_{name}.png')
    checkpoint(out / 'authoring.json')
    print('AUTHORING_COMPLETE', flush=True)


if __name__ == '__main__':
    main()
