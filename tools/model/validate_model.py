#!/usr/bin/env python3
"""Validate an Afterglow character GLB before it reaches Blender or the game.

The checker is deliberately dependency-free.  It reads the glTF JSON and binary
chunks directly, so CI can reject broken weights, morph tables, bones, and clips
without starting a renderer.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


COMPONENTS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT2": 4, "MAT3": 9, "MAT4": 16}
COMPONENT_INFO = {
    5120: ("b", 1),   # BYTE
    5121: ("B", 1),   # UNSIGNED_BYTE
    5122: ("h", 2),   # SHORT
    5123: ("H", 2),   # UNSIGNED_SHORT
    5125: ("I", 4),   # UNSIGNED_INT
    5126: ("f", 4),   # FLOAT
}


@dataclass
class Issue:
    level: str
    where: str
    message: str


class Glb:
    def __init__(self, path: Path):
        self.path = path
        raw = path.read_bytes()
        if len(raw) < 20 or raw[:4] != b"glTF":
            raise ValueError("missing glTF header")
        version, length = struct.unpack_from("<II", raw, 4)
        if version != 2:
            raise ValueError(f"unsupported glTF version {version}")
        if length != len(raw):
            raise ValueError(f"header length {length} differs from file length {len(raw)}")
        self.json: dict[str, Any] | None = None
        self.bin = b""
        off = 12
        while off + 8 <= len(raw):
            size, kind = struct.unpack_from("<II", raw, off)
            data = raw[off + 8:off + 8 + size]
            off += 8 + size
            if kind == 0x4E4F534A:  # JSON
                self.json = json.loads(data.rstrip(b" \t\r\n\x00"))
            elif kind == 0x004E4942:  # BIN
                self.bin = data
        if self.json is None:
            raise ValueError("missing JSON chunk")

    def accessor(self, index: int) -> list[tuple[float | int, ...]]:
        doc = self.json
        acc = doc["accessors"][index]
        count = int(acc["count"])
        typ = acc["type"]
        width = COMPONENTS[typ]
        fmt, size = COMPONENT_INFO[acc["componentType"]]
        view_index = acc.get("bufferView")
        if view_index is None:
            return [tuple(0 for _ in range(width)) for _ in range(count)]
        view = doc["bufferViews"][view_index]
        stride = int(view.get("byteStride", width * size))
        start = int(view.get("byteOffset", 0)) + int(acc.get("byteOffset", 0))
        unpack = struct.Struct("<" + fmt * width).unpack_from
        normal = bool(acc.get("normalized", False))
        out: list[tuple[float | int, ...]] = []
        for i in range(count):
            row = unpack(self.bin, start + i * stride)
            if normal:
                if acc["componentType"] == 5120:
                    row = tuple(max(-1.0, x / 127.0) for x in row)
                elif acc["componentType"] == 5121:
                    row = tuple(x / 255.0 for x in row)
                elif acc["componentType"] == 5122:
                    row = tuple(max(-1.0, x / 32767.0) for x in row)
                elif acc["componentType"] == 5123:
                    row = tuple(x / 65535.0 for x in row)
            out.append(row)
        return out


def finite(values: Iterable[float | int]) -> bool:
    return all(math.isfinite(float(x)) for x in values)


def target_names(mesh: dict[str, Any], primitive_count: int) -> list[str]:
    extras = mesh.get("extras") or {}
    names = extras.get("targetNames") or extras.get("target_names") or []
    names = [str(x) for x in names]
    if len(names) < primitive_count:
        names.extend(f"target_{i}" for i in range(len(names), primitive_count))
    return names


def add(issues: list[Issue], level: str, where: str, message: str) -> None:
    issues.append(Issue(level, where, message))


def validate(doc: Glb, required_bones: set[str], strict: bool = False) -> list[Issue]:
    g = doc.json
    issues: list[Issue] = []
    nodes = g.get("nodes", [])
    meshes = g.get("meshes", [])
    skins = g.get("skins", [])
    accessors = g.get("accessors", [])
    names = [n.get("name", f"node_{i}") for i, n in enumerate(nodes)]
    seen: set[str] = set()
    for i, (name, node) in enumerate(zip(names, nodes)):
        if name in seen:
            add(issues, "error", f"nodes[{i}]", f"duplicate node name {name!r}")
        seen.add(name)
        for key in ("translation", "rotation", "scale"):
            if key in node and not finite(node[key]):
                add(issues, "error", f"nodes[{i}].{key}", "non-finite transform")
        if "scale" in node and any(abs(float(x)) < 1e-7 for x in node["scale"]):
            add(issues, "error", f"nodes[{i}].scale", "zero scale collapses skinning")
    missing = sorted(required_bones - set(names))
    if missing:
        add(issues, "error", "rig", "missing required bones: " + ", ".join(missing))

    mesh_users: dict[int, list[tuple[int, int]]] = {}
    for ni, node in enumerate(nodes):
        if "mesh" in node:
            mesh_users.setdefault(int(node["mesh"]), []).append((ni, int(node.get("skin", -1))))

    for si, skin in enumerate(skins):
        joints = skin.get("joints", [])
        if not joints:
            add(issues, "error", f"skins[{si}]", "skin has no joints")
        if len(set(joints)) != len(joints):
            add(issues, "error", f"skins[{si}]", "duplicate joint index")
        if any(j < 0 or j >= len(nodes) for j in joints):
            add(issues, "error", f"skins[{si}]", "joint index outside nodes")
        ibm = skin.get("inverseBindMatrices")
        if ibm is not None and int(accessors[ibm]["count"]) != len(joints):
            add(issues, "error", f"skins[{si}]", "inverse bind count does not match joints")

    for mi, mesh in enumerate(meshes):
        prims = mesh.get("primitives", [])
        morph_count = max((len(p.get("targets", [])) for p in prims), default=0)
        names_for_targets = target_names(mesh, morph_count)
        if len(set(names_for_targets)) != len(names_for_targets):
            add(issues, "error", f"meshes[{mi}].extras", "duplicate morph target name")
        for pi, prim in enumerate(prims):
            attrs = prim.get("attributes", {})
            pos_index = attrs.get("POSITION")
            if pos_index is None:
                add(issues, "error", f"meshes[{mi}].primitives[{pi}]", "missing POSITION")
                continue
            pos = doc.accessor(int(pos_index))
            if any(not finite(row) for row in pos):
                add(issues, "error", f"meshes[{mi}].primitives[{pi}].POSITION", "non-finite vertex")
            users = mesh_users.get(mi, [])
            skinned = any(s >= 0 for _, s in users)
            if skinned:
                for semantic in ("JOINTS_0", "WEIGHTS_0"):
                    if semantic not in attrs:
                        add(issues, "error", f"meshes[{mi}].primitives[{pi}]", f"missing {semantic} on skinned mesh")
                if "JOINTS_0" in attrs and "WEIGHTS_0" in attrs:
                    joints = doc.accessor(int(attrs["JOINTS_0"]))
                    weights = doc.accessor(int(attrs["WEIGHTS_0"]))
                    for vi, (jrow, wrow) in enumerate(zip(joints, weights)):
                        if not finite(jrow) or not finite(wrow):
                            add(issues, "error", f"meshes[{mi}].primitives[{pi}].vertex[{vi}]", "non-finite skin data")
                            break
                        if any(int(x) < 0 for x in jrow):
                            add(issues, "error", f"meshes[{mi}].primitives[{pi}].vertex[{vi}]", "negative joint index")
                            break
                        total = sum(float(x) for x in wrow)
                        if total < 0.98 or total > 1.02:
                            add(issues, "error", f"meshes[{mi}].primitives[{pi}].vertex[{vi}]", f"weights sum {total:.4f}, expected 1")
                            break
                    for _, skin_index in users:
                        if skin_index < 0 or skin_index >= len(skins):
                            continue
                        joint_count = len(skins[skin_index].get("joints", []))
                        if "JOINTS_0" in attrs:
                            max_joint = max((int(x) for row in joints for x in row), default=-1)
                            if max_joint >= joint_count:
                                add(issues, "error", f"meshes[{mi}].primitives[{pi}]", f"joint {max_joint} exceeds skin {joint_count}")
            for ti, target in enumerate(prim.get("targets", [])):
                for semantic, ai in target.items():
                    vals = doc.accessor(int(ai))
                    if len(vals) != len(pos):
                        add(issues, "error", f"meshes[{mi}].primitives[{pi}].target[{ti}]", f"{semantic} count differs from POSITION")
                    if any(not finite(row) for row in vals):
                        add(issues, "error", f"meshes[{mi}].primitives[{pi}].target[{ti}]", f"non-finite {semantic} delta")
                nm = names_for_targets[ti] if ti < len(names_for_targets) else f"target_{ti}"
                # Normals are required for expression/anatomy targets; body volume and hair
                # targets intentionally remain POSITION-only to avoid unstable shading normals.
                shape = nm.split("_")[0]
                needs_normal = not (shape in {"bust", "hips", "waist", "butt", "weight", "muscle", "belly", "age", "length", "girth"} or "hair" in mesh.get("name", "").lower())
                if needs_normal and "NORMAL" not in target:
                    add(issues, "error", f"meshes[{mi}].primitives[{pi}].target[{ti}].{nm}", "missing NORMAL delta")

    for ai, anim in enumerate(g.get("animations", [])):
        samplers = anim.get("samplers", [])
        for ci, channel in enumerate(anim.get("channels", [])):
            target = channel.get("target", {})
            node = target.get("node")
            if node is not None and (int(node) < 0 or int(node) >= len(nodes)):
                add(issues, "error", f"animations[{ai}].channels[{ci}]", "channel targets unknown node")
            sampler_index = int(channel.get("sampler", -1))
            if sampler_index < 0 or sampler_index >= len(samplers):
                add(issues, "error", f"animations[{ai}].channels[{ci}]", "unknown sampler")
                continue
            sampler = samplers[sampler_index]
            if "input" not in sampler or "output" not in sampler:
                add(issues, "error", f"animations[{ai}].samplers[{sampler_index}]", "missing input/output")
                continue
            times = doc.accessor(int(sampler["input"]))
            flat_times = [float(x[0]) for x in times]
            if any(not math.isfinite(x) for x in flat_times) or any(b < a for a, b in zip(flat_times, flat_times[1:])):
                add(issues, "error", f"animations[{ai}].samplers[{sampler_index}].input", "times are not finite and sorted")
            vals = doc.accessor(int(sampler["output"]))
            if target.get("path") == "rotation":
                for vi, row in enumerate(vals):
                    n = math.sqrt(sum(float(x) * float(x) for x in row[:4]))
                    if not math.isfinite(n) or n < 1e-5:
                        add(issues, "error", f"animations[{ai}].samplers[{sampler_index}].output[{vi}]", "zero or non-finite quaternion")
                        break

    return issues


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("glb", type=Path)
    ap.add_argument("--manifest", type=Path, help="model manifest with required bones")
    ap.add_argument("--slot", help="manifest slot to validate")
    args = ap.parse_args()
    required: set[str] = set()
    if args.manifest:
        manifest = json.loads(args.manifest.read_text())
        slots = manifest.get("slots", manifest)
        if args.slot:
            required = set(slots[args.slot].get("requiredBones", []))
    try:
        glb = Glb(args.glb)
        issues = validate(glb, required)
    except (OSError, ValueError, KeyError, IndexError, struct.error, json.JSONDecodeError) as exc:
        print(f"ERROR {args.glb}: {exc}")
        return 2
    errors = [i for i in issues if i.level == "error"]
    for issue in issues:
        print(f"{issue.level.upper()} {issue.where}: {issue.message}")
    if errors:
        print(f"FAILED {args.glb}: {len(errors)} error(s)")
        return 1
    print(f"OK {args.glb}: nodes={len(glb.json.get('nodes', []))} meshes={len(glb.json.get('meshes', []))} animations={len(glb.json.get('animations', []))}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
