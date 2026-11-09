"""个人战后继可选字段的封闭作者合同；不重复战斗数值、AI或随机矩阵。"""
from copy import deepcopy
import json
from pathlib import Path
import pytest
from tools.content.validation import SCHEMA, StrictValidator
from tools.content.personal_links import check_personal

ROOT = Path(__file__).resolve().parents[2]


def skill(kind='strike'):
    """使用完整原技能结构，后继字段只能在正确技能类别显式启用。"""
    return dict(id='skill.sample', name_key='title', kind=kind, weapons=['spear'], stamina_cost=10, power_bp=10000, defense_bp=5000, move_bonus=0, status='none', status_rounds=0, practice_ticks=1, description_key='title')


def valid(kind, value):
    """共享StrictValidator与同一Python语义口核结构和类别，不执行作者自定义逻辑。"""
    schema = StrictValidator({'$defs': SCHEMA['$defs'], '$ref': '#/$defs/' + kind})
    return schema.is_valid(value) and not check_personal({'personal_skills' if kind == 'personal_skill' else 'personal_objectives': [value]}, {})


def test_closed_strike_extensions():
    """原地、射程及有限区域各自独立，完整合法组合可以进入正式作者定义。"""
    row = skill(); row.update(requires_stationary=True, range_modifier={'maximum_bonus': 80}, area={'shape': 'line', 'half_width': 30, 'max_targets': 2})
    assert valid('personal_skill', row)
    row['area'] = {'shape': 'sector', 'arc_degrees': 90, 'max_targets': 3}
    assert valid('personal_skill', row)
    for field, value in [('requires_stationary', False), ('range_modifier', {'maximum_bonus': 0}), ('area', {'shape': 'line', 'half_width': 30, 'max_targets': 3})]:
        bad = deepcopy(row); bad[field] = value
        assert not valid('personal_skill', bad)


@pytest.mark.parametrize('field,value,allowed', [('requires_stationary',True,'strike'),('range_modifier',{'maximum_bonus':1},'strike'),('guard_facing','front','guard'),('defend_recovery_bonus',5,'heart')])
def test_skill_category(field, value, allowed):
    """每一可选能力都有确定类别，不因形状正确而向所有技能开放。"""
    for kind in ['strike', 'guard', 'heart']:
        row = skill(kind); row[field] = value
        assert valid('personal_skill', row) == (kind == allowed)


def test_area_requires_spear_only():
    """首片区域只长兵，混合武器或未知脚本都拒绝。"""
    row = skill(); row['area'] = {'shape': 'line', 'half_width': 1, 'max_targets': 2}
    for weapons in [['sword'], ['spear','sword']]:
        row['weapons'] = weapons
        assert not valid('personal_skill', row)
    row['weapons'] = ['spear']; row['area']['script'] = 'anything'
    assert not valid('personal_skill', row)


def test_objective_models_and_initial_stamina():
    """默认旧字段仍可用；显式后继模型和初值封闭且不接受小数。"""
    row = dict(id='objective.sample',name_key='title',kind='escort',max_rounds=4,injury_on_defeat=1,allow_withdrawal=True)
    assert valid('personal_objective', row)
    row.update(stamina_model='round_v1',escort_model='exit_v1',ai_model='scored_v1',initial_stamina=0)
    assert valid('personal_objective', row)
    for key,value in [('kind','sparring'),('initial_stamina',101),('initial_stamina',50.0),('ai_model','custom'),('stamina_model','custom')]:
        bad=deepcopy(row);bad[key]=value
        assert not valid('personal_objective', bad)
