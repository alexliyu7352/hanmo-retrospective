"""旁支显式版本、实际结果、同候选与冷前置的源码门；不替代World行为。"""
import argparse
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
OWNED = ROOT

class CollateralRegistration(unittest.TestCase):
    def text(self, path):
        return (ROOT / path).read_text()

    def test_explicit_three_producers_and_tail(self):
        codec = self.text('native/world/src/resident/kernel/world_codec.cpp')
        initial = self.text('native/world/src/resident/kernel/native_initial_state.cpp')
        gd = self.text('game/ui/native_resident_backend.gd')
        for text, prefix in ((codec, 'id.module_versions.emplace_back'), (initial, 'admitted.module_versions.emplace_back')):
            self.assertEqual(text.count(prefix+'("family_collateral_legacy","1")'), 1)
            self.assertLess(text.index(prefix+'("known_personal_dialogues","1")'), text.index(prefix+'("family_collateral_legacy","1")'))
        self.assertLess(gd.index('"name":"known_personal_dialogues"'), gd.index('"name":"family_collateral_legacy"'))
        self.assertIn('"3" if legacy_profile=="posthumous_control_sources_v3"', gd)
        self.assertIn('catalog.family_legacy->version==3?decode_family_legacy_v3', codec)
        self.assertIn('w.catalog->family_legacy->version==3?encode_family_legacy_v3', codec)
        self.assertIn('section.codec_version==3?visit_family_legacy_checkpoint_v3', codec)
        self.assertLess(codec.index('r=section("known_personal_dialogues"'), codec.index('r=section("family_collateral_legacy"'))

    def test_old_visitor_shape_remains_explicit(self):
        text = (OWNED/'native/world/src/resident/domains/family/family_legacy.hpp').read_text()
        self.assertIn('visit_family_legacy_checkpoint_v3', text)
        self.assertIn('visit_family_legacy_checkpoint_v2', text)
        codec = self.text('native/world/src/resident/kernel/world_codec.cpp')
        self.assertIn('CollateralControlLegacyVersionRequired', codec)
        self.assertIn('UnexpectedCollateralControlState', codec)

    def test_copy_npc_and_source_order(self):
        world = self.text('native/world/src/resident/kernel/world_bundle.cpp')
        for source in ['family_collateral_legacy(seed.family_collateral_legacy?', 'family_collateral_legacy(other.family_collateral_legacy)', 'family_collateral_legacy_seed_complete(other.family_collateral_legacy_seed_complete)']:
            self.assertIn(source, world)
        calls = ['validate_known_personal_dialogue_world', 'validate_family_collateral_recognition_world', 'validate_family_collateral_world', 'validate_private_shop_command_result_player_world']
        positions = [world.index('valid='+call) for call in calls]
        self.assertEqual(positions, sorted(positions))
        self.assertIn('!family_collateral_legacy_enabled()){valid=validate_family_legacy', world)
        for name in ['phase_plan.cpp', 'dynamic_integration.cpp']:
            text = self.text('native/world/src/resident/kernel/'+name)
            self.assertIn('std::holds_alternative<CollateralControlCommand>', text)
            self.assertIn('family_collateral_', text)

    def test_actual_before_and_result_binding(self):
        text = self.text('native/world/src/resident/kernel/resident_world.cpp')
        self.assertIn('current.family_collateral_legacy_enabled()?&current.family_collateral_legacy:nullptr', text)
        self.assertIn('collateral_before', text)
        self.assertLess(text.index('bind_family_collateral_result_world'), text.index('finish_player_plan_commit'))
        bridge = self.text('native/world/src/resident/domains/family/collateral_control_world.cpp')
        self.assertLess(bridge.index('valid=bind_collateral_recognition_result_in_candidate'), bridge.index('return bind_collateral_control_result_in_candidate'))
        receipt = (OWNED/'native/world/src/resident/domains/family/collateral_control_receipts.cpp').read_text()
        self.assertIn('cached->get()!=&result', receipt)

    def test_same_world_borrow_and_no_recursive_validation(self):
        bridge = self.text('native/world/src/resident/domains/family/collateral_control_world.cpp')
        header = self.text('native/world/src/resident/domains/family/collateral_control_world.hpp')
        self.assertIn('KnownPersonalDialogueWorldAccess::create(w)', bridge)
        self.assertIn('family_player_before_fact_with_legacy(c,w.family,w.family_legacy,tick,fact)', bridge)
        self.assertIn('((n<number)!=(where<at))', bridge)
        self.assertIn('CollateralControlWorldAccess(const CollateralControlWorldAccess&)=delete', header)
        self.assertLess(header.index('meetings_;'), header.index('CollateralRecognitionContext recognition_;'))
        self.assertNotIn('world_.validate()', bridge)

    def test_catalog_and_capabilities(self):
        from jsonschema import Draft202012Validator
        schema = json.loads(self.text('schemas/content.schema.json'))
        legacy = Draft202012Validator(dict(schema['$defs']['family_legacy_profile'], **{'$defs':schema['$defs']}))
        self.assertFalse(legacy.is_valid({'profile':'authored_posthumous_control_consent_v1','request_ids':[]}))
        for version in [2, 3]:
            self.assertTrue(legacy.is_valid({'profile':f'posthumous_control_sources_v{version}','request_ids':[]}))
        profile = schema['$defs']['family_collateral_legacy_profile']
        self.assertFalse(profile['additionalProperties'])
        self.assertEqual(profile['properties']['version']['enum'], [1])
        caps = json.loads(self.text('game/data/rule_capabilities.json'))['capabilities']
        for capability in ['family_legacy_v3', 'family_collateral_legacy_v1']:
            self.assertEqual(sum(row['capability_id']==capability for row in caps), 1)
            self.assertIn(capability, schema['properties']['required_capabilities']['items']['enum'])

    def test_identity_and_json_payload(self):
        cmake = self.text('native/world/resident.cmake')
        files = list((OWNED/'native/world/src/resident/domains/family').glob('*collateral*'))
        files += [ROOT/'native/world/src/resident/domains/family/collateral_control_world.hpp', ROOT/'native/world/src/resident/domains/family/collateral_control_world.cpp']
        for file in files:
            if file.suffix in ['.cpp','.hpp']:
                path = 'src/resident/domains/family/'+file.name
                self.assertEqual(cmake.count('    '+path+'\n'), 2 if file.suffix=='.cpp' else 1, path)
        common = self.text('native/world/src/resident/storage/common_codec.cpp')
        self.assertIn('kind=="family_collateral_recognized"', common)
        self.assertIn('"family_collateral_legacy"', common)
        self.assertIn('validate_collateral_recognition_json_payloads', self.text('native/world/src/resident/kernel/world_codec.cpp'))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--owned', type=Path, required=True)
    args = parser.parse_args()
    ROOT, OWNED = args.root, args.owned
    unittest.main(argv=['test_family_collateral_registration'])
