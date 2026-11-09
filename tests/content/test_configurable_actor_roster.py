"""现存成人名单作者语义：版本、完整候选覆盖与身份非变，不代替完整目录验收。"""
import copy
import unittest

from tools.content.appearance_choices import check_appearance_choices


def fixture(count=3):
    """提供具有独立初态和固定外貌的原人物；不生成资源图或修改正式包。"""
    ids = [f'actor.person{number}' for number in range(count)]
    scenario = {
        'player_actor': ids[0] if ids else 'actor.person0',
        'playable_actor_ids': ids,
        'dynamic_world': {'founder_policy': 'selected_existing_actor_roster_v1'},
        'enabled': {'actors': ids.copy()},
        'initial': {'actors': [
            {'actor_id': actor, 'alive': True, 'available': True, 'age_years': 18 + number,
             'appearance_definition': {'kind': 'fixed', 'generator_version': 'appearance-v1'}}
            for number, actor in enumerate(ids)]},
        'playable_actor_appearance_policies': [
            {'actor_id': actor, 'policy': 'preserve_existing_v1'} for actor in ids],
    }
    return {'scenarios': [scenario]}, {'actors': {actor: {'origin': 'original'} for actor in ids}}


class ConfigurableActorRosterTests(unittest.TestCase):
    """同一输入的旧双人政策继续拒绝扩表，新政策逐人校验且不隐式修补。"""

    def inspect(self, pack, index):
        """每次生产作者检查都验证整个输入非变，避免成功或失败重写真实人物。"""
        before = copy.deepcopy((pack, index))
        result = check_appearance_choices(pack, index)
        self.assertEqual((pack, index), before)
        return result

    def test_one_or_many_existing_adults(self):
        """一人、双人和多人名单均可显式配置，保留者允许已核老年年龄。"""
        for count in (1, 2, 3, 27):
            with self.subTest(count=count):
                pack, index = fixture(count)
                pack['scenarios'][0]['initial']['actors'][-1]['age_years'] = 120
                self.assertEqual(self.inspect(pack, index), [])

    def test_legacy_policy_keeps_exact_two(self):
        """新功能不会扩大历史两人政策的作者域。"""
        for count in (1, 3):
            pack, index = fixture(count)
            pack['scenarios'][0]['dynamic_world']['founder_policy'] = 'selected_existing_actor_v1'
            self.assertTrue(self.inspect(pack, index))

    def test_bad_unselected_candidates_are_rejected(self):
        """未选末项的年龄、生命、名单覆盖和初态歧义仍阻止整个坏名单。"""
        for corruption in ('child', 'dead', 'unavailable', 'missing_policy', 'duplicate_policy',
                           'disabled', 'missing_initial', 'duplicate_initial', 'empty', 'foreign_default'):
            with self.subTest(corruption=corruption):
                pack, index = fixture()
                scenario = pack['scenarios'][0]
                if corruption == 'child': scenario['initial']['actors'][-1]['age_years'] = 17
                elif corruption == 'dead': scenario['initial']['actors'][-1]['alive'] = False
                elif corruption == 'unavailable': scenario['initial']['actors'][-1]['available'] = False
                elif corruption == 'missing_policy': scenario['playable_actor_appearance_policies'].pop()
                elif corruption == 'duplicate_policy': scenario['playable_actor_appearance_policies'][-1] = copy.deepcopy(scenario['playable_actor_appearance_policies'][0])
                elif corruption == 'disabled': scenario['enabled']['actors'].pop()
                elif corruption == 'missing_initial': scenario['initial']['actors'].pop()
                elif corruption == 'duplicate_initial': scenario['initial']['actors'].append(copy.deepcopy(scenario['initial']['actors'][-1]))
                elif corruption == 'empty': scenario['playable_actor_ids'] = []
                else: scenario['player_actor'] = 'actor.outside'
                self.assertTrue(self.inspect(pack, index), corruption)


if __name__ == '__main__':
    unittest.main()
