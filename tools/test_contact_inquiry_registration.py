"""固定Git共享闭包的来源、顺序与真实登记门；不代World运行或GUI验收。"""
import json
from pathlib import Path
import subprocess
import unittest
from contact_inquiry_registration_candidate import BASE,CPP,HEADERS,ROOT as DOMAIN,SHARED,transform
ROOT=Path(__file__).resolve().parents[1]
class ContactInquiryRegistrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """全程从固定对象取头，中央在写的工作树不能混入。"""
        cls.source={p:transform(p,subprocess.check_output(['git','-C',str(ROOT),'show',BASE+':'+p],text=True)) for p in SHARED}
    def leaf(self,name):
        """共享清单中每叶唯一。"""
        found=[s for p,s in self.source.items() if p.endswith('/'+name)];self.assertEqual(len(found),1);return found[0]
    def test_three_identity_producers_tail(self):
        """目录/新局/GD marker严格同一新尾，原清店双段保留。"""
        for name,old,new in [('world_codec.cpp','if(result->private_shop_liquidations){id.module_versions','if(result->contact_inquiries)id.module_versions'),('native_initial_state.cpp','auto liquidations=decode_private_shop_liquidation_profile','auto inquiries=decode_contact_inquiry_profile'),('native_resident_backend.gd','expected["module_versions"].append({"name":"task_shop_order_estate_cancellations"','if definition.get("contact_inquiry_profile"')]:
            text=self.leaf(name);self.assertLess(text.index(old),text.index(new))
    def test_binary_and_title_match_tail(self):
        """实际写段与标题选择均在旧最后段后，导入严格独立段。"""
        text=self.leaf('world_codec.cpp')
        self.assertIn('if(catalog.contact_inquiries)domains.push_back({"contact_inquiries",1});return domains;',text)
        self.assertLess(text.index('{const bool estate=has("private_shop_liquidations");'),text.index('if(has("contact_inquiries")){'))
        self.assertLess(text.index('r=section("task_shop_order_estate_cancellations"'),text.index('r=section("contact_inquiries"'))
        self.assertIn('else if(section.domain=="contact_inquiries")',text)
        self.assertIn('UnexpectedContactInquiryDomain',text)
        self.assertIn('!has("personal_appointments")&&!has("task_message_handoffs")',text)
    def test_actual_result_binder_before_plan(self):
        """实际Common缓存/历史之后才永久绑定，本人Plan只能消费这份结果。"""
        text=self.leaf('resident_world.cpp')
        self.assertLess(text.index('contact_inquiry_new_result_world(current,*candidate'),text.index('auto completed=commit_tail('))
        self.assertLess(text.index('state.history.push_back('),text.index('bind_contact_inquiry_result_world('))
        self.assertLess(text.index('bind_contact_inquiry_result_world('),text.index('finish_player_plan_commit('))
        for old in ['bind_personal_appointment_result_world','bind_political_shop_registry_player_receipt','bind_task_shop_order_estate_result_world','bind_private_shop_liquidation_result_world']:self.assertIn(old,text)
    def test_phase_and_full_savepoint(self):
        """原半日先减步，再答复，原统一释放；失败仍存整个Interaction。"""
        text=self.leaf('phase_plan.cpp');start=text.index('}else if(action.kind==contact_inquiry_action_kind)');end=text.index('}else if(action.kind==personal_appointment_action_kind)',start);section=text[start:end]
        self.assertLess(section.index('remaining_ticks=0'),section.index('step_contact_inquiry_world'))
        self.assertIn('outcome=contact_inquiry_completion(step.value)',section)
        self.assertIn('std::holds_alternative<ContactInquiryRequest>(command.data)||',text)
        self.assertIn('std::optional<InteractionState>{w.interaction}',text)
        npc=text[text.index('Status try_npc_command('):];self.assertLess(npc.index('ContactInquiryPlayerCommandRequired'),npc.index('begin_savepoint('))
    def test_source_validation_before_plan(self):
        """创建/出生和原预约史先验，之后新探听史，再准Plan冷读新结果。"""
        text=self.leaf('world_bundle.cpp');late=text.index('valid=validate_contact_inquiry_world(*this)')
        self.assertIn('if(player_plan_enabled()){valid=validate_player_plan(common,player_plan);if(!valid.ok)return valid;',text)
        self.assertLess(text.index('valid=validate_family_birth_world(*this)'),late)
        self.assertLess(text.index('valid=validate_personal_appointment_world(*this)'),late)
        self.assertGreater(text.index('if(player_plan_enabled()&&catalog->contact_inquiries)'),late)
        for line in text[:late].splitlines():
            if 'validate_player_plan_world(*this)' in line:self.assertIn('!catalog->contact_inquiries',line)
    def test_actual_build_and_source_artifacts(self):
        """十CPP两原编译表、八头及三必要测试都真实登记。"""
        text=self.leaf('resident.cmake')
        for suffix in CPP:self.assertEqual(text.count('    '+DOMAIN.removeprefix('native/world/')+'contact_inquiry_'+suffix+'.cpp\n'),2)
        for suffix in HEADERS:self.assertEqual(text.count('    '+DOMAIN.removeprefix('native/world/')+'contact_inquiry'+suffix+'.hpp\n'),1)
        for name in ['source','action','receipts']:self.assertIn('add_test(NAME resident_contact_inquiry_'+name,text)
        self.assertIn("'test_contact_inquiries_panel_guard'",self.leaf('verify.py'))
    def test_explicit_schema_and_native_query(self):
        """枚举和cap精确相配，页面两入口都先核启用，不显示远方政策。"""
        schema=json.loads(self.leaf('content.schema.json'));caps=json.loads(self.leaf('rule_capabilities.json'));cap=next(c for c in caps['capabilities'] if c['capability_id']=='contact_inquiries_v1')
        self.assertEqual(schema['$defs']['scenario']['properties']['contact_inquiry_profile']['enum'],[cap['schema_selectors'][0]['equals']])
        self.assertEqual(cap['support_status'],'implemented/unverified')
        self.assertIn('query_contact_inquiry_world(world',self.leaf('live_view_queries.cpp'))
        self.assertIn('contact_inquiries_enabled()',self.leaf('resident_page_requests.gd'))
        self.assertEqual(self.leaf('screen_composer.gd').count('ContactInquiriesPanel.build('),2)
    def test_original_visitors_untouched(self):
        """共享列表没有旧Interaction/Task visitor；Common只承认两具名新头。"""
        self.assertFalse(any(p.endswith('/interaction_codec.cpp') or p.endswith('/task_codec.cpp') for p in SHARED))
        text=self.leaf('common_codec.cpp');self.assertIn('const bool inquiry=kind=="contact_inquiry_started"||kind=="contact_inquiry_finished";',text)
        self.assertIn('if(inquiry&&named!="contact_inquiries")fail',text)
if __name__=='__main__':unittest.main()
