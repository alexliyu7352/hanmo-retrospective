"""用真实小归档验证固定工具链恢复；传输替身只覆盖网络或 gh 边界。"""
from __future__ import annotations

from copy import deepcopy
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[1]
BUILD = '4.7.2.stable.official.ed1daf0bf'
BINARY = 'Godot_v4.7.2-stable_linux.x86_64'
TEMPLATES = ['version.txt', 'icudt_godot.dat', 'linux_debug.x86_64',
             'linux_release.x86_64', 'web_debug.zip', 'web_release.zip',
             'web_nothreads_debug.zip', 'web_nothreads_release.zip']


def load_tool():
    """加载实际恢复入口；缺失时断言失败而非导入错误。"""
    path = ROOT / 'tools/restore_toolchain.py'
    assert path.is_file(), '缺少固定工具链恢复入口'
    spec = importlib.util.spec_from_file_location('restore_toolchain_test', path)
    tool = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tool)
    return tool


def digest(data):
    """计算夹具真实字节摘要，不替代生产校验。"""
    return hashlib.sha256(data).hexdigest()


def write_zip(path, members):
    """写入可重现的普通 ZIP 文件，明确保留负向条目的 Unix 类型。"""
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data, mode in members:
            info = zipfile.ZipInfo(name)
            info.create_system = 3
            info.external_attr = mode << 16
            archive.writestr(info, data)



def patch_official_response(tool, monkeypatch, response):
    """仅替换 opener 的实际传输边界；重定向安全由独立真实 handler 链测试验证。"""
    def open_response(self, request, **kwargs):
        """把真实小归档响应交给生产恢复，不更改摘要或安装规则。"""
        return response(request, **kwargs)
    monkeypatch.setattr(tool.urllib.request.OpenerDirector, 'open', open_response)


@pytest.fixture
def fixture(tmp_path, monkeypatch):
    """替换固定清单读取边界，真实恢复仍操作真实文件、ZIP 和校验。"""
    tool = load_tool() if (ROOT / 'tools/restore_toolchain.py').is_file() else None
    archives = tmp_path / 'archives'; archives.mkdir()
    executable = f"#!/bin/sh\nprintf '%s\\n' '{BUILD}'\n".encode()
    editor_members = [(BINARY, executable, stat.S_IFREG | 0o755)]
    template_members = [('templates/' + name,
                         b'4.7.2.stable\n' if name == 'version.txt' else name.encode(),
                         stat.S_IFREG | 0o644) for name in TEMPLATES]
    template_members.append(('templates/android_debug.apk', b'not installed', stat.S_IFREG | 0o644))
    manifest = {'format': 'HANMO-TOOLCHAIN/1', 'version': '4.7.2',
                'editor_build': BUILD, 'template_version': '4.7.2.stable',
                'install_directory': 'godot-4.7.2',
                'release': {'repo': '（原私有仓库）', 'tag': 'toolchain-godot-4.7.2'},
                'archives': {}}
    for kind, asset, members in [('editor', BINARY + '.zip', editor_members),
                                 ('templates', 'Godot_v4.7.2-stable_export_templates.tpz', template_members)]:
        path = archives / asset
        write_zip(path, members)
        entries = {}
        for name, data, mode in members:
            installed = (f'godot/4.7.2-stable/{name}' if kind == 'editor' else
                         f'runtime/data/godot/export_templates/4.7.2.stable/{name.split("/")[-1]}')
            if name.endswith('android_debug.apk'):
                entries[name] = {'bytes': len(data), 'install': None}
            else:
                entries[name] = {'bytes': len(data), 'sha256': digest(data),
                                 'install': installed, 'executable': kind == 'editor'}
        manifest['archives'][kind] = {'asset': asset, 'bytes': path.stat().st_size,
                                      'sha256': digest(path.read_bytes()), 'entries': entries,
                                      'official_url': 'https://downloads.godotengine.org/?fixture=' + kind}
    if tool is not None:
        monkeypatch.setattr(tool, '_load_manifest', lambda: deepcopy(manifest))
    return tool, manifest, archives, tmp_path / 'fixed-toolchain'


