"""Strict comparison of one absent/present V2 visual-diagnostic pair.

Candidates are evidence to inspect, never automatically labelled defects.
Usage: python tools/compare-visual.py NATIVE.jsonl WITH.jsonl OUTPUT_PREFIX
"""
import csv
import html
import json
import sys
from importlib import import_module
from pathlib import Path
from audit_json import read_events

KINDS={'full':0,'left':2,'right':4,'bottom':8,'top':16,'repeated_signature':32}


def rows(path):
    with path.open(encoding='utf-8-sig',newline='') as stream:
        return list(csv.DictReader(stream))


def one(events,name):
    found=[e for e in events if e['event']==name]
    if len(found)!=1:
        raise ValueError(f'Expected exactly one {name}, got {len(found)}')
    return found[0]


def read(path,loaded):
    events=read_events(path)
    if not events or events[-1].get('event')!='finished' or events[-1].get('success') is not True:
        raise ValueError(f'{path}: unsuccessful/incomplete run')
    variant=one(events,'comparison_variant')
    if variant.get('production_loaded') is not loaded or (loaded and variant.get('production_enabled') is not True):
        raise ValueError('Production presence/setting mismatch')
    quality=one(events,'test_quality')
    if quality.get('low_detail_effective') is not False or quality.get('skip_camera_shake_effective') is not True:
        raise ValueError('Full detail / no-shake not verified')
    coverage=one(events,'coverage')
    if coverage.get('deaths')!=0 or not (coverage.get('completed') or coverage.get('target_completed')):
        raise ValueError('Requested coverage not reached without deaths')
    watch=one(events,'visual_watch'); visual_coverage=one(events,'visual_coverage')
    visual=rows(Path(str(path)+'.visual.csv'))
    if (watch.get('version')!=2 or visual_coverage.get('version')!=2 or not visual_coverage.get('complete')
        or visual_coverage.get('invalid')!=0 or visual_coverage.get('skipped')!=0
        or visual_coverage.get('pending_at_finish')!=0
        or visual_coverage.get('queued')!=visual_coverage.get('measured')
        or visual_coverage.get('attempted')!=visual_coverage.get('queued')
        or visual_coverage.get('measured')!=len(visual)
        or any(r.get('valid')!='1' for r in visual)
        or any(int(b['frame'])<=int(a['frame']) for a,b in zip(visual,visual[1:]))):
        raise ValueError('Readback V2 coverage is invalid or has gaps')
    if any(e['event'] in ('visual_gl_error','visual_read_error','visual_context_changed') for e in events):
        raise ValueError('Readback errors/context change require investigation')
    profile=one(events,'mod_profile')
    if profile.get('expected_mods_verified') is not True:
        raise ValueError('Mod inventory was not verified')
    inventory=one(events,'mod_inventory')['mods']
    other_mods=sorted((m['id'],m['version'],m['loaded']) for m in inventory
                      if m['id'] not in ('local.chlorium','local.chlorium.audit'))
    timing={int(r['frame']):r for r in rows(Path(str(path)+'.frames.csv'))}
    excluded=0
    for r in visual:
        f=timing.get(int(r['frame']),{})
        r['eligible']=(f.get('foreground')=='1' and f.get('context')=='1' and f.get('paused')=='0'
                       and f.get('dead1')=='0' and f.get('dead2')=='0')
        excluded+=not r['eligible']
    provenance=json.loads(Path(str(path)+'.provenance.json').read_text(encoding='utf-8-sig'))
    return dict(events=events,visual=visual,coverage=coverage,watch=watch,visual_coverage=visual_coverage,
                mods=other_mods,provenance=provenance,excluded_visual_frames=excluded)


def episodes(data,kind):
    mask=KINDS[kind]; found=[]; current=None
    for row in data:
        flagged=row['eligible'] and (row['confirmed_dark']=='1' if not mask else bool(int(row['partial_flags'])&mask))
        if current and not flagged:
            current['duration_s']=float(row['elapsed'])-current['start_elapsed']
            current['coverage_gap_at_end']=not row['eligible']
            found.append(current);current=None
        if flagged:
            if current is None:
                current=dict(start_percent=float(row['percent']),start_elapsed=float(row['elapsed']),
                             start_game_time=float(row['game_time']),start_frame=int(row['frame']),frames=0)
            current.update(end_percent=float(row['percent']),end_elapsed=float(row['elapsed']),end_frame=int(row['frame']))
            current['frames']+=1
    if current:
        current['duration_s']=current['end_elapsed']-current['start_elapsed']
        current['coverage_gap_at_end']=True;found.append(current)
    return found


