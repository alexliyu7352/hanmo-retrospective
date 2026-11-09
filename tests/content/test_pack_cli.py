"""从实际CLI入口校验显式包组合与受信旧归档，不跑全量发行或复制完整美术。"""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
ARCHIVE = ROOT / 'content/packs/legacy/archive'


def run_cli(*arguments):
    """启动真实命令行进程，使用当前固定工具链解释器。"""
    return subprocess.run([sys.executable, str(ROOT / 'tools/content_pipeline.py'), *map(str, arguments)],
                          cwd=ROOT, capture_output=True, text=True, timeout=30)


def inputs(tmp_path):
    """复制两张tiny PNG包并写显式选择文件，避免全套美术导出。"""
    root = tmp_path / 'author'
    shutil.copytree(ROOT / 'tests/fixtures/content_packs/compose/new_packs', root)
    selection = tmp_path / 'selection.json'
    selection.write_text(json.dumps({'format': 'HM-PACK-SELECTION', 'format_version': 1,
                                    'packs': [{'id': 'base', 'version': '1.0.0'}, {'id': 'addon', 'version': '1.0.0'}]}), encoding='utf8')
    return root, selection


def generation_bytes(output):
    """逐文件比较原代实际字节，不能只检查CLI报告成功与否。"""
    return {path.relative_to(output).as_posix(): path.read_bytes() for path in output.rglob('*') if path.is_file()}


def test_cli_compose_accepts_repeated_pack_roots(tmp_path):
    """重复--packs走同一冻结组合API，真实产物与报告锁摘要一致。"""
    root, selection = inputs(tmp_path)
    output = tmp_path / 'generation'
    result = run_cli('compose', '--packs', root / 'addon', '--packs', root / 'base', '--selection', selection, '--output', output)
    assert result.returncode == 0, result.stdout + result.stderr
    report = json.loads(result.stdout)
    assert report['status'] == 'STRUCTURE_VALID' and report['runtime_verified'] is False
    catalog = json.loads((output / 'catalog.hmcatalog.json').read_bytes())
    assert catalog['pack_set_digest'] == report['pack_set_digest']
    assert 'addon::actor.extension' in catalog['catalog']['actors']
    assert result.stderr == ''


def test_cli_pack_errors_are_stable_json_and_keep_previous_generation(tmp_path):
    """所选包缺失时返回非零及稳定码，不把失败打印成可发行内容或覆盖旧代。"""
    root, selection = inputs(tmp_path)
    output = tmp_path / 'generation'
    initial = run_cli('compose', '--packs', root, '--selection', selection, '--output', output)
    assert initial.returncode == 0, initial.stdout + initial.stderr
    before = generation_bytes(output)
    data = json.loads(selection.read_bytes())
    data['packs'].append({'id': 'missing', 'version': '1.0.0'})
    selection.write_text(json.dumps(data), encoding='utf8')
    failed = run_cli('compose', '--packs', root, '--selection', selection, '--output', output)
    assert failed.returncode == 2
    error = json.loads(failed.stdout)
    assert error['status'] == 'INVALID_PACK_SET'
    assert error['code'] == 'MissingPack'
    assert error['runtime_verified'] is False
    assert generation_bytes(output) == before


