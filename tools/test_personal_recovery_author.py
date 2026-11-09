"""个人普通住宿/医馆作者窄门，不创建人物伤势或运行世界。"""
import copy
import unittest
from content.personal_recovery import check_personal_recovery


class PersonalRecoveryAuthorTest(unittest.TestCase):
    """同一原岗位、版本和明确设施资格必须闭合。"""
    def fixture(self):
        """只提供作者条款，运行时证据不得来自本夹具。"""
        role = {'id': 'role.clinic', 'services': ['rest'], 'activities': {'rest': {'duration': 2}}}
        scenario = {'family_life': {}, 'enabled': {'roles': ['role.clinic']}, 'dynamic_world': {'personal_injury_recovery': {'version': 1, 'services': [{'role_id': 'role.clinic', 'method': 'qualified_clinic'}]}}}
        return {'roles': [role], 'scenarios': [scenario], 'required_capabilities': ['family', 'family_life_v1', 'regional_life_v1', 'native_dynamic_world', 'personal_injury_recovery_v1']}

    def test_real_facility(self):
        """明确设施映射可表达普通服务，不代表真实履约已运行。"""
        self.assertEqual(check_personal_recovery(self.fixture(), {}), [])

    def test_reject_unproven_or_extra_fields(self):
        """伤势、免费资助、重复岗位和未知资质不能藏入普通策略。"""
        for mutation in ('initial_injury', 'duplicate', 'unknown_method', 'missing_rest', 'missing_cap', 'future_version'):
            pack = copy.deepcopy(self.fixture())
            config = pack['scenarios'][0]['dynamic_world']['personal_injury_recovery']
            if mutation == 'initial_injury': config['initial_injury'] = 1
            if mutation == 'duplicate': config['services'] *= 2
            if mutation == 'unknown_method': config['services'][0]['method'] = 'poverty_aid'
            if mutation == 'missing_rest': pack['roles'][0]['services'] = []
            if mutation == 'missing_cap': pack['required_capabilities'].remove('family_life_v1')
            if mutation == 'future_version': config['version'] = 2
            self.assertTrue(check_personal_recovery(pack, {}), mutation)


if __name__ == '__main__':
    unittest.main()
