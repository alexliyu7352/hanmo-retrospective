"""续行作者只增加受信可选策略；严格能力/引用/三目录顺序门不替代完整World运行。"""
import copy
import json
from pathlib import Path
import re
import unittest
from tools.content.capability_contract import load_contract
from tools.content.references import iter_references
from tools.content.validation import SCHEMA, StrictValidator
from tools.content.task_authored_continuation_links import check_task_authored_continuation
ROOT=Path(__file__).resolve().parents[1]


class AuthoredTaskContinuationAuthorTest(unittest.TestCase):
    """同一新目录在普通schema、限定命名空间和原生身份生产者中具有唯一含义。"""

    def test_contract(self):
        """真实cap选择器必须沿原schema到达单一显式枚举。"""
        rows=load_contract()['capabilities']
        self.assertEqual(sum(row['capability_id']=='task_authored_continuation_v1' for row in rows),1)
        self.assertEqual(sum(row['capability_id']=='task_authored_continuation_v2' for row in rows),1)

    def test_explicit_responsibility_version(self):
        """特殊已受领责任只随v2显式开启，旧能力不能冒充新版本。"""
        for version in [1,2]:
            scenario={'dynamic_world':{'profile':'local_society_v1','director':{'authored_task_continuation':{'version':version,'message_addresses':[]}}}}
            pack={'required_capabilities':['native_dynamic_world','task_authored_continuation_v'+str(version)],'scenarios':[scenario]}
            self.assertEqual(check_task_authored_continuation(pack,{}),[])
            pack['required_capabilities'][-1]='task_authored_continuation_v'+str(3-version)
            self.assertTrue(check_task_authored_continuation(pack,{}))
        schema={'$defs':SCHEMA['$defs'],'$ref':'#/$defs/dynamic_director'}
        director={'review_interval_ticks':1,'retry_delay_ticks':1,'decision_budget':2,'candidates_per_subject':2,'subjects':{}}
        for version in [1,2,3,True]:
            director['authored_task_continuation']={'version':version,'message_addresses':[]}
            self.assertEqual(not list(StrictValidator(schema).iter_errors(director)),type(version) is int and version in (1,2))

    def test_same_version_native_identity(self):
        """新局、标题/完整冷存和GD都携作者实际版本，marker位置沿旧序保留。"""
        initial=(ROOT/'native/world/src/resident/kernel/native_initial_state.cpp').read_text()
        codec=(ROOT/'native/world/src/resident/kernel/world_codec.cpp').read_text()
        world=(ROOT/'native/world/src/resident/kernel/world_bundle.cpp').read_text()
        gd=(ROOT/'game/ui/native_resident_backend.gd').read_text()
        self.assertIn('std::to_string(response_director.value.authored_task_continuation->version)',initial)
        self.assertIn('{"authored_task_continuation",{"1","2"}}',codec)
        self.assertIn('std::to_string(result->director->authored_task_continuation->version)',codec)
        self.assertIn('marker.second!=std::to_string(catalog->director->authored_task_continuation->version)',world)
        self.assertIn('str(definition["dynamic_world"]["director"]["authored_task_continuation"]["version"])',gd)

    def test_schema_and_reference(self):
        """新策略不能写任意坐标、任务状态或货权，跨包引用仍沿原注解限定。"""
        config={'version':1,'message_addresses':[{'offer_id':'letter','recipient_role_id':'recipient'}]}
        schema={'$defs':SCHEMA['$defs'],'$ref':'#/$defs/dynamic_director'}
        director={'review_interval_ticks':1,'retry_delay_ticks':1,'decision_budget':2,'candidates_per_subject':2,'subjects':{},'authored_task_continuation':config}
        validate=StrictValidator(schema)
        self.assertFalse(list(validate.iter_errors(director)))
        bad=copy.deepcopy(director);bad['authored_task_continuation']['version']=True
        self.assertTrue(list(validate.iter_errors(bad)))
        bad=copy.deepcopy(director);bad['authored_task_continuation']['message_addresses'][0]['recipient_actor_id']='somebody'
        self.assertTrue(list(validate.iter_errors(bad)))
        value={'scenarios':[{'dynamic_world':{'director':director}}]}
        refs=list(iter_references(value))
        self.assertEqual([(kind,key) for _,kind,key in refs],[('offers','letter'),('roles','recipient')])

    def test_original_recipient_links(self):
        """仅原收件岗位能作已知地址；启用未知委托与调换收件人都拒。"""
        scenario={'dynamic_world':{'profile':'local_society_v1','director':{'authored_task_continuation':{'version':1,'message_addresses':[{'offer_id':'letter','recipient_role_id':'recipient'}]}}},'enabled':{'offers':['letter'],'roles':['recipient']}}
        pack={'required_capabilities':['native_dynamic_world','task_authored_continuation_v1'],'scenarios':[scenario]}
        index={'offers':{'letter':{'template':'message','goal':{'receiver_actor':'person'}}},'roles':{'recipient':{'initial_actor':'person','services':['deliver_message']}}}
        self.assertEqual(check_task_authored_continuation(pack,index),[])
        index['roles']['recipient']['initial_actor']='other'
        self.assertTrue(check_task_authored_continuation(pack,index))
        index['roles']['recipient']['initial_actor']='person';scenario['enabled']['offers']=[]
        self.assertTrue(check_task_authored_continuation(pack,index))

    def test_identity_order(self):
        """三目录生产者有序向量一致，旧相邻marker不移动。"""
        for path,prefix in [('native/world/src/resident/kernel/world_codec.cpp','id'),('native/world/src/resident/kernel/native_initial_state.cpp','admitted'),('game/ui/native_resident_backend.gd','gd')]:
            text=(ROOT/path).read_text();positions=[]
            for name in ['director_registry','diplomatic_response','authored_task_continuation','obligation_needs']:
                pattern=(r'expected\["module_versions"\]\.append\(\{"name":\s*"'+name+'"') if prefix=='gd' else (re.escape(prefix)+r'\.module_versions\.emplace_back\("'+name+'",')
                matches=list(re.finditer(pattern,text));self.assertEqual(len(matches),1,(path,name));positions.append(matches[0].start())
            self.assertEqual(positions,sorted(positions),path)


if __name__=='__main__':unittest.main()
