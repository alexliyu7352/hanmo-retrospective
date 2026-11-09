"""静态货物作者/模块身份和三冷尾源门，不替代原生World或GUI验收。"""
import copy
from pathlib import Path
import re
import unittest
from tools.content.capability_contract import load_contract, required_capabilities
from tools.content.validation import SCHEMA, StrictValidator
from tools.content.task_static_asset_links import check_task_static_assets
ROOT=Path(__file__).resolve().parents[1]


class TaskStaticAssetAuthorTest(unittest.TestCase):
    """独立能力不扩旧场景，永远不默默恢复旧货物历史。"""

    def test_contract_and_enum(self):
        """原cap加载必须识别真实enum，不接受const或凭安装并集启动。"""
        contract=load_contract()
        self.assertEqual(sum(row['capability_id']=='task_static_assets_v1' for row in contract['capabilities']),1)
        shape=SCHEMA['$defs']['scenario']['properties']['task_static_asset_profile']
        self.assertFalse(list(StrictValidator(shape).iter_errors('original_task_assets_v1')))
        self.assertTrue(list(StrictValidator(shape).iter_errors('original_task_assets_v2')))
        self.assertNotIn('task_static_assets_v1',required_capabilities({'scenarios':[{}]},contract))
        self.assertIn('task_static_assets_v1',required_capabilities({'scenarios':[{'task_static_asset_profile':'original_task_assets_v1'}]},contract))

    def test_explicit_native_profile(self):
        """只增当前作者实际指定段；缺依赖、错版本或legacy场景明确拒绝。"""
        pack={'required_capabilities':['task_static_assets_v1','native_dynamic_world','multi_actor'],'scenarios':[{'task_static_asset_profile':'original_task_assets_v1','dynamic_world':{'profile':'local_society_v1'}}]}
        self.assertEqual(check_task_static_assets(pack,{}),[])
        self.assertEqual(check_task_static_assets({'required_capabilities':pack['required_capabilities'],'scenarios':[{}]},{}),[])
        bad=copy.deepcopy(pack);bad['required_capabilities'].remove('task_static_assets_v1');self.assertTrue(check_task_static_assets(bad,{}))
        bad=copy.deepcopy(pack);bad['scenarios'][0]['dynamic_world']['profile']='legacy';self.assertTrue(check_task_static_assets(bad,{}))

    def test_three_identity_producers(self):
        """三生产者同一显式位置；旧相邻婚姻和活人接续marker不互换。"""
        for path,prefix in [('native/world/src/resident/kernel/world_codec.cpp','id'),('native/world/src/resident/kernel/native_initial_state.cpp','admitted'),('game/ui/native_resident_backend.gd','gd')]:
            source=(ROOT/path).read_text();positions=[]
            for name in ['family_marriage','task_static_assets','voluntary_succession']:
                pattern=r'expected\["module_versions"\]\.append\(\{"name":\s*"'+name+'"' if prefix=='gd' else re.escape(prefix)+r'\.module_versions\.emplace_back\("'+name+'",'
                matches=list(re.finditer(pattern,source));self.assertEqual(len(matches),1,(path,name));positions.append(matches[0].start())
            self.assertEqual(positions,sorted(positions),path)

    def test_three_cold_tails_and_export(self):
        """写出、标题选段与完整读取选择同一尾段；Common例外只准具名实际payload。"""
        source=(ROOT/'native/world/src/resident/kernel/world_codec.cpp').read_text()
        for token in ['if(catalog.task_static_assets_enabled)domains.push_back({"task_static_assets",1});return domains;', 'domains.push_back({"task_static_assets",1});}', 'r=section("task_static_assets",', 'else if(section.domain=="task_static_assets")']:
            self.assertEqual(source.count(token),1,token)
        self.assertIn('header->kind==task_static_asset_fact_kind&&w.tasks.static_assets->receipts.find(row.first)',source)
        for name in ['family_marriage','task_static_assets']:
            self.assertIn('r=section("'+name+'",',source)
        self.assertLess(source.index('r=section("family_marriage",'),source.index('r=section("task_static_assets",'))
        rules=(ROOT/'native/world/src/resident/domains/tasks/task_rules.cpp').read_text()
        self.assertIn('auto changed=c.apply_resources(recipe);',rules)
        self.assertLess(rules.index('auto changed=c.apply_resources(recipe);'),rules.index('record_task_static_asset_transfer_after_resources'))


if __name__=='__main__':unittest.main()
