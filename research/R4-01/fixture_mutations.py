"""Private offline oracle self-check, prepared but not executed in the V5 preflight."""
from copy import deepcopy
from pathlib import Path
from ledger_oracle import compare, read_trace


def main():
    reference = read_trace(Path(__file__).with_name("fixture.json"))
    if compare(reference, deepcopy(reference)) is not None:
        raise AssertionError("oracle rejected the unchanged control")
    mutations = []

    def candidate(name):
        trace = deepcopy(reference)
        mutations.append((name, trace))
        return trace["frames"][0]

    candidate("missing primitive")["draws"].pop()
    value = candidate("duplicate primitive")
    value["draws"].append(deepcopy(value["draws"][0]))
    candidate("transparent painter order")["draws"].reverse()
    candidate("quad bytes")["draws"][0]["quad_sha256"] = "c" * 64
    candidate("stale resource generation")["draws"][0]["resource_generation"] = 2
    candidate("foreign context")["context_generation"] = 2
    candidate("blend disabled")["draws"][0]["blend"]["enabled"] = False
    candidate("shader changed")["draws"][0]["shader_key"] = "fixture:unexpected"
    candidate("missing offscreen trigger")["triggers"].pop()
    candidate("trigger order")["triggers"].reverse()
    candidate("read framebuffer leak")["state_after"]["read_fbo"] = "fixture:diagnostic"
    candidate("Cocos cache drift")["state_after"]["cocos_cache"]["blend_enabled"] = False

    rejected = 0
    for name, trace in mutations:
        try:
            difference = compare(reference, trace)
        except ValueError:
            rejected += 1
            continue
        if difference is None:
            raise AssertionError(f"oracle accepted sabotage: {name}")
        rejected += 1
    print(f"Control accepted; {rejected}/{len(mutations)} private ledger mutations rejected")


if __name__ == "__main__":
    main()