def replace_archive(fixture, kind, members):
    """只更新小夹具的归档身份，保留安装条目身份供安全验证。"""
    _, manifest, archives, _ = fixture
    item = manifest['archives'][kind]
    path = archives / item['asset']
    write_zip(path, members)
    item['bytes'] = path.stat().st_size
    item['sha256'] = digest(path.read_bytes())


@pytest.mark.parametrize('corruption', ['length', 'sha256'])
def test_archive_identity_is_checked_before_execution(fixture, corruption):
    """归档长度或摘要错误必须停止，编辑器不得进入执行阶段。"""
    tool, manifest, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    path = archives / manifest['archives']['editor']['asset']
    data = bytearray(path.read_bytes())
    if corruption == 'length': data.extend(b'x')
    else: data[35] ^= 1
    path.write_bytes(data)
    with pytest.raises(RuntimeError, match='size|SHA256'):
        tool.restore_toolchain(root, 'local', archives)
    assert not (root / 'godot-4.7.2').exists()


def test_unknown_binary_is_never_executed(fixture, tmp_path):
    """已通过归档身份的未知二进制仍需自身摘要匹配，不能执行其负向标记。"""
    tool, _, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    marker = tmp_path / 'must-not-run'
    bad = f'#!/bin/sh\ntouch "{marker}"\necho 9.9.9\n'.encode()
    replace_archive(fixture, 'editor', [(BINARY, bad, stat.S_IFREG | 0o755)])
    with pytest.raises(RuntimeError, match='size|SHA256'):
        tool.restore_toolchain(root, 'local', archives)
    assert not marker.exists()


@pytest.mark.parametrize('name,mode', [('../escape', stat.S_IFREG | 0o644),
                                     ('/absolute', stat.S_IFREG | 0o644),
                                     ('templates/../../escape', stat.S_IFREG | 0o644),
                                     ('templates/link', stat.S_IFLNK | 0o777),
                                     ('templates/pipe', stat.S_IFIFO | 0o644),
                                     ('templates/unexpected', stat.S_IFREG | 0o644)])
def test_unsafe_or_unknown_zip_member_is_rejected(fixture, name, mode):
    """即使归档摘要匹配清单，路径逃逸、链接、特殊文件与未知条目仍不能安装。"""
    tool, manifest, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    path = archives / manifest['archives']['templates']['asset']
    with zipfile.ZipFile(path) as archive:
        members = [(i.filename, archive.read(i), i.external_attr >> 16) for i in archive.infolist()]
    replace_archive(fixture, 'templates', members + [(name, b'x', mode)])
    with pytest.raises(RuntimeError, match='unsafe|unexpected|symlink|type'):
        tool.restore_toolchain(root, 'local', archives)
    assert not (root / 'godot-4.7.2').exists()
    assert not (root.parent / 'escape').exists()


def test_missing_required_template_is_rejected(fixture):
    """指定 Web 模板缺失时整体恢复失败，不能启用部分安装。"""
    tool, manifest, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    path = archives / manifest['archives']['templates']['asset']
    with zipfile.ZipFile(path) as archive:
        members = [(i.filename, archive.read(i), i.external_attr >> 16)
                   for i in archive.infolist() if not i.filename.endswith('/web_release.zip')]
    replace_archive(fixture, 'templates', members)
    with pytest.raises(RuntimeError, match='missing'):
        tool.restore_toolchain(root, 'local', archives)
    assert not (root / 'godot-4.7.2').exists()


