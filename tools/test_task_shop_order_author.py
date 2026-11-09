"""店单作者准入只核显式版本、目录配对和原模板；不代替真实受领/交货World门。"""
import copy
import unittest

from tools.content.task_shop_order_links import check_task_shop_orders


class TaskShopOrderAuthorTest(unittest.TestCase):
    """覆盖实际索引路径与缺失/错配源，旧无订单情景不被新增能力默启。"""

    def setUp(self):
        """最小作者夹具只声明目录，不预造店铺、Task或完成事实。"""
        self.template_id = 'author::template.customer'
        self.policy_id = 'author::order.customer'
        self.index = {'task_templates': {self.template_id: {
            'id': self.template_id, 'template': 'procurement'}}}
        self.pack = {'required_capabilities': [
            'task_shop_orders_v1', 'native_dynamic_world', 'multi_actor', 'tasks.procurement'],
            'scenarios': [{'task_shop_order_profile': 'private_shop_orders_v1',
                'enabled': {'task_templates': [self.template_id]},
                'dynamic_world': {'profile': 'local_society_v1', 'private_shops': {
                    'orders': {'policies': {self.policy_id: {
                        'id': self.policy_id, 'task_template_id': self.template_id}}}}}}]}

    def codes(self, pack=None, index=None):
        """诊断保留真实作者路径，测试只比较本域必要错误身份。"""
        return {row[0] for row in check_task_shop_orders(
            self.pack if pack is None else pack, self.index if index is None else index)}

    def test_actual_index_and_enabled_template(self):
        """顶层template索引与情景启用集均真实匹配时可进入后续店铺完整校验。"""
        self.assertEqual(self.codes(), set())

    def test_old_scenario_and_global_capability_do_not_enable_orders(self):
        """安装并集中的店单能力不会给旧情景或空店单目录追加持久段。"""
        self.assertEqual(self.codes({'required_capabilities': self.pack['required_capabilities'],
                                     'scenarios': [{}, {'dynamic_world': {'private_shops': {
                                         'orders': {'policies': {}}}}}]}), set())

    def test_nonempty_orders_require_explicit_profile(self):
        """S5非空订单不能独自启用而省略Task独立段。"""
        del self.pack['scenarios'][0]['task_shop_order_profile']
        self.assertIn('TASK_SHOP_ORDER_PROFILE', self.codes())

    def test_unknown_profile_and_null_are_not_disabled(self):
        """显式但未知/空版本必须报错，不能降级为未启用。"""
        for value in ('private_shop_orders_v2', None, True):
            with self.subTest(profile=value):
                self.pack['scenarios'][0]['task_shop_order_profile'] = value
                self.assertIn('TASK_SHOP_ORDER_PROFILE', self.codes())

    def test_each_required_capability(self):
        """每项必要能力分别缺失均拒，不由模板名称推断能力。"""
        for cap in self.pack['required_capabilities']:
            with self.subTest(capability=cap):
                pack = copy.deepcopy(self.pack)
                pack['required_capabilities'].remove(cap)
                self.assertIn('TASK_SHOP_ORDER_CAPABILITY', self.codes(pack))

    def test_native_and_shop_pair(self):
        """legacy场景、缺铺目录或伪数组均不能激活同候选店单。"""
        for dynamic in (None, {}, {'profile': 'legacy', 'private_shops': {}},
                        {'profile': 'local_society_v1'},
                        {'profile': 'local_society_v1', 'private_shops': []}):
            with self.subTest(dynamic=dynamic):
                pack = copy.deepcopy(self.pack)
                pack['scenarios'][0]['dynamic_world'] = dynamic
                self.assertIn('TASK_SHOP_ORDER_NATIVE_PROFILE', self.codes(pack))

    def test_selected_requires_nonempty_order_map(self):
        """显式店单能力不接受空目录、数组或伪policies载荷。"""
        for policies in ({}, [], None, 'orders'):
            with self.subTest(policies=policies):
                pack = copy.deepcopy(self.pack)
                pack['scenarios'][0]['dynamic_world']['private_shops']['orders']['policies'] = policies
                self.assertIn('TASK_SHOP_ORDER_POLICIES', self.codes(pack))

    def test_missing_disabled_or_nonprocurement_template(self):
        """存在普通Offer或别的情景启用同名template不足以授权本店单。"""
        self.assertIn('TASK_SHOP_ORDER_TEMPLATE', self.codes(index={'offers': self.index['task_templates']}))
        self.pack['scenarios'][0]['enabled']['task_templates'] = []
        self.assertIn('TASK_SHOP_ORDER_TEMPLATE', self.codes())
        self.pack['scenarios'][0]['enabled']['task_templates'] = [self.template_id]
        for kind in ('message', 'escort', None):
            with self.subTest(kind=kind):
                self.index['task_templates'][self.template_id]['template'] = kind
                self.assertIn('TASK_SHOP_ORDER_TEMPLATE', self.codes())

    def test_index_identity_and_diagnostic_path(self):
        """索引键与原ID错配拒绝，错误精确定位到原政策template字段。"""
        self.index['task_templates'][self.template_id]['id'] = 'author::template.other'
        errors = check_task_shop_orders(self.pack, self.index)
        self.assertEqual(errors[0][1], ('scenarios', 0, 'dynamic_world', 'private_shops',
                                      'orders', 'policies', self.policy_id, 'task_template_id'))
        self.assertIn('TASK_SHOP_ORDER_TEMPLATE', self.codes())

    def test_malformed_policy_reference_is_rejected(self):
        """畸形模板引用明确拒绝，不让不可散列JSON导致checker崩溃。"""
        for reference in (None, [], {}, 1, ''):
            with self.subTest(reference=reference):
                row = self.pack['scenarios'][0]['dynamic_world']['private_shops']['orders']['policies'][self.policy_id]
                row['task_template_id'] = reference
                self.assertIn('TASK_SHOP_ORDER_TEMPLATE', self.codes())


if __name__ == '__main__':
    unittest.main()
