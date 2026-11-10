"""Bounded native end-frame geometry witnesses; no game speed or CPU-reader claims."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'research/R4-72'))
from analyze_trace import HEADER as ROOT_HEADER,STATE,decode as decode_root
HEADER=struct.Struct('<8s8IQIId');SPRITE=struct.Struct('<2Q26IQ16I');META=struct.Struct('<4I2Q');ROW_BYTES=368

def decode_origins(raw,root_raw):
    if len(raw)>16384:raise ValueError('Origin sidecar outside bounded byte budget')
    root=decode_root(root_raw,mixed=True);data=json.loads(raw)
    if data.get('diagnostic_only') is not True or data.get('bounded_first_gap_per_actor_frame') is not True or data.get('phases')!={'0':'other','1':'processFollowActions','2':'processTransformActions'}:raise ValueError('Unknown origin sidecar metadata')
    rows=data.get('witnesses');seen=set();counts=Counter()
    identities={struct.unpack_from('<I',root_raw,at+184)[0] for at in range(ROOT_HEADER.size,len(root_raw),208)}
    if not isinstance(rows,list) or len(rows)>64:raise ValueError('Origin witness count exceeds budget')
    for row in rows:
        if set(row)!={'uid','object_id','frame','root_row_ordinal','phase'} or any(type(v) is not int for v in row.values()):raise ValueError('Unknown origin witness fields')
        key=(row['uid'],row['frame'])
        if row['uid'] not in identities or row['frame'] not in (1,2) or row['phase'] not in (0,1,2) or not 0<=row['object_id']<=0xffffffff or not 0<=row['root_row_ordinal']<=root['rows'] or key in seen:raise ValueError('Foreign or duplicate origin witness')
        seen.add(key);counts[data['phases'][str(row['phase'])]]+=1
    return dict(diagnostic_only=True,bounded_first_gaps=len(rows),phase_counts=dict(counts),complete_writer_closure=False)


def decode(raw,root_raw):
    roots=decode_root(root_raw,mixed=True);root=ROOT_HEADER.unpack_from(root_raw)
    if not HEADER.size<=len(raw)<=HEADER.size+256*ROW_BYTES:raise ValueError('Geometry outside bounded byte budget')
    h=HEADER.unpack_from(raw);magic,version,row_bytes,frames,count,actors,reasons,matched,unsupported,owner,uid,thread,percent=h
    if magic!=b'CHLGEO75' or version!=75 or row_bytes!=ROW_BYTES or frames!=2 or not 0<count<=256 or not 0<actors<=128 or reasons or matched+unsupported>count:raise ValueError('Incomplete geometry header')
    if (owner,uid,thread,percent,actors)!=(root[9],root[7],root[8],root[10],root[5]):raise ValueError('Geometry differs from the exact root capture identity')
    if len(raw)!=HEADER.size+count*ROW_BYTES:raise ValueError('Geometry exact length differs')
    seen=set();identities=set();flags=Counter();matches=0;visible=0;batched=0;dirty=0
    for i in range(count):
        at=HEADER.size+i*ROW_BYTES;state=STATE.unpack_from(raw,at);source=SPRITE.unpack_from(raw,at+48);frame,controls,roles,reserved,atlas,batch=META.unpack_from(raw,at+336)
        if frame not in (1,2) or controls&~65535 or roles&~511 or reserved or state[5]&~255 or any(state[8:]):raise ValueError('Unknown geometry control/state metadata')
        if not source[0] or source[0]&7 or not source[2] or source[28]!=source[0] or source[29]!=source[2] or source[4]&~2047 or source[5]>1:raise ValueError('Unknown geometry receiver/source identity')
        key=(frame,source[0],source[2])
        if key in seen:raise ValueError('Duplicate end-frame sprite witness')
        seen.add(key);identities.add((source[0],source[2]));matches+=source[5];flags[str(source[4])]+=1;visible+=(source[4]&1537)==1536;dirty+=bool(source[4]&2);batched+=bool(atlas and batch)
    if len(identities)!=actors or {k[0] for k in seen}!={1,2} or matches!=matched:raise ValueError('Native geometry coverage/match counters differ')
    return dict(root=roots,records=count,actors=actors,matched=matched,unsupported=unsupported,visible_nonhidden=visible,dirty=dirty,batched=batched,source_flags=dict(flags))


def analyze(geometry,trace,report):
    sys.path.insert(0,str(ROOT/'tools'));from audit_json import read_events
    events=read_events(report);p=json.loads(Path(str(report)+'.provenance.json').read_text(encoding='utf-8-sig'));records=[e for e in events if e.get('event')=='native_root_geometry_r75']
    if not events or events[-1].get('event')!='finished' or events[-1].get('success') is not True or len(records)!=1 or not records[0].get('read') or not records[0].get('complete') or records[0].get('missing') or records[0].get('reasons'):raise ValueError('Independent geometry audit is incomplete')
    if p.get('wide_root_geometry_diagnostic') is not True or p.get('native') is not False or p.get('mod_profile')!='isolated':raise ValueError('Expected isolated active R75 geometry diagnostic')
    raw=geometry.read_bytes();root_raw=trace.read_bytes();summary=decode(raw,root_raw)
    origins=Path(str(report)+'.origins-r75.json')
    if origins.exists():summary['origins']=decode_origins(origins.read_bytes(),root_raw)
    if any(records[0].get(k)!=summary[k] for k in ('records','matched','unsupported')) or records[0].get('frames')!=2:raise ValueError('Geometry differs from independent auditor counters')
    return dict(diagnostic_only=True,game_fps_evidence=False,global_reader_closure=False,geometry_sha256=hashlib.sha256(raw).hexdigest(),root_sha256=hashlib.sha256(root_raw).hexdigest(),summary=summary,provenance=p)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('geometry','trace','report','output'):parser.add_argument(name,type=Path)
    args=parser.parse_args();result=analyze(args.geometry,args.trace,args.report)
    with args.output.open('x',encoding='utf-8') as output:json.dump(result,output,indent=2);output.write('\n')
    print(json.dumps(result['summary']))