def test_restore_check_and_repeat_preserve_files(fixture):
    """首次真实夹具恢复与纯校验成功，二次运行不得改写文件或安装其他平台。"""
    tool, manifest, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    first = tool.restore_toolchain(root, 'local', archives)
    assert first['status'] == 'PASS' and not first['reused']
    install = root / 'godot-4.7.2'
    assert Path(first['env']) == install / 'env.sh'
    assert not list(install.rglob('android_debug.apk'))
    user_file = install / 'personal-note.txt'; user_file.write_text('preserve')
    before = {str(p.relative_to(root)): (p.read_bytes(), p.stat().st_mtime_ns)
              for p in root.rglob('*') if p.is_file()}
    checked = tool.verify_toolchain(root)
    assert checked['status'] == 'PASS' and len(checked['templates']) == 8
    second = tool.restore_toolchain(root, 'local', archives)
    assert second['reused']
    assert before == {str(p.relative_to(root)): (p.read_bytes(), p.stat().st_mtime_ns)
                      for p in root.rglob('*') if p.is_file()}


def test_existing_mismatched_archive_is_preserved(fixture):
    """既有下载文件身份不符时须保留原字节，不覆盖也不自动删除。"""
    tool, manifest, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    downloads = root / 'downloads'; downloads.mkdir(parents=True)
    path = downloads / manifest['archives']['editor']['asset']; path.write_bytes(b'personal file')
    with pytest.raises(RuntimeError, match='size|SHA256'):
        tool.restore_toolchain(root, 'local', archives)
    assert path.read_bytes() == b'personal file'


def test_existing_mismatched_installation_is_preserved(fixture):
    """已有安装缺损时只能报错，不能重铺并覆盖用户文件。"""
    tool, _, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    install = root / 'godot-4.7.2'; install.mkdir(parents=True)
    path = install / 'env.sh'; path.write_text('personal env')
    with pytest.raises(RuntimeError, match='missing|mismatch'):
        tool.restore_toolchain(root, 'local', archives)
    assert path.read_text() == 'personal env'


def test_read_only_root_reports_without_chmod(fixture):
    """只读目标明确失败且权限不变，不使用 sudo 或改变目录权限。"""
    tool, _, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    root.mkdir(); root.chmod(0o555)
    try:
        with pytest.raises(RuntimeError, match='writable|Permission|read-only'):
            tool.restore_toolchain(root, 'local', archives)
        assert stat.S_IMODE(root.stat().st_mode) == 0o555
    finally:
        root.chmod(0o755)


def test_generated_environment_is_sourceable_and_repo_explicit(fixture, tmp_path):
    """实际 Bash source 固定工具目录并接受显式源码目录，路径空格不得破坏命令。"""
    tool, _, archives, _ = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    root = tmp_path / "fixed directory's tools"
    result = tool.restore_toolchain(root, 'local', archives)
    repo = tmp_path / 'other repo'; repo.mkdir()
    command = 'HANMO_ROOT="$2"; source "$1"; printf "%s\\n" "$HANMO_ROOT" "$GODOT_BIN" "$XDG_DATA_HOME"; "$GODOT_BIN" --version'
    child = subprocess.run(['bash', '-c', command, 'source-test', result['env'], str(repo)],
                           env=dict(os.environ, HANMO_ROOT='old'), text=True, capture_output=True)
    assert child.returncode == 0, child.stderr
    assert child.stdout.splitlines() == [str(repo), result['editor'], result['xdg_data_home'], BUILD]
    assert '<本地路径>' not in Path(result['env']).read_text()


def test_check_does_not_execute_or_download(fixture, monkeypatch):
    """--check 只读已固定文件；即使可执行校验通过也不能运行程序或取得归档。"""
    tool, _, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    tool.restore_toolchain(root, 'local', archives)
    def forbidden(*args, **kwargs):
        """网络与子进程边界一旦被调用就失败，用于证明纯校验不触发副作用。"""
        raise AssertionError('check must not execute or download')
    monkeypatch.setattr(tool.subprocess, 'run', forbidden)
    monkeypatch.setattr(tool.urllib.request, 'urlopen', forbidden)
    monkeypatch.setattr(tool.urllib.request.OpenerDirector, 'open', forbidden)
    assert tool.verify_toolchain(root)['status'] == 'PASS'
    assert tool.main(['--root', str(root), '--check']) == 0


def test_check_missing_root_is_read_only(fixture):
    """不存在的固定目录校验失败且不创建它。"""
    tool, _, _, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    with pytest.raises(RuntimeError, match='missing'):
        tool.verify_toolchain(root)
    assert not root.exists()


