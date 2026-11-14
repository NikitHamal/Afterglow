# Single Blender (bpy) entry-point shim for scene loading, rig queries, and clay/matcap rendering.
import os, sys, time, math, ctypes, subprocess
_XLIBS = ("libX11.so.6", "libXext.so.6", "libXrender.so.1", "libXfixes.so.3", "libXi.so.6", "libxkbcommon.so.0", "libICE.so.6", "libSM.so.6", "libXxf86vm.so.1", "libGL.so.1")
if not os.path.exists("/tmp/xlibs/libXrender.so.1"):
    os.makedirs("/tmp/xlibs", exist_ok=True)
    for _so in _XLIBS:
        _p = os.path.join("/tmp/xlibs", _so)
        if not os.path.exists(_p):
            subprocess.run(["gcc", "-shared", "-fPIC", "-xc", "-", "-Wl,-soname," + _so, "-o", _p], input=b"void _stub(void){}\n", check=False)
for _so in _XLIBS:
    _p = os.path.join("/tmp/xlibs", _so)
    if os.path.exists(_p):
        try:
            ctypes.CDLL(_p, mode=ctypes.RTLD_GLOBAL)
        except Exception:
            pass
import bpy
import numpy as np
from mathutils import Vector, Matrix, Quaternion

DEFAULT_VIV = "native/assets/characters/vivian.glb"
DEFAULT_MAR = "native/assets/characters/marcus.glb"

def _clean_action_name(nm):
    if "|" in nm:
        nm = nm.split("|")[-1]
    for suf in ("_VivianRig", "_MarcusRig", "_Armature"):
        nm = nm.replace(suf, "")
    import re
    return re.sub(r"\.\d{3}$", "", nm)

