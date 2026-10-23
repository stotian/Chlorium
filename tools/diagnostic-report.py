"""Evidence-based level report: FPS per percent, dark frames, coverage/errors.

Usage: python tools/diagnostic-report.py RUN_DIRECTORY [OUTPUT_DIRECTORY] [MORE_RUN_DIRECTORIES...]
Visual diagnostics retain indicative FPS, never a claim of production speed.
"""
import csv
import json
import math
import html
import re
import shutil
import statistics
import sys
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from audit_json import read_events

LEVELS = {133260800: 'Orbit auto', 129598490: 'Heliopolis auto', 88952972: 'Grief auto'}


def events(path):
    return read_events(path, allow_partial_tail=True)


def rows(path):
    if not path.exists(): return []
    with path.open(encoding='utf-8-sig', newline='') as stream:
        return list(csv.DictReader(stream))


def percentile(values, q):
    return sorted(values)[int(q * (len(values)-1))] if values else None


def stats(data):
    if not data: return {}
    times = [float(r['raw_frame_ms']) for r in data]
    result = dict(samples=len(times), fps=1000/statistics.mean(times),
                  p95_ms=percentile(times,.95), p99_ms=percentile(times,.99), worst_ms=max(times),
                  stddev_ms=statistics.pstdev(times),
                  min_fps=1000/max(times), slow_frames=sum(v > 1000/60 for v in times))
    result['slow_time_percent'] = 100*sum(v for v in times if v > 1000/60)/sum(times)
    result['slow_percent'] = 100*result['slow_frames']/len(times)
    for name in ('update','visibility','move','rotation','transform','area','follow','collisions','particles'):
        values = [float(r[name+'_ms']) for r in data]
        result[name+'_mean_ms'] = statistics.mean(values)
        result[name+'_worst_ms'] = max(values)
    return result


def episodes(data):
    result = []; current = None; previous = None
    for r in data:
        dark = r.get('confirmed_dark') == '1'
        frame, time, percent = int(r['frame']), float(r['elapsed']), float(r['percent'])
        gap = previous is not None and frame != previous+1
        if current and (not dark or gap):
            current['end_elapsed'] = time
            current['duration_s'] = max(0,time-current['start_elapsed'])
            current['coverage_gap_at_end'] = gap
            result.append(current); current = None
        if dark:
            if not current:
                current = dict(start_frame=frame, start_elapsed=time, start_percent=percent, frames=0)
            current['frames'] += 1; current['end_percent'] = percent; current['end_elapsed'] = time
        previous = frame
    if current:
        current['duration_s'] = max(0,current['end_elapsed']-current['start_elapsed'])
        current['coverage_gap_at_end'] = True; result.append(current)
    return result


def load(path):
    es = events(path)
    start = next((e for e in es if e.get('event') == 'started'), None)
    if not start or start['level'] not in LEVELS: return None
    provenance_path = Path(str(path)+'.provenance.json')
    provenance = json.loads(provenance_path.read_text(encoding='utf-8-sig')) if provenance_path.exists() else {}
    if provenance.get('atlas_storage_regression'): return None
    visual = provenance.get('visual_diagnostic', False)
    trace = rows(Path(str(path)+'.frames.csv'))
    eligible = []
    excluded = defaultdict(int)
    for r in trace:
        reasons = set(r.get('reason','').split('|'))-{'','visual_diagnostic'}
        if reasons: excluded['|'.join(sorted(reasons))] += 1
        elif r.get('included') == '1' or visual:
            if float(r.get('raw_frame_ms',0)) > 0: eligible.append(r)
    deaths=[e for e in es if e.get('event') == 'death']
    bins = defaultdict(list); attempt_bins=defaultdict(list); attempts=defaultdict(list)
    for r in eligible:
        attempt=1+sum(float(r['elapsed']) > e['elapsed'] for e in deaths)
        r['attempt']=attempt
        percent=min(99,max(0,math.floor(float(r['percent']))))
        bins[percent].append(r); attempt_bins[attempt,percent].append(r); attempts[attempt].append(r)
    coverage = next((e for e in reversed(es) if e.get('event') == 'coverage'), {})
    variant = next((e for e in es if e.get('event') == 'comparison_variant'), {})
    completed = bool(coverage.get('completed'))
    success=es[-1].get('event')=='finished' and es[-1].get('success',False)
    mode = 'sans' if start['native'] else 'avec'
    verified = variant.get('production_loaded') == (mode == 'avec') and (mode == 'sans' or variant.get('production_enabled'))
    visual_rows = rows(Path(str(path)+'.visual.csv'))
    return dict(path=path, level=start['level'], mode=mode, visual=visual, events=es, provenance=provenance,
                variant_verified=bool(verified), completed=completed, success=success, summary=stats(eligible),
                bins={p:stats(rs) for p,rs in bins.items()}, excluded=dict(excluded), coverage=coverage,
                attempt_bins={p:stats(rs) for p,rs in attempt_bins.items()},
                attempts={a:dict(**stats(rs),max_percent=max(float(r['percent']) for r in rs)) for a,rs in attempts.items()},
                deaths=deaths, dark=episodes(visual_rows),
                dark_frames=sum(r.get('confirmed_dark') == '1' for r in visual_rows), visual_samples=len(visual_rows),
                captures=[e for e in es if e.get('event') == 'dark_frame'])


