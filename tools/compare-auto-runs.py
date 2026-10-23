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
    return dict(samples=len(times), fps=1000 / statistics.mean(times),
                p95_ms=percentile(times, .95), p99_ms=percentile(times, .99), worst_ms=max(times),
                slow_frames=len(slow), slow_frame_percent=100 * len(slow) / len(times),
                slow_time_percent=100 * sum(slow) / sum(times),
                update_mean_ms=statistics.mean(float(row['update_ms']) for row in rows),
                visibility_mean_ms=statistics.mean(float(row['visibility_ms']) for row in rows))


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
    destination.with_suffix('.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(json.dumps({k: result[k] for k in ('level', 'sans', 'avec')}, ensure_ascii=False))


if __name__ == '__main__':
    compare(*(Path(p) for p in sys.argv[1:4]))
