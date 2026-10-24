#!/usr/bin/env python3
"""Validate the modular character manifest before it reaches Blender or the game."""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

from validate_model import Glb


def issue(errors: list[str], where: str, message: str) -> None:
    errors.append(f"{where}: {message}")


def finite_number(value: object) -> bool:
    return isinstance(value, (int, float)) and math.isfinite(float(value))


def check(path: Path, root: Path) -> list[str]:
    errors: list[str] = []
    try:
        manifest = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        return [f"{path}: {exc}"]
    if not isinstance(manifest, dict):
        return [f"{path}: root must be an object"]

    slots = manifest.get("slots")
    variants = manifest.get("variants")
    shots = manifest.get("shots")
    if not isinstance(slots, dict) or not slots:
        issue(errors, "slots", "must be a non-empty object")
        slots = {}
    if not isinstance(variants, dict) or not variants:
        issue(errors, "variants", "must be a non-empty object")
        variants = {}
    if not isinstance(shots, dict) or not shots:
        issue(errors, "shots", "must be a non-empty object")
        shots = {}

    tone_names = {"porcelain", "fair", "honey", "olive", "caramel", "bronze", "deep", "ebony"}
    for slot_name, slot in slots.items():
        where = f"slots.{slot_name}"
        if not isinstance(slot, dict):
            issue(errors, where, "must be an object")
            continue
        asset = slot.get("asset")
        if not isinstance(asset, str) or not asset:
            issue(errors, where, "asset must be a non-empty path")
        elif not (root / "native/assets/characters" / asset).is_file():
            issue(errors, where, f"asset does not exist: {asset}")
        else:
            try:
                glb = Glb(root / "native/assets/characters" / asset)
                clip_names = {str(a.get("name", "")) for a in glb.json.get("animations", [])}
                for shot_name, shot in (shots.items() if isinstance(shots, dict) else []):
                    if isinstance(shot, dict) and shot.get("clip") not in clip_names:
                        issue(errors, where, f"shot {shot_name!r} references missing clip {shot.get('clip')!r}")
            except (OSError, ValueError, KeyError, IndexError, json.JSONDecodeError) as exc:
                issue(errors, where, f"cannot inspect GLB: {exc}")
        bones = slot.get("requiredBones")
        if not isinstance(bones, list) or not bones:
            issue(errors, where, "requiredBones must be a non-empty list")
        elif len(set(bones)) != len(bones):
            issue(errors, where, "requiredBones contains duplicates")
        expressions = slot.get("expressionSets")
        shapes = slot.get("shapeSets")
        if not isinstance(expressions, list) or not expressions:
            issue(errors, where, "expressionSets must be a non-empty list")
            expressions = []
        if not isinstance(shapes, list) or not shapes:
            issue(errors, where, "shapeSets must be a non-empty list")
            shapes = []
        if len(set(expressions)) != len(expressions):
            issue(errors, where, "expressionSets contains duplicates")
        if len(set(shapes)) != len(shapes):
            issue(errors, where, "shapeSets contains duplicates")
        envelope = slot.get("poseEnvelope")
        if not isinstance(envelope, dict):
            issue(errors, where, "poseEnvelope must be an object")
        else:
            for key in ("maxScale", "maxTranslation", "maxMorph"):
                if not finite_number(envelope.get(key)) or float(envelope[key]) <= 0:
                    issue(errors, where, f"poseEnvelope.{key} must be positive and finite")
            if finite_number(envelope.get("maxMorph")) and float(envelope["maxMorph"]) > 1:
                issue(errors, where, "poseEnvelope.maxMorph exceeds the runtime morph guard")
        for key, value in slot.items():
            if key in {"asset", "height", "baseAge", "requiredBones", "expressionSets", "shapeSets", "poseEnvelope"}:
                continue
            if key.startswith("_"):
                continue

    for variant_name, variant in variants.items():
        where = f"variants.{variant_name}"
        if not isinstance(variant, dict):
            issue(errors, where, "must be an object")
            continue
        slot_name = variant.get("slot")
        if slot_name not in slots:
            issue(errors, where, f"unknown slot {slot_name!r}")
            continue
        if variant.get("adult") is not True:
            issue(errors, where, "adult must be true")
        if variant.get("fictional") is not True:
            issue(errors, where, "fictional must be true")
        if variant.get("tone") not in tone_names:
            issue(errors, where, f"unknown tone {variant.get('tone')!r}")
        slot = slots[slot_name]
        allowed_shapes = set(slot.get("shapeSets", []))
        sliders = variant.get("sliders", {})
        if not isinstance(sliders, dict):
            issue(errors, where, "sliders must be an object")
        else:
            for name, value in sliders.items():
                if name not in allowed_shapes:
                    issue(errors, where, f"slider {name!r} is not declared by slot {slot_name!r}")
                if not finite_number(value) or not -1 <= float(value) <= 1:
                    issue(errors, where, f"slider {name!r} must be finite in [-1, 1]")
        expressions = variant.get("expressions", {})
        if not isinstance(expressions, dict):
            issue(errors, where, "expressions must be an object")
        else:
            allowed_expressions = set(slot.get("expressionSets", []))
            for shot_name, expression in expressions.items():
                if shot_name not in shots:
                    issue(errors, where, f"expression references unknown shot {shot_name!r}")
                if expression not in allowed_expressions:
                    issue(errors, where, f"expression {expression!r} is not declared by slot {slot_name!r}")

    for shot_name, shot in shots.items():
        where = f"shots.{shot_name}"
        if not isinstance(shot, dict):
            issue(errors, where, "must be an object")
            continue
        if not isinstance(shot.get("label"), str) or not shot["label"]:
            issue(errors, where, "label must be non-empty")
        if not isinstance(shot.get("clip"), str) or not shot["clip"]:
            issue(errors, where, "clip must be non-empty")
        target = shot.get("target")
        if not isinstance(target, list) or len(target) != 3 or not all(finite_number(x) for x in target):
            issue(errors, where, "target must contain three finite numbers")
        if not finite_number(shot.get("distance")) or float(shot["distance"]) <= 0:
            issue(errors, where, "distance must be positive and finite")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--root", type=Path, default=Path.cwd(), help="repository root containing native/assets")
    args = parser.parse_args()
    errors = check(args.manifest, args.root)
    if errors:
        for error in errors:
            print(f"ERROR {error}")
        print(f"FAILED {args.manifest}: {len(errors)} error(s)")
        return 1
    data = json.loads(args.manifest.read_text())
    print(f"OK {args.manifest}: slots={len(data['slots'])} variants={len(data['variants'])} shots={len(data['shots'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
