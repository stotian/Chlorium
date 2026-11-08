"""Static this-relative quad candidates in first-base CCSprite-derived GD methods."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
from native_access_inventory import IDENTITIES, below_normal_priority
from sprite_alias_inventory import analyze_method
import capstone
import pefile
from pe_ranges import function_ranges


def selectors(text):
    classes, methods = {}, []
    current = None
    for number,line in enumerate(text.splitlines(),1):
        match = re.match(r'^class ([\w:]+)(?:\s*:\s*([^\{]+))?\s*\{',line)
        if match:
            current=match.group(1)
            classes[current]=(match.group(2) or '').split(',')[0].strip()
            continue
        if current and re.match(r'^\}',line):
            current=None
        if not current or 'static ' in line:
            continue
        match=re.search(r'\bvoid\s+(\w+)\s*\(.*\)\s*=\s*win\s+(0x[0-9a-fA-F]+)',line)
        if match:
            methods.append(dict(class_name=current,method=match.group(1),entry=int(match.group(2),16),line=number))
    def sprite_base(name):
        visited=set()
        while name and name not in visited:
            if name in ('cocos2d::CCSprite','CCSprite'):
                return True
            visited.add(name);name=classes.get(name,'')
        return False
    return [m for m in methods if sprite_base(m['class_name'])]


def components(access):
    begin=int(access['offset'],16);width=1 if access['access']=='address_only' else max(access['width'],1)
    found=set()
    for n in range(4):
        for name,start,size in [('positions',0x1cc+n*24,12),('colors',0x1cc+n*24+12,4),('uv',0x1cc+n*24+16,8)]:
            if begin < start+size and begin+width > start:
                found.add(name)
    return sorted(found)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('bindings',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args();below_normal_priority()
    if args.image.name.lower()!='geometrydash.exe' or args.image.stat().st_size>64*1024*1024 or args.bindings.stat().st_size>16*1024*1024:
        raise ValueError('Bounded GD image and bindings required')
    raw=args.image.read_bytes();binding_bytes=args.bindings.read_bytes();methods=selectors(binding_bytes.decode('utf-8-sig'))
    if len(methods)>20000:
        raise ValueError('Method selection budget')
    pe=pefile.PE(data=raw,fast_load=True)
    try:
        if pe.FILE_HEADER.Machine!=0x8664 or (pe.FILE_HEADER.TimeDateStamp,pe.OPTIONAL_HEADER.SizeOfImage)!=IDENTITIES['geometrydash.exe']:
            raise ValueError('Unreviewed GD image')
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']])
        image=pe.get_memory_mapped_image();ranges=function_ranges(pe)
        boundaries=sorted({m['entry'] for m in methods}|{a for a,_,_ in ranges})
        sections=[(s.VirtualAddress,s.VirtualAddress+s.Misc_VirtualSize) for s in pe.sections if s.Characteristics&0x20000000]
        decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);decoder.detail=True
        results=[];counts=Counter();cache={}
        for method in methods:
            start=method['entry']
            if start not in cache:
                owned=[(a,b) for a,b,owner in ranges if owner==start]
                kind='unwind_owned_ranges'
                if not owned:
                    section_end=next((b for a,b in sections if a<=start<b),0)
                    end=min([start+4096,section_end,len(image)]+[b for b in boundaries if b>start])
                    owned=[(start,end)] if start<end else []
                    kind='bounded_binding_slot_not_unwind_end'
                if not owned:
                    cache[start]=None
                else:
                    instructions=[];complete=True;digest=hashlib.sha256()
                    for begin,end in owned:
                        if not 0<=begin<end<=len(image) or end-begin>1024*1024:
                            raise ValueError('Procedure interval budget')
                        block=list(decoder.disasm(image[begin:end],begin));instructions.extend(block)
                        complete &= sum(i.size for i in block)==end-begin;digest.update(image[begin:end])
                    accesses,escapes,status=analyze_method(instructions,start)
                    for access in accesses:access['components']=components(access)
                    cache[start]=dict(boundary_kind=kind,body_sha256=digest.hexdigest(),
                                      full_ranges_decoded=complete,status=status,accesses=accesses,escapes=escapes)
            result=cache[start];counts['selected']+=1
            if result is None:
                counts['unavailable_entry']+=1;continue
            counts['converged']+=bool(result['status']['converged'])
            counts['with_this_relative_quad_access']+=bool(result['accesses'])
            quad_escapes=[e for e in result['escapes'] if any(0x1cc<=int(v,16)<0x22c for v in e.get('aliases',{}).values())]
            counts['with_quad_pointer_escape']+=bool(quad_escapes)
            if result['accesses'] or quad_escapes or not result['status']['converged']:
                results.append(dict(method,entry=hex(start),analysis=result))
        report=dict(static_analysis_only=True,target_code_executed=False,all_cpu_consumers_proven=False,
                    game_sha256=hashlib.sha256(raw).hexdigest(),bindings_sha256=hashlib.sha256(binding_bytes).hexdigest(),
                    entry_assumption='Selected non-static void methods; first declared base chain reaches CCSprite; RCX is this',
                    limitations=['First-base layout assumption is not an ABI/runtime owner proof',
                                 'Non-void methods, unknown aliases, child pointers, indirect calls and other modules remain open'],
                    counts=dict(counts),results=results)
        with args.output.open('x',encoding='utf-8') as stream:json.dump(report,stream,indent=2)
        print(json.dumps(dict(counts)))
    finally:
        pe.close()


if __name__=='__main__':main()
