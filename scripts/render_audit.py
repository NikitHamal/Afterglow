# Step 1B: Full visual & quantitative audit renderer for any clip.
# Produces: sheet_3q.png, sheet_side.png, onion.png, trace.png, velocity.png, preview.mp4, metrics.json
import os, sys, json, time, subprocess
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import blender_env as BE
from mathutils import Vector
import motion_metrics as MM
import contact_sheet as CS

def _project_3d_to_2d(pt, cam_obj, width, height):
    from bpy_extras.object_utils import world_to_camera_view
    import bpy
    co_ndc = world_to_camera_view(bpy.context.scene, cam_obj, Vector(pt))
    return int(co_ndc.x * width), int((1.0 - co_ndc.y) * height)

def _build_onion_skin(frame_paths, v_hand, out_path):
    if not frame_paths:
        return
    n = len(frame_paths)
    top_idx = sorted(range(n), key=lambda i: v_hand[min(i, len(v_hand) - 1)] if len(v_hand) else 0, reverse=True)[:6]
    top_idx = sorted(top_idx)
    tiles = []
    for idx in top_idx:
        base = Image.open(frame_paths[idx]).convert("RGBA")
        comp = Image.new("RGBA", base.size, (20, 20, 24, 255))
        for off, alpha in ((-2, 0.30), (-1, 0.30), (1, 0.30), (2, 0.30)):
            j = max(0, min(n - 1, idx + off))
            if j != idx:
                im_o = Image.open(frame_paths[j]).convert("RGBA")
                comp = Image.blend(comp, im_o, alpha)
        comp = Image.blend(comp, base, 0.72)
        tiles.append(comp.convert("RGB"))
    if not tiles:
        return
    tw, th = 240, 240
    strip = Image.new("RGB", (len(tiles) * tw, th), (18, 18, 22))
    draw = ImageDraw.Draw(strip)
    for i, t_im in enumerate(tiles):
        strip.paste(t_im.resize((tw, th)), (i * tw, 0))
        draw.text((i * tw + 8, 6), f"peak#{i+1}", fill=(255, 210, 140))
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    strip.save(out_path, "PNG", optimize=True)

def _build_trace_image(base_frame_path, series, cam_obj, out_path):
    if not (base_frame_path and os.path.exists(base_frame_path)):
        return
    im = Image.open(base_frame_path).convert("RGB")
    w, h = im.size
    draw = ImageDraw.Draw(im)
    colors = {"hand_L": (80, 230, 255), "head": (255, 210, 80), "pelvis": (255, 95, 140)}
    for k, col in colors.items():
        pts3d = series.get(k, [])
        pts2d = [_project_3d_to_2d(p, cam_obj, w, h) for p in pts3d]
        if len(pts2d) >= 2:
            draw.line(pts2d, fill=col, width=3)
            for px, py in pts2d[::4]:
                draw.ellipse([px - 3, py - 3, px + 3, py + 3], fill=col)
    draw.rectangle([8, 8, 250, 32], fill=(16, 16, 20))
    draw.text((14, 12), "Trace: Hand(C) Head(Y) Root(M)", fill=(240, 240, 240))
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    im.save(out_path, "PNG", optimize=True)

def _build_velocity_plot(clip, series, out_path):
    w, h = 640, 320
    im = Image.new("RGB", (w, h), (22, 22, 28))
    dr = ImageDraw.Draw(im)
    px0, py0, px1, py1 = 54, 36, w - 20, h - 36
    dr.rectangle([px0, py0, px1, py1], fill=(30, 30, 38), outline=(68, 68, 85))
    dr.text((px0, 12), f"Velocity Profile — {clip}  [Hand(C) Head(Y) Root(M)]", fill=(236, 232, 226))
    vmax = max(1e-3, max([max(series.get(k, [0.0]) or [0.0]) for k in ("v_hand", "v_head", "v_root")]))
    for k, col in (("v_hand", (79, 216, 255)), ("v_head", (255, 209, 92)), ("v_root", (255, 99, 146))):
        vals = series.get(k, [])
        if len(vals) >= 2:
            pts = [(int(px0 + (px1 - px0) * i / (len(vals) - 1)), int(py1 - (py1 - py0) * (v / vmax))) for i, v in enumerate(vals)]
            dr.line(pts, fill=col, width=2)
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    im.save(out_path, "PNG", optimize=True)

