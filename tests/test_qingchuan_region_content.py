"""正式青川1.6作者输入及结果回访叶的有限检查；不构造游戏世界、不宣称路线成功。"""
from copy import deepcopy
import json
from pathlib import Path
import unittest

from tools.content.regional_outcome_links import check_regional_outcomes
from tools.content.validation import TABLES, validate_pack

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / 'content/packs/authored/local_society_1_6/content.json'
BASE = ROOT / 'content/packs/authored/local_society_1_5/content.json'


def read(path=PACK):
    """每次读取正式源的独立副本，不改已有作者原件。"""
    return json.loads(path.read_text())


def index(pack):
    """叶校验只借原正式记录身份，不给测试注入运行期结果。"""
    return {table: {row['id']: row for row in pack.get(table, [])} for table in TABLES}


def outcome_codes(pack):
    """确认只读检查不改输入，稳定错误码由实际叶实现返回。"""
    before = deepcopy(pack)
    codes = {code for code, _path, _message in check_regional_outcomes(pack, index(pack))}
    assert before == pack
    return codes


class QingchuanRegionContentTests(unittest.TestCase):
    """角色、资源、原责任和结果对白边界均从正式继承输入核验。"""

    def test_formal_single_chapter_and_inherited_resources(self):
        """新章只启用五原场所，钱粮不因叙事增加，人物仍是原实际身份。"""
        pack, base = read(), read(BASE)
        self.assertTrue(validate_pack(pack).ok)
        self.assertFalse(outcome_codes(pack))
        self.assertEqual(len(pack['scenarios']), 1)
        scenario = pack['scenarios'][0]
        self.assertEqual(scenario['id'], 'scenario.qingchuan_spring_reserve')
        self.assertEqual(set(scenario['enabled']['places']), {'place.courtyard', 'place.school', 'place.grain', 'place.sword_gate', 'place.sword_court'})
        original = base['scenarios'][0]['initial']
        for table, key in [('wallets', 'id'), ('inventories', 'id'), ('actors', 'actor_id')]:
            old = {row[key]: row for row in original[table]}
            for row in scenario['initial'][table]:
                before, after = deepcopy(old[row[key]]), deepcopy(row)
                if table == 'actors':
                    before.pop('appearance_definition', None)
                    after.pop('appearance_definition', None)
                self.assertEqual(before, after, (table, row[key]))
        self.assertEqual({row['id'] for row in pack['actors']}, {row['id'] for row in base['actors']})

    def test_actual_reserve_pressure_and_original_deadlines(self):
        """只改变松门本章储备目标，不造粮、改原报酬或放宽期限强迫成功。"""
        pack, base = read(), read(BASE)
        world = pack['scenarios'][0]['dynamic_world']
        self.assertEqual(world['local_economy']['supply_sources']['supply.city']['target_amount'], 124)
        self.assertEqual(world['local_economy']['supply_sources']['supply.sect']['target_amount'], 64)
        self.assertEqual(pack['task_templates'], base['task_templates'])
        self.assertEqual(world['affairs'], base['scenarios'][0]['dynamic_world']['affairs'])
        self.assertEqual(world['local_economy']['rates'], base['scenarios'][0]['dynamic_world']['local_economy']['rates'])
        self.assertEqual(world['organization']['membership_terms'], base['scenarios'][0]['dynamic_world']['organization']['membership_terms'])

    def test_npc_and_member_paths_use_existing_commands(self):
        """谷诚只获合法承包候选，书佐只获原招聘职事，没有预写合同、任职或成功。"""
        world = read()['scenarios'][0]['dynamic_world']
        gu = world['director']['subjects']['actor.study_caravanner']
        self.assertEqual(gu['templates'], ['maintain_commitment', 'replenish_supply', 'fulfill_contract', 'await_adjustment'])
        self.assertNotIn('private_livelihood', gu)
        self.assertEqual(gu['report_source_keys'], ['supply.city', 'source.market'])
        posts = world['director']['subjects']['actor.city_steward']['recruitment_posts']
        self.assertEqual(posts, [{'office_id': 'office.city_steward', 'terms_id': 'terms.city_contract', 'candidate_actor_ids': ['actor.apprentice']}])
        self.assertNotIn('actor.apprentice', {row['actor_id'] for row in world['organization']['members'].values()})
        self.assertEqual(world['organization']['initial_activities'], {})

    def test_outcome_dialogue_cannot_submit_business_commands(self):
        """结果话题拒绝任何action节点，避免看回访时重发奖励或代交货。"""
        pack = read()
        dialogue = next(row for row in pack['dialogues'] if row['id'] == 'dialog.region_city_outcome')
        dialogue['nodes'][0] = {'id': 'review', 'kind': 'action', 'command': {'kind': 'accept_offer'}, 'on_success': 'choose', 'on_failure': 'choose'}
        self.assertIn('REGIONAL_OUTCOME_COMMAND', outcome_codes(pack))

    def test_outcome_role_template_and_dialogue_are_same_source(self):
        """同一个结果不能换成别处办理人、另一组织模板或无来源的陌生人。"""
        for field, value, expected in [('role_id', 'role.merchant', 'REGIONAL_OUTCOME_ROLE'),
                                       ('task_template_id', 'template.sect_grain', 'REGIONAL_OUTCOME_TEMPLATE'),
                                       ('dialogue_id', 'dialog.region_sect_outcome', 'REGIONAL_OUTCOME_DIALOGUE')]:
            with self.subTest(field=field):
                pack = read()
                pack['scenarios'][0]['dynamic_world']['regional_life']['outcome_topics'][0][field] = value
                self.assertIn(expected, outcome_codes(pack))

    def test_outcome_duplicate_and_cooldown_bounds(self):
        """三类主题与对白共享身份域，超出原运行冷却上限必须明确拒绝。"""
        pack = read()
        regional = pack['scenarios'][0]['dynamic_world']['regional_life']
        regional['outcome_topics'][0]['id'] = regional['need_topics'][0]['id']
        self.assertIn('REGIONAL_OUTCOME_DUPLICATE', outcome_codes(pack))
        pack = read()
        pack['scenarios'][0]['dynamic_world']['regional_life']['outcome_topics'][0]['cooldown_ticks'] = 721
        self.assertIn('REGIONAL_OUTCOME_COOLDOWN', outcome_codes(pack))


if __name__ == '__main__':
    unittest.main()
