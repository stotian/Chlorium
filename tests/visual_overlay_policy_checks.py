"""Reject unknown binaries and external overlays without weakening episodes."""
import copy
import importlib
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
mod=importlib.import_module('compare-visual')
base={'provenance':{'mod_profile':'isolated',
    'measurement_sha256':mod.ISOLATED_NO_DEBUG_PAIR[0],
    'production_sha256':mod.ISOLATED_NO_DEBUG_PAIR[1]},
    'profile':{'name':'isolated'},
    'inventory':[{'id':name,'loaded':True} for name in
        ['geode.loader','geode.node-ids','local.chlorium.audit','local.chlorium']]}
assert mod.overlay_policy(base)=='audited_V8_binary_pair_without_external_mods'
native=copy.deepcopy(base);native['inventory'].pop()
native['provenance']['production_sha256']=None
assert mod.overlay_policy(native)==mod.overlay_policy(base)
mutations=[]
for key,value in [('measurement_sha256','unknown'),('production_sha256','unknown'),('mod_profile','user-mods')]:
    d=copy.deepcopy(base);d['provenance'][key]=value;mutations.append(d)
d=copy.deepcopy(base);d['inventory'].append({'id':'external.debug-overlay','loaded':True});mutations.append(d)
d=copy.deepcopy(base);d['inventory'].pop(2);mutations.append(d)
d=copy.deepcopy(base);d['profile']['name']='user-mods';mutations.append(d)
for d in mutations:
    try:mod.overlay_policy(d)
    except ValueError:pass
    else:raise AssertionError('Unverified overlay protocol accepted')
full=copy.deepcopy(mutations[3]);full['provenance']['debug_overlays_disabled_requested']=True
assert mod.overlay_policy(full)=='debug_disabling_requested'
print('PASS: isolated binary identity, inventory, profile and existing full-mod overlay guards')
