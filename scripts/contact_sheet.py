# Contact sheet & before/after 2-up strip builder for animation review.
import os, math
from PIL import Image, ImageDraw, ImageFont

def build_contact_sheet(image_paths, out_path, cols=6, tile_size=(240, 240), labels=None, title=None):
    """Tile a sequence of PNG paths into a contact sheet (max 60 tiles)."""
    paths = [p for p in image_paths if os.path.exists(p)][:60]
    if not paths:
        return None
    n = len(paths)
    cols = min(cols, max(1, n))
    rows = math.ceil(n / cols)
    tw, th = tile_size
    header = 32 if title else 0
    canvas = Image.new("RGB", (cols * tw, rows * th + header), (24, 24, 28))
    draw = ImageDraw.Draw(canvas)
    if title:
        draw.text((10, 8), str(title), fill=(235, 230, 220))
    for idx, p in enumerate(paths):
        r, c = divmod(idx, cols)
        im = Image.open(p).convert("RGB").resize((tw, th), Image.Resampling.LANCZOS)
        x0, y0 = c * tw, header + r * th
        canvas.paste(im, (x0, y0))
        lbl = labels[idx] if labels and idx < len(labels) else f"f{idx}"
        draw.rectangle([x0 + 4, y0 + 4, x0 + 68, y0 + 22], fill=(16, 16, 20))
        draw.text((x0 + 8, y0 + 6), str(lbl), fill=(240, 210, 170))
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    canvas.save(out_path, "PNG", optimize=True)
    return out_path

def build_before_after_strip(before_path, after_path, out_path, label_before="BASELINE", label_after="CURRENT"):
    """Create a 2-up side-by-side comparison image from two contact sheets or renders."""
    if not (os.path.exists(before_path) and os.path.exists(after_path)):
        return None
    im_b = Image.open(before_path).convert("RGB")
    im_a = Image.open(after_path).convert("RGB")
    h = max(im_b.height, im_a.height)
    w = im_b.width + im_a.width + 12
    canvas = Image.new("RGB", (w, h + 30), (18, 18, 22))
    draw = ImageDraw.Draw(canvas)
    draw.text((12, 7), label_before, fill=(230, 140, 140))
    draw.text((im_b.width + 24, 7), label_after, fill=(140, 230, 165))
    canvas.paste(im_b, (0, 30))
    canvas.paste(im_a, (im_b.width + 12, 30))
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    canvas.save(out_path, "PNG", optimize=True)
    return out_path

if __name__ == "__main__":
    import sys
    if len(sys.argv) >= 4 and sys.argv[1] == "--compare":
        build_before_after_strip(sys.argv[2], sys.argv[3], sys.argv[4])
