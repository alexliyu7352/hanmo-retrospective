"""双倒地目标作者有限反例，不把纯目录通过称为实际战果。"""
import copy
import unittest
from tools.content.personal_simultaneous_outcomes import check_personal_simultaneous_outcomes


class PersonalSimultaneousOutcomeAuthorTest(unittest.TestCase):
    """只选择原目标与原状态，不生成胜负或新军战人物。"""
    def setUp(self):
        """两种允许目标共用正常声明，护送另保关键失败。"""
        self.index = {'personal_objectives': {'spar': {'kind': 'sparring'}, 'road': {'kind': 'skirmish'}, 'escort': {'kind': 'escort'}}}
        self.scene = {'dynamic_world': {'profile': 'local_society_v1'}, 'enabled': {'personal_objectives': ['spar', 'road', 'escort']},
                      'personal_simultaneous_outcomes': {'version': 1, 'objective_ids': ['spar', 'road']}}
        self.pack = {'required_capabilities': ['personal_combat', 'native_dynamic_world', 'personal_simultaneous_outcomes_v1'], 'scenarios': [self.scene]}

    def test_valid_and_old_empty(self):
        """旧无策略与新显式选择都不写入作者对象。"""
        before = copy.deepcopy(self.pack)
        self.assertEqual([], check_personal_simultaneous_outcomes(self.pack, self.index))
        self.assertEqual(before, self.pack)
        self.assertEqual([], check_personal_simultaneous_outcomes({'scenarios': [{}]}, {}))

    def test_exact_version_and_fields(self):
        """未知/布尔版本与额外字段不能降级启用。"""
        for version in [True, 0, 2, '1']:
            bad = copy.deepcopy(self.pack); bad['scenarios'][0]['personal_simultaneous_outcomes']['version'] = version
            self.assertTrue(check_personal_simultaneous_outcomes(bad, self.index))
        self.scene['personal_simultaneous_outcomes']['extra'] = True
        self.assertTrue(check_personal_simultaneous_outcomes(self.pack, self.index))

    def test_bounded_unique_objectives(self):
        """空、重复、非身份与超上限集合必须拒绝。"""
        for values in [[], ['spar', 'spar'], [True], [''] , [str(i) for i in range(33)]]:
            bad = copy.deepcopy(self.pack); bad['scenarios'][0]['personal_simultaneous_outcomes']['objective_ids'] = values
            self.assertTrue(check_personal_simultaneous_outcomes(bad, self.index))

    def test_real_selected_kind(self):
        """不能选择未启用目标、未知目标或护送关键目标。"""
        for values in [['escort'], ['missing']]:
            bad = copy.deepcopy(self.pack); bad['scenarios'][0]['personal_simultaneous_outcomes']['objective_ids'] = values
            self.assertTrue(check_personal_simultaneous_outcomes(bad, self.index))
        self.scene['enabled']['personal_objectives'].remove('spar')
        self.assertTrue(check_personal_simultaneous_outcomes(self.pack, self.index))

    def test_capabilities(self):
        """通用个人战和新能力均为真实独立前置。"""
        for capability in self.pack['required_capabilities']:
            bad = copy.deepcopy(self.pack); bad['required_capabilities'].remove(capability)
            self.assertTrue(check_personal_simultaneous_outcomes(bad, self.index))


if __name__ == '__main__':
    unittest.main()
