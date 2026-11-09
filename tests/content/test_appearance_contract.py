"""固定与候选外貌使用同一作者Schema，语义不冒充美术或人物引用。"""
import copy
import json
from pathlib import Path
import pytest
from tools.content.validation import validate_pack
from tools.content.references import iter_references

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = json.loads((ROOT / 'tests/fixtures/appearance/contract.json').read_text())


def pack_with(kind):
    """在实际合法作者包的独立副本添加新外貌，不改生产人物或资源。"""
    pack = json.loads((ROOT / 'content/foundation/lived_city.json').read_text())
    pack['actors'][0]['appearance_definition'] = copy.deepcopy(FIXTURE[kind])
    return pack


@pytest.mark.parametrize('kind', ['fixed', 'generated'])
def test_appearance_definition_uses_semantic_values(kind):
    """两种声明都能通过，所有选项仍是语义而非表现资源引用。"""
    pack = pack_with(kind)
    before = copy.deepcopy(pack)
    assert validate_pack(pack).ok
    assert pack == before
    assert not any('appearance_definition' in path for path, _, _ in iter_references(pack))


@pytest.mark.parametrize('mutation', ['missing', 'unknown', 'path', 'empty', 'duplicate', 'future', 'extra', 'inheritance', 'numeric', 'fixed_candidates'])
def test_invalid_appearance_has_author_path(mutation):
    """缺槽、脚本路径、未知版本与混合来源在准备前拒绝并保留定位。"""
    pack = pack_with('generated')
    definition = pack['actors'][0]['appearance_definition']
    if mutation == 'missing': del definition['candidates']['eyes']
    elif mutation == 'unknown': definition['candidates']['power'] = ['strong']
    elif mutation == 'path': definition['candidates']['eyes'] = ['res://face.png']
    elif mutation == 'empty': definition['candidates']['eyes'] = []
    elif mutation == 'duplicate': definition['candidates']['eyes'] = ['plain', 'plain']
    elif mutation == 'future': definition['generator_version'] = 'appearance-v2'
    elif mutation == 'extra': definition['script'] = 'res://generate.gd'
    elif mutation == 'inheritance': definition['inheritance_features'] = ['ability']
    elif mutation == 'numeric': definition['candidates']['build'] = [1.5]
    else: definition['kind'] = 'fixed'
    result = validate_pack(pack)
    assert not result.ok
    assert any('/actors/0/appearance_definition' in issue.path for issue in result.issues)


def test_historical_actor_fixed_or_explicit_legacy_only():
    """旧历史作者可缺新字段，但声明后不能生成随机历史脸。"""
    pack = pack_with('generated'); pack['actors'][0]['origin'] = 'historical'
    assert 'APPEARANCE_HISTORICAL_FIXED' in {issue.code for issue in validate_pack(pack).issues}
    pack['actors'][0]['appearance_definition'] = copy.deepcopy(FIXTURE['fixed'])
    assert validate_pack(pack).ok
    del pack['actors'][0]['appearance_definition']
    assert validate_pack(pack).ok


def test_v1_fixture_records_exact_sha_selection():
    """参考向量固定编码与取样边界；此测试不是Python游戏实现。"""
    import hashlib
    for feature, values in FIXTURE['generated']['candidates'].items():
        parts = ['appearance-v1', FIXTURE['seed'], FIXTURE['actor_id'], feature]
        digest = hashlib.sha256(json.dumps(parts, ensure_ascii=False, separators=(',', ':')).encode()).hexdigest()
        assert sorted(values)[int(digest[:8], 16) % len(values)] == FIXTURE['expected_features'][feature]


@pytest.mark.parametrize('case', FIXTURE['mutations'], ids=lambda case: case['name'])
def test_shared_definition_mutations(case):
    """Python与Godot执行同一坏候选样本，避免Schema与运行纯合同分叉。"""
    pack = pack_with('generated')
    node = pack['actors'][0]['appearance_definition']
    for part in case['path'][:-1]: node = node[part]
    node[case['path'][-1]] = case['value']
    assert not validate_pack(pack).ok