@pytest.mark.parametrize('name', ['procurement', 'two_stories', 'lived_city', 'alpha_life', 'journey_life'])
def test_production_archive_matches_verified_exact_legacy_bytes(name):
    """正式受信归档与已核原件及其source/Schema摘要逐字匹配，不重新prepare。"""
    manifest = json.loads((ARCHIVE / 'manifest.json').read_bytes())
    row = manifest['records'][name]
    catalog_raw = (ARCHIVE / (name + '.hmcatalog.json')).read_bytes()
    source_raw = (ARCHIVE / (name + '.source.json')).read_bytes()
    schema_raw = (ARCHIVE / (row['schema_sha256'] + '.schema.json')).read_bytes()
    assert hashlib.sha256(catalog_raw).hexdigest() == row['catalog_file_sha256']
    assert hashlib.sha256(source_raw).hexdigest() == row['source_sha256']
    assert hashlib.sha256(schema_raw).hexdigest() == row['schema_sha256']
    header = json.loads(catalog_raw)
    assert header['content_id'] == row['content_id']
    assert header['content_version'] == row['content_version']
    assert header['catalog_sha256'] == row['catalog_sha256']
    assert 'pack_set_digest' not in header
    expected = ROOT / 'tests/fixtures/content_packs/compose/legacy_baseline' / (name + '.hmcatalog.json')
    assert catalog_raw == expected.read_bytes()


@pytest.mark.parametrize('raw', [b'{"format":"HM-PACK-SELECTION","format":"other"}', b'[]', b'{"value":NaN}'])
def test_cli_selection_file_parse_failures_are_json_diagnostics(tmp_path, raw):
    """坏选择文件在入口严格读取阶段失败，不打印堆栈或创建输出。"""
    root, selection = inputs(tmp_path)
    selection.write_bytes(raw)
    output = tmp_path / 'generation'
    failed = run_cli('compose', '--packs', root, '--selection', selection, '--output', output)
    assert failed.returncode == 2
    assert json.loads(failed.stdout)['code'] == 'InvalidPackSelectionFile'
    assert failed.stderr == '' and not output.exists()


def test_cli_output_cannot_overwrite_authored_pack(tmp_path):
    """CLI不得把--output误指向作者包，失败后原字节保持。"""
    root, selection = inputs(tmp_path)
    before = generation_bytes(root)
    failed = run_cli('compose', '--packs', root, '--selection', selection, '--output', root / 'base')
    assert failed.returncode == 2
    assert json.loads(failed.stdout)['code'] == 'ProtectedPackOutput'
    assert generation_bytes(root) == before


def test_cli_forwards_source_changed_without_rewriting_previous_output(tmp_path, monkeypatch, capsys):
    """底层组合的SourceChanged稳定传到CLI，不重新prepare或降级尝试其他包。"""
    from tools.content import cli
    from tools.content.pack_errors import PackError
    root, selection = inputs(tmp_path)
    output = tmp_path / 'generation'
    output.mkdir()
    (output / 'old.txt').write_bytes(b'previous generation')
    def changed(*_args):
        """模拟已由compose真实文件用例覆盖的发布前冲突，只测试CLI错误桥接。"""
        raise PackError('SourceChanged', '作者源已变更', {'source': 'content.json'})
    monkeypatch.setattr(cli, 'compose_pack_set', changed)
    status = cli.main(['compose', '--packs', str(root), '--selection', str(selection), '--output', str(output)])
    assert status == 2
    assert json.loads(capsys.readouterr().out)['code'] == 'SourceChanged'
    assert (output / 'old.txt').read_bytes() == b'previous generation'


def test_current_alpha_legacy_cli_refuses_borrowing_old_identity(tmp_path):
    """公开CLI对当前Alpha与旧原件不符给明确诊断，无任何新代产物。"""
    output = tmp_path / 'legacy-alpha'
    result = run_cli('compose', '--packs', ROOT / 'content/packs/legacy/alpha_life',
                     '--selection', ROOT / 'content/sets/legacy-alpha_life.json', '--output', output)
    assert result.returncode == 2, result.stdout + result.stderr
    error = json.loads(result.stdout)
    assert error['code'] == 'LegacySourceChanged'
    assert error['details']['expected_source_sha256'] == 'eb0a57e1a439b21513300b474d27519ada723b34cadd4d18cbae3bfd052a7296'
    assert not output.exists()


