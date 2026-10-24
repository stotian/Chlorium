"""Prevent sampled blink fragmentation from becoming a false mod attribution."""
import importlib
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
match=importlib.import_module('compare-visual').match_episodes
native=[dict(start_percent=1,end_percent=2,duration_s=.5)]
candidate=[dict(start_percent=1.1,end_percent=1.2,duration_s=.1),
           dict(start_percent=1.8,end_percent=1.9,duration_s=.1),
           dict(start_percent=3,end_percent=3.1,duration_s=.1)]
results=match(native,candidate)
assert results[0]['sans'] is native[0] and not results[0]['native_reference_reused']
assert results[1]['sans'] is native[0] and results[1]['native_reference_reused']
assert results[2]['sans'] is None
assert abs(results[0]['duration_delta_s']+.4)<1e-12
assert match(native,[dict(start_percent=2.031,end_percent=2.04,duration_s=.01)])[0]['sans'] is None
print('Visual episode checks passed: shared reference, preserved duration, absent reference and tolerance boundary')
