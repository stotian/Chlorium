"""Compare a literal absent/present production pair, retaining slow sections.

Usage: python tools/compare-auto-runs.py BASELINE.jsonl CHLORIUM.jsonl OUTPUT_PREFIX
Only focused, alive, measured frames count. A single pair is not a causal
proof of a small improvement: CPU/GPU clocks and background load can vary.
"""
import csv
import json
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path
from audit_json import read_events


def percentile(values, q):
    return sorted(values)[int(q * (len(values) - 1))] if values else None


def summarize(rows):
    times = [float(row['frame_ms']) for row in rows]
    if not times:
        return {'samples': 0}
    slow = [v for v in times if v > 1000 / 60]
    result = dict(samples=len(times), fps=1000 / statistics.mean(times),
                p95_ms=percentile(times, .95), p99_ms=percentile(times, .99), worst_ms=max(times),
                slow_frames=len(slow), slow_frame_percent=100 * len(slow) / len(times),
                slow_time_percent=100 * sum(slow) / sum(times),
                update_mean_ms=statistics.mean(float(row['update_ms']) for row in rows),
                visibility_mean_ms=statistics.mean(float(row['visibility_ms']) for row in rows))
    for stage in ('cpu', 'scene', 'commands', 'move', 'rotation', 'transform', 'area', 'follow', 'collisions', 'particles'):
        key = stage + '_ms'
        if all(key in row for row in rows):
            result[stage + '_mean_ms'] = statistics.mean(float(row[key]) for row in rows)
    return result


def resource_summary(events, provenance):
    samples = [e for e in events if e.get('event') == 'resources' and
               provenance.get('target_from_percent', 0) <= e.get('percent', -1) <= provenance.get('target_to_percent', 100)]
    memory = [e for e in samples if e.get('system_memory_valid')]
    process = [e for e in samples if e.get('process_memory_valid')]
    return dict(samples=len(samples),
                available_physical_mib_min=min((e['available_physical_bytes'] / 1048576 for e in memory), default=None),
                private_mib_max=max((e['private_bytes'] / 1048576 for e in process), default=None),
                working_set_mib_max=max((e['working_set_bytes'] / 1048576 for e in process), default=None),
                page_fault_count_delta=process[-1]['page_faults'] - process[0]['page_faults'] if len(process) > 1 else None)


def work_summary(rows):
    # Adjacent accepted frames only: never bridge a capture, pause, loss of
    # focus, reset or missing frame. Game time is a workload proxy, not a
    # direct counter of physics steps or proof of causation.
    pairs = [(a, b) for a, b in zip(rows, rows[1:]) if int(b['frame']) == int(a['frame']) + 1
             and float(b['game_time']) > float(a['game_time'])]
    simulated_ms = sum(float(b['game_time']) - float(a['game_time']) for a, b in pairs) * 1000
    result = dict(adjacent_samples=len(pairs), simulated_ms=simulated_ms)
    for key in ('cpu_ms', 'update_ms', 'move_ms', 'rotation_ms', 'transform_ms', 'area_ms'):
        if all(key in b for a, b in pairs):
            result[key + '_per_sim_ms'] = sum(float(b[key]) for a, b in pairs) / simulated_ms if simulated_ms else None
    return result


def read(report, expected_loaded):
    events = read_events(report)
    variants = [e for e in events if e.get('event') == 'comparison_variant']
    if len(variants) != 1 or variants[0]['production_loaded'] != expected_loaded:
        raise ValueError(f'{report}: production presence was not verified')
    if not events[-1].get('success') or events[-1].get('event') != 'finished':
        raise ValueError(f'{report}: incomplete run')
    with Path(str(report) + '.frames.csv').open(newline='') as stream:
        rows = [r for r in csv.DictReader(stream) if r['included'] == '1']
    bins = defaultdict(list)
    for row in rows:
        bins[min(99, math.floor(float(row['percent'])))].append(row)
    provenance = json.loads(Path(str(report) + '.provenance.json').read_text(encoding='utf-8-sig'))
    return events, rows, bins, provenance