def test_gh_missing_is_explicit(fixture, monkeypatch):
    """私有 Release 需要已有 gh，缺失时直接报告，不创建凭据或登录。"""
    tool, _, _, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    monkeypatch.setattr(tool.shutil, 'which', lambda name: None)
    with pytest.raises(RuntimeError, match='gh'):
        tool.restore_toolchain(root, 'github', None)
    assert not (root / 'godot-4.7.2').exists()


def test_gh_permission_error_is_explicit(fixture, monkeypatch):
    """现有 gh 返回权限错误时不换源、不登录、不将残包定名。"""
    tool, _, _, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    monkeypatch.setattr(tool.shutil, 'which', lambda name: '/fixture/gh')
    def denied(args, **kwargs):
        """仅模拟 gh 传输命令被拒，实际文件和失败传播不替换。"""
        return subprocess.CompletedProcess(args, 1, '', 'HTTP 403 fixture')
    monkeypatch.setattr(tool.subprocess, 'run', denied)
    with pytest.raises(RuntimeError, match='gh.*403|403.*gh'):
        tool.restore_toolchain(root, 'github', None)
    assert not list((root / 'downloads').glob('*.zip'))


def test_gh_download_uses_exact_repo_tag_and_asset(fixture, monkeypatch):
    """gh 下载边界模拟输出字节；精确附件选择后仍由真实摘要与 ZIP 验证安装。"""
    tool, manifest, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    monkeypatch.setattr(tool.shutil, 'which', lambda name: '/fixture/gh')
    original_run = tool.subprocess.run
    calls = []
    def download(args, **kwargs):
        """只拦截 gh，并写入它应下载的真实小归档；编辑器版本调用真实子进程。"""
        if args[0] != '/fixture/gh':
            return original_run(args, **kwargs)
        calls.append(args)
        assert args[1:4] == ['release', 'download', manifest['release']['tag']]
        assert args[args.index('--repo') + 1] == manifest['release']['repo']
        asset = args[args.index('--pattern') + 1]
        output = Path(args[args.index('--output') + 1])
        assert output.suffix == '.part'
        output.write_bytes((archives / asset).read_bytes())
        return subprocess.CompletedProcess(args, 0, '', '')
    monkeypatch.setattr(tool.subprocess, 'run', download)
    assert tool.restore_toolchain(root, 'github', None)['status'] == 'PASS'
    assert len(calls) == 2


def test_official_transport_streams_and_validates_before_rename(fixture, monkeypatch):
    """仅替换 HTTPS 响应流；完整大小和摘要必须真实校验后才能形成可用归档。"""
    tool, manifest, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    calls = []
    def response(request, **kwargs):
        """官方网络边界返回真实夹具字节，不能将此测试宣称为公网下载。"""
        url = request.full_url if hasattr(request, 'full_url') else request
        calls.append(url)
        kind = next(k for k, item in manifest['archives'].items() if item['official_url'] == url)
        return io.BytesIO((archives / manifest['archives'][kind]['asset']).read_bytes())
    patch_official_response(tool, monkeypatch, response)
    assert tool.restore_toolchain(root, 'official', None)['status'] == 'PASS'
    assert calls == [manifest['archives'][kind]['official_url'] for kind in ['editor', 'templates']]


def test_incomplete_official_transfer_never_becomes_archive(fixture, monkeypatch):
    """截断网络流留下 .part 证据，失败重试不能把它当成完整下载。"""
    tool, manifest, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    patch_official_response(tool, monkeypatch, lambda *args, **kwargs: io.BytesIO(b'truncated'))
    with pytest.raises(RuntimeError, match='size'):
        tool.restore_toolchain(root, 'official', None)
    assert not (root / 'downloads' / manifest['archives']['editor']['asset']).exists()
    assert list((root / 'downloads').glob('*.part'))
    assert tool.restore_toolchain(root, 'local', archives)['status'] == 'PASS'


