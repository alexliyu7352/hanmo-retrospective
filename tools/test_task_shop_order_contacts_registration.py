"""核实际合流源码或明确隔离候选；不重新生成旧后像冒中央通过。"""
import os,unittest
from test_native_module_identity_order import actual_submit_tail_arguments
from pathlib import Path
from task_shop_order_contact_registration_candidate import CPP,HEADERS,ROOT
class ShopContactRegistration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.root=Path(os.environ.get('HANMO_SOURCE_ROOT',Path(__file__).resolve().parents[1]))
    def source(self,path):return (self.root/path).read_text()
    def kernel(self,name):return self.source('native/world/src/resident/kernel/'+name)
    def test_three_identity_tails(self):
        for path,old,new in [('native/world/src/resident/kernel/world_codec.cpp','if(result->known_personal_dialogues)id.module_versions','if(result->task_shop_order_contacts_enabled)id.module_versions'),('native/world/src/resident/kernel/native_initial_state.cpp','auto known_dialogues=decode_known_personal_dialogue_profile','auto shop_contacts=decode_task_shop_order_contact_profile'),('game/ui/native_resident_backend.gd','expected["module_versions"].append({"name":"known_personal_dialogues"','expected["module_versions"].append({"name":"task_shop_order_contacts"')]:
            s=self.source(path);self.assertLess(s.index(old),s.index(new))
    def test_selected_inspect_same_layout(self):
        s=self.kernel('world_codec.cpp');self.assertIn('task_shop_order_contact_selected_version(identity.catalog.module_versions)',s);self.assertIn('task_shop_order_contact_selected_version(versions)',s);self.assertIn('selected.value!=version(task_shop_order_contact_module)',s)
    def test_real_independent_section(self):
        s=self.kernel('world_codec.cpp');self.assertIn('section(task_shop_order_contact_module',s);self.assertIn('else if(section.domain==task_shop_order_contact_module)',s);self.assertIn('UnexpectedTaskShopOrderContactDomain',s);self.assertIn('validate_task_shop_order_contact_json_payloads',s);self.assertIn('merge_unique(facts,shop_contacts.value)',s)
    def test_original_state_is_optional(self):
        self.assertIn('std::optional<TaskShopOrderContactState> contacts;',self.source('native/world/src/resident/domains/tasks/shop_orders/task_shop_order_state.hpp'))
    def test_new_source_after_old_cost_read(self):
        s=self.kernel('private_shop_integration.cpp');start=s.index('if(command.kind==TaskShopOrderCommand::Kind::deliver){if(!quote.delivery)');self.assertIn('validate_private_shops(access.value->context())',s[max(0,start-300):start]);self.assertIn('record_task_shop_order_contact_world',s[start:start+500]);self.assertIn('nullptr,phase}),',s)
    def test_start_binding_after_real_result(self):
        s=self.kernel('resident_world.cpp');self.assertIn('task_shop_order_contact_new_result_world(current,*candidate',s);self.assertLess(s.index('auto receipt='),s.index('bind_task_shop_order_contact_result_world'));self.assertEqual(actual_submit_tail_arguments(s)['shop_order_contact_source'],'shop_order_contact_source.value')
    def test_two_npc_write_guards(self):
        for name in ['phase_plan.cpp','dynamic_integration.cpp']:self.assertIn('TaskShopOrderContactOuterResultRequired',self.kernel(name))
    def test_late_control_then_contacts_then_cost(self):
        s=self.kernel('world_bundle.cpp');at=s.index('valid=validate_task_shop_order_contact_world(*this)');self.assertLess(s.index('valid=validate_family_biological_world(*this)'),at);self.assertGreater(s.index('valid=validate_private_shop_world(*this)',at),at);self.assertIn('if(!catalog->task_shop_order_contacts_enabled)',s);self.assertLess(s.index('validate_player_plan_shape(player_plan)'),at)
    def test_final_inheritance_and_plan(self):
        s=self.kernel('world_bundle.cpp');at=s.index('if(catalog->task_shop_order_contacts_enabled){auto inheritance=');self.assertLess(s.index('valid=validate_private_shop_world(*this)',s.index('valid=validate_task_shop_order_contact_world(*this)')),at);self.assertIn('validate_player_plan_world(*this)',s[at:at+1000])
    def test_s5_reads_same_candidate(self):
        s=self.kernel('private_shop_integration.cpp');self.assertIn('order_contacts_(w.tasks.shop_orders',s);self.assertIn('order_contacts_?&*order_contacts_:nullptr,w.catalog->task_shop_order_contacts_enabled',s);self.assertLess(s.index('validate_task_shop_order_contact_sources_world(w,access.orders(),access.context().phase)'),s.index('capture_private_shop_order_execution_in_candidate(w.common,w.private_shops,access.context(),source.order_id)'))
    def test_cmake_complete(self):
        s=self.source('native/world/resident.cmake')
        for name in CPP:self.assertEqual(s.count('    '+ROOT.removeprefix('native/world/')+name+'.cpp'),2)
        for name in HEADERS:self.assertEqual(s.count('    '+ROOT.removeprefix('native/world/')+name+'.hpp'),1)
        self.assertEqual(s.count('    src/resident/domains/commerce/private_shop_order_contact.cpp'),2);self.assertIn('test_resident_task_shop_order_contacts',s);self.assertIn('test_resident_private_shop_order_contact',s)
if __name__=='__main__':unittest.main()