def compare(baseline, candidate, destination):
    a, ar, ab, ap = read(baseline, False)
    b, br, bb, bp = read(candidate, True)
    if ap['level'] != bp['level'] or ap['measurement_sha256'] != bp['measurement_sha256'] or ap['game_sha256'] != bp['game_sha256']:
        raise ValueError('Different levels, measurement modules or game binaries')
    if ap.get('level_sha256') != bp.get('level_sha256') or not ap.get('level_sha256'):
        raise ValueError('Level data equivalence has not been recorded')
    for key in ('target_from_percent', 'target_to_percent', 'visual_diagnostic', 'low_detail_requested', 'disable_shake_requested',
                'mod_profile', 'mod_profile_sha256', 'profile_settings_sha256'):
        if ap.get(key) != bp.get(key):
            raise ValueError(f'Different comparison conditions: {key}')
    for key in ('exact_transform_math_verification', 'math_verification_stride_requested',
                'math_diagnostics_requested', 'area_math_trace_requested'):
        if (key in ap) != (key in bp) or ap.get(key) != bp.get(key):
            raise ValueError(f'Different or incomplete math diagnostic condition: {key}')
    coverage = []
    for events in (a, b):
        items = [e for e in events if e.get('event') == 'coverage']
        if len(items) != 1:
            raise ValueError('Missing or ambiguous coverage')
        item = items[0]
        if not (item.get('completed') or item.get('target_completed')):
            raise ValueError('Requested coverage was not reached')
        coverage.append(item)
    comparisons = []
    for percent in sorted(ab.keys() | bb.keys()):
        left, right = summarize(ab[percent]), summarize(bb[percent])
        item = dict(percent=percent)
        for mode, data in [('sans', left), ('avec', right)]:
            item.update({mode + '_' + k: v for k, v in data.items()})
        item['p95_change_percent'] = (100 * (right['p95_ms'] / left['p95_ms'] - 1)
                                      if left['samples'] and right['samples'] else None)
        comparisons.append(item)
    result = dict(level=ap['level'], sans=summarize(ar), avec=summarize(br),
                  resources_sans=resource_summary(a, ap), resources_avec=resource_summary(b, bp),
                  work_sans=work_summary(ar), work_avec=work_summary(br),
                  coverage_sans=coverage[0], coverage_avec=coverage[1],
                  production_sha256=bp['production_sha256'], measurement_sha256=ap['measurement_sha256'],
                  percent_bins=comparisons, limitation='One paired run; small differences require repeat runs.')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.with_suffix('.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    keys = list(dict.fromkeys(k for row in comparisons for k in row))
    with destination.with_suffix('.csv').open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=keys); writer.writeheader(); writer.writerows(comparisons)
    lines = [f"# Comparaison du niveau {ap['level']}", '',
             '| Mesure | Sans Chlorium | Avec Chlorium |', '|---|---:|---:|']
    for label, key in [('FPS moyens', 'fps'), ('p95 image (ms)', 'p95_ms'), ('p99 image (ms)', 'p99_ms'),
                       ('Pire image (ms)', 'worst_ms'), ('Images > 16,67 ms (%)', 'slow_frame_percent'),
                       ('Temps dans ces images (%)', 'slow_time_percent')]:
        lines.append(f"| {label} | {result['sans'][key]:.2f} | {result['avec'][key]:.2f} |")
    lines += ['', 'Collisions normales.',
              f"Plage demandée : {ap.get('target_from_percent', 0)}–{ap.get('target_to_percent', 100)} %.",
              f"Niveau entièrement terminé sans/avec : {coverage[0].get('completed', False)}/{coverage[1].get('completed', False)}. "
              f"Morts sans/avec : {coverage[0].get('deaths', 'inconnu')}/{coverage[1].get('deaths', 'inconnu')}.",
              'Même module de mesure et mêmes données du niveau ; paquet normal réellement absent/présent.',
              'Un seul couple de parcours : les petits écarts peuvent venir des variations du PC.', '',
              '## Passages les plus lents avec Chlorium', '',
              '| Passage | p95 sans (ms) | p95 avec (ms) | Images sans/avec |', '|---|---:|---:|---:|']
    for row in sorted((r for r in comparisons if r.get('avec_p95_ms') is not None), key=lambda r: r['avec_p95_ms'], reverse=True)[:12]:
        before = f"{row['sans_p95_ms']:.2f}" if row.get('sans_p95_ms') is not None else 'non mesuré'
        lines.append(f"| {row['percent']} % | {before} | {row['avec_p95_ms']:.2f} | {row['sans_samples']}/{row['avec_samples']} |")
    lines += ['', '## Ressources pendant la section', '',
              '| Mode | RAM disponible minimale (Mio) | Mémoire privée maximale (Mio) | Défauts de page (tous types) |',
              '|---|---:|---:|---:|']
    for mode in ('sans', 'avec'):
        resources = result['resources_' + mode]
        fields = [resources[key] for key in ('available_physical_mib_min', 'private_mib_max', 'page_fault_count_delta')]
        values = [f'{value:.1f}' if value is not None else 'non mesuré' for value in fields]
        lines.append('| ' + mode + ' | ' + ' | '.join(values) + ' |')
    lines += ['', 'Les défauts de page incluent les défauts sans lecture disque ; ce compteur seul ne prouve pas des accès au fichier d’échange.']
    destination.with_suffix('.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(json.dumps({k: result[k] for k in ('level', 'sans', 'avec')}, ensure_ascii=False))


if __name__ == '__main__':
    compare(*(Path(p) for p in sys.argv[1:4]))
