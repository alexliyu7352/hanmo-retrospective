"""正式组合沿真实作者包、Schema和快照入口验证，失败不修改输入或放宽旧政策。"""
import copy
import json
from pathlib import Path

import unittest

from tools.content.validation import validate_pack
from tools.content.appearance_choices import check_appearance_choices

ROOT = Path(__file__).resolve().parents[2]


def candidate():
    """只替换原青川情景的显式创角政策与目录，保持真实角色初态及其余内容。"""
    pack = json.loads((ROOT / 'content/packs/authored/local_society_1_6/content.json').read_text())
    scenario = next(row for row in pack['scenarios'] if row.get('player_appearance_choices'))
    records = json.loads((ROOT / 'game/data/adult_appearance_compositions.json').read_text())['records']
    scenario['dynamic_world']['founder_policy'] = 'fixed_actor_appearance_composition_v1'
    scenario['player_appearance_choices'] = [
        {key: copy.deepcopy(row[key]) for key in ('id', 'base_features', 'attire_style')}
        for row in records]
    return pack, scenario


def _assert_real_author_composition_is_valid_without_changing_input():
    """192完整元组进入原作者校验链，返回后原包逐字段保持不变。"""
    pack, scenario = candidate()
    before = copy.deepcopy(pack)
    result = validate_pack(pack)
    assert result.ok, [(row.code, row.path) for row in result.issues]
    assert len(scenario['player_appearance_choices']) == 192
    assert pack == before


def _assert_invalid_or_old_policy_rejects_composition_without_mutation(mutation):
    """未覆盖槽、旧政策、非成人或历史身份始终拒绝，不能以候选数合格蒙混。"""
    pack, scenario = candidate()
    choices = scenario['player_appearance_choices']
    base = choices[0]['base_features']
    if mutation == 'old_policy': scenario['dynamic_world']['founder_policy'] = 'fixed_actor_appearance_v1'
    elif mutation == 'fixed_policy': scenario['dynamic_world']['founder_policy'] = 'fixed_actor'
    elif mutation == 'missing': choices.pop()
    elif mutation == 'duplicate': choices[-1] = copy.deepcopy(choices[0])
    elif mutation == 'unknown_face': base['face_shape'] = 'unmade_face'
    elif mutation == 'unknown_eye': base['eyes'] = 'unmade_eye'
    elif mutation == 'extra_slot': base['equipment'] = 'sword'
    elif mutation == 'hair': base['hair'] = 'headwrap'
    elif mutation == 'beard': base['beard'] = 'short'
    elif mutation == 'skin': base['skin_palette'] = 'tanned'
    elif mutation == 'build': base['build'] = 'strong'
    elif mutation == 'attire': choices[0]['attire_style'] = 'traveler'
    elif mutation == 'age':
        next(row for row in scenario['initial']['actors'] if row['actor_id'] == scenario['player_actor'])['age_years'] = 60
    else:
        next(row for row in pack['actors'] if row['id'] == scenario['player_actor'])['origin'] = 'historical'
    before = copy.deepcopy(pack)
    assert not validate_pack(pack).ok
    assert pack == before


def _assert_legacy_twelve_policy_stays_valid_and_never_accepts_new_count():
    """原1.6十二代表原件继续可读，不把新策略数量规则覆盖旧规则。"""
    old = json.loads((ROOT / 'content/packs/authored/local_society_1_6/content.json').read_text())
    assert validate_pack(old).ok
    pack, scenario = candidate()
    scenario['dynamic_world']['founder_policy'] = 'fixed_actor_appearance_v1'
    index = {'actors': {row['id']: row for row in pack['actors']}}
    assert 'APPEARANCE_CHOICES_REQUIRED' in {row[0] for row in check_appearance_choices(pack, index)}


class FormalAppearanceCompositionTests(unittest.TestCase):
    """使用标准库运行真实作者合同检查，不依赖尚未取得的额外测试运行器。"""

    def test_real_author_composition(self):
        """成功入口保持全部作者字节语义。"""
        _assert_real_author_composition_is_valid_without_changing_input()

    def test_rejected_domains(self):
        """逐一核对十四类拒绝及输入原子性，保留每个子用例的真实诊断。"""
        for mutation in ['old_policy', 'fixed_policy', 'missing', 'duplicate', 'unknown_face',
                         'unknown_eye', 'extra_slot', 'hair', 'beard', 'skin', 'build',
                         'attire', 'age', 'historical']:
            with self.subTest(mutation=mutation):
                _assert_invalid_or_old_policy_rejects_composition_without_mutation(mutation)

    def test_legacy_twelve_policy(self):
        """独立核旧包原样准入与旧政策拒绝新候选数。"""
        _assert_legacy_twelve_policy_stays_valid_and_never_accepts_new_count()
