"""Summarize measured, focused/alive frames by level percentage.

Usage: python tools/analyze-auto-run.py REPORT.jsonl OUTPUT_PREFIX
The raw interval includes measurement overhead; frame_ms excludes it. Both
are retained, and GPU samples are joined by frame ID rather than position.
"""
import csv
import json
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path


def percentile(values, q):
    values = sorted(values)
    return values[int(q * (len(values) - 1))] if values else None


def analyze(report, destination):
    with Path(str(report) + '.gpu.csv').open(newline='') as stream:
        gpu = {int(row['frame']): float(row['gpu_ms']) for row in csv.DictReader(stream)}
    groups = defaultdict(list)
    exclusions = defaultdict(int)
    with Path(str(report) + '.frames.csv').open(newline='') as stream:
        for row in csv.DictReader(stream):
            if row['included'] != '1':
                exclusions[row['reason']] += 1
                continue
            groups[min(100, math.floor(float(row['percent'])))].append(row)
    output = []
    for percent, rows in sorted(groups.items()):
        frames = [float(row['frame_ms']) for row in rows]
        item = dict(percent=percent, samples=len(rows), fps=1000 / statistics.mean(frames),
                    over_16_67_ms=sum(ms > 1000 / 60 for ms in frames))
        for key in ('frame_ms', 'raw_frame_ms', 'cpu_ms', 'update_ms', 'visibility_ms', 'scene_ms',
                    'commands_ms', 'move_ms', 'rotation_ms', 'transform_ms', 'area_ms',
                    'follow_ms', 'collisions_ms', 'particles_ms'):
            values = [float(row[key]) for row in rows if key in row]
            item[key + '_p95'] = percentile(values, .95)
            item[key + '_worst'] = max(values) if values else None
        item['gpu_ms_p95'] = percentile([gpu[int(row['frame'])] for row in rows if int(row['frame']) in gpu], .95)
        output.append(item)
    if not output:
        raise SystemExit('No eligible frames in this run')
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.with_suffix('.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=output[0])
        writer.writeheader()
        writer.writerows(output)
    events = [json.loads(line) for line in report.read_text().splitlines() if line.strip()]
    summary = dict(source=str(report), percent_bins=output, excluded_frames=dict(exclusions),
                   deaths=[e for e in events if e.get('event') == 'death'],
                   coverage=[e for e in events if e.get('event') in ('coverage', 'finished')])
    destination.with_suffix('.json').write_text(json.dumps(summary, indent=2))
    for row in sorted(output, key=lambda row: row['frame_ms_p95'], reverse=True)[:12]:
        print(f"{row['percent']:3}%: {row['fps']:5.1f} FPS, p95 {row['frame_ms_p95']:6.1f} ms, "
              f"worst {row['frame_ms_worst']:6.1f} ms, n={row['samples']}, GPU p95={row['gpu_ms_p95']}")


if __name__ == '__main__':
    analyze(Path(sys.argv[1]), Path(sys.argv[2]))
