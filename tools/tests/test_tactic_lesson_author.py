"""军阵教学作者必要边界；这不是自然课程/军事习得运行证明。"""
import copy
import unittest
from content.tactic_lessons import check_tactic_lessons


class TacticLessonAuthorTest(unittest.TestCase):
    """当前原教师say、私学费与完整原掌握定义分别核验。"""
    def setUp(self):
        """最小作者输入仅原人物和条款，不写运行事实或技能结果。"""
        self.data = {'required_capabilities': ['growth_tactic_lessons_v1', 'military_tactic_learning_v1', 'family_life_v1', 'family', 'native_dynamic_world', 'cultivation', 'dialogue', 'multi_actor'], 'actors': [{'id': 'teacher'}], 'roles': [{'id': 'role', 'initial_actor': 'teacher', 'organization_id': None}], 'wallets': [{'id': 'wallet', 'owner': {'kind': 'actor', 'id': 'teacher'}}], 'dialogues': [{'id': 'dialogue', 'context_role': 'role', 'nodes': [{'id': 'offer', 'kind': 'say', 'speaker': '$provider'}]}], 'scenarios': [{'enabled': {'actors': ['teacher'], 'roles': ['role']}, 'family_life': {'profile': 'verified_military_deaths_v1', 'player_death': 'await_explicit_continuation'}, 'military_tactic_learning': {'version': 1, 'definitions': [{'id': 'rally', 'teacher_actor_ids': ['teacher']}]}, 'dynamic_world': {'profile': 'local_society_v1', 'growth_tactic_lessons': {'version': 1, 'definitions': {'lesson': {'id': 'lesson', 'tactic_id': 'rally', 'teacher_actor_id': 'teacher', 'dialogue_id': 'dialogue', 'offer_node_id': 'offer', 'recipient_wallet_id': 'wallet', 'duration': 4, 'total_fee': 12}}}}}]}

    def test_explicit_real_offer(self):
        """条款没有预学状态，合法目录形状可以通过。"""
        self.assertEqual(check_tactic_lessons(self.data, {}), [])

    def test_no_choice_or_player_signature(self):
        """后续action、choice、本人听者都不能借旧say表达教师愿意。"""
        for key, value in [('kind', 'choice'), ('kind', 'action'), ('speaker', '$player')]:
            altered = copy.deepcopy(self.data); altered['dialogues'][0]['nodes'][0][key] = value
            self.assertTrue(check_tactic_lessons(altered, {}))

    def test_pair_and_no_seeded_learning(self):
        """新模块不能单独启用，更不能作者预写完成来源。"""
        for field in ('initial_learned', 'completed', 'credits'):
            altered = copy.deepcopy(self.data); altered['scenarios'][0]['dynamic_world']['growth_tactic_lessons'][field] = []
            self.assertTrue(check_tactic_lessons(altered, {}))
        self.data['scenarios'][0].pop('military_tactic_learning')
        self.assertTrue(check_tactic_lessons(self.data, {}))

    def test_exact_private_fee_and_finite_periods(self):
        """公库、布尔伪整数或无限课时都不能作为私人承诺。"""
        row = self.data['scenarios'][0]['dynamic_world']['growth_tactic_lessons']['definitions']['lesson']
        for field, value in [('duration', True), ('duration', 0), ('duration', 721), ('total_fee', -1), ('total_fee', 2147483648)]:
            old = row[field]; row[field] = value; self.assertTrue(check_tactic_lessons(self.data, {})); row[field] = old
        self.data['wallets'][0]['owner']['kind'] = 'organization'
        self.assertTrue(check_tactic_lessons(self.data, {}))

    def test_same_node_ambiguous(self):
        """同一真实意愿不能在作者层重挂不同收费版本。"""
        rows = self.data['scenarios'][0]['dynamic_world']['growth_tactic_lessons']['definitions']
        rows['other'] = dict(rows['lesson'], id='other', total_fee=30)
        self.assertTrue(check_tactic_lessons(self.data, {}))


if __name__ == '__main__':
    unittest.main()
