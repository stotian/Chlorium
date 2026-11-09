"""Validate the R72 selected-actor native rotation witnesses, without claiming FPS or reader closure."""
from collections import Counter
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[2]
HEADER=struct.Struct('<8s8IQdQ');STATE=struct.Struct('<2d4I2f2I');COMMAND=struct.Struct('<2d12f4I');TAIL=struct.Struct('<Q3If2I');ROW_BYTES=208


def decode(raw, mixed=False):
    if not HEADER.size<=len(raw)<=HEADER.size+65536*ROW_BYTES:raise ValueError('Rotation trace outside bounded budget')
    magic,version,row_bytes,frames,count,actors,reasons,uid,thread,owner,percent,calls=HEADER.unpack_from(raw)
    if (magic!=(b'CHLMIX74' if mixed else b'CHLROT72') or version!=(74 if mixed else 72) or row_bytes!=ROW_BYTES or frames!=2 or not 0<count<=65536 or
        not 0<actors<=128 or reasons or not uid or not thread or not owner or not math.isfinite(percent) or not 0<=percent<=100 or calls<count):raise ValueError('Incomplete native rotation header')
    if len(raw)!=HEADER.size+count*ROW_BYTES:raise ValueError('Rotation trace length mismatch')
    tags=set();identities=set();commands=Counter();controls_seen=Counter();kinds=Counter();sources=Counter();chains={};steps=Counter();position_breaks=0;states_changed=0;local_rotation=0;last_frame=0
    for i in range(count):
        at=HEADER.size+i*ROW_BYTES;before=STATE.unpack_from(raw,at);command=COMMAND.unpack_from(raw,at+48);after=STATE.unpack_from(raw,at+128)
        address,actor_uid,frame,closed,angle,controls,reserved=TAIL.unpack_from(raw,at+176)
        if not address or address&7 or not actor_uid or frame not in (1,2) or frame<last_frame or closed!=1:raise ValueError('Invalid native rotation row identity/window')
        if any(before[8:]) or any(after[8:]) or before[5]&~255 or after[5]&~255:raise ValueError('Unknown native state fields')
        if reserved&~(0x1ff03 if mixed else 0) or controls&~0xffff:raise ValueError('Unknown native FP capture metadata')
        kind=command[15]
        if kind>1 or (not mixed and kind!=1) or command[16]>1 or command[17] or (kind==1 and any(command[:2])) or command[12] or command[13]:raise ValueError('Invalid captured root-rotation command')
        if mixed and ((kind==0 and (any(command[2:14]) or command[16] or angle or not reserved&1)) or (kind==1 and reserved&3) or (reserved&2 and not reserved&1)):raise ValueError('Invalid joint input-source metadata')
        key=(address,actor_uid)
        if key in chains:position_breaks+=chains[key][:16]!=raw[at:at+16]
        chains[key]=raw[at+128:at+176];steps[(frame,key)]+=1;kinds[kind]+=1;sources[reserved&3]+=1
        tags.add(frame);identities.add((address,actor_uid));last_frame=frame
        commands[bytes(raw[at+48:at+128])]+=1;controls_seen[hex(controls&0xffc0)]+=1;states_changed+=before[:2]!=after[:2];local_rotation+=angle!=0
    if tags!={1,2} or len(identities)!=actors:raise ValueError('Incomplete observed frames/actors')
    return dict(frames=frames,rows=count,actors=actors,native_calls=calls,percent=percent,unique_commands=len(commands),
                position_changes=states_changed,nonzero_local_rotations=local_rotation,movement_rows=kinds[0],rotation_rows=kinds[1],input_source_counts=dict(sources),position_continuity_breaks=position_breaks,max_actor_steps_per_frame=max(steps.values()),
                shared_command_bytes=len(commands)*80,per_invocation_command_bytes=count*80,controls=dict(controls_seen))


def analyze(trace,report,mixed=False):
    sys.path.insert(0,str(ROOT/'tools'));from audit_json import read_events
    events=read_events(report);provenance=json.loads(Path(str(report)+'.provenance.json').read_text(encoding='utf-8-sig'))
    records=[e for e in events if e.get('event')=='native_rotation_trace_r72']
    if (not events or events[-1].get('event')!='finished' or events[-1].get('success') is not True or len(records)!=1 or
        not records[0].get('read') or not records[0].get('complete') or records[0].get('reasons')):raise ValueError('Independent rotation auditor did not certify completion')
    if provenance.get('native_rotation_trace_diagnostic') is not True or provenance.get('native') is not False or provenance.get('mod_profile')!='isolated':raise ValueError('Expected isolated Chlorium-active rotation diagnostic')
    if provenance.get('game_sha256','').lower()!='fc5a16c292278bc2e8e078fb1d5023c2bd658322dd72712767ea70c2dd9ec6d0':raise ValueError('Native game image differs from arithmetic oracle')
    if bool(provenance.get('mixed_native_trace_diagnostic'))!=mixed or records[0].get('version',72)!=(74 if mixed else 72):raise ValueError('Wrong native trace format/auditor family')
    raw=trace.read_bytes();summary=decode(raw,mixed=mixed)
    if any(records[0].get(k)!=summary[k] for k in ('frames','rows','actors')) or records[0].get('closed')!=summary['rows']:raise ValueError('Independent auditor counters differ from trace')
    return dict(diagnostic_only=True,performance_evidence=False,global_reader_closure=False,trace_sha256=hashlib.sha256(raw).hexdigest(),
                summary=summary,scope=('Joint selected-actor movement and rotation; movement inputs are call-entry snapshots and explicitly marked duplicate continuation. No callback or reader closure.' if mixed else 'First 128 observed decoration actors, two frames. Seeds are captured after native stamp/dirty updates and before native position arithmetic; orientation/callback closure not claimed.'),provenance=provenance)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('trace',type=Path);parser.add_argument('report',type=Path);parser.add_argument('output',type=Path);parser.add_argument('--mixed',action='store_true');args=parser.parse_args()
    result=analyze(args.trace,args.report,mixed=args.mixed)
    with args.output.open('x',encoding='utf-8') as output:json.dump(result,output,indent=2);output.write('\n')
    print(json.dumps(result['summary']))