def match_episodes(left,right,tolerance=.03):
    # Sampling can split one reference blink into several candidate episodes.
    # Preserve that fact instead of incorrectly calling the second fragment
    # an episode with no native reference. This is proximity, not pixel identity.
    used=set();matches=[]
    for item in right:
        candidates=[(i,n) for i,n in enumerate(left)
                    if item['start_percent']<=n['end_percent']+tolerance
                    and item['end_percent']>=n['start_percent']-tolerance]
        if candidates:
            i,match=min(candidates,key=lambda pair:abs(pair[1]['start_percent']-item['start_percent']))
            matches.append(dict(avec=item,sans=match,native_episode_index=i,native_reference_reused=i in used,
                                start_delta_percent=item['start_percent']-match['start_percent'],
                                end_delta_percent=item['end_percent']-match['end_percent'],
                                duration_delta_s=item['duration_s']-match['duration_s']))
            used.add(i)
        else: matches.append(dict(avec=item,sans=None,native_reference_reused=False))
    return matches


def probe_report(a,b,native,candidate,destination):
    requested=a['provenance'].get('visual_probe_percents',[])
    if not requested:
        return []
    pairs=[]; groups=[]
    for source,path,mode in ((a,native,'sans'),(b,candidate,'avec')):
        points={e['requested_percent']:e for e in source['events'] if e['event']=='visual_probe'}
        if len(points)!=len(requested) or set(points)!=set(requested):
            raise ValueError('Missing or duplicate requested probe')
        groups.append((points,path,mode))
    images=destination.parent/'images';images.mkdir(exist_ok=True)
    content=[f'<h1>Sondes visuelles — {a["provenance"]["level"]}</h1>',
             '<p>Captures sans et avec Chlorium. Les pourcentages réels peuvent différer ; chaque écart est affiché. '
             'Même point demandé ne signifie pas même pixel ou même instant de simulation. '
             'Les signatures spatiales ne constituent pas un verdict automatique de défaut.</p>']
    for percent in requested:
        pair={'requested_percent':percent};figures=[]
        for points,path,mode in groups:
            event=points[percent]
            if event.get('saved') is not True:
                raise ValueError('A requested probe capture was not saved')
            raw=Path(str(path)+f'.probe-{event["frame"]}.ppm')
            if not raw.exists():
                raise ValueError('A requested probe image is missing')
            target=images/f'probe-{destination.name}-{mode}-{event["frame"]}.png'
            from PIL import Image
            if not target.exists():
                with Image.open(raw) as im: im.save(target)
            delta=event['percent']-percent
            pair[mode]=dict(frame=event['frame'],percent=event['percent'],game_time=event['game_time'],
                            offset_percent=delta,image='images/'+target.name)
            figures.append(f'<figure><figcaption><strong>{mode.upper()} CHLORIUM</strong> — '
                           f'{event["percent"]:.5f} % (écart à la cible : {delta:+.5f}) ; '
                           f'temps du jeu : {event["game_time"]:.5f} s</figcaption>'
                           f'<img loading="lazy" src="images/{html.escape(target.name)}"></figure>')
        pairs.append(pair)
        content.append(f'<h2>Cible {percent:.3f} %</h2><p>Écart avec − sans : '
                       f'{pair["avec"]["percent"]-pair["sans"]["percent"]:+.5f} point ; '
                       f'{pair["avec"]["game_time"]-pair["sans"]["game_time"]:+.5f} s sur l’horloge du jeu. '
                       'Les particules et animations peuvent varier.</p><div class="pair">'+''.join(figures)+'</div>')
    page='<!doctype html><meta charset="utf-8"><title>Sondes Chlorium</title><style>' \
         'body{font:16px/1.5 Segoe UI,Arial;margin:24px;background:#f3f5f7;color:#182432}' \
         '.pair{display:flex;gap:12px}figure{margin:0;flex:1;min-width:0;background:white}' \
         'figcaption{padding:10px}img{width:100%;height:auto;display:block}' \
         '@media(max-width:850px){.pair{display:block}}h2{margin-top:32px}</style>'+''.join(content)
    destination.with_name(destination.name+'-probes').with_suffix('.html').write_text(page,encoding='utf-8')
    return pairs


