# Hand-target offsets for Afterglow's GLB IK (GLB_LAND_OFF), measured on the game-scale model.
# World convention when she lies on her back: +Y = out of her chest, +Z = toward her feet.
import bpy, json
import numpy as np
arm = bpy.data.objects["VivianRig"]; body = bpy.data.objects["vivian_body"]
V = np.array([body.matrix_world @ v.co for v in body.data.vertices])
def bh(n): return np.array(arm.matrix_world @ arm.data.bones[n].head_local)
out = {}
for side, sgn in ((".L", 1), (".R", -1)):
    b = bh("乳親" + side)
    near = V[(np.sign(V[:, 0]) == sgn) & (np.abs(V[:, 0] - b[0]) < 0.06) & (np.abs(V[:, 2] - b[2]) < 0.08)]
    tip = near[np.argmin(near[:, 1])]                     # most anterior point = nipple
    out["breast" + side] = dict(bone=b.round(4).tolist(), tip=tip.round(4).tolist(),
                                ant=round(float(b[1] - tip[1]), 4), down=round(float(b[2] - tip[2]), 4), lat=round(float(tip[0] - b[0]), 4))
p = bh("下半身")
pub = V[(np.abs(V[:, 0]) < 0.012) & (V[:, 2] < p[2] - 0.05) & (V[:, 2] > p[2] - 0.16)]
mons = pub[np.argmin(pub[:, 1] + 0.0 * pub[:, 2])]
vul = V[(np.abs(V[:, 0]) < 0.006) & (V[:, 2] < p[2] - 0.08) & (V[:, 2] > p[2] - 0.2)]
crotch_low = vul[np.argmin(vul[:, 2])]
out["mons"] = dict(bone=p.round(4).tolist(), mons=mons.round(4).tolist(), ant=round(float(p[1] - mons[1]), 4), down=round(float(p[2] - mons[2]), 4),
                   crotch_low=crotch_low.round(4).tolist())
print("LAND", json.dumps(out))
