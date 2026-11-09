"""本人回信的固定共享来源、实际登记和提交先验；源码门不替代同代World运行。"""
import json
from pathlib import Path
import subprocess
import unittest
from visit_reply_registration_candidate import BASE,CPP,HEADERS,ROOT as DOMAIN,SHARED,transform
ROOT=Path(__file__).resolve().parents[1]
class VisitReplyRegistrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """固定Git原文作唯一共享基准，不读取中央移动草稿。"""
        cls.before={p:subprocess.check_output(['git','-C',str(ROOT),'show',BASE+':'+p],text=True) for p in SHARED}
        cls.source={p:transform(p,s) for p,s in cls.before.items()}
    def leaf(self,name):
        """在完整清单中定位唯一原叶。"""
        found=[s for p,s in self.source.items() if p.endswith('/'+name)];self.assertEqual(len(found),1);return found[0]
    def test_three_identity_tails(self):
        """实际三producer均追加在询问之后，严格pair仍保原拜帖。"""
        for name,old,new in [('world_codec.cpp','if(result->contact_inquiries)id.module_versions','if(result->visit_message_replies)id.module_versions'),('native_initial_state.cpp','auto inquiries=decode_contact_inquiry_profile','auto visit_replies=decode_visit_reply_profile'),('native_resident_backend.gd','expected["module_versions"].append({"name":"contact_inquiries"','expected["module_versions"].append({"name":"visit_message_replies"')]:
            text=self.leaf(name);self.assertLess(text.index(old),text.index(new))
    def test_binary_title_json_same_tail(self):
        """标题、完整二进制和JSON只接同一独立新段；旧visitor不添空字段。"""
        text=self.leaf('world_codec.cpp')
        self.assertIn('if(catalog.contact_inquiries)domains.push_back({"contact_inquiries",1});if(catalog.visit_message_replies)domains.push_back({"visit_message_replies",1});return domains;',text)
        self.assertLess(text.index('if(has("contact_inquiries")){'),text.index('if(has("visit_message_replies")){'))
        self.assertLess(text.index('r=section("contact_inquiries"'),text.index('r=section("visit_message_replies"'))
        for part in ['else if(section.domain=="visit_message_replies")','UnexpectedVisitReplyDomain','!dynamic||!has("visit_messages")','validate_visit_reply_json_payloads','visit_reply_fact_payloads']:self.assertIn(part,text)
    def test_actual_result_commit_source(self):
        """前后唯一差分先定，原actual缓存及history落下后绑定，随后Plan末验。"""
        text=self.leaf('resident_world.cpp')
        self.assertLess(text.index('visit_reply_new_result_world(current,*candidate'),text.index('auto completed=commit_tail('))
        self.assertLess(text.index('state.history.push_back('),text.index('bind_visit_reply_result_world('))
        self.assertLess(text.index('bind_contact_inquiry_result_world('),text.index('bind_visit_reply_result_world('))
        self.assertLess(text.index('bind_visit_reply_result_world('),text.index('finish_player_plan_commit('))
        for old in ['bind_political_shop_registry_player_receipt','bind_family_apprenticeship_result_world','bind_political_governance_continuation','bind_private_shop_liquidation_result_world']:self.assertIn(old,text)
    def test_no_npc_writer_and_whole_interaction_savepoint(self):
        """即使NPC envelope指向player也写前拒，原整体Interaction仍在保存集。"""
        text=self.leaf('phase_plan.cpp');npc=text[text.index('Status try_npc_command('):]
        self.assertLess(npc.index('VisitReplyPlayerCommandRequired'),npc.index('begin_savepoint('))
        self.assertIn('std::holds_alternative<VisitReplyCommand>(command.data)||visit_reply_command_kind(command.kind)',npc)
        self.assertIn('std::holds_alternative<VisitReplyCommand>(command.data)||std::holds_alternative<ContactInquiryRequest>',text)
        self.assertIn('std::optional<InteractionState>{w.interaction}',text)
    def test_original_plan_structure_and_late_sources(self):
        """完整出生/原询问史先验，基础Plan结构与原所有Continue绑定位置保留。"""
        text=self.leaf('world_bundle.cpp');reply=text.index('valid=validate_visit_reply_world(*this)')
        self.assertLess(text.index('valid=validate_family_birth_world(*this)'),reply)
        self.assertLess(text.index('valid=validate_contact_inquiry_world(*this)'),reply)
        old=next(s for p,s in self.before.items() if p.endswith('/world_bundle.cpp'))
        self.assertEqual([x for x in old.splitlines() if 'validate_player_plan' in x],[x for x in text.splitlines() if 'validate_player_plan' in x])
        self.assertFalse(any('/player_plan/' in p for p in SHARED))
    def test_build_registry(self):
        """九CPP两表、八头、一Native与真实GD门全部具名登记。"""
        text=self.leaf('resident.cmake')
        for name in CPP:self.assertEqual(text.count('    '+DOMAIN.removeprefix('native/world/')+'visit_reply_'+name+'.cpp\n'),2)
        for name in HEADERS:self.assertEqual(text.count('    '+DOMAIN.removeprefix('native/world/')+'visit_reply'+name+'.hpp\n'),1)
        self.assertIn('add_test(NAME resident_visit_reply',text)
        self.assertIn("'test_visit_message_replies_panel_guard'",self.leaf('verify.py'))
    def test_cap_schema_and_actual_navigation(self):
        """仅明确profile可见，同一列表query及两实际侧栏入口。"""
        schema=json.loads(self.leaf('content.schema.json'));caps=json.loads(self.leaf('rule_capabilities.json'));cap=next(c for c in caps['capabilities'] if c['capability_id']=='visit_message_replies_v1')
        self.assertEqual(schema['$defs']['scenario']['properties']['visit_reply_profile']['enum'],[cap['schema_selectors'][0]['equals']])
        self.assertEqual(cap['support_status'],'implemented/unverified')
        self.assertIn('query_visit_reply_world(world',self.leaf('live_view_queries.cpp'))
        self.assertIn('visit_message_replies_enabled()',self.leaf('resident_page_requests.gd'))
        self.assertEqual(self.leaf('screen_composer.gd').count('VisitMessageRepliesPanel.build('),2)
    def test_named_headers_only(self):
        """Common只新增两具名实交事实，原Interaction/Task visitors完全不在差量。"""
        self.assertFalse(any(p.endswith('/interaction_codec.cpp') or p.endswith('/task_codec.cpp') for p in SHARED))
        text=self.leaf('common_codec.cpp');self.assertIn('kind=="visit_message_reply_posted"||kind=="visit_message_reply_collected"',text);self.assertIn('if(visit_reply&&named!="visit_message_replies")fail',text)
if __name__=='__main__':unittest.main()
