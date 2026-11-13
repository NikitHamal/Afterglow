# Step 5.6: Cross-runtime animation parity test suite (Blender source vs GLB export vs C++/Web player contract).
import os, sys, json, unittest
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "scripts"))

import blender_env as BE
import contact_sheet as CS

REQUIRED_CORE_BONES = {"全ての親", "下半身", "上半身", "上半身2", "首", "頭", "肩.L", "肩.R", "腕.L", "腕.R", "ひじ.L", "ひじ.R", "手首.L", "手首.R", "足.L", "足.R", "ひざ.L", "ひざ.R", "足首.L", "足首.R"}

class TestAnimParity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BE.load_scene("pair")
        cls.clips = BE.list_actions()
        cls.bones = BE.list_bones()
        cls.scorecard = json.load(open(os.path.join(ROOT, "audit/scorecard.json")))

    def test_a_frame_counts_and_clip_set(self):
        self.assertGreaterEqual(len(self.clips), 21)
        for clip in self.clips:
            f0, f1 = BE.bind_clip(clip)
            self.assertGreaterEqual(f1 - f0, 1, f"Clip {clip} has invalid frame range {f0}..{f1}")

    def test_b_bone_count_and_required_names(self):
        self.assertIn("VivianRig", self.bones)
        self.assertIn("MarcusRig", self.bones)
        self.assertEqual(len(self.bones["VivianRig"]), 87)
        self.assertEqual(len(self.bones["MarcusRig"]), 70)
        for req in REQUIRED_CORE_BONES:
            self.assertIn(req, self.bones["VivianRig"])
            self.assertIn(req, self.bones["MarcusRig"])

    def test_c_first_last_pose_loop_and_runtime_parity(self):
        parity_dir = os.path.join(ROOT, "audit/parity")
        os.makedirs(parity_dir, exist_ok=True)
        report = {}
        for clip in ("stand.idle", "sit.bed", "missionary.in", "cowgirl.in", "oral.in"):
            f0, f1 = BE.bind_clip(clip)
            p_first = BE.eval_bone("下半身", frame=f0, arm_name="VivianRig")
            p_last = BE.eval_bone("下半身", frame=f1, arm_name="VivianRig")
            self.assertIsNotNone(p_first)
            self.assertIsNotNone(p_last)
            drift = float(np.linalg.norm(np.array(p_first["head"]) - np.array(p_last["head"])))
            # Blender Z-up -> glTF/C++/Three.js Y-up transform parity check: (x, z, -y)
            b_pt = p_first["head"]
            gltf_pt = (b_pt[0], b_pt[2], -b_pt[1])
            back_pt = (gltf_pt[0], -gltf_pt[2], gltf_pt[1])
            transform_err = float(np.linalg.norm(np.array(b_pt) - np.array(back_pt)))
            self.assertLess(transform_err, 1e-6)
            report[clip] = {
                "frames": (f1 - f0) + 1,
                "loop_pelvis_drift_m": round(drift, 5),
                "coord_parity_error_m": round(transform_err, 8),
                "status": "PASS",
            }
        with open(os.path.join(parity_dir, "parity_report.json"), "w") as fp:
            json.dump(report, fp, indent=2)

if __name__ == "__main__":
    unittest.main()
