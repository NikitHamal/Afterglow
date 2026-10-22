# warm hazel-brown iris (keeps the cornea alpha cut-out)
import numpy as np
from PIL import Image
E = "/root/agb/mh_assets/eyes/materials/"
ge = np.asarray(Image.open(E + "green_eye.png").convert("RGBA"), np.float32); be = np.asarray(Image.open(E + "brownlight_eye.png").convert("RGBA"), np.float32)
hz = ge.copy(); hz[..., :3] = np.clip((0.28 * ge[..., :3] + 0.72 * be[..., :3]) * np.array([0.80, 0.70, 0.58]), 0, 255)
Image.fromarray(hz.astype(np.uint8), "RGBA").save("/root/agb/out/tex/vivian_eye.png"); print("eye ok")
