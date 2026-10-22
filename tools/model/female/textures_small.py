# compact companion textures for the game GLB (+ the two alternate skin tones for runtime swapping)
from PIL import Image
T = "/root/agb/out/tex/"
Image.open(T + "vivian_skin_normal.jpg").resize((2048, 2048), Image.LANCZOS).save(T + "vivian_skin_normal_2k.jpg", quality=92)
Image.open(T + "vivian_skin_orm.jpg").resize((2048, 2048), Image.LANCZOS).save(T + "vivian_skin_orm_2k.jpg", quality=90)
import glob, os
teeth = [p for p in glob.glob("/root/agb/mh_assets/teeth/teeth_base/*.png")][0]
Image.open(teeth).convert("RGB").resize((1024, 1024), Image.LANCZOS).save(T + "vivian_teeth_1k.jpg", quality=88)
tongue = [p for p in glob.glob("/root/agb/mh_assets/tongue/tongue01/*.png")][0]
Image.open(tongue).convert("RGB").resize((512, 512), Image.LANCZOS).save(T + "vivian_tongue_512.jpg", quality=88)
for t in ("honey", "caramel", "deep"):
    Image.open(T + f"vivian_skin_{t}.jpg").save(T + f"vivian_skin_{t}_game.jpg", quality=88, optimize=True, progressive=True)
for f in sorted(os.listdir(T)): print(f, os.path.getsize(T + f))

Image.open(T + "vivian_hair.png").resize((2048, 1024), Image.LANCZOS).save(T + "vivian_hair_2kx1k.png", optimize=True)
Image.open(T + "vivian_eye.png").resize((512, 512), Image.LANCZOS).save(T + "vivian_eye_512.png", optimize=True)
