"""显式版本、三目录序与同候选来源登记门；源码断言不代World或旧档行为。"""
import argparse
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
OWNED = ROOT


class BiologicalRegistration(unittest.TestCase):
    def text(self, relative):
        """只读已选固定候选的实际源文件。"""
        return (ROOT / relative).read_text()

    def test_three_markers_and_binary_tail(self):
        """真实Born模板先于亲生结果尾；原许可段用明确目录版本选择。"""
        codec = self.text('native/world/src/resident/kernel/world_codec.cpp')
        self.assertIn('catalog.family_legacy->version==2?decode_family_legacy_v2', codec)
        self.assertIn('w.catalog->family_legacy->version==2?encode_family_legacy_v2', codec)
        self.assertIn('section.codec_version==2?visit_family_legacy_checkpoint_v2', codec)
        for text, marker in [(codec, 'id.module_versions.emplace_back'),
                             (self.text('native/world/src/resident/kernel/native_initial_state.cpp'), 'admitted.module_versions.emplace_back')]:
            self.assertLess(text.index(marker+'("born_personal_dialogues","1")'), text.index(marker+'("family_biological_legacy","1")'))
            self.assertIn('std::to_string(', text)
        gd = self.text('game/ui/native_resident_backend.gd')
        self.assertLess(gd.index('"name":"born_personal_dialogues"'), gd.index('"name":"family_biological_legacy"'))
        self.assertIn('"2" if legacy_profile=="posthumous_control_sources_v2" else "1"', gd)
        self.assertLess(codec.index('r=section("born_personal_dialogues"'), codec.index('r=section("family_biological_legacy"'))
        self.assertIn('!has("family_biological_legacy")&&has("family_legacy")&&modules.at("family_legacy")=="2"', codec)
        self.assertIn('modules.at("family_legacy")!="2"&&modules.at("family_legacy")!="3"', codec)

    def test_copy_bind_and_late_history(self):
        """克隆保账；先原出生会面、再唯一许可全史，结果绑定先于计划核。"""
        world = self.text('native/world/src/resident/kernel/world_bundle.cpp')
        for expression in ['family_biological_legacy(seed.family_biological_legacy?',
                           'family_biological_legacy(other.family_biological_legacy)',
                           'family_biological_legacy_seed_complete(other.family_biological_legacy_seed_complete)']:
            self.assertIn(expression, world)
        self.assertLess(world.index('valid=validate_family_birth_world'), world.index('valid=validate_born_personal_dialogue_world'))
        self.assertLess(world.index('valid=validate_born_personal_dialogue_world'), world.index('valid=validate_family_biological_world'))
        self.assertIn('if(!family_disciple_legacy_enabled()&&!family_biological_legacy_enabled()&&!family_collateral_legacy_enabled()){valid=validate_family_legacy', world)
        commit = self.text('native/world/src/resident/kernel/resident_world.cpp')
        self.assertIn('current.family_biological_legacy_enabled()?&current.family_legacy:nullptr', commit)
        self.assertLess(commit.index('auto biological_bound=bind_family_biological_result_world'), commit.index('finish_player_plan_commit'))
        phase = self.text('native/world/src/resident/kernel/phase_plan.cpp')
        self.assertIn('std::holds_alternative<BiologicalControlCommand>(command.data)', phase)

    def test_exact_catalog_and_owned_rollback(self):
        """工厂持S3对象，原交易原址回滚，不以目录或血缘代本人来源。"""
        bridge = self.text('native/world/src/resident/domains/family/biological_control_world.cpp')
        self.assertIn('BornPersonalDialogueWorldAccess::create(w)', bridge)
        self.assertIn('born_?&born_->read():nullptr', bridge)
        ledger = (OWNED/'native/world/src/resident/domains/family/family_legacy.cpp').read_text()
        self.assertEqual(ledger.count('auto original_state=state;auto& next=state;'), 2)
        self.assertEqual(ledger.count('state=std::move(original_state)'), 2)
        receipt = (OWNED/'native/world/src/resident/domains/family/biological_control_receipts.cpp').read_text()
        self.assertIn('cached->get()!=&result', receipt)
        self.assertIn('difference.value.size()!=1', receipt)
        self.assertIn('BiologicalControlCachedSourceReused', receipt)

    def test_npc_write_guards_and_late_personal_history(self):
        """两个NPC入口在写前拒控制许可；后继本人账只消费完整Legacy2。"""
        for file, marker, first_write in [('phase_plan.cpp', 'Status try_npc_command', 'auto mark=w.common.begin_savepoint'), ('dynamic_integration.cpp', 'Status adopt_director_proposal', 'auto mark=w.common.begin_savepoint')]:
            text = self.text('native/world/src/resident/kernel/'+file).split(marker, 1)[1]
            before = text.split(first_write, 1)[0]
            self.assertIn('std::holds_alternative<BiologicalControlCommand>', before)
            self.assertIn('"family_biological_accept"', before)
            self.assertIn('"family_biological_revoke"', before)
            self.assertIn('family_biological_legacy_enabled()', before)
            self.assertIn('std::holds_alternative<FamilyLegacyCommand>', before)
        world = self.text('native/world/src/resident/kernel/world_bundle.cpp')
        self.assertLess(world.index('valid=validate_family_biological_world'), world.index('valid=validate_private_shop_daily_rent_player_history_world'))

    def test_catalog_schema_and_identity(self):
        """新schema允许明确/2，原/1空请求仍拒；所有新增领域头CPP进入身份。"""
        from jsonschema import Draft202012Validator
        schema = json.loads(self.text('schemas/content.schema.json'))
        profile = schema['$defs']['family_legacy_profile']
        validator = Draft202012Validator(dict(profile, **{'$defs': schema['$defs']}))
        self.assertFalse(validator.is_valid({'profile':'authored_posthumous_control_consent_v1','request_ids':[]}))
        self.assertTrue(validator.is_valid({'profile':'posthumous_control_sources_v2','request_ids':[]}))
        cmake = self.text('native/world/resident.cmake')
        for file in sorted((OWNED/'native/world/src/resident/domains/family').glob('biological_*')):
            if file.suffix not in ('.hpp','.cpp'):
                continue
            relative = 'src/resident/domains/family/'+file.name
            self.assertEqual(cmake.count('    '+relative+'\n'), 2 if file.suffix=='.cpp' else 1, relative)
        self.assertIn('FamilyLegacyScreen.page(host)', self.text('game/ui/resident_page_requests.gd'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--owned', type=Path, required=True)
    args = parser.parse_args()
    ROOT, OWNED = args.root, args.owned
    unittest.main(argv=['test_family_biological_registration'])
