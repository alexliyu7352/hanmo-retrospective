"""回复2版本/实际NPC来源接线源门；不把syntax当自然往返已验。"""
import json,subprocess,unittest
from pathlib import Path
from visit_reply_v2_registration_candidate import BASE,SHARED,transform
ROOT=Path(__file__).resolve().parents[1]
class VisitReplyV2RegistrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """固定Git对象生成一次原文与候选，中央可同时继续后继源码。"""
        cls.source={p:transform(p,subprocess.check_output(['git','-C',str(ROOT),'show',BASE+':'+p],text=True)) for p in SHARED}
    def leaf(self,name):
        """每叶从精确清单取，不扫运行产物。"""
        rows=[s for p,s in self.source.items() if p.endswith('/'+name)];self.assertEqual(len(rows),1);return rows[0]
    def test_markers_append_after_legacy(self):
        """三producer对齐版本，意向只追加在原弟子接续尾后。"""
        for name,old,new in [('native_initial_state.cpp','if(const auto* config=scenario.find("family_disciple_legacy"))','if(visit_replies.value==2)admitted.module_versions'),('world_codec.cpp','if(result->family_disciple_legacy)id.module_versions','if(result->visit_reply_intents)id.module_versions'),('native_resident_backend.gd','if definition.has("family_disciple_legacy")','if definition.get("visit_reply_profile","")=="witnessed_authored_visit_replies_v2": expected')]:
            s=self.leaf(name);self.assertLess(s.index(old),s.index(new))
        self.assertIn('std::to_string(visit_replies.value)',self.leaf('native_initial_state.cpp'))
        self.assertIn('interaction->visit_message_replies->version=mutable_catalog->visit_message_replies->version',self.leaf('native_initial_state.cpp'))
    def test_title_and_binary_version_exact(self):
        """回复1/2实际段版本按作者值选，意向没有第二副本持久段。"""
        s=self.leaf('world_codec.cpp');self.assertIn('{"visit_message_replies",{"1","2"}}',s);self.assertIn('has("visit_reply_intents")!=(has("visit_message_replies")&&modules.at("visit_message_replies")=="2")',s);self.assertIn('has("visit_reply_intents","1")!=(version("visit_message_replies")==2)',s);self.assertIn('static_cast<std::uint32_t>(catalog.visit_message_replies->version)',s);self.assertNotIn('section("visit_reply_intents"',s)
    def test_catalog_after_real_reception(self):
        """新意向完整验证用真正解码的原reception，不用空占位目录。"""
        s=self.leaf('world_codec.cpp');self.assertLess(s.index('result->director=std::make_shared<const DirectorCatalog>'),s.index('auto parsed=decode_visit_reply_intents('));self.assertIn('*result->director->visit_reception,*result->interaction->visit_messages,*result->interaction',s);self.assertIn('decoded.value.version!=visit_replies.value',s)
    def test_actual_typed_npc_only(self):
        """内部类型严格双向kind并重prepare，公开JSON明确拒绝。"""
        s=self.leaf('dispatcher.cpp');self.assertIn('(command.kind==authored_visit_reply_command)!=(authored_reply!=nullptr)',s);self.assertIn('(op.command.kind==authored_visit_reply_command)!=(authored_reply!=nullptr)',s);self.assertIn('prepare_authored_visit_reply_world(w,id,*authored_reply)',s);self.assertIn('commit_authored_visit_reply_world(w,id,*authored_reply',s);self.assertIn('if(kind==authored_visit_reply_command)return',self.leaf('command_boundary.cpp'))
    def test_whole_interaction_savepoints(self):
        """新内部命令存完整Interaction；原提案层同样持整状态，失败回滚两层覆盖。"""
        s=self.leaf('phase_plan.cpp');self.assertIn('std::holds_alternative<AuthoredVisitReply>(command.data)||std::holds_alternative<VisitReplyCommand>',s);self.assertIn('std::optional<InteractionState>{w.interaction}',s)
        original=subprocess.check_output(['git','-C',str(ROOT),'show',BASE+':native/world/src/resident/kernel/dynamic_integration.cpp'],text=True);self.assertIn('interaction(w.interaction)',original);self.assertIn('w.interaction=std::move(interaction)',original)
    def test_no_old_shared_visitors_or_commit_hooks(self):
        """保原实际player结果钩和模板枚举，不借空玩家result为NPC来源。"""
        for suffix in ['resident_world.cpp','director_model.hpp','interaction_codec.cpp','task_codec.cpp']:self.assertFalse(any(p.endswith('/'+suffix) for p in SHARED))
        s=self.leaf('phase_plan.cpp');self.assertIn('VisitReplyPlayerCommandRequired',s)
    def test_control_history_before_v2_only(self):
        """原v1冷序不改，v2必须在完整弟子控制史后；基础Plan结构门仍原样。"""
        s=self.leaf('world_bundle.cpp');original=subprocess.check_output(['git','-C',str(ROOT),'show',BASE+':native/world/src/resident/kernel/world_bundle.cpp'],text=True)
        self.assertLess(s.index('validate_family_disciple_legacy_world(*this)'),s.index('if(catalog->visit_message_replies&&catalog->visit_message_replies->version==2){valid=validate_visit_reply_world(*this)'))
        self.assertIn('if(!catalog->visit_message_replies||catalog->visit_message_replies->version==1)',s)
        self.assertEqual(original.count('validate_player_plan(common,player_plan)'),s.count('validate_player_plan(common,player_plan)'))
    def test_build_and_cap_schema(self):
        """三个新CPP两表/三个头/原生源用例是真登记。"""
        s=self.leaf('resident.cmake')
        for name in ['intents','authored','director','control']:
            self.assertEqual(s.count('    src/resident/domains/interaction/visit_replies/visit_reply_'+name+'.cpp\n'),2);self.assertEqual(s.count('    src/resident/domains/interaction/visit_replies/visit_reply_'+name+'.hpp\n'),1)
        self.assertIn('add_test(NAME resident_visit_reply_authored',s)
        schema=json.loads(self.leaf('content.schema.json'));cap=next(x for x in json.loads(self.leaf('rule_capabilities.json'))['capabilities'] if x['capability_id']=='visit_message_replies_v2');self.assertIn(cap['schema_selectors'][0]['equals'],schema['$defs']['scenario']['properties']['visit_reply_profile']['enum']);self.assertEqual(schema['$defs']['scenario']['properties']['visit_message_replies']['properties']['version']['enum'],[1,2]);self.assertIn('check_visit_reply_intents(pack,index)',self.leaf('links.py'))
if __name__=='__main__':unittest.main()
