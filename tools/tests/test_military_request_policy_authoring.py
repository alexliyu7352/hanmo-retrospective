"""请求政策的有限作者语义门；读取真实函数，不冒充全包compose或Native准入。"""
import ast
import copy
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'tools/content/military_links.py'
module = ast.parse(SOURCE.read_text())
function = next(node for node in module.body if isinstance(node, ast.FunctionDef) and node.name == '_request_policies')
namespace = {}
exec(compile(ast.Module(body=[function], type_ignores=[]), str(SOURCE), 'exec'), namespace)
check_policy = namespace['_request_policies']


class MilitaryRequestPolicyAuthoring(unittest.TestCase):
    """边界用例只给政策必需字段，不创建伪军团或世界。"""

    def setUp(self):
        """每例独立输入；合法policy不写回缺省情景。"""
        self.pack = {'required_capabilities': ['military'], 'scenarios': [{
            'id': 'request-fixture', 'dynamic_world': {'profile': 'local_society_v1'},
            'military_request_policy': {'profile': 'request_policy/1', 'per_actor_round_limit': 1}}]}

    def test_valid_policy_is_read_only(self):
        """正整数政策可用且原作者字节语义不变。"""
        before = copy.deepcopy(self.pack)
        self.assertEqual(check_policy(self.pack), [])
        self.assertEqual(self.pack, before)

    def test_legacy_absence_is_not_auto_enabled(self):
        """旧情景不自动扩展新格式。"""
        self.pack['scenarios'][0].pop('military_request_policy')
        self.assertEqual(check_policy(self.pack), [])
        self.assertNotIn('military_request_policy', self.pack['scenarios'][0])

    def test_static_profile_rejected(self):
        """静态军事3不能当动态军事5使用。"""
        self.pack['scenarios'][0].pop('dynamic_world')
        self.assertEqual(check_policy(self.pack)[0][0], 'MILITARY_REQUEST_PROFILE')

    def test_missing_military_capability_rejected(self):
        """无军事能力不能只加政策绕过。"""
        self.pack['required_capabilities'] = []
        self.assertEqual(check_policy(self.pack)[0][0], 'MILITARY_REQUEST_PROFILE')

    def test_unknown_policy_rejected(self):
        """版本未知不回退到默认1。"""
        self.pack['scenarios'][0]['military_request_policy']['profile'] = 'request_policy/2'
        self.assertEqual(check_policy(self.pack)[0][0], 'MILITARY_REQUEST_POLICY')

    def test_boolean_limit_rejected(self):
        """Python bool不允许混作请求次数。"""
        self.pack['scenarios'][0]['military_request_policy']['per_actor_round_limit'] = True
        self.assertEqual(check_policy(self.pack)[0][0], 'MILITARY_REQUEST_POLICY')

    def test_zero_limit_rejected(self):
        """无额度不是此显式启用政策。"""
        self.pack['scenarios'][0]['military_request_policy']['per_actor_round_limit'] = 0
        self.assertEqual(check_policy(self.pack)[0][0], 'MILITARY_REQUEST_POLICY')

    def test_unknown_fields_rejected(self):
        """作者不能藏一份由自己选的统帅或AI单位。"""
        self.pack['scenarios'][0]['military_request_policy']['commander_actor_id'] = 'invented'
        self.assertEqual(check_policy(self.pack)[0][0], 'MILITARY_REQUEST_POLICY')


if __name__ == '__main__':
    unittest.main()