def f(value, digits=2): return '—' if value is None else f'{value:.{digits}f}'


def readable_html(markdown, target):
    # Small renderer for our own report syntax; escape all source text first.
    def inline(text):
        text=html.escape(text)
        text=re.sub(r'`([^`]+)`',r'<code>\1</code>',text)
        text=re.sub(r'\*\*([^*]+)\*\*',r'<strong>\1</strong>',text)
        def link(m):
            href=m[2]
            if href.endswith('.md'): href=href[:-3]+'.html'
            return f'<a href="{href}">{m[1]}</a>'
        return re.sub(r'\[([^\]]+)\]\(([^)]+)\)',link,text)
    content=[]; table=False; listing=False
    for line in markdown.splitlines():
        if line.startswith('|'):
            if listing: content.append('</ul>'); listing=False
            if not table: content.append('<div class="table"><table>'); table=True; first=True
            if re.match(r'^\|[\s:|\-]+$',line): continue
            tag='th' if first else 'td'; first=False
            content.append('<tr>'+''.join(f'<{tag}>{inline(c.strip())}</{tag}>' for c in line.strip('|').split('|'))+'</tr>')
            continue
        if table: content.append('</table></div>'); table=False
        if line.startswith('- '):
            if not listing: content.append('<ul>'); listing=True
            content.append('<li>'+inline(line[2:])+'</li>'); continue
        if listing: content.append('</ul>'); listing=False
        if not line.strip(): continue
        if line.startswith('#'):
            n=len(line)-len(line.lstrip('#')); content.append(f'<h{n}>{inline(line[n:].strip())}</h{n}>')
        else: content.append('<p>'+inline(line)+'</p>')
    if table: content.append('</table></div>')
    if listing: content.append('</ul>')
    title=html.escape(markdown.splitlines()[0].lstrip('# '))
    page='''<!doctype html><html lang="fr"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>'''+title+'''</title><style>
body{font:15px/1.55 "Segoe UI",Arial,sans-serif;color:#182432;background:#f3f5f7;margin:0}
main{max-width:1200px;margin:24px auto;background:white;padding:28px 36px;border:1px solid #dce2e8}
h1{font-size:30px;line-height:1.2}h2{font-size:21px;margin-top:32px;border-bottom:1px solid #dce2e8;padding-bottom:8px}
p{max-width:105ch}a{color:#075c99}code{font-size:12px;overflow-wrap:anywhere;background:#f1f4f6;padding:1px 4px}
.table{overflow-x:auto;margin:20px 0}table{border-collapse:collapse;width:100%;font-size:13px}td,th{padding:8px 10px;border:1px solid #dce2e8;text-align:right;vertical-align:top}th{background:#eaf0f5}td:first-child,th:first-child{text-align:left}tr:nth-child(2n){background:#f8fafb}
li{margin-bottom:6px}@media(max-width:650px){main{margin:0;padding:18px}}@media print{body{background:white}main{margin:0;border:0;padding:0;max-width:none}.table{overflow:visible}h2{break-after:avoid}tr{break-inside:avoid}}
</style><main><nav><a href="DIAGNOSTIC.html">Rapport général</a></nav>'''+''.join(content)+'</main></html>'
    target.write_text(page,encoding='utf-8')


