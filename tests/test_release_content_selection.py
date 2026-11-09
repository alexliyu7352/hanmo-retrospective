"""显式发行选择复用真实组合和staging；工具边界夹具不冒称引擎或发行验收。"""
import json
import shutil
from pathlib import Path

import pytest

from tests.test_installed_content_staging import generation, stage_root, tree_bytes
from tests.test_portable_linux_release import release_tool


def selected_project(tmp_path, monkeypatch):
    """仅对tiny测试源关闭生产六原件绑定，实际组合/安装/验证仍走原入口。"""
    root = stage_root(tmp_path / 'project')
    release = release_tool()
    monkeypatch.setattr(release.build, 'GAME', root / 'game')
    real_stage = release.build.stage_content

    def stage(root, *, content_sets=(), expected_content_sets=()):
        """夹具没有历史31张原图，显式使用已支持的无foundation测试模式。"""
        return real_stage(root, content_sets=content_sets, foundation_sets=False,
                          expected_content_sets=expected_content_sets)

    monkeypatch.setattr(release.build, 'stage_content', stage)
    return release, root


def test_explicit_selection_keeps_exact_members_and_other_registered_versions(tmp_path, monkeypatch):
    """重复参数稳定去重，旧锁继续列入完整发行清单而不被隐式卸载。"""
    release, root = selected_project(tmp_path, monkeypatch)
    first, first_id = generation(tmp_path / 'first')
    second, second_id = generation(tmp_path / 'second', '2.0.0')
    originals = tree_bytes(first), tree_bytes(second)
    release.stage_release_content([first])
    expected = release.stage_release_content([second, second])
    assert [row['pack_set_digest'] for row in expected] == [second_id]
    content = release.collect_content(expected)
    assert content['selected_content_sets'] == expected
    paths = {row['path'] for row in content['expected_files']}
    for identity in (first_id, second_id):
        assert f'res://data/generated/content-sets/{identity}/pack-set.lock.json' in paths
    assert (tree_bytes(first), tree_bytes(second)) == originals
    assert str(tmp_path) not in json.dumps(expected)
    before = tree_bytes(root / 'game/data/generated')
    with pytest.raises(RuntimeError, match='Selected content set'):
        release.collect_content([{**expected[0], 'files': []}])
    assert tree_bytes(root / 'game/data/generated') == before


def test_changed_compose_is_rejected_without_changing_previous_stage(tmp_path, monkeypatch):
    """未核资源原代不得成为显式选择，失败保留已发布完整字节。"""
    release, root = selected_project(tmp_path, monkeypatch)
    source, _ = generation(tmp_path / 'first')
    release.stage_release_content([source])
    before = tree_bytes(root / 'game/data/generated')
    target = source / 'presentation.json'
    target.write_bytes(target.read_bytes() + b' ')
    with pytest.raises((ValueError, RuntimeError)):
        release.stage_release_content([source])
    assert tree_bytes(root / 'game/data/generated') == before


def test_valid_other_lock_cannot_substitute_the_explicit_choice(tmp_path, monkeypatch):
    """另一完整合法组合代也不能替代本次用户选择的准确锁。"""
    release, _ = selected_project(tmp_path, monkeypatch)
    first, _ = generation(tmp_path / 'first')
    other, _ = generation(tmp_path / 'other', '2.0.0')
    release.stage_release_content([first])
    expected = [release.content_stage.composed_set_record(other)]
    with pytest.raises(RuntimeError, match='Selected content set'):
        release.collect_content(expected)


@pytest.mark.parametrize('platform', ['linux', 'web'])
def test_release_cli_passes_frozen_selection_to_export(tmp_path, monkeypatch, platform):
    """真实选择与安装执行，长验证/引擎仅记录调用，不偷跑集中验收。"""
    release, _ = selected_project(tmp_path, monkeypatch)
    source, identity = generation(tmp_path / 'first')
    calls = []
    monkeypatch.setattr(release.build, 'find_engine', lambda: 'fixture-engine')
    monkeypatch.setattr(release.build, 'require_font', lambda: None)

    def suite(engine, profile):
        """只验证调用顺序与已安装选择，不能产生真实门禁PASS。"""
        calls.append(('suite', profile))
        assert release.collect_content()['stage']
        return {'fixture_only': True}

    def export(engine, report, *args, **kwargs):
        """记录实际导出入口收到的同一锁和成员，不创建可执行。"""
        expected = kwargs['expected_sets']
        assert [row['pack_set_digest'] for row in expected] == [identity]
        assert release.collect_content(expected)['selected_content_sets'] == expected
        calls.append(('export', platform))
        return tmp_path / 'not-exported'

    monkeypatch.setattr(release, 'run_suite', suite)
    monkeypatch.setattr(release, 'export_native', export)
    monkeypatch.setattr(release, 'export_web', export)
    assert release.main(['export', '--platform', platform, '--content-set', str(source)]) == 0
    assert calls == [('suite', platform), ('export', platform)]


def test_package_cannot_reselect_or_stage_content(tmp_path, monkeypatch):
    """打包只消费已核BUILD，参数错误在任何引擎或staging调用前退出。"""
    release = release_tool()
    calls = []
    monkeypatch.setattr(release.build, 'find_engine', lambda: calls.append('engine'))
    with pytest.raises(SystemExit) as exc:
        release.main(['package', '--content-set', str(tmp_path / 'absent')])
    assert exc.value.code == 2 and calls == []


def test_selection_change_before_stage_publish_preserves_previous_generation(tmp_path, monkeypatch):
    """冻结选择后路径被换成另一合法代，也必须在staging替换旧代之前失败。"""
    release, root = selected_project(tmp_path, monkeypatch)
    source, _ = generation(tmp_path / 'first')
    other, _ = generation(tmp_path / 'other', '2.0.0')
    release.stage_release_content([source])
    before = tree_bytes(root / 'game/data/generated')
    original = release.build.stage_content

    def replaced(root, **kwargs):
        """只在自有临时夹具中复现选择冻结与正式stage之间的真实路径替换。"""
        source.rename(source.with_name('original-retained'))
        shutil.copytree(other, source)
        return original(root, **kwargs)

    monkeypatch.setattr(release.build, 'stage_content', replaced)
    with pytest.raises(RuntimeError, match='Selected content set changed'):
        release.stage_release_content([source])
    assert tree_bytes(root / 'game/data/generated') == before