def compare(native,candidate,destination):
    a=read(native,False);b=read(candidate,True)
    required=('level','game_sha256','measurement_sha256','level_sha256','mod_profile','mod_profile_sha256',
              'profile_settings_sha256','target_from_percent','target_to_percent','visual_diagnostic')
    for key in required:
        av,bv=a['provenance'].get(key),b['provenance'].get(key)
        if av is None or av!=bv:
            raise ValueError(f'Missing or different comparison condition: {key}')
    # Old V2 traces predate these fields. Preserve their comparability only
    # when both sides lack a field; never silently equate missing and present.
    for key in ('exact_transform_math_verification','math_verification_stride_requested',
                'math_diagnostics_requested','area_math_trace_requested'):
        ap,bp=a['provenance'],b['provenance']
        if (key in ap)!=(key in bp) or ap.get(key)!=bp.get(key):
            raise ValueError(f'Different or incomplete math diagnostic condition: {key}')
    if a['mods']!=b['mods'] or a['watch']!=b['watch']:
        raise ValueError('Different loaded mod state or visual settings')
    if a['provenance'].get('visual_probe_percents',[])!=b['provenance'].get('visual_probe_percents',[]):
        raise ValueError('Different requested visual probe percentages')
    if any(r['provenance'].get('debug_overlays_disabled_requested') is not True for r in (a,b)):
        raise ValueError('Unmasked debug overlays can hide black frames: use the corrected full-mod protocol')
    report=dict(level=a['provenance']['level'],conditions_verified=True,
                production_sha256=b['provenance']['production_sha256'],
                measurement_sha256=a['provenance']['measurement_sha256'],
                sample_counts={'sans':len(a['visual']),'avec':len(b['visual'])},
                excluded_visual_frames={'sans':a['excluded_visual_frames'],'avec':b['excluded_visual_frames']},
                coverage={'sans':a['coverage'],'avec':b['coverage']},
                matching_tolerance_percent=.03,classes={})
    destination.parent.mkdir(parents=True,exist_ok=True)
    report['probes']=probe_report(a,b,native,candidate,destination)
    lines=[f"# Comparaison visuelle V2 — {report['level']}",'',
           'Lectures GPU valides, complètes et chronologiques ; mêmes paquets, mods chargés, réglages, données et module de mesure.',
           'LDM OFF / SHAKE OFF ; collisions natives. Les images hors focus, en pause ou mortes sont exclues de la comparaison.',
           'Les zones partielles sont des candidats issus d’un échantillonnage spatial. Une signature répétée ne prouve pas une image figée.',
           'La zone analysée couvre toute la largeur entre 25 % et 85 % de la hauteur depuis le bas. « Full » signifie quasi-noir dans cette zone, pas chaque pixel de l’écran.',
           'Les durées et les bornes sont conservées : le rapprochement à 0,03 point ne prouve pas une identité pixel à pixel.','',
           '| Classe | Episodes sans / avec | Avec sans référence proche | Référence partagée |','|---|---:|---:|---:|']
    all_rows=[]
    for kind in KINDS:
        left,right=episodes(a['visual'],kind),episodes(b['visual'],kind)
        matches=match_episodes(left,right)
        report['classes'][kind]=dict(sans=left,avec=right,matches=matches,
                                    unmatched_with=sum(m['sans'] is None for m in matches))
        lines.append(f"| {kind} | {len(left)} / {len(right)} | {report['classes'][kind]['unmatched_with']} | {sum(m['native_reference_reused'] for m in matches)} |")
        for mode,items in [('sans',left),('avec',right)]:
            all_rows.extend(dict(kind=kind,mode=mode,**item) for item in items)
    lines+=['','Une référence partagée signifie que plusieurs fragments du parcours avec se rapprochent du même épisode sans : les nombres et durées restent distincts, sans affirmation d’identité.',
            'Les candidats sans correspondance doivent être examinés dans les captures et répétés avant attribution au mod.',
            'Ce rapport ne valide pas 60 FPS ni les textures de chaque image. Les FPS du diagnostic ne sont pas une mesure de gain en jeu normal.']
    if report['probes']:
        lines+=['',f'[Captures appariées avec pourcentages réels]({destination.name}-probes.html).']
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.with_suffix('.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    markdown='\n'.join(lines)+'\n'
    destination.with_suffix('.md').write_text(markdown,encoding='utf-8')
    import_module('diagnostic-report').readable_html(markdown,destination.with_suffix('.html'))
    with destination.with_suffix('.csv').open('w',newline='',encoding='utf-8-sig') as stream:
        keys=list(dict.fromkeys(k for row in all_rows for k in row))
        if keys:
            writer=csv.DictWriter(stream,fieldnames=keys);writer.writeheader();writer.writerows(all_rows)
    print(json.dumps(dict(level=report['level'],samples=report['sample_counts'],excluded=report['excluded_visual_frames'],
                         classes={k:{'sans':len(v['sans']),'avec':len(v['avec']),'unmatched_with':v['unmatched_with']}
                                  for k,v in report['classes'].items()})))


if __name__=='__main__':
    compare(*(Path(p) for p in sys.argv[1:4]))