def audit_clip(clip, scene_id="story", work_root="/work/renders", audit_root="audit", already_loaded=False):
    if not already_loaded:
        BE.load_scene("pair")
    metrics = MM.compute_clip_metrics(clip)
    f0, f1 = BE.bind_clip(clip)
    step4 = list(range(f0, f1 + 1, 4))[:60]
    wdir = os.path.join(work_root, scene_id, clip)
    adir = os.path.join(audit_root, scene_id, clip)
    os.makedirs(wdir, exist_ok=True); os.makedirs(adir, exist_ok=True)
    f3q, cam3q, _ = BE.render_clay(clip, os.path.join(wdir, "frames_3q"), cam="3q", res=(720, 720), frames=step4)
    fside, _, _ = BE.render_clay(clip, os.path.join(wdir, "frames_side"), cam="side", res=(720, 720), frames=step4)
    lbls = [f"f{f}" for f in step4]
    for dst in (wdir, adir):
        CS.build_contact_sheet(f3q, os.path.join(dst, "sheet_3q.png"), labels=lbls, title=f"{clip} (3/4 clay)")
        CS.build_contact_sheet(fside, os.path.join(dst, "sheet_side.png"), labels=lbls, title=f"{clip} (side clay)")
        _build_onion_skin(f3q, metrics["series"]["v_hand"], os.path.join(dst, "onion.png"))
        _build_trace_image(f3q[len(f3q) // 2] if f3q else None, metrics["series"], cam3q, os.path.join(dst, "trace.png"))
        _build_velocity_plot(clip, metrics["series"], os.path.join(dst, "velocity.png"))
        with open(os.path.join(dst, "metrics.json"), "w") as fp:
            json.dump(metrics, fp, indent=2)
    mp4_path = os.path.join(wdir, "preview.mp4")
    ff_bin = "ffmpeg"
    try:
        import shutil
        if not shutil.which(ff_bin):
            import imageio_ffmpeg
            ff_bin = imageio_ffmpeg.get_ffmpeg_exe()
        subprocess.run([ff_bin, "-y", "-framerate", "8", "-pattern_type", "glob", "-i",
                        os.path.join(wdir, "frames_3q", "frame_3q_*.png"), "-c:v", "libx264", "-pix_fmt", "yuv420p", mp4_path],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception:
        pass
    return metrics

def audit_all_baseline(clips=None, work_root="/work/renders", audit_root="audit"):
    import shutil
    BE.load_scene("pair")
    all_clips = clips or BE.list_actions()
    sc_path = os.path.join(audit_root, "scorecard.json")
    scorecard = json.load(open(sc_path)) if os.path.exists(sc_path) else {"version": 1, "baseline": {}, "current": {}}
    base_dir = os.path.join(audit_root, "baseline")
    os.makedirs(base_dir, exist_ok=True)
    t0 = time.time()
    for idx, c in enumerate(all_clips):
        t_c = time.time()
        m = audit_clip(c, scene_id="baseline", work_root=work_root, audit_root=audit_root, already_loaded=True)
        slug = c.replace(".", "_")
        cdir = os.path.join(base_dir, c)
        for fn in ("sheet_3q.png", "sheet_side.png", "onion.png", "trace.png", "velocity.png", "metrics.json"):
            src_f = os.path.join(cdir, fn)
            if os.path.exists(src_f):
                shutil.copyfile(src_f, os.path.join(base_dir, f"{slug}_{fn}"))
        scorecard["baseline"][c] = {k: v for k, v in m.items() if k != "series"}
        scorecard["current"][c] = {k: v for k, v in m.items() if k != "series"}
        el = time.time() - t0
        eta = (el / (idx + 1)) * (len(all_clips) - (idx + 1))
        print(f"[baseline {idx+1}/{len(all_clips)}] {c}: score={m['doll_score']} fails={m['fail_count']} ({time.time()-t_c:.1f}s, ETA {eta:.0f}s)", flush=True)
    with open(os.path.join(audit_root, "scorecard.json"), "w") as fp:
        json.dump(scorecard, fp, indent=2)
    return scorecard

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--all-baseline":
        subset = sys.argv[2].split(",") if len(sys.argv) > 2 else None
        audit_all_baseline(subset)
    else:
        c = sys.argv[1] if len(sys.argv) > 1 else "stand.idle"
        sc = sys.argv[2] if len(sys.argv) > 2 else "the_massage"
        m = audit_clip(c, sc)
        print(f"[render_audit] {c} ({sc}): doll_score={m['doll_score']} fails={m['fail_count']} {m['failing_fields']}", flush=True)