def write_report(source, output, additional_sources=()):
    output.mkdir(parents=True,exist_ok=True)
    paths = sorted({p for folder in (source, *additional_sources) for p in folder.glob('auto-*.jsonl')})
    runs = [r for p in paths if (r:=load(p))]
    groups = defaultdict(dict)
    for r in runs: groups[r['level']][r['mode']] = r
    all_bins = []; all_dark = []
    lines = ['# Diagnostic Chlorium', '', 'Généré le '+datetime.now(timezone.utc).isoformat(timespec='seconds')+' (UTC).', '',
             'Niveaux autorisés : Orbit auto **133260800**, Heliopolis auto **129598490**, Grief auto **88952972**. ',
             'Collisions natives, aucun noclip ni input artificiel ; arrêt à la fin, après deux morts ou sur demande.', '',
             'Les parcours utilisent une installation de test. Le profil de chaque parcours indique si les mods personnels ont été copiés et leur chargement vérifié. Les mods désactivés par l’utilisateur restent désactivés.', '',
             '## Lecture des résultats', '',
             '« Sans » signifie que le paquet normal est absent ; « avec » signifie chargé et activé, vérification faite dans le jeu. ',
             'Les FPS ci-dessous utilisent les intervalles réels entre images, comme le compteur visible. ',
             'Avec le diagnostic visuel actif, ils incluent son coût : ce sont des mesures de diagnostic, pas une preuve de gain en utilisation normale. ',
             'Les captures synchrones, le démarrage, les morts, les pauses et les périodes sans focus sont exclus.', '',
             'Une image > 16,67 ms dépasse le budget de 60 FPS. Une bonne moyenne ne valide pas la stabilité. ',
             'Un seul couple de parcours par niveau : température, fréquence du PC, mémoire et tâches externes peuvent varier. Les moyennes globales sont à lire avec la couverture par pourcentage.',
             'Une image très sombre peut être un effet voulu du niveau ; la présence du même phénomène sans Chlorium est indiquée séparément.', '',
             '## État des parcours', '',
             '| Niveau | Mode | Fin du niveau atteinte | Cible atteinte | Morts | FPS moyens | p99 (ms) | Pire image (ms) | Images > 16,67 ms | Images très sombres |',
             '|---|---|---|---|---:|---:|---:|---:|---:|---:|']
    for level in LEVELS:
        for mode in ('sans','avec'):
            r = groups[level].get(mode)
            if not r:
                lines.append(f'| {LEVELS[level]} | {mode} | Non testé | — | — | — | — | — | — | — |'); continue
            s=r['summary']
            dark = r['dark_frames'] if r['visual_samples'] else 'Non surveillé'
            reached = r['completed'] or r['coverage'].get('target_completed', False)
            lines.append(f"| {LEVELS[level]} | {mode} | {'Oui' if r['completed'] else 'Non'} | {'Oui' if reached else 'Non'} | {len(r['deaths'])} | {f(s.get('fps'))} | {f(s.get('p99_ms'))} | {f(s.get('worst_ms'))} | {s.get('slow_frames','—')} | {dark} |")
    for level in LEVELS:
        pair=groups[level]
        details=[f'# {LEVELS[level]} — {level}', '',
                 'Les valeurs FPS sont indicatives quand la surveillance visuelle est active. « Pire » désigne la plus longue image du passage.', '',
                 '## Vérification et couverture', '']
        for mode,r in pair.items():
            es=r['events']; p=r['provenance']; coverage=r['coverage']
            visual_coverage=next((e for e in es if e.get('event')=='visual_coverage'),{})
            graphics=next((e for e in es if e.get('event')=='graphics_context'),{})
            objects=next((e.get('objects') for e in es if e.get('event')=='level_objects'),None)
            quality=next((e for e in es if e.get('event')=='test_quality'),{})
            profile=next((e for e in es if e.get('event')=='mod_profile'),{})
            inventory=next((e.get('mods',[]) for e in es if e.get('event')=='mod_inventory'),[])
            resources=[e for e in es if e.get('event')=='resources' and e.get('process_memory_valid')]
            peak=max((e['private_bytes'] for e in resources),default=0)/1024**2
            details += [f"- **{mode.upper()} Chlorium** : présence du mod {'vérifiée' if r['variant_verified'] else 'NON VÉRIFIÉE'}, "
                        f"{'niveau terminé' if r['completed'] else 'parcours incomplet'}, {len(r['deaths'])} mort(s).",
                        f"- Images observées : {coverage.get('observed_frames','—')}. Mémoire privée maximale observée : {peak:.1f} Mio.",
                        f"- Objets : {objects if objects is not None else 'non mesurés'}. Résolution : {graphics.get('width','—')} × {graphics.get('height','—')}.",
                        f"- Rendu : {graphics.get('renderer','non mesuré')} ; OpenGL {graphics.get('version','non mesuré')}.",
                        f"- Surveillance : {r['visual_samples']} images analysées ; {visual_coverage.get('skipped',0)} non prélevées quand la file GPU était pleine.",
                        f"- Module de mesure : `{p.get('measurement_sha256','inconnu')}`.",
                        f"- Paquet normal : `{p.get('production_sha256') or 'absent'}`.",
                        f"- Réglages effectifs : LDM={quality.get('low_detail_effective','non vérifié')}, secousses désactivées={quality.get('skip_camera_shake_effective','non vérifié')}.",
                        f"- Profil : {p.get('mod_profile','isolated')}, chargement attendu vérifié={profile.get('expected_mods_verified','non vérifié')}, {sum(bool(m.get('loaded')) for m in inventory)} modules chargés.",
                        f"- Exclusions : `{json.dumps(r['excluded'],ensure_ascii=False)}`.", '']
            if coverage.get('targeted'):
                details += [f"**Test ciblé : {coverage.get('target_from')}–{coverage.get('target_to')} %.** "
                            f"Section {'atteinte' if coverage.get('target_completed') else 'non terminée'}. "
                            "Le jeu atteint la section avec ses collisions et déclencheurs natifs ; les pourcentages hors section ne sont pas mesurés.", '']
            unfocused=sum(n for reason,n in r['excluded'].items() if 'unfocused' in reason)
            if unfocused:
                details += [f"**Couverture FPS limitée : {unfocused} images hors focus exclues dans ce parcours.** Les passages manquants ne valident pas 60 FPS et peuvent rendre la moyenne globale trompeuse.", '']
            if not r['completed'] and not coverage.get('target_completed'):
                stop=next((e.get('reason') for e in reversed(es) if e.get('event')=='finished'),'Fin normale non enregistrée : fermeture ou crash à examiner.')
                details += ['**Interruption :** '+stop, '']
            for death in r['deaths']:
                details += [f"Mort à {death['percent']:.3f} %, temps écoulé {death['elapsed']:.3f} s.", '']
            for (attempt,percent),s in sorted(r['attempt_bins'].items()):
                all_bins.append(dict(level=level,mode=mode,attempt=attempt,visual_diagnostic=r['visual'],percent=percent,**s))
        details += ['## Tentatives', '', '| Mode | Tentative | Maximum observé (%) | FPS moyens | p99 (ms) | Pire (ms) | Images mesurées |', '|---|---:|---:|---:|---:|---:|---:|']
        for mode,r in pair.items():
            for attempt,s in r['attempts'].items():
                details.append(f"| {mode} | {attempt} | {s['max_percent']:.3f} | {f(s['fps'])} | {f(s['p99_ms'])} | {f(s['worst_ms'])} | {s['samples']} |")
        details += ['', 'Le tableau suivant agrège les observations du même pourcentage ; le CSV sépare chaque tentative. Des nombres de tentatives différents limitent les comparaisons globales.', '']
        comparison_verified = False
        if len(pair)==2:
            keys=('measurement_sha256','game_sha256','level_sha256')
            matched=all(pair['sans']['provenance'].get(k) and pair['sans']['provenance'].get(k)==pair['avec']['provenance'].get(k) for k in keys)
            quality_keys=('low_detail_effective','skip_camera_shake_effective')
            qualities=[next((e for e in pair[m]['events'] if e.get('event')=='test_quality'),{}) for m in ('sans','avec')]
            quality_matched=all(k in qualities[0] and k in qualities[1] and qualities[0][k]==qualities[1][k] for k in quality_keys)
            conditions=('target_from_percent','target_to_percent','visual_diagnostic','mod_profile','mod_profile_sha256','profile_settings_sha256')
            conditions_matched=all(pair['sans']['provenance'].get(k)==pair['avec']['provenance'].get(k) for k in conditions)
            comparison_verified=matched and quality_matched and conditions_matched and all(r['variant_verified'] for r in pair.values())
            details += ['Collecte, exécutable et données du niveau : **'+('identiques' if matched else 'différents ou non vérifiés ; comparaison limitée')+'**.', '']
            details += ['Réglages LDM/secousses : **'+('identiques et vérifiés' if quality_matched else 'non vérifiés ou différents ; comparaison limitée')+'**.', '']
        details += ['## FPS par pourcentage', '', 'Un quantile avec peu d’images est incertain ; la colonne N permet de repérer ces passages. Un tiret signifie non mesuré, jamais zéro lag.', '', '| % | FPS sans | FPS avec | p99 sans (ms) | p99 avec (ms) | Pire sans (ms) | Pire avec (ms) | N sans / avec |', '|---:|---:|---:|---:|---:|---:|---:|---:|']
        for percent in range(100):
            a=pair.get('sans',{}).get('bins',{}).get(percent,{})
            b=pair.get('avec',{}).get('bins',{}).get(percent,{})
            details.append(f"| {percent} | {f(a.get('fps'))} | {f(b.get('fps'))} | {f(a.get('p99_ms'))} | {f(b.get('p99_ms'))} | {f(a.get('worst_ms'))} | {f(b.get('worst_ms'))} | {a.get('samples',0)} / {b.get('samples',0)} |")
        if pair.get('avec',{}).get('bins'):
            details += ['', '## Passages les plus lents avec Chlorium', '',
                        'L’étape CPU indiquée est une observation du profil, pas une preuve isolée de causalité. Les écarts entre images figurent aussi dans le CSV.', '',
                        '| % | FPS moyens | p99 (ms) | Pire image (ms) | N | Étape CPU la plus coûteuse | Moyenne de cette étape (ms) |',
                        '|---:|---:|---:|---:|---:|---|---:|']
            stages=('visibility','move','rotation','transform','area','follow','collisions','particles')
            for percent,s in sorted(pair['avec']['bins'].items(),key=lambda item:item[1]['worst_ms'],reverse=True)[:12]:
                stage=max(stages,key=lambda k:s[k+'_mean_ms'])
                details.append(f"| {percent} | {f(s['fps'])} | {f(s['p99_ms'])} | {f(s['worst_ms'])} | {s['samples']} | {stage} | {f(s[stage+'_mean_ms'])} |")
        details += ['', '## Épisodes très sombres', '',
                    'Analyse de l’image présentée juste avant l’échange des tampons, hors HUD. Les FPS et le libellé de test ne masquent pas la détection.', '',
                    '| Mode | Début (%) | Fin (%) | Durée observée (s) | Images | Vérification native |', '|---|---:|---:|---:|---:|---|']
        native_dark=pair.get('sans',{}).get('dark',[])
        for mode,r in pair.items():
            for episode in r['dark']:
                also_native=comparison_verified and any(episode['start_percent']<=n['end_percent']+.15 and episode['end_percent']>=n['start_percent']-.15 for n in native_dark)
                verdict='Référence native' if mode=='sans' else 'À vérifier : comparaison native non vérifiée' if not comparison_verified else 'Également observé sans, à proximité' if also_native else 'À vérifier : aucune correspondance native capturée'
                details.append(f"| {mode} | {episode['start_percent']:.3f} | {episode['end_percent']:.3f} | {episode['duration_s']:.4f} | {episode['frames']} | {verdict} |")
                all_dark.append(dict(level=level,mode=mode,**episode,native_nearby=also_native))
        details += ['', 'Une durée peut être tronquée par une lacune de prélèvement ; le CSV conserve ce drapeau. Les captures d’apparition sont bornées à 32 par parcours.', '', '## Captures des apparitions sombres', '']
        for mode,r in pair.items():
            for e in r['captures']:
                ppm=Path(str(r['path'])+f".dark-{e['frame']}.ppm")
                if not ppm.exists(): continue
                images=output/'images'; images.mkdir(exist_ok=True)
                target=images/f"{level}-{mode}-{e['frame']}.png"
                try:
                    from PIL import Image
                    with Image.open(ppm) as im: im.save(target)
                    details.append(f"- {mode}, {e['percent']:.3f} %, image {e['frame']} : [capture](images/{target.name}).")
                except ImportError:
                    details.append(f"- {mode}, {e['percent']:.3f} % : fichier brut {ppm.name}.")
        file=output/f'niveau-{level}.md'; text='\n'.join(details)+'\n'; file.write_text(text,encoding='utf-8')
        readable_html(text,file.with_suffix('.html'))
        lines += ['', f"Détail de **{LEVELS[level]}** : [FPS par pourcentage, épisodes sombres et captures](niveau-{level}.md)."]
    lines += ['', '## Régression graphique historique', '',
              'Ce contrôle de l’ancien cache reproduisait une désynchronisation corrigée entre les alpha.2 et alpha.3. Il ne valide pas le paquet des parcours ci-dessus ; leurs empreintes identifient la version réellement testée.', '',
              '| Version | Octet initial | Après recréation | Après rafraîchissement | Attendu | Résultat |', '|---|---:|---:|---:|---:|---|']
    root=Path(__file__).resolve().parent.parent
    for name,title in [('storage-before','Avant correction'),('storage-after','Après correction')]:
        p=root/'audit-artifacts'/name/'auto-88952972-chlorium.jsonl'
        e=next((e for e in events(p) if e.get('event')=='atlas_storage_regression'),None)
        if e:
            lines.append(f"| {title} | {e['initial_gpu_byte']} | {e['remapped_gpu_byte']} | {e['final_gpu_byte']} | 77 | {'Conforme' if e['final_gpu_byte']==77 else 'Données périmées reproduites'} |")
            evidence=output/'preuves'; evidence.mkdir(exist_ok=True); shutil.copyfile(p,evidence/(name+'.jsonl'))
    lines += ['', 'Cette preuve historique ne démontre pas que toutes les coupures signalées avaient cette cause. Le cache d’atlas est absent du paquet normal depuis l’alpha.6.', '',
              '## Fichiers de mesure', '',
              '- [FPS, temps d’image et étapes CPU par pourcentage](fps-par-pourcentage.csv).',
              '- [Épisodes sombres et couverture](episodes-sombres.csv).',
              '- Les rapports bruts conservent les images exclues, les événements de mort, la fin du parcours et les empreintes des paquets.', '',
              'Aucun résultat de cette série ne suffit à garantir 60 FPS stables sur tous les niveaux ni l’absence universelle de bugs.']
    for filename,data in [('fps-par-pourcentage.csv',all_bins),('episodes-sombres.csv',all_dark)]:
        keys=list(dict.fromkeys(k for r in data for k in r))
        with (output/filename).open('w',newline='',encoding='utf-8-sig') as stream:
            if keys:
                w=csv.DictWriter(stream,fieldnames=keys); w.writeheader(); w.writerows(data)
    text='\n'.join(lines)+'\n'
    (output/'DIAGNOSTIC.md').write_text(text,encoding='utf-8')
    readable_html(text,output/'DIAGNOSTIC.html')
    print(f'Diagnostic écrit : {output / "DIAGNOSTIC.md"} ({len(runs)} parcours, {len(all_bins)} passages mesurés)')


if __name__=='__main__':
    source=Path(sys.argv[1]).resolve()
    write_report(source,Path(sys.argv[2]).resolve() if len(sys.argv)>2 else source/'rapport',
                 tuple(Path(p).resolve() for p in sys.argv[3:]))