def test_official_http_error_does_not_fallback(fixture, monkeypatch):
    """官方拒绝请求时保留错误，不自动换源或伪装浏览器。"""
    tool, _, _, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    import urllib.error
    def denied(*args, **kwargs):
        """只模拟 HTTP 403，要求恢复入口准确传播来源和失败。"""
        raise urllib.error.HTTPError('https://downloads.godotengine.org/', 403, 'Forbidden', {}, None)
    patch_official_response(tool, monkeypatch, denied)
    with pytest.raises(RuntimeError, match='official.*403|403.*official'):
        tool.restore_toolchain(root, 'official', None)


def test_symlink_target_is_rejected_without_touching_destination(fixture, tmp_path):
    """用户目标中的下载符号链接不得被跟随写入或覆盖外部文件。"""
    tool, manifest, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    outside = tmp_path / 'outside'; outside.mkdir()
    root.mkdir(); (root / 'downloads').symlink_to(outside, target_is_directory=True)
    with pytest.raises(RuntimeError, match='symlink|unsafe'):
        tool.restore_toolchain(root, 'local', archives)
    assert list(outside.iterdir()) == []


def test_invalid_source_fails_before_creating_root(fixture):
    """未知来源在任何写入前失败，不推断替代下载路线。"""
    tool, _, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    with pytest.raises(RuntimeError, match='source'):
        tool.restore_toolchain(root, 'mirror', archives)
    assert not root.exists()


def test_cli_default_root_obeys_explicit_environment(fixture, monkeypatch):
    """CLI 默认目录取显式 HANMO_TOOLCHAIN_ROOT，不沿用历史临时目录。"""
    tool, _, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    monkeypatch.setenv('HANMO_TOOLCHAIN_ROOT', str(root))
    assert tool.main(['--source', 'local', '--archives', str(archives)]) == 0
    assert tool.verify_toolchain(root)['status'] == 'PASS'


def test_cli_check_real_missing_root_fails_without_creation(tmp_path):
    """实际命令行加载生产清单，缺少安装时非零退出且不写入目标。"""
    tool = load_tool()
    target = tmp_path / 'absent'
    child = subprocess.run([sys.executable, tool.__file__, '--root', str(target), '--check'],
                           capture_output=True, text=True)
    assert child.returncode != 0
    assert json.loads(child.stdout)['status'] == 'FAIL'
    assert not target.exists()


def test_official_insecure_redirect_is_rejected(fixture, monkeypatch):
    """即使响应字节正确，官方入口降级到 HTTP 的重定向仍须拒绝。"""
    tool, manifest, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    class InsecureReply(io.BytesIO):
        """只模拟网络响应最终地址，真实夹具完整摘要不能覆盖 HTTPS 要求。"""
        def geturl(self):
            """报告不安全重定向，供生产传输边界判断。"""
            return 'http://godot-releases.nbg1.your-objectstorage.com/fixture'
    def insecure_response(url, **kwargs):
        """按请求返回对应包，确保红测只因缺少 HTTPS 检查而失败。"""
        kind = next(k for k, item in manifest['archives'].items() if item['official_url'] == url)
        return InsecureReply((archives / manifest['archives'][kind]['asset']).read_bytes())
    patch_official_response(tool, monkeypatch, insecure_response)
    with pytest.raises(RuntimeError, match='HTTPS|redirect'):
        tool.restore_toolchain(root, 'official', None)
    assert not (root / 'downloads' / manifest['archives']['editor']['asset']).exists()


def test_environment_verification_does_not_depend_on_checkout(fixture, monkeypatch, tmp_path):
    """固定工具目录可由另一源码检出校验，环境身份不能依赖本机临时仓库绝对路径。"""
    tool, _, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    tool.restore_toolchain(root, 'local', archives)
    monkeypatch.setattr(tool, 'REPO', tmp_path / 'another-checkout')
    assert tool.verify_toolchain(root)['status'] == 'PASS'


