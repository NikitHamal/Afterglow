from PIL import Image
import glob
T = "/root/agb/male/tex/"
Image.open(T + "marcus_skin_normal.jpg").resize((2048, 2048), Image.LANCZOS).save(T + "marcus_skin_normal_2k.jpg", quality=92)
Image.open(T + "marcus_skin_orm.jpg").resize((2048, 2048), Image.LANCZOS).save(T + "marcus_skin_orm_2k.jpg", quality=90)
Image.open(glob.glob("/root/agb/mh_assets/teeth/teeth_base/*.png")[0]).convert("RGB").resize((1024, 1024), Image.LANCZOS).save(T + "marcus_teeth_1k.jpg", quality=88)
Image.open(glob.glob("/root/agb/mh_assets/tongue/tongue01/*.png")[0]).convert("RGB").resize((512, 512), Image.LANCZOS).save(T + "marcus_tongue_512.jpg", quality=88)
for t in ("honey", "caramel", "deep"):
    Image.open(T + f"marcus_skin_{t}.jpg").resize((2048, 2048), Image.LANCZOS).save(T + f"skin_{t}.jpg", quality=90, optimize=True, progressive=True)
    Image.open(T + f"marcus_genital_{t}.jpg").save(T + f"genital_{t}.jpg", quality=90, optimize=True)
print("ok")