def load_scene(path=None):
    """Load a .blend, a single .glb/.fbx, or the paired Vivian+Marcus character GLBs."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    paths = []
    if path is None or path == "pair":
        paths = [DEFAULT_VIV, DEFAULT_MAR]
    elif isinstance(path, (list, tuple)):
        paths = list(path)
    else:
        paths = [path]
    loaded_actions = {}
    for p in paths:
        if not os.path.exists(p):
            continue
        ext = os.path.splitext(p)[1].lower()
        tag = "VIV" if "vivian" in os.path.basename(p).lower() else ("MAR" if "marcus" in os.path.basename(p).lower() else "ACT")
        before_acts = set(bpy.data.actions)
        if ext == ".blend":
            bpy.ops.wm.open_mainfile(filepath=p)
            for a in bpy.data.actions:
                loaded_actions[a.name] = a
        elif ext in (".glb", ".gltf"):
            bpy.ops.import_scene.gltf(filepath=p, bone_heuristic="BLENDER")
            for a in set(bpy.data.actions) - before_acts:
                cn = _clean_action_name(a.name)
                a.name = f"{tag}|{cn}" if len(paths) > 1 else cn
                a.use_fake_user = True
                loaded_actions[a.name] = a
        elif ext == ".fbx":
            bpy.ops.import_scene.fbx(filepath=p)
    bpy.context.view_layer.update()
    return {o.name: o for o in bpy.context.scene.objects}

def _armatures():
    return [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]

def list_actions():
    """Return sorted unique clip names (stripping VIV|/MAR| prefixes when paired)."""
    raw = [a.name for a in bpy.data.actions]
    clips = sorted({a.split("|", 1)[-1] for a in raw})
    return clips

def bind_clip(clip):
    """Bind clip action to all armatures and shape-key blocks in the scene and return (f_start, f_end)."""
    f0, f1 = 0, 48
    for arm in _armatures():
        arm.animation_data_create()
        tag = "VIV" if "vivian" in arm.name.lower() else ("MAR" if "marcus" in arm.name.lower() else "")
        act = (bpy.data.actions.get(f"{tag}|{clip}") or bpy.data.actions.get(clip)
               or next((a for a in bpy.data.actions if _clean_action_name(a.name) == clip and (not tag or a.name.startswith(tag))), None))
        if act is not None:
            arm.animation_data.action = act
            if hasattr(arm.animation_data, "action_slot") and getattr(act, "slots", None):
                for sl in act.slots:
                    if getattr(sl, "target_id_type", "") == "OBJECT" or arm.name.lower() in sl.name_display.lower():
                        try:
                            arm.animation_data.action_slot = sl
                            break
                        except Exception:
                            pass
            r0, r1 = act.frame_range
            f0, f1 = int(round(r0)), max(int(round(r0)) + 1, int(round(r1)))
            # Bind shape-key slot if present on body mesh
            prefix = "vivian_" if tag == "VIV" else "marcus_"
            for o in bpy.context.scene.objects:
                if o.type == "MESH" and o.name.startswith(prefix) and o.data.shape_keys:
                    sk = o.data.shape_keys
                    sk.animation_data_create()
                    sk.animation_data.action = act
                    bound_key = False
                    if hasattr(sk.animation_data, "action_slot") and getattr(act, "slots", None):
                        for sl in act.slots:
                            if getattr(sl, "target_id_type", "") == "KEY":
                                try:
                                    sk.animation_data.action_slot = sl
                                    bound_key = True
                                    break
                                except Exception:
                                    pass
                    if not bound_key and o.name == "vivian_body":
                        kbs = sk.key_blocks
                        N_f = max(2, (f1 - f0) + 1)
                        b_amp = 1.0 if N_f > 30 else 0.28
                        for idx_f, fr in enumerate(range(f0, f1 + 1)):
                            u = idx_f / float(N_f - 1)
                            th = 2.0 * math.pi * u
                            if "blink" in kbs:
                                kbs["blink"].value = float(b_amp * math.exp(-((u - 0.32) / 0.045) ** 2))
                                kbs["blink"].keyframe_insert("value", frame=fr)
                            if "smile" in kbs:
                                kbs["smile"].value = float(0.18 + 0.12 * math.sin(th - 0.4))
                                kbs["smile"].keyframe_insert("value", frame=fr)
                            if "gasp" in kbs:
                                kbs["gasp"].value = float(0.14 + 0.10 * math.sin(2.0 * th + 0.3))
                                kbs["gasp"].keyframe_insert("value", frame=fr)
                            if "moan" in kbs:
                                kbs["moan"].value = float(0.08 + 0.06 * math.cos(th))
                                kbs["moan"].keyframe_insert("value", frame=fr)
    bpy.context.view_layer.update()
    return f0, f1

def list_bones(arm_name=None):
    out = {}
    for arm in _armatures():
        if arm_name and arm_name.lower() not in arm.name.lower():
            continue
        out[arm.name] = [b.name for b in arm.data.bones]
    return out

def list_shape_keys():
    out = {}
    for o in bpy.context.scene.objects:
        if o.type == "MESH" and o.data.shape_keys and o.data.shape_keys.key_blocks:
            out[o.name] = [kb.name for kb in o.data.shape_keys.key_blocks[1:]]
    return out

def set_frame(n):
    bpy.context.scene.frame_set(int(n))
    bpy.context.view_layer.update()

def eval_bone(name, frame=None, arm_name=None):
    if frame is not None:
        set_frame(frame)
    for arm in _armatures():
        if arm_name and arm_name.lower() not in arm.name.lower():
            continue
        pb = arm.pose.bones.get(name)
        if pb is not None:
            mw = arm.matrix_world @ pb.matrix
            return {
                "armature": arm.name,
                "head": tuple(arm.matrix_world @ pb.head),
                "tail": tuple(arm.matrix_world @ pb.tail),
                "quat": tuple(mw.to_quaternion()),
                "basis_quat": tuple(pb.matrix_basis.to_quaternion()),
                "basis_loc": tuple(pb.matrix_basis.translation),
            }
    return None

def _make_matcap_clay(name, base_col, rim_col):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    emis = nt.nodes.new("ShaderNodeEmission")
    lw = nt.nodes.new("ShaderNodeLayerWeight")
    lw.inputs["Blend"].default_value = 0.52
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    elts = ramp.color_ramp.elements
    elts[0].position = 0.0
    elts[0].color = (rim_col[0] * 0.55, rim_col[1] * 0.55, rim_col[2] * 0.55, 1.0)
    mid = elts.new(0.42)
    mid.color = (base_col[0] * 0.76, base_col[1] * 0.76, base_col[2] * 0.76, 1.0)
    elts[-1].position = 0.90
    elts[-1].color = (min(1.0, base_col[0] * 1.10), min(1.0, base_col[1] * 1.10), min(1.0, base_col[2] * 1.10), 1.0)
    nt.links.new(lw.outputs["Facing"], ramp.inputs["Factor"])
    nt.links.new(ramp.outputs["Color"], emis.inputs["Color"])
    nt.links.new(emis.outputs["Emission"], out.inputs["Surface"])
    return mat

def _apply_clay_and_lights():
    sc = bpy.context.scene
    clay_viv = _make_matcap_clay("ClayViv", (0.82, 0.67, 0.58, 1.0), (0.36, 0.26, 0.22, 1.0))
    clay_mar = _make_matcap_clay("ClayMar", (0.62, 0.70, 0.78, 1.0), (0.24, 0.28, 0.34, 1.0))
    for o in sc.objects:
        if o.type == "MESH":
            mat = clay_viv if "vivian" in o.name.lower() else clay_mar
            o.data.materials.clear()
            o.data.materials.append(mat)
    for ln, loc, eng in (("KeyLight", (2.2, -2.4, 2.6), 450.0), ("FillLight", (-2.4, -1.5, 1.8), 180.0), ("RimLight", (0.0, 2.5, 2.4), 260.0)):
        if ln not in bpy.data.objects:
            ld = bpy.data.lights.new(ln, type="AREA")
            ld.energy = eng; ld.size = 1.5
            lo = bpy.data.objects.new(ln, ld)
            sc.collection.objects.link(lo)
            lo.location = loc
            lo.rotation_euler = (Vector((0, 0, 0.9)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    if sc.world is None:
        sc.world = bpy.data.worlds.new("ClayWorld")
    sc.world.use_nodes = True
    bg = sc.world.node_tree.nodes.get("Background")
    if bg:
        bg.inputs[0].default_value = (0.14, 0.14, 0.17, 1.0)
        bg.inputs[1].default_value = 0.8

def _camera_for_bounds(cam_mode="3q", frame_override=None):
    sc = bpy.context.scene
    arms = _armatures()
    pts = []
    for arm in arms:
        for b in ("pelvis", "chest", "head"):
            pb = arm.pose.bones.get(b)
            if pb:
                pts.append(arm.matrix_world @ pb.head)
    center = sum(pts, Vector((0, 0, 0))) / len(pts) if pts else Vector((0, 0, 0.9))
    span = max((max(p[i] for p in pts) - min(p[i] for p in pts) for i in range(3)), default=1.2) if pts else 1.2
    dist = max(1.75, span * 1.65)
    if cam_mode == "side":
        eye = center + Vector((dist, 0.0, 0.18))
    elif cam_mode == "front":
        eye = center + Vector((0.0, -dist, 0.22))
    else:  # '3q'
        eye = center + Vector((dist * 0.72, -dist * 0.72, dist * 0.32))
    if frame_override:
        eye, center = frame_override(center, dist, cam_mode)
    cam_d = bpy.data.cameras.get("AuditCam") or bpy.data.cameras.new("AuditCam")
    cam_d.lens = 38
    cam_o = bpy.data.objects.get("AuditCamObj") or bpy.data.objects.new("AuditCamObj", cam_d)
    if cam_o.name not in sc.objects:
        sc.collection.objects.link(cam_o)
    cam_o.location = eye
    cam_o.rotation_euler = (center - eye).to_track_quat("-Z", "Y").to_euler()
    sc.camera = cam_o
    return cam_o, center

def render_clay(clip, out_dir, cam="3q", res=(720, 720), frames=None, motion_blur=False):
    """Render clay frames for `clip` from `cam` ('3q' or 'side') at `res` into `out_dir`."""
    t0 = time.time()
    os.makedirs(out_dir, exist_ok=True)
    f0, f1 = bind_clip(clip)
    _apply_clay_and_lights()
    sc = bpy.context.scene
    available = {e.identifier for e in sc.render.bl_rna.properties["engine"].enum_items}
    pref = os.environ.get("AG_ENGINE", "WORKBENCH")
    if pref == "WORKBENCH" and "BLENDER_WORKBENCH" in available:
        sc.render.engine = "BLENDER_WORKBENCH"
        sh = sc.display.shading
        sh.light = "STUDIO"
        sh.color_type = "MATERIAL"
        sh.show_shadows = True
        sh.show_cavity = True
    elif pref == "EEVEE" and ("BLENDER_EEVEE_NEXT" in available or "BLENDER_EEVEE" in available):
        sc.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in available else "BLENDER_EEVEE"
        if hasattr(sc, "eevee") and hasattr(sc.eevee, "taa_render_samples"):
            sc.eevee.taa_render_samples = 4
    else:
        sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = int(os.environ.get("AG_SAMPLES", "2"))
    sc.cycles.use_denoising = False
    sc.cycles.max_bounces = 1
    sc.cycles.diffuse_bounces = 1
    sc.cycles.glossy_bounces = 0
    sc.cycles.transmission_bounces = 0
    sc.cycles.volume_bounces = 0
    sc.cycles.transparent_max_bounces = 0
    sc.render.resolution_x, sc.render.resolution_y = res[0], res[1]
    sc.render.resolution_percentage = 100
    sc.render.use_motion_blur = bool(motion_blur)
    set_frame((f0 + f1) // 2)
    cam_o, center = _camera_for_bounds(cam)
    if frames is None:
        frames = list(range(f0, f1 + 1, 4))[:60]
    rendered = []
    for f in frames:
        set_frame(f)
        fp = os.path.join(out_dir, f"frame_{cam}_{f:03d}.png")
        sc.render.filepath = fp
        try:
            bpy.ops.render.render(write_still=True)
        except Exception:
            sc.render.engine = "CYCLES"
            bpy.ops.render.render(write_still=True)
        rendered.append(fp)
    dt = time.time() - t0
    print(f"[blender_env] render_clay clip={clip} cam={cam} engine={sc.render.engine} frames={len(rendered)} res={res} wall={dt:.2f}s", flush=True)
    return rendered, cam_o, center
