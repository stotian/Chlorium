"""Four verified Chlorium runs, with both renderers loaded; no vanilla claim."""
import argparse
import importlib.util
import json
import math
import statistics
from pathlib import Path

spec = importlib.util.spec_from_file_location("chlorium_pair", Path(__file__).with_name("compare-auto-runs.py"))
pair = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pair)


def compare(reports):
    if len(reports) != 4 or len({p.resolve() for p in reports}) != 4:
        raise ValueError("Four distinct reports are required")
    runs = [pair.read(path, True) for path in reports]
    runs.sort(key=lambda run: run[3]["started_utc"])
    if len({run[3]["started_utc"] for run in runs}) != 4:
        raise ValueError("Run timestamps must be distinct")
    variants = [run[3].get("renderer_variant") for run in runs]
    if any(not v or v == "previous-profile" for v in variants) or len(set(variants)) != 2:
        raise ValueError("Two explicitly identified renderer variants are required")
    order = [variants.index(v) for v in variants]
    if order not in ([0, 1, 1, 0], [0, 1, 0, 1]):
        raise ValueError("Use an ABBA or ABAB sequence, two runs per renderer")
    identities = {}
    first = runs[0]
    evidence = []
    for events, rows, bins, provenance in runs:
        target, coverage = pair.validate_conditions(first[0], first[1], first[3], events, rows, provenance)
        observed = coverage[1].get("observed_frames", 0)
        if (not observed or coverage[1].get("excluded_frames", observed) / observed > .05 or
                coverage[1].get("renderer_inactive_frames", 1) or coverage[1].get("gpu_pending", 1)):
            raise ValueError("Incomplete or disturbed measured-frame coverage")
        variant = provenance["renderer_variant"]
        if provenance.get("native") or provenance.get("low_detail_requested") is not False or provenance.get("disable_shake_requested") is not True:
            raise ValueError("Chlorium, full detail and disabled shake must be explicit")
        fingerprint = provenance.get("production_sha256")
        if not fingerprint or identities.setdefault(variant, fingerprint) != fingerprint:
            raise ValueError("A renderer changed between its two runs")
        mode = [e for e in events if e.get("event") == "comparison_variant"]
        if len(mode) != 1 or not mode[0].get("production_enabled"):
            raise ValueError("Renderer activation was not verified")
        if any(e.get("event") == "death" for e in events):
            raise ValueError("Deaths disturb paired workload coverage")
        start, end = provenance["target_from_percent"], provenance["target_to_percent"]
        expected = set(range(math.floor(start), math.ceil(end)))
        if set(bins) != expected:
            raise ValueError("Missing or extra percentage bins")
        evidence.append(dict(variant=variant, started_utc=provenance["started_utc"],
                             summary=pair.summarize(rows, target), coverage=coverage[1]))
    if len(set(identities.values())) != 2:
        raise ValueError("Both variants used the same package")
    # Match bins rather than allowing a slower section to gain sample weight.
    result_bins = []
    for percent in sorted(first[2]):
        item = {"percent": percent, "variants": {}}
        for variant in dict.fromkeys(variants):
            values = [pair.summarize(run[2][percent], target) for run in runs if run[3]["renderer_variant"] == variant]
            item["variants"][variant] = dict(runs=values, median_p95_ms=statistics.median(v["p95_ms"] for v in values))
        result_bins.append(item)
    return dict(level=first[3]["level"], target_fps=target, sequence=variants, runs=evidence,
                package_hashes=identities, percent_bins=result_bins,
                evidence_status="four_chlorium_runs_descriptive_only", normal_activation_proven=False,
                limitation="No vanilla baseline or level-image equivalence; no global FPS or zero-blackscreen claim.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("reports", type=Path, nargs=4)
    args = parser.parse_args()
    result = compare(args.reports)
    with args.output.open("x", encoding="utf-8") as output:
        json.dump(result, output, indent=2)
        output.write("\n")
