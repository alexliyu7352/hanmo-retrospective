"""本人委托响应作者叶窄门；全局schema和版本能力digest仍由中央单独登记。"""
import copy
import unittest

from content.task_offer_response_links import check_task_offer_responses, check_task_offer_response_command


class TaskOfferResponseAuthorTests(unittest.TestCase):
    """核显式profile、旧情景兼容与纯读语义，不重做任务或资产算法。"""

    def test_old_scenario_stays_unchanged(self):
        """旧作者包没有响应能力时既不报错，也不补历史拒绝。"""
        pack = {'scenarios': [{'id': 'old'}], 'required_capabilities': []}
        before = copy.deepcopy(pack)
        self.assertEqual(check_task_offer_responses(pack, {}), [])
        self.assertEqual(pack, before)

    def test_explicit_supported_profile(self):
        """当前版本同时声明能力与选择profile才通过。"""
        pack = {'scenarios': [{'task_offer_response_profile': 'known_offers_v1'}],
                'required_capabilities': ['task_offer_responses_v1']}
        self.assertEqual(check_task_offer_responses(pack, {}), [])

    def test_unknown_profile_is_not_silently_accepted(self):
        """名字相近也不能把永久拒绝或其他未知语义偷偷启用。"""
        pack = {'scenarios': [{'task_offer_response_profile': 'permanent_refusal_v1'}],
                'required_capabilities': ['task_offer_responses_v1']}
        self.assertEqual(check_task_offer_responses(pack, {})[0][0], 'TASK_OFFER_RESPONSE_PROFILE')

    def test_capability_is_required(self):
        """没有原生能力登记的目录不能只靠作者字段开启按钮。"""
        pack = {'scenarios': [{'task_offer_response_profile': 'known_offers_v1'}]}
        self.assertEqual(check_task_offer_responses(pack, {})[0][0], 'TASK_OFFER_RESPONSE_CAPABILITY')

    def test_command_uses_original_receiving_authority(self):
        """原受理岗位可接洽婉拒，另一岗位或未知委托不能因按钮名字而获权。"""
        command = {'kind': 'decline_offer', 'offer_id': 'grain'}
        index = {'offers': {'grain': {'accept_role': 'clerk'}}}
        self.assertEqual(check_task_offer_response_command(command, {'id': 'clerk', 'services': ['accept_offer']}, index), '')
        self.assertTrue(check_task_offer_response_command(command, {'id': 'other', 'services': ['accept_offer']}, index))
        self.assertTrue(check_task_offer_response_command(command, {'id': 'clerk', 'services': []}, index))
        self.assertTrue(check_task_offer_response_command(command, {'id': 'clerk', 'services': ['accept_offer']}, {}))


if __name__ == '__main__':
    unittest.main()
