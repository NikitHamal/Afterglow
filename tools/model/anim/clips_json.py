# Write native/assets/characters/clips.json from anim.blend + verify.json (camera presets measured on the baked poses).
#   blender -b /root/agb/native/anim.blend --python clips_json.py -- <out.json>
import bpy, sys, os, json, math
from mathutils import Vector
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import positions as PO
import positions2  # noqa: F401  (registers clips)
import positions3  # noqa: F401
import positions4  # noqa: F401
out = sys.argv[sys.argv.index("--") + 1]
NAT = os.environ.get("AG_NAT", "/root/agb/native")
ver = json.load(open(NAT + "/verify.json")) if os.path.exists(NAT + "/verify.json") else {}
V = bpy.data.objects["VivianRig"]; M = bpy.data.objects["MarcusRig"]
def gl(v): return [round(v.x, 4), round(v.z, 4), round(-v.y, 4)]          # Blender Z-up -> glTF Y-up
def at(arm, act_name, frame):
    arm.animation_data_create(); arm.animation_data.action = bpy.data.actions.get(act_name)
    bpy.context.scene.frame_set(frame); bpy.context.view_layer.update()
    return {pb.name: arm.matrix_world @ pb.head for pb in arm.pose.bones}, {pb.name: (arm.matrix_world @ pb.matrix).to_3x3() for pb in arm.pose.bones}
# his eye point in the head bone's local frame (rest): mid-eye from the eyes mesh, plus the face-forward axis
eyes = bpy.data.objects["marcus_eyes"]
ec = sum((eyes.matrix_world @ v.co for v in eyes.data.vertices), Vector()) / len(eyes.data.vertices)
hb = M.data.bones["頭"]
eye_local = hb.matrix_local.inverted() @ ec
fwd_local = (hb.matrix_local.to_3x3().inverted() @ Vector((0, -1, 0))).normalized()
clips = {}
for name, (fn, frames, kind) in PO.CLIPS.items():
    if f"VIV|{name}" not in bpy.data.actions: continue
    f = (frames + 1) // 2
    vh, _ = at(V, f"VIV|{name}", f); mh, mrot = at(M, f"MAR|{name}", f)
    solo = name.startswith("solo")
    tgt = vh["下半身"].copy() if solo else (vh["下半身"] + mh["下半身"]) / 2
    tgt.z += 0.12
    body_axis = (vh["頭"] - vh["下半身"]).normalized()
    side = Vector((1, 0, 0)) if abs(body_axis.x) < 0.7 else Vector((0, -1, 0))
    def orbit(yaw_deg, pitch_deg, dist):
        return dict(target=gl(tgt), yaw=yaw_deg, pitch=pitch_deg, distance=dist)
    lay = PO.LAYER_BONES.get(name, {})
    rep = ver.get(name, {})
    clips[name] = dict(
        duration=round((frames - 1) / 24.0, 4), frames=frames, kind=kind,
        time="normalized depth (0 = tip at the entrance, 1 = full)" if kind == "depth" else ("loop phase 0..1" if kind == "loop" else "single pose"),
        rubHand=("vivian.R" if lay.get("vivian") == "arm.R" else None),
        layerBones=lay or None,
        stroke_m=(0.0 if kind != "depth" else (0.05 if name.startswith(("cowgirl", "revcowgirl")) else (0.075 if name.startswith("oral") else (0.016 if name.startswith("lick") else (positions2.PRONE_LINS if name.startswith("prone") else PO.LINS))))),
        insert_m=(None if kind != "depth" or name.startswith(("lick", "handjob", "titjob")) else
                  ([0.059, 0.109] if name.startswith("cowgirl") else [0.069, 0.119] if name.startswith("revcowgirl") else
                   [-0.004, 0.071] if name.startswith("oral") else
                   [-PO.TIP_OUT, round(positions2.PRONE_LINS - PO.TIP_OUT, 3)] if name.startswith("prone") else [-PO.TIP_OUT, round(PO.LINS - PO.TIP_OUT, 3)])),
        morphs=dict(vivian=({"vag_open": 1.0} if name.startswith(("cowgirl", "revcowgirl")) and kind == "depth" else
                            {"vag_open": "smoothstep(0, 0.12, depth)"} if kind == "depth" and not name.startswith(("oral", "lick", "handjob", "titjob")) else {}),
                    marcus=({"aroused": "0..1 by arousal"} if solo else {"aroused": {"lick": 0.0, "cuddle": 0.0, "kiss": 0.35, "facesit": 0.8, "stand": 0.0, "sit": 0.0}.get(name.split(".")[0], 1.0)})),
        marcusHidden=solo,
        cameras=dict(
            orbit_side=orbit(90 if side.x else 0, 18, 2.3),
            orbit_front=orbit(200, 24, 2.6),
            orbit_top=orbit(120, 55, 2.2),
            fpv=dict(character="marcus", bone="頭", offset_local=[round(eye_local.x, 4), round(eye_local.y, 4), round(eye_local.z, 4)],
                     forward_local=[round(fwd_local.x, 4), round(fwd_local.y, 4), round(fwd_local.z, 4)], note="bone-local (glTF node frame); look along forward_local"),
        ),
        contacts=rep.get("contacts", []),
        verify=dict(max_bad_tri_pairs=rep.get("max_bad_pairs"), max_depth_mm=rep.get("max_depth_mm"), worst_flip_deg=rep.get("worst_flip_deg"),
                    max_rom_excess_deg=rep.get("max_rom_excess_deg"),
                    floors_mm=dict(vivian=[s["her_floor_mm"] for s in rep.get("samples", [])], marcus=[s["his_floor_mm"] for s in rep.get("samples", [])])),
    )
doc = dict(version=1, units="m", up="+Y", fps=24,
           world="Both characters' clips share the room's world space (bed centre x=0, z=-1.22, mattress top y=0.604); no runtime placement needed.",
           conventions=dict(
               depth_clips="sample <pos>.in at t = depth * duration for BOTH characters with the same t; insert_m = shaft length inside her at t=0 / t=1 (in cowgirl + reverse she sits down onto him and never leaves him)",
               contact="verify counts triangle pairs outside the intended contact: genitals, hands, mouth (oral/lick), the shaft vs her inner thighs/buttocks within 6 cm of the entrance, and pelvis-to-pelvis flesh (her within 19 cm of the entrance, his within 22 cm of the shaft root)",
               layers="<pos>.rub only keys the listed bones; blend it over <pos>.in with a bone mask (rub phase loops 0..1)",
               embrace="missionary.embrace is a pose layer for upper body + arms; blend weight = embrace amount",
               not_animated="hair chains 髪.* and eyes 目.L/目.R are not keyed: simulate hair (colliders in node extras) and drive gaze live",
               morph_normals="body-shape targets (bust/hips/waist/butt/weight/muscle/belly/age/length/girth _up/_dn) and every hair-mesh target ship POSITION deltas only; expressions and anatomy (vag_open, vag_aroused, anal_open, aroused) include NORMAL",
               contacts="per clip: where a hand / mouth touches skin. {char, effector, bone, offset (bone-local, like fpv.offset_local), "
                        "radius, target {char, bone, region}, gap_mm [min, max] over the clip (0 = touching, - = pressing in), t [first, last] "
                        "normalized time}. Contact point each frame = bone world matrix x offset; deform/react around target.bone/region."),
           clips=clips)
json.dump(doc, open(out, "w"), indent=1, ensure_ascii=False)
print("clips.json:", len(clips), "clips ->", out)
