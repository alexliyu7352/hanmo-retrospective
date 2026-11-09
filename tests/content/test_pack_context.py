"""验证显式作者依赖的只读身份、正式多包组合与事务边界；GUI验收后置。"""
from copy import deepcopy
import json
from pathlib import Path
import shutil

import pytest

from tests.content.test_pack_compose import ROOT, read_generation, manifest, write_pack, empty_source
from tests.content.test_pack_editor_protocol import call
from tests.content.test_editor_protocol import worker
from tests.content.test_scenario_authoring import author_fixture
from tools.content.document import DocumentError
from tools.content.io import canonical
from tools.content.pack_document import PackDocument
from tools.content.pack_errors import PackError
from tools.content import pack_compose


def pair(tmp_path):
    """仅复制既有两张tinyPNG正式包，不读或改原工坊作者。"""
    root = ROOT / 'tests/fixtures/content_packs/compose/new_packs'
    for name in ('base', 'addon'):
        shutil.copytree(root / name, tmp_path / name)
    return PackDocument.open_pack(tmp_path / 'addon/pack.json'), tmp_path / 'addon', tmp_path / 'base'


def select(document, base):
    """每次由调用方明确给出依赖根及精确版本，不枚举邻目录。"""
    return document.select_dependencies([str(base)], [{'id': 'base', 'version': '1.0.0'}], document.status()['revision'])


def test_reference_context_is_detached_and_dependency_never_writable(tmp_path):
    """返回带来源的限定ID及副本，依赖没有可写AuthorDocument入口。"""
    document, primary, base = pair(tmp_path)
    before = read_generation(base)
    result = select(document, base)
    assert result['revision'] == 1 and not document.status()['dirty']
    assert result['dependencies'][0]['id'] == 'base'
    context = document.reference_context('actors')
    row = next(row for row in context['records'] if row['id'] == 'base::actor.player')
    assert row['read_only'] and row['selectable'] and row['entry'] == 'content.json'
    row['record']['attributes'].clear()
    assert document.reference_context('actors')['records'] != context['records']
    with pytest.raises(DocumentError) as caught:
        document.edit('@dependency/base/1.0.0/content.json', 'set_text', {'key': 'bad', 'value': 'write'}, 1)
    assert caught.value.code == 'UNKNOWN_ENTRY'
    document.edit('content.json', 'set_text', {'key': 'addon.note', 'value': '仅主包'}, 1)
    document.save(2)
    assert read_generation(base) == before and 'addon.note' in json.loads((primary / 'content.json').read_text())['texts']


def test_formal_compose_matches_explicit_multi_pack_cli_input(tmp_path):
    """文档组合与正式compiler多根选择产物逐字一致，锁按实际包逐项绑定。"""
    document, primary, base = pair(tmp_path)
    select(document, base)
    output, direct = tmp_path / 'composed', tmp_path / 'direct'
    result = document.compose(output, 1)
    selection = {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                 'packs': [{'id': 'addon', 'version': '1.0.0'}, {'id': 'base', 'version': '1.0.0'}]}
    pack_compose.compose_pack_set([primary, base], selection, direct)
    assert read_generation(output) == read_generation(direct)
    lock = json.loads((output / 'pack-set.lock.json').read_text())
    assert [row['id'] for row in lock['packs']] == ['base', 'addon']
    assert result['runtime_verified'] is False


def test_missing_wrong_version_and_failed_selection_preserve_session(tmp_path):
    """缺依赖不自动寻找邻目录，错版切换保留已选择上下文和版本。"""
    document, _primary, base = pair(tmp_path)
    with pytest.raises(PackError) as caught:
        document.compose(tmp_path / 'missing', 0)
    assert caught.value.code == 'MissingPackDependency'
    select(document, base)
    before = document.status()
    with pytest.raises(PackError):
        document.select_dependencies([base], [{'id': 'base', 'version': '2.0.0'}], 1)
    assert document.status() == before
    with pytest.raises(DocumentError) as caught:
        document.select_dependencies([tmp_path], [{'id': 'base', 'version': '1.0.0'}], 1)
    assert caught.value.code == 'INVALID_DEPENDENCY_ROOT'
    assert document.status() == before


@pytest.mark.parametrize('kind', ['save', 'link', 'overlap'])
def test_unsafe_root_and_source_output_are_refused(tmp_path, kind):
    """存档、链接及重叠主包不能充作依赖根，组合输出也不得覆盖依赖。"""
    document, primary, base = pair(tmp_path)
    if kind == 'save':
        target = tmp_path / 'world.save.json'; target.write_text('{}')
    elif kind == 'link':
        target = tmp_path / 'alias'; target.symlink_to(base, target_is_directory=True)
    else:
        target = primary
    with pytest.raises(DocumentError):
        document.select_dependencies([target], [{'id': 'base', 'version': '1.0.0'}], 0)
    assert document.status()['revision'] == 0
    select(document, base)
    before = read_generation(base)
    with pytest.raises(PackError):
        document.compose(base / 'generated', 1)
    assert read_generation(base) == before


