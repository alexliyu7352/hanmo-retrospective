"""中央原日租尾/事务/本人前界接头源门；不能替代实际World或旧档字节执行。"""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
K='native/world/src/resident/kernel/'

def read(path):
    """始终核当前源码，不从开发缓存拼装候选。"""
    return (ROOT/path).read_text()

class DailyRentRegistration(unittest.TestCase):
    def test_three_producer_order(self):
        for path,first,last in [(K+'world_codec.cpp','if(result->political_governance_invitation_replies)id.module_versions','if(result->private_shop_daily_rent)id.module_versions'),(K+'native_initial_state.cpp','if(invitation_replies.value)admitted.module_versions','if(daily_rent.value)admitted.module_versions'),('game/ui/native_resident_backend.gd','if definition.get("dynamic_world",{}).has("political_governance_invitation_replies")','if definition.get("dynamic_world",{}).has("private_shop_daily_rent")')]:
            text=read(path);self.assertLess(text.index(first),text.index(last))
    def test_independent_exact_selected_tail(self):
        s=read(K+'world_codec.cpp')
        self.assertIn('if(catalog.private_shop_daily_rent)domains.push_back({private_shop_daily_rent_module,1})',s)
        self.assertIn('validate_private_shop_daily_rent_pair(has(private_shop_daily_rent_module),identity.catalog.module_versions)',s)
        self.assertLess(s.index('r=section(governance_invitation_reply_module'),s.index('r=section(private_shop_daily_rent_module'))
        self.assertIn('else if(section.domain==private_shop_daily_rent_module)',s)
        for who in ['writer.schema_features','reader.schema_features','political_features']:
            self.assertEqual(s.count(who+'|=private_shop_daily_rent_checkpoint_feature'),1)
    def test_json_strict_header_separate_payload(self):
        s=read(K+'world_codec.cpp');common=read('native/world/src/resident/storage/common_codec.cpp')
        self.assertIn('if(daily_rent)fields(row.second,{"kind","domain"},{},row.first)',common)
        self.assertIn('daily_rent!=(named=="private_shop_daily_rent")',common)
        self.assertIn('daily_rent_fact_kind(h.kind)||h.domain==private_shop_daily_rent_module)continue',s)
        self.assertIn('validate_private_shop_daily_rent_layout(view,result.private_shop_daily_rent.get())',s)
        self.assertIn('else if(source.find(private_shop_daily_rent_module))fail(',s)
        self.assertIn('if(!w.private_shop_daily_rent_enabled()||payload)fail(',s)
        self.assertIn('set(output,private_shop_daily_rent_module,encode_private_shop_daily_rent_state',s)
    def test_initial_ownership_and_seed_copy(self):
        s=read(K+'private_shop_integration.cpp');self.assertLess(s.index('w.private_shops_seed_complete=true'),s.index('return initialize_private_shop_daily_rent_world_in_candidate'))
        self.assertNotIn('seed.private_shop_daily_rent=',read(K+'native_initial_state.cpp'))
        s=read(K+'world_bundle.cpp');self.assertIn('private_shop_daily_rent(seed.private_shop_daily_rent?',s);self.assertIn('private_shop_daily_rent(other.private_shop_daily_rent)',s)
    def test_clock_and_npc_atomic_paths(self):
        s=read(K+'phase_plan.cpp');self.assertIn('case Phase::clock_commit:w.common.state().identity.tick=to_tick;r=reconcile_private_shop_daily_rent_world_in_candidate',s)
        for path,start in [(K+'phase_plan.cpp','Status try_npc_command('),(K+'dynamic_integration.cpp','Status adopt_director_proposal(')]:
            body=read(path).split(start,1)[1];self.assertLess(body.index('PrivateShopDailyRentPlayerCommandRequired'),body.index('begin_savepoint'))
        self.assertIn('if(daily_rent)w.private_shop_daily_rent=std::move(*daily_rent)',s)
        self.assertIn('w.private_shop_daily_rent=std::move(private_shop_daily_rent)',read(K+'dynamic_integration.cpp'))
    def test_actual_result_and_late_player(self):
        s=read(K+'resident_world.cpp').split('Status commit_tail(',1)[1];self.assertLess(s.index('history.push_back'),s.index('bind_private_shop_daily_rent_result_world'));self.assertLess(s.index('bind_private_shop_daily_rent_result_world'),s.index('w.validate('))
        s=read(K+'world_bundle.cpp');self.assertLess(s.index('validate_private_shop_liquidation_world(*this)'),s.index('validate_private_shop_daily_rent_world(*this)'));self.assertLess(s.index('validate_family_disciple_legacy_world(*this)'),s.index('validate_private_shop_daily_rent_player_history_world(*this)'))
        s=read(K+'private_shop_daily_rent_integration.cpp');self.assertIn('read_private_shop_rent_player_before_fact',s)
    def test_exact_receipt_variant_without_new_shape(self):
        s=read('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertIn('private_shop_daily_rent_command_kind(kind)&&(!(c.schema_features&private_shop_daily_rent_checkpoint_feature)||tag!=4)',s);self.assertIn('private_shop_daily_rent_command_kind(value.kind)&&!has_details',s)
    def test_no_changed_legacy_shop_visitor(self):
        import subprocess
        p='native/world/src/resident/domains/commerce/private_shop_codec.cpp'
        self.assertEqual((ROOT/p).read_bytes(),subprocess.check_output(['git','show','5566d061522a51cf9bf596a4ac0a2ca7b7ac2e94:'+p],cwd=ROOT))

if __name__=='__main__':unittest.main()