def test_unknown_version_manifest_is_rejected_before_execution(tmp_path, monkeypatch):
    """生产清单版本漂移在任何恢复动作前失败，不执行自称新版本的程序。"""
    tool = load_tool()
    data = json.loads((ROOT / 'tools/toolchain/godot-4.7.2.json').read_text())
    data['editor_build'] = '4.8.0.stable.official.unknown'
    path = tmp_path / 'unknown-manifest.json'; path.write_text(json.dumps(data))
    monkeypatch.setattr(tool, 'MANIFEST', path)
    root = tmp_path / 'must-not-exist'
    with pytest.raises(RuntimeError, match='unsupported'):
        tool.restore_toolchain(root, 'local', tmp_path)
    assert not root.exists()


def test_zip_duplicate_entry_is_rejected(fixture):
    """归档同名条目必须拒绝，不能让后面的内容覆盖先前校验结果。"""
    tool, manifest, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    path = archives / manifest['archives']['templates']['asset']
    with zipfile.ZipFile(path) as archive:
        members = [(i.filename, archive.read(i), i.external_attr >> 16) for i in archive.infolist()]
    with pytest.warns(UserWarning, match='Duplicate'):
        replace_archive(fixture, 'templates', members + [members[0]])
    with pytest.raises(RuntimeError, match='unexpected'):
        tool.restore_toolchain(root, 'local', archives)
    assert not (root / 'godot-4.7.2').exists()


def test_real_manifest_matches_original_evidence():
    """生产清单必须准确记录官方固定包、原编辑器身份和仅八项安装目标。"""
    tool = load_tool()
    manifest = tool._load_manifest()
    old = json.loads((ROOT / 'verification/godot-2026-09-10/sdk-manifest.json').read_text())
    assert manifest['editor_build'] == old['editor_build'] == BUILD
    editor = manifest['archives']['editor']
    templates = manifest['archives']['templates']
    assert editor['bytes'] == old['editor']['bytes'] == 77860424
    assert editor['sha256'] == old['editor']['archive_sha256']
    assert templates['bytes'] == old['template_bytes'] == 1281349702
    assert templates['sha256'] == old['template_sha256']
    assert editor['entries'][BINARY]['sha256'] == old['editor']['binary_sha256']
    installed = [name.split('/')[-1] for name, item in templates['entries'].items() if item['install']]
    assert set(installed) == set(old['installed_templates']) == set(TEMPLATES)
    assert len(templates['entries']) == old['zip_entries'] == 35
    assert manifest['release'] == {'repo': '（原私有仓库）', 'tag': 'toolchain-godot-4.7.2',
                                    'url': '（原私有仓库）'}


def test_atomic_activation_never_replaces_even_empty_user_directory(tmp_path):
    """原子启用必须有不覆盖语义，竞争出现的空目录也属于须保留的用户内容。"""
    tool = load_tool()
    assert hasattr(tool, '_activate'), '缺少不覆盖原子启用入口'
    staging = tmp_path / 'staging'; staging.mkdir()
    (staging / 'verified-file').write_bytes(b'checked')
    target = tmp_path / 'target'; target.mkdir()
    inode = target.stat().st_ino
    with pytest.raises(RuntimeError, match='exist|activate|activation'):
        tool._activate(staging, target)
    assert target.stat().st_ino == inode and list(target.iterdir()) == []
    assert (staging / 'verified-file').read_bytes() == b'checked'


@pytest.mark.parametrize('has_existing_repo', [True, False])
def test_source_without_arguments_ignores_callers_positional_parameters(fixture, has_existing_repo):
    """无参 source 必须保留已有源码根或空值，且不能读写调用者位置参数。"""
    tool, _, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    result = tool.restore_toolchain(root, 'local', archives)
    environment = dict(os.environ, TOOLCHAIN_ENV=result['env'])
    environment.pop('HANMO_ROOT', None)
    expected_repo = '/correct existing repo' if has_existing_repo else ''
    if has_existing_repo:
        environment['HANMO_ROOT'] = expected_repo
    command = ('set -- caller-one "caller two"; source "$TOOLCHAIN_ENV"; '
               'printf "%s\\n" "$HANMO_ROOT" "$1" "$2" "$#"')
    child = subprocess.run(['bash', '-c', command], env=environment, text=True, capture_output=True)
    assert child.returncode == 0, child.stderr
    assert child.stdout.splitlines() == [expected_repo, 'caller-one', 'caller two', '2']


