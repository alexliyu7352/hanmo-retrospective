"""内容发布必须消费当前清单并核对包内字节；伪引擎仅测试工具接线，不宣称真实导出。"""
import json
from pathlib import Path
import shutil

import pytest

from tests.test_content_staging import staged, extra_source, MANIFEST
from tests.test_portable_linux_release import release_tool
from tests.test_web_release import font_fixture


def release_fixture(tmp_path, monkeypatch):
    """建立真实staging和测试字体，仅在引擎、Git与验证回执边界使用夹具。"""
    tool, generated = staged(tmp_path, tiny_resources=True)
    font_fixture(tmp_path)
    release = release_tool()
    shutil.copytree(release.ROOT / 'docs/release', tmp_path / 'docs/release')
    shutil.copytree(release.ROOT / 'web', tmp_path / 'web')
    monkeypatch.setattr(release, 'ROOT', tmp_path)
    monkeypatch.setattr(release.build, 'GAME', tmp_path / 'game')
    monkeypatch.setattr(release, 'FOLDER', tmp_path / 'output/linux')
    monkeypatch.setattr(release, 'LOGS', tmp_path / 'logs')
    monkeypatch.setattr(release.subprocess, 'check_output', lambda *a, **k: '')
    receipt = {'format': 'HM-VERIFY/1', 'profile': 'linux', 'engine': 'fixture-only',
               'receipt': str(tmp_path / 'receipt/report.json')}
    monkeypatch.setattr(release.verification, 'require_receipt', lambda *a: receipt)
    monkeypatch.setattr(release.verification, 'record_build_artifacts', lambda *a: receipt)
    commands = []

    def engine(command, **kwargs):
        """创建明确假ELF/Web标头及probe输出；这些文件不是Godot运行证明。"""
        commands.append(command)
        if '--export-release' in command:
            target = Path(command[-1])
            if target.suffix == '.html':
                for ext, data in [('.html', b'fake html'), ('.js', b'fake js'), ('.wasm', b'\0asmfixture'), ('.pck', b'fake pck')]:
                    target.with_suffix(ext).write_bytes(data)
            else:
                target.write_bytes(b'\x7fELFfixture-only')
            return ''
        if '--' in command:
            from tools.content_stage import package_files
            manifest = json.loads((generated / MANIFEST).read_text())
            Path(command[-1]).write_text(json.dumps({'format': 'HM-CONTENT-PACKAGE/1', 'files': package_files(tmp_path, manifest)}))
            return ''
        return 'EXPORT_FONT_CHECKS=1 FAILURES=0\n'

    monkeypatch.setattr(release.build, 'run_checked', engine)
    return release, receipt, commands, tool, generated


@pytest.mark.parametrize('platform', ['native', 'web'])
@pytest.mark.parametrize('damage', ['changed', 'missing', 'extra', 'source', 'schema', 'capability'])
def test_exports_reject_unverified_content_before_engine(tmp_path, monkeypatch, platform, damage):
    """两条导出入口都必须在启动引擎前拒绝清单/源/合同漂移或多余资源。"""
    release, receipt, commands, _, generated = release_fixture(tmp_path, monkeypatch)
    target = generated / 'two_stories.hmcatalog.json'
    if damage == 'missing': target.unlink()
    elif damage == 'extra': (generated / 'unknown.json').write_bytes(b'{}')
    else:
        target = {'changed': target, 'source': tmp_path / 'content/foundation/two_stories.json',
                  'schema': tmp_path / 'schemas/content.schema.json',
                  'capability': tmp_path / 'game/data/rule_capabilities.json'}[damage]
        target.write_bytes(target.read_bytes() + b'\n')
    with pytest.raises(RuntimeError, match='[Cc]ontent|content'):
        if platform == 'native': release.export_native('fake', receipt)
        else: release.export_web('fake', {'engine': 'fixture-only'}, tmp_path / 'output/web', internal=True)
    assert commands == []


@pytest.mark.parametrize('platform', ['native', 'web'])
def test_release_records_current_content_and_verified_package_membership(tmp_path, monkeypatch, platform):
    """删除测试包后清单、BUILD来源列表和受检包集合均不得保留已删除路径。"""
    release, receipt, commands, tool, generated = release_fixture(tmp_path, monkeypatch)
    source = extra_source(tmp_path); tool.stage_content(tmp_path); source.unlink(); tool.stage_content(tmp_path)
    if platform == 'native': output = release.export_native('fake', receipt)
    else: output = release.export_web('fake', {'engine': 'fixture-only'}, tmp_path / 'output/web', internal=True)
    manifest = json.loads((output.parent / 'BUILD.json').read_text())
    assert 'content' in manifest, '发行缺少staging来源及包内成员证明'
    assert manifest['content']['stage']['format'] == 'HM-CONTENT-STAGE/1'
    assert manifest['content']['package']['format'] == 'HM-CONTENT-PACKAGE/1'
    names = {r['path'] for r in manifest['content']['package']['files']}
    assert 'res://data/generated/removed.hmcatalog.json' not in names
    assert 'res://data/generated/' + MANIFEST in names
    assert any('--' in command for command in commands), '必须检查实际包而非复制staging声明'


@pytest.mark.parametrize('damage', ['extra', 'missing', 'changed'])
def test_package_probe_mismatch_prevents_success_manifest(tmp_path, monkeypatch, damage):
    """导出后包内额外/丢失/篡改资源必须失败，不能只核验导出前的目录。"""
    release, receipt, _, _, _ = release_fixture(tmp_path, monkeypatch)
    original = release.build.run_checked

    def bad_probe(command, **kwargs):
        """保留真实发布接线，仅修改导出工具返回的包内观察记录。"""
        result = original(command, **kwargs)
        if '--' in command:
            path = Path(command[-1]); report = json.loads(path.read_text())
            if damage == 'extra': report['files'].append({'path': 'res://data/generated/deleted.hmcatalog.json', 'sha256': '0' * 64})
            elif damage == 'missing': report['files'].pop()
            else: report['files'][0]['sha256'] = '0' * 64
            path.write_text(json.dumps(report))
        return result

    monkeypatch.setattr(release.build, 'run_checked', bad_probe)
    with pytest.raises(RuntimeError, match='[Pp]ackage|[Cc]ontent|包'):
        release.export_native('fake', receipt)
    assert not (release.FOLDER / 'BUILD.json').exists()
    assert not list(release.FOLDER.parent.glob('*.zip'))


def test_internal_exports_use_fresh_package_evidence_per_output(tmp_path, monkeypatch):
    """两个新内部输出即使共享日志根也各自读取新包，不能撞上或复用上次证据。"""
    release, _, commands, _, _ = release_fixture(tmp_path, monkeypatch)
    for name in ('first', 'second'):
        result = release.export_web('fake', {'engine': 'fixture-only'}, tmp_path / 'output' / name, internal=True)
        assert (result.parent / 'BUILD.json').is_file()
    probes = [command for command in commands if '--' in command]
    assert len(probes) == 2 and probes[0][-1] != probes[1][-1]
