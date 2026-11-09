"""新主体/欠款作者合同保持旧分支原貌，并拒绝混合版本和越界输入。"""
import copy
import hashlib
import json
from pathlib import Path
import unittest
from tools.content.validation import SCHEMA, StrictValidator, _references
from tools.content.pack_compose import _annotated_schema

ROOT = Path(__file__).resolve().parents[2]
OLD = 'c972d534f586ff1f6983cc1aa861c18ad8fefa43dce99aaf54be621c5d5d95ef'


def candidate():
    """使用最小真实观察政策，准入人物与政策ID分离。"""
    return {'registry_version': 1, 'review_interval_ticks': 2, 'retry_delay_ticks': 1,
            'decision_budget': 4, 'candidates_per_subject': 4,
            'policies': {'observe': {'templates': ['maintain_commitment'],
                'request_arrear_after_ticks': 1, 'decline_work_after_ticks': 2, 'leave_after_ticks': 4}},
            'admissions': {'adults': {'kind': 'all_actors', 'policy_id': 'observe',
                'minimum_age_years': 18, 'excluded_actor_ids': ['actor.child']}}}


class FoundationSchemaTest(unittest.TestCase):
    """作者结构及同源引用遍历，不代替原生玩法或完整作者包运行。"""
    def valid(self, key, value):
        """使用生产严格整数校验器。"""
        return StrictValidator({'$defs': SCHEMA['$defs'], '$ref': '#/$defs/' + key}).is_valid(value)

    def test_legacy_preserved_exactly(self):
        """历史原字节及两个旧定义均保持，原包不获得新语义。"""
        raw = (ROOT / 'schemas/content-history' / (OLD + '.json')).read_bytes()
        self.assertEqual(hashlib.sha256(raw).hexdigest(), OLD)
        old = json.loads(raw)
        for key in ('dynamic_director', 'dynamic_affairs'):
            self.assertEqual(old['$defs'][key], SCHEMA['$defs'][key + '_legacy'])
        data = candidate(); del data['registry_version']; data['subjects'] = data.pop('policies'); del data['admissions']
        self.assertTrue(self.valid('dynamic_director', data))

    def test_explicit_new_and_mixed_versions(self):
        """新版本合法；删版本、混旧subjects及浮点/布尔标识拒绝。"""
        data = candidate(); self.assertTrue(self.valid('dynamic_director', data))
        bad = copy.deepcopy(data); bad['subjects'] = {}; self.assertFalse(self.valid('dynamic_director', bad))
        for version in (None, 0, 2, True, 1.0):
            bad = copy.deepcopy(data)
            if version is None: del bad['registry_version']
            else: bad['registry_version'] = version
            self.assertFalse(self.valid('dynamic_director', bad))

    def test_admission_scope_and_bounds(self):
        """组织型必须有真实typed组织引用，全员型不可混入组织范围。"""
        data = candidate()
        for patch in ({'kind': 'unknown'}, {'organization_id': 'org.city'}, {'minimum_age_years': -1},
                      {'minimum_age_years': 121}, {'minimum_age_years': True}, {'minimum_age_years': 18.0},
                      {'excluded_actor_ids': ['actor.child', 'actor.child']}):
            bad = copy.deepcopy(data); bad['admissions']['adults'].update(patch)
            self.assertFalse(self.valid('dynamic_director', bad))
        data['admissions']['adults'].update(kind='organization_members', organization_id='org.city')
        self.assertTrue(self.valid('dynamic_director', data))
        del data['admissions']['adults']['organization_id']; self.assertFalse(self.valid('dynamic_director', data))
        for key in ('policies', 'admissions'):
            bad = candidate(); row = next(iter(bad[key].values())); bad[key] = {f'entry.{i}': row for i in range(65)}
            self.assertFalse(self.valid('dynamic_director', bad))

    def test_pair_obligations_flag(self):
        """欠款只扩声明来源，不向旧need_policies悄悄注入新版。"""
        data = {'need_policies': {}, 'obligation_needs_version': 1, 'obligation_policies': {
            'salary': {'id': 'salary', 'obligation_id': 'wage.city', 'organization_id': 'org.city', 'semantic_key': 'salary.city'}}}
        self.assertTrue(self.valid('dynamic_affairs', data))
        for key in ('obligation_needs_version', 'obligation_policies'):
            bad = copy.deepcopy(data); del bad[key]; self.assertFalse(self.valid('dynamic_affairs', bad))
        self.assertTrue(self.valid('dynamic_affairs', {'need_policies': {}}))

    def test_reference_consumers_keep_new_actor_and_organization(self):
        """正式验证和组合器同样看到新分支typed引用，政策键保持本地ID。"""
        data = candidate(); data['admissions']['adults'].update(kind='organization_members', organization_id='org.city')
        rule = {'$ref': '#/$defs/dynamic_director'}
        expected = {('actors', 'actor.child'), ('organizations', 'org.city')}
        self.assertEqual({(table, value) for _, table, value in _references(data, rule)}, expected)
        self.assertEqual({(table, value) for _, kind, table, value in _annotated_schema(data, rule, SCHEMA) if kind == 'x-hanmo-ref'}, expected)


if __name__ == '__main__':
    unittest.main()