@pytest.mark.parametrize('member', ['pack.json', 'content.json', 'assets/art/pixel.png'])
def test_dependency_source_change_rejects_reads_save_and_old_output(tmp_path, member):
    """任一已冻结依赖输入变化后只读查询、保存与组合均关闭失败且旧代保留。"""
    document, _primary, base = pair(tmp_path)
    select(document, base)
    output = tmp_path / 'output'; document.compose(output, 1)
    before = read_generation(output)
    path = base / member; path.write_bytes(path.read_bytes() + b'\n')
    for operation in (lambda: document.reference_context('actors'), lambda: document.save(1), lambda: document.compose(output, 1)):
        with pytest.raises(DocumentError) as caught:
            operation()
        assert caught.value.code == 'SOURCE_CHANGED'
    assert read_generation(output) == before


def test_transient_dependency_change_cannot_bind_a_different_lock(tmp_path, monkeypatch):
    """inner compiler瞬态读改后还原仍须由实际产物锁拒绝，不能只核最终磁盘。"""
    document, _primary, base = pair(tmp_path)
    select(document, base)
    output = tmp_path / 'output'; document.compose(output, 1)
    before = read_generation(output)
    real = pack_compose.compose_pack_set
    def changed(roots, selection, target):
        """只在独立测试原件中注入瞬态不同文本，并在真实组合后还原。"""
        path = base / 'content.json'; original = path.read_bytes()
        value = json.loads(original); value['texts']['office.hello'] += ' 瞬态'
        path.write_bytes(canonical(value))
        try:
            return real(roots, selection, target)
        finally:
            path.write_bytes(original)
    monkeypatch.setattr(pack_compose, 'compose_pack_set', changed)
    with pytest.raises(DocumentError) as caught:
        document.compose(output, 1)
    assert caught.value.code == 'SOURCE_CHANGED' and read_generation(output) == before


def test_cross_pack_initial_definitions_stay_read_only(tmp_path):
    """初态可显式使用依赖人物与资产，生成限定引用但仅修改主包剧本。"""
    source, selection = author_fixture()
    base = write_pack(tmp_path / 'base', manifest(required_capabilities=source['required_capabilities']), source)
    draft = empty_source()
    scenario = deepcopy(source['scenarios'][0])
    for field in ('enabled', 'initial', 'start_date', 'playable_actor_ids'):
        scenario.pop(field)
    scenario.update(id='scenario.authored', player_actor='base::actor.player', name_key='base::name.scenario.west', world_map_id='base::map.west')
    draft['scenarios'] = [scenario]
    requirement = {'id': 'base', 'min_version': '1.0.0', 'max_version_exclusive': '2.0.0'}
    primary = write_pack(tmp_path / 'addon', manifest('addon', dependencies=[requirement]), draft)
    document = PackDocument.open_pack(primary / 'pack.json'); select(document, base)
    for table in selection['enabled']:
        selection['enabled'][table] = ['base::' + identifier for identifier in selection['enabled'][table]]
    selection['playable_actor_ids'] = ['base::' + identifier for identifier in selection['playable_actor_ids']]
    for key in ('actor_options', 'role_options'):
        selection[key] = {'base::' + identifier: value for identifier, value in selection[key].items()}
    before = read_generation(base)
    preview = document.scenario_snapshot_preview('content.json', 'scenario.authored', selection)
    assert preview['can_apply'], preview['issues']
    assert preview['proposal']['initial']['actors'][0]['actor_id'].startswith('base::')
    document.apply_scenario_snapshot(preview['token'], 1); document.save(2)
    assert read_generation(base) == before
    assert document.status()['entries'].keys() == {'content.json'}


def test_duplicate_dependency_definitions_do_not_get_silently_collapsed(tmp_path):
    """依赖源的重复ID不被dict构造吞掉，引用候选必须明确拒绝歧义。"""
    document, _primary, base = pair(tmp_path)
    path = base / 'content.json'; value = json.loads(path.read_text())
    value['actors'].append(deepcopy(value['actors'][0])); path.write_bytes(canonical(value))
    select(document, base)
    with pytest.raises(DocumentError) as caught:
        document.reference_context('actors')
    assert caught.value.code == 'AMBIGUOUS_REFERENCE'


def test_worker_dependency_selection_keeps_three_guards(tmp_path):
    """正式worker传显式根与版本；错token不绑定，会话只暴露主包可写入口。"""
    _document, primary, base = pair(tmp_path)
    with worker(tmp_path) as process:
        assert call(process, 'open_pack', {'path': str(primary / 'pack.json')})['ok']
        args = {'roots': [str(base)], 'selection': [{'id': 'base', 'version': '1.0.0'}]}
        rejected = call(process, 'select_dependencies', args, source_sha256='wrong')
        assert not rejected['ok'] and rejected['error']['code'] == 'SOURCE_CHANGED'
        assert call(process, 'select_dependencies', args)['ok']
        reply = call(process, 'reference_context', {'table': 'places'})
        assert reply['ok'] and any(row['id'] == 'base::west.street' and row['read_only'] for row in reply['result']['records'])
        snapshot = call(process, 'snapshot')['result']
        assert snapshot['documents'].keys() == {'content.json'}


def test_bulk_reference_context_matches_existing_single_table_shape(tmp_path):
    """批量只读入口与单表记录逐项相同，不改变会话或返还内部可写引用。"""
    document, _primary, base = pair(tmp_path); select(document, base)
    before = document.status()
    result = document.reference_context('*')
    single = document.reference_context('actors')
    assert result['tables']['actors'] == single['records'] and single['table'] == 'actors'
    assert document.status() == before and result['runtime_verified'] is False
    result['tables']['actors'].clear()
    assert document.reference_context('*')['tables']['actors'] == single['records']
