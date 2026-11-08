"""Balanced native/reference/R36 check. No automatic promotion or FPS claims."""
import csv
import importlib.util
import json
import math
from pathlib import Path
import statistics
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
spec = importlib.util.spec_from_file_location('pair', ROOT / 'tools/compare-auto-runs.py')
pair = importlib.util.module_from_spec(spec); spec.loader.exec_module(pair)
ORDER = ['native', 'reference', 'r36', 'r36', 'reference', 'native']


def production_version(events):
    variants=[e for e in events if e.get('event')=='comparison_variant']
    if len(variants)!=1 or not variants[0].get('production_loaded') or not variants[0].get('production_enabled') or not isinstance(variants[0].get('production_version'),str) or not variants[0]['production_version']:
        raise ValueError('Actual loaded production version must be recorded')
    return variants[0]['production_version']


def conditions(provenance):
    for name in ('visual_diagnostic','external_sampling_requested','opacity_shadow_diagnostic','native_pass_capture_diagnostic','native_quad_reads_diagnostic','owned_register_oracle_diagnostic'):
        if provenance.get(name):raise ValueError('Diagnostic run cannot count as performance evidence: '+name)
    keys = ('game_sha256','measurement_sha256','level_sha256','level','target_fps','target_from_percent','target_to_percent','low_detail_requested','disable_shake_requested','mod_profile','mod_profile_sha256','profile_settings_sha256')
    if any(k not in provenance for k in keys):raise ValueError('Incomplete comparison conditions')
    if provenance['target_fps']!=120 or provenance['low_detail_requested'] or not provenance['disable_shake_requested'] or provenance['mod_profile']!='isolated':raise ValueError('Unexpected test configuration')
    return tuple(provenance[k] for k in keys)


def compare(paths,reference_only=False):
    order=['reference','r36','r36','reference'] if reference_only else ORDER
    if len(paths)!=len(order):raise ValueError('Exactly the selected balanced protocol is required')
    runs=[];reference=None;packages={};versions=set()
    for variant,path in zip(order,paths,strict=True):
        events,rows,bins,provenance=pair.read(path,variant!='native')
        check=conditions(provenance)
        if reference is None:reference=check
        if check!=reference:raise ValueError('Different game, measurement, level, settings or profiles')
        if provenance['native']!=(variant=='native') or bool(provenance.get('native_opacity_hash_r36_required'))!=(variant=='r36'):raise ValueError('Wrong variant or missing R36 activation requirement')
        coverage=[e for e in events if e.get('event')=='coverage']
        if len(coverage)!=1 or not coverage[0].get('target_completed') or coverage[0].get('deaths')!=0 or coverage[0].get('renderer_inactive_frames')!=0:raise ValueError('Invalid target coverage or inactive candidate')
        core=[e for e in events if e.get('event')=='canonical_core_v10']
        pacing=[e for e in events if e.get('event')=='frame_pacing']
        if not core or not all(e.get('active') and not e.get('inactive_frames') for e in core):raise ValueError('Native core/patch activation not verified')
        if len(pacing)!=1 or not pacing[0].get('verified') or pacing[0].get('target_fps')!=120 or pacing[0].get('swap_interval')!=0 or not math.isclose(pacing[0]['interval_seconds'],1/120,abs_tol=1e-6):raise ValueError('Different pacing')
        with Path(str(path)+'.frames.csv').open() as stream:
            all_rows=list(csv.DictReader(stream))
        for row in all_rows:
            if provenance['target_from_percent']<=float(row['percent'])<provenance['target_to_percent'] and row['included']!='1' and row['reason']!='coverage_boundary':raise ValueError('Disturbed or incomplete target frames')
        expected=set(range(math.floor(provenance['target_from_percent']),math.ceil(provenance['target_to_percent'])))
        if set(bins)!=expected or any(len(bins[b])<10 for b in expected):raise ValueError('Insufficient per-percent coverage')
        if variant!='native':
            package=provenance.get('production_sha256')
            if not package or (variant in packages and packages[variant]!=package):raise ValueError('Changing production package within variant')
            packages[variant]=package
            versions.add(production_version(events))
        runs.append(dict(variant=variant,path=str(path),metrics=pair.summarize(rows,120),by_percent={p:pair.summarize(v,120) for p,v in bins.items()},resources=pair.resource_summary(events,provenance)))
    if len(versions)!=1 or len(set(packages.values()))!=2:raise ValueError('Expected two distinct same-version production builds')
    summary={v:{key:statistics.median(run['metrics'][key] for run in runs if run['variant']==v) for key in ('fps','p95_ms','p99_ms','worst_ms')} for v in dict.fromkeys(order)}
    return dict(evidence_status='balanced_single_level_two_passes_per_variant',protocol=order,native_arm_validated=not reference_only,normal_activation_proven=False,black_screens_measured=False,global_gain_proven=False,summary=summary,runs=runs)


if __name__=='__main__':
    reference_only=sys.argv[1:2]==['--reference-abba'];args=sys.argv[2:] if reference_only else sys.argv[1:]
    if len(args)!=(5 if reference_only else 7):raise SystemExit('[--reference-abba] OUTPUT.json and exactly four/six balanced reports are required')
    result=compare([Path(p) for p in args[1:]],reference_only)
    with Path(args[0]).open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2)
    print(json.dumps(result['summary']))
