"""显式十六技艺的作者与快照门；复用真实作者目录，拒绝隐式升级旧情景。"""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import unittest
import jsonschema
from tools.content import scenarios
ROOT = Path(__file__).resolve().parents[1]
EXTRA = ('naval', 'espionage', 'fortification', 'agriculture', 'medicine', 'forging')

class ExtendedSkillProfileTest(unittest.TestCase):
    """每个用例修改独立冷输入，成功和拒绝都检查原目录没有被改写。"""
    def setUp(self):
        """取已保存作者原数据及当前真实schema，不生成影子游戏世界。"""
        raw = (ROOT / 'schemas/content.schema.json').read_bytes()
        self.schema = json.loads(raw)
        scenarios.register_schema(raw)
        data = json.loads((ROOT / 'docs/native/shared-world-foundation-20261008/author-qualified/foundation-author-qualified.hmcatalog.json').read_text())['catalog']
        self.catalog = {'ok': True, 'schema_sha256': hashlib.sha256(raw).hexdigest(), 'data': data}
        self.sid = next(iter(data['scenarios']))
        self.scenario = data['scenarios'][self.sid]

    def extended(self):
        """只在派生测试情景显式启用政策并逐人写齐六项经验。"""
        self.scenario['skill_profile'] = 'full_sixteen_v1'
        for row in self.scenario['initial']['actors']:
            row['skills'].update({key: number * 17 for number, key in enumerate(EXTRA, 1)})

    def resolve(self):
        """观察真实投影及来源不变，未调用任何命令或随机接口。"""
        before = deepcopy(self.catalog)
        result = scenarios.resolve(self.catalog, self.sid)
        self.assertEqual(self.catalog, before)
        return result

    def test_legacy_projection_keeps_ten(self):
        """旧情景成功后仍只有原十项，不能自动添六个零。"""
        result = self.resolve()
        self.assertTrue(result['ok'], result)
        self.assertTrue(all(len(row['skills']) == 10 for row in result['initial']['actors']))

    def test_schema_accepts_exact_sixteen(self):
        """真实schema接受完整十六项并拒绝部分或未知技艺。"""
        self.extended()
        row = self.scenario['initial']['actors'][0]
        rule = self.schema['$defs']['scenario_initial_actor']['properties']['skills']
        jsonschema.validate(row['skills'], rule)
        bad = deepcopy(row['skills']); bad.pop('naval')
        with self.assertRaises(jsonschema.ValidationError): jsonschema.validate(bad, rule)
        bad = deepcopy(row['skills']); bad['naval'] = True
        with self.assertRaises(jsonschema.ValidationError): jsonschema.validate(bad, rule)

    def test_explicit_profile_preserves_values(self):
        """各经验原值完整投影，不能只登记模块却遗漏人物数据。"""
        self.extended(); result = self.resolve()
        self.assertTrue(result['ok'], result)
        for row in result['initial']['actors']:
            self.assertEqual({key: row['skills'][key] for key in EXTRA}, {key: i*17 for i, key in enumerate(EXTRA, 1)})

    def test_missing_extended_field_rejected(self):
        """新政策任一缺字段都失败，不补缺省经验。"""
        self.extended(); del self.scenario['initial']['actors'][0]['skills']['naval']
        result = self.resolve(); self.assertFalse(result['ok']); self.assertEqual(result['code'], 'InvalidScenarioSkillProfile')

    def test_old_profile_extra_fields_rejected(self):
        """旧政策带新字段拒绝，不能用字段存在推断存档迁移。"""
        self.extended(); del self.scenario['skill_profile']
        result = self.resolve(); self.assertFalse(result['ok']); self.assertEqual(result['code'], 'InvalidScenarioSkillProfile')

    def test_unknown_profile_rejected(self):
        """未知版本拒绝而不是按十项或十六项猜测。"""
        self.scenario['skill_profile'] = 'future'
        result = self.resolve(); self.assertFalse(result['ok']); self.assertEqual(result['code'], 'UnsupportedSkillProfile')

    def test_extended_profile_requires_native(self):
        """未声明原生动态世界的情景不能启用新增格式。"""
        self.extended(); del self.scenario['dynamic_world']
        result = self.resolve(); self.assertFalse(result['ok']); self.assertEqual(result['code'], 'ExtendedSkillsNativeProfileRequired')

if __name__ == '__main__': unittest.main()
