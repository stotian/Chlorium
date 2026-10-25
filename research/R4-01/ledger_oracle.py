"""Offline ledger comparator. Prepared only; no GD, GL, screenshots or imports of game code."""
import json
import math
import re
import sys
from pathlib import Path

MAX_BYTES = 2 * 1024 * 1024
FRAME_KEYS = {"scene_epoch", "frame_id", "context_generation", "draws", "triggers", "state_before", "state_after"}
DRAW_KEYS = {"native_draw_ordinal", "primitive_ordinal", "resource_generation", "texture_key", "shader_key",
             "quad_sha256", "blend", "target", "viewport", "scissor", "stencil", "depth"}
STATE_KEYS = {"program", "vao", "draw_fbo", "read_fbo", "texture", "sampler", "blend", "scissor", "stencil", "depth", "cocos_cache"}
BLEND_KEYS = {"enabled", "src_rgb", "dst_rgb", "src_alpha", "dst_alpha", "equation_rgb", "equation_alpha"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def positive_integer(value):
    return type(value) is int and value > 0


def nonnegative_integer(value):
    return type(value) is int and value >= 0


def exact_keys(value, keys, path):
    require(type(value) is dict and set(value) == keys, f"{path}: invalid fields")


def validate_blend(value, path):
    exact_keys(value, BLEND_KEYS, path)
    require(type(value["enabled"]) is bool, f"{path}.enabled: expected bool")
    for key in BLEND_KEYS - {"enabled"}:
        require(type(value[key]) is str and value[key], f"{path}.{key}: expected enum")


def validate(trace):
    exact_keys(trace, {"schema_version", "capture_complete", "frames"}, "trace")
    require(type(trace["schema_version"]) is int and trace["schema_version"] == 1, "unsupported schema")
    require(trace["capture_complete"] is True, "capture incomplete")
    frames = trace["frames"]
    require(type(frames) is list and 0 < len(frames) <= 4096, "invalid frame count")
    previous = None
    for number, frame in enumerate(frames):
        at = f"frames[{number}]"
        exact_keys(frame, FRAME_KEYS, at)
        for key in ("scene_epoch", "context_generation"):
            require(positive_integer(frame[key]), f"{at}.{key}: expected generation")
        require(nonnegative_integer(frame["frame_id"]), f"{at}.frame_id: expected index")
        identity = (frame["scene_epoch"], frame["frame_id"])
        require(previous is None or identity > previous, f"{at}: frame order/reuse")
        previous = identity
        for side in ("state_before", "state_after"):
            state = frame[side]
            exact_keys(state, STATE_KEYS, f"{at}.{side}")
            validate_blend(state["blend"], f"{at}.{side}.blend")
            require(type(state["cocos_cache"]) is dict, f"{at}.{side}: missing cache snapshot")
        require(type(frame["draws"]) is list and len(frame["draws"]) <= 16384, f"{at}: invalid draw count")
        for index, draw in enumerate(frame["draws"]):
            path = f"{at}.draws[{index}]"
            exact_keys(draw, DRAW_KEYS, path)
            for key in ("native_draw_ordinal", "primitive_ordinal"):
                require(nonnegative_integer(draw[key]), f"{path}.{key}: invalid ordinal")
            require(positive_integer(draw["resource_generation"]), f"{path}: invalid generation")
            require(type(draw["quad_sha256"]) is str and re.fullmatch(r"[0-9a-f]{64}", draw["quad_sha256"]), f"{path}: invalid digest")
            for key in ("texture_key", "shader_key", "target"):
                require(type(draw[key]) is str and draw[key], f"{path}.{key}: invalid identity")
            require(type(draw["viewport"]) is list and len(draw["viewport"]) == 4 and
                    all(type(v) is int for v in draw["viewport"]), f"{path}: invalid viewport")
            validate_blend(draw["blend"], f"{path}.blend")
        require(type(frame["triggers"]) is list and len(frame["triggers"]) <= 16384, f"{at}: invalid trigger count")
        last_sequence = -1
        for trigger in frame["triggers"]:
            exact_keys(trigger, {"sequence", "event", "target", "value_bits"}, f"{at}.trigger")
            require(nonnegative_integer(trigger["sequence"]) and trigger["sequence"] > last_sequence,
                    f"{at}: trigger order/reuse")
            last_sequence = trigger["sequence"]
            require(type(trigger["event"]) is str and trigger["event"], f"{at}: invalid event")
            require(type(trigger["target"]) is str and trigger["target"], f"{at}: invalid target")
            require(type(trigger["value_bits"]) is str and re.fullmatch(r"(?:[0-9a-f]{8}|[0-9a-f]{16})", trigger["value_bits"]),
                    f"{at}: invalid float-bit encoding")


def first_difference(reference, candidate, path="trace"):
    # Python normally considers True == 1 and 1 == 1.0. Keep schema types exact.
    if type(reference) is not type(candidate):
        return f"{path}: type differs"
    if isinstance(reference, dict):
        if reference.keys() != candidate.keys():
            return f"{path}: fields differ"
        for key in reference:
            difference = first_difference(reference[key], candidate[key], f"{path}.{key}")
            if difference:
                return difference
    elif isinstance(reference, list):
        if len(reference) != len(candidate):
            return f"{path}: multiplicity differs ({len(reference)} vs {len(candidate)})"
        for index, (left, right) in enumerate(zip(reference, candidate)):
            difference = first_difference(left, right, f"{path}[{index}]")
            if difference:
                return difference
    elif reference != candidate:
        return f"{path}: value differs"
    return None


def compare(reference, candidate):
    validate(reference)
    validate(candidate)
    return first_difference(reference, candidate)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON key: {key}")
        result[key] = value
    return result


def forbidden_constant(value):
    raise ValueError(f"non-finite JSON number: {value}")


def finite_float(value):
    parsed = float(value)
    require(math.isfinite(parsed), "non-finite JSON float")
    return parsed


def read_trace(path):
    with Path(path).open("rb") as stream:
        content = stream.read(MAX_BYTES + 1)
    require(len(content) <= MAX_BYTES, "fixture exceeds 2 MiB limit")
    return json.loads(content, object_pairs_hook=unique_object,
                      parse_constant=forbidden_constant, parse_float=finite_float)


def main():
    if len(sys.argv) != 3:
        print("usage: ledger_oracle.py reference.json candidate.json", file=sys.stderr)
        return 2
    try:
        difference = compare(read_trace(sys.argv[1]), read_trace(sys.argv[2]))
    except (ValueError, TypeError, OSError, RecursionError) as error:
        print(f"INVALID: {error}", file=sys.stderr)
        return 2
    print(f"MISMATCH: {difference}" if difference else "MATCH: declared ledger fields")
    return 1 if difference else 0


if __name__ == "__main__":
    raise SystemExit(main())