def test_source_explicit_repo_environment_preserves_caller_parameters(fixture, tmp_path):
    """显式设置含空格源码根后 source，可覆盖旧值且不能污染调用者位置参数。"""
    tool, _, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    result = tool.restore_toolchain(root, 'local', archives)
    selected = str(tmp_path / "selected repo's directory")
    environment = dict(os.environ, TOOLCHAIN_ENV=result['env'], SELECTED_REPO=selected, HANMO_ROOT='old repo')
    command = ('set -- "unrelated first" "second caller"; HANMO_ROOT="$SELECTED_REPO"; '
               'source "$TOOLCHAIN_ENV"; printf "%s\\n" "$HANMO_ROOT" "$1" "$2" "$#"')
    child = subprocess.run(['bash', '-c', command], env=environment, text=True, capture_output=True)
    assert child.returncode == 0, child.stderr
    assert child.stdout.splitlines() == [selected, 'unrelated first', 'second caller', '2']


@pytest.mark.parametrize('intermediate', [None, 'http://redirect.invalid/', 'https://unlisted.invalid/'])
def test_official_redirect_is_checked_before_each_request(fixture, monkeypatch, intermediate):
    """用真实 urllib 重定向链证明合法跳转可用，未知中间跳回合法地址也不能请求。"""
    tool, manifest, archives, root = fixture
    assert tool is not None, '缺少固定工具链恢复入口'
    from email.message import Message
    import urllib.response
    nodes = {}
    for kind, item in manifest['archives'].items():
        final = 'https://godot-releases.nbg1.your-objectstorage.com/fixture/' + kind
        item['official_redirect_url'] = final
        first_hop = final if intermediate is None else intermediate + kind
        nodes[item['official_url']] = (302, first_hop, b'')
        if first_hop != final:
            nodes[first_hop] = (302, final, b'')
        nodes[final] = (200, None, (archives / item['asset']).read_bytes())
    calls = []
    class FixtureTransport(tool.urllib.request.BaseHandler):
        """仅替换 HTTP(S) 字节传输，保留实际 urllib 重定向与错误处理链。"""
        handler_order = 100
        def http_open(self, request):
            """记录每个真实请求选择并返回对应网络夹具，绝不发起 socket。"""
            return self.https_open(request)
        def https_open(self, request):
            """为官方入口、中间地址与最终对象提供真实 urllib 响应对象。"""
            calls.append(request.full_url)
            code, location, data = nodes[request.full_url]
            headers = Message()
            if location:
                headers['Location'] = location
            response = urllib.response.addinfourl(io.BytesIO(data), headers, request.full_url, code)
            response.msg = 'fixture'
            return response
    actual_build = tool.urllib.request.build_opener
    legacy_opener = actual_build(FixtureTransport())
    def build_with_fixture(*handlers):
        """只插入传输夹具，其余生产安全 handler 和 urllib 控制流保持原样。"""
        return actual_build(*handlers, FixtureTransport())
    monkeypatch.setattr(tool.urllib.request, 'build_opener', build_with_fixture)
    monkeypatch.setattr(tool.urllib.request, 'urlopen', legacy_opener.open)
    if intermediate is None:
        assert tool.restore_toolchain(root, 'official', None)['status'] == 'PASS'
        assert calls == [url for kind, item in manifest['archives'].items()
                         for url in (item['official_url'], item['official_redirect_url'])]
    else:
        with pytest.raises(RuntimeError, match='redirect|HTTPS'):
            tool.restore_toolchain(root, 'official', None)
        assert calls == [manifest['archives']['editor']['official_url']]
        assert not (root / 'downloads' / manifest['archives']['editor']['asset']).exists()
        assert list((root / 'downloads').glob('*.part'))
