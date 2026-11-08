import unittest
from compare_variants import conditions,compare,production_version

class Conditions(unittest.TestCase):
    def base(self):return dict(game_sha256='g',measurement_sha256='m',level_sha256='l',level=129598490,target_fps=120,target_from_percent=74,target_to_percent=78,low_detail_requested=False,disable_shake_requested=True,mod_profile='isolated',mod_profile_sha256='p',profile_settings_sha256='s')
    def test_diagnostic_never_fps(self):
        for name in ('visual_diagnostic','external_sampling_requested','opacity_shadow_diagnostic','native_pass_capture_diagnostic','native_quad_reads_diagnostic','owned_register_oracle_diagnostic'):
            p=self.base();p[name]=True
            with self.subTest(name=name),self.assertRaises(ValueError):conditions(p)
    def test_missing_or_wrong_configuration_rejected(self):
        for key,value in [('target_fps',60),('low_detail_requested',True),('disable_shake_requested',False),('mod_profile','user-mods')]:
            p=self.base();p[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):conditions(p)
        p=self.base();del p['level_sha256']
        with self.assertRaises(ValueError):conditions(p)
    def test_workload_identity_is_part_of_comparison(self):
        base=self.base()
        for key in ('measurement_sha256','level_sha256','profile_settings_sha256'):
            p=base.copy();p[key]+='changed';self.assertNotEqual(conditions(base),conditions(p))
    def test_unbalanced_protocol_not_silently_accepted(self):
        for paths,reference in [([],False),([None]*4,False),([None]*6,True)]:
            with self.assertRaises(ValueError):compare(paths,reference)
    def test_production_version_from_actual_presence_event(self):
        event=dict(event='comparison_variant',production_loaded=True,production_enabled=True,production_version='v0.3.0-alpha.6')
        self.assertEqual(production_version([event]),event['production_version'])
        for rows in ([],[event,event],[dict(event,production_enabled=False)],[dict(event,production_version=None)]):
            with self.assertRaises(ValueError):production_version(rows)

if __name__=='__main__':unittest.main()