def test_current_alpha_prepare_gets_a_distinct_directory_digest_without_source_edit(tmp_path):
    """旧prepare合法生成当前源的新目录，版本不擅改且绝不覆盖旧Alpha归档。"""
    source = ROOT / 'content/foundation/alpha_life.json'
    source_raw = source.read_bytes()
    source_digest = hashlib.sha256(source_raw).hexdigest()
    old_raw = (ARCHIVE / 'alpha_life.hmcatalog.json').read_bytes()
    old_header = json.loads(old_raw)
    output = tmp_path / 'current-alpha' / source_digest / 'alpha_life.hmcatalog.json'
    result = run_cli('prepare', source, '--output', output)
    assert result.returncode == 0, result.stdout + result.stderr
    report = json.loads(result.stdout)
    current = json.loads(output.read_bytes())
    assert report['source_sha256'] == source_digest
    assert current['content_id'] == old_header['content_id'] == 'foundation.alpha_life'
    assert current['content_version'] == old_header['content_version'] == '0.7.0'
    assert current['catalog_sha256'] != old_header['catalog_sha256']
    assert current['catalog_sha256'] == '36388b3cd4b4cfd7cf19c7a154b53f5be8fcc1ef761919ff61540cc2126b2b15'
    assert current['source_sha256'] != old_header['source_sha256']
    assert source.read_bytes() == source_raw
    assert (ARCHIVE / 'alpha_life.hmcatalog.json').read_bytes() == old_raw
    assert current['runtime_verified'] is False


def test_selection_file_is_frozen_when_cli_reads_it(tmp_path, monkeypatch, capsys):
    """启动后选择文件变化不会混入已开始的组合，也不在发布后伪报失败。"""
    from tools.content import cli
    root, selection = inputs(tmp_path)
    compose = cli.compose_pack_set
    def edit_selection_after_read(roots, selected, output):
        """真实组合仍执行，仅模拟CLI读取选择值之后的外部编辑。"""
        replacement = {**selected, 'packs': [{'id': 'not-selected-at-start', 'version': '1.0.0'}]}
        selection.write_text(json.dumps(replacement), encoding='utf8')
        return compose(roots, selected, output)
    monkeypatch.setattr(cli, 'compose_pack_set', edit_selection_after_read)
    output = tmp_path / 'generation'
    status = cli.main(['compose', '--packs', str(root), '--selection', str(selection), '--output', str(output)])
    assert status == 0
    assert json.loads(capsys.readouterr().out)['status'] == 'STRUCTURE_VALID'
    lock = json.loads((output / 'pack-set.lock.json').read_bytes())
    assert [pack['id'] for pack in lock['packs']] == ['base', 'addon']
    assert json.loads(selection.read_bytes())['packs'][0]['id'] == 'not-selected-at-start'


@pytest.mark.parametrize('target', ['catalog', 'schema'])
def test_trusted_archive_still_rejects_tampered_catalog_or_schema(tmp_path, monkeypatch, target):
    """受信位置不替代摘要回核；复制后改任一原件都不能获取legacy sidecar。"""
    from tools.content import pack_compose
    archive = tmp_path / 'archive'
    shutil.copytree(ARCHIVE, archive)
    monkeypatch.setattr(pack_compose, 'LEGACY_BASELINE_ROOT', archive)
    row = json.loads((archive / 'manifest.json').read_bytes())['records']['procurement']
    filename = 'procurement.hmcatalog.json' if target == 'catalog' else row['schema_sha256'] + '.schema.json'
    path = archive / filename
    path.write_bytes(path.read_bytes() + b' ')
    logical = 'content/foundation/procurement.json'
    pack = {'manifest': {'legacy_source': 'procurement', 'id': row['content_id'], 'version': row['content_version']},
            'content_paths': [logical], 'files': {logical: (archive / 'procurement.source.json').read_bytes()}}
    from tools.content.pack_errors import PackError
    with pytest.raises(PackError) as caught:
        pack_compose._legacy_header(pack, {})
    assert caught.value.code == 'InvalidLegacyArtifact'
