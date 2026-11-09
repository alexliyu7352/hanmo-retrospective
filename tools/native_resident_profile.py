"""沿现有构建/发布入口准备本章resident试玩；冻结真实来源，不改变Main或存档协议。"""
from __future__ import annotations

import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from datetime import datetime, timezone

try:
    from tools import build, content_stage
    from tools.native_growth_profile import _blob, _clean_source, _git, _regular_path
    from tools.verification_report import sha256_file
except ModuleNotFoundError:
    import build
    import content_stage
    from native_growth_profile import _blob, _clean_source, _git, _regular_path
    from verification_report import sha256_file

PROFILE = 'native-resident'
CONFIG_PATH = 'tools/release_profiles/native-resident.json'
MANIFEST = 'native-resident-stage.json'
FORMAT = 'HM-NATIVE-RESIDENT-STAGE/1'
LIBRARY = 'native_resident_bridge/libhanmo_world_bridge.so'
EXTENSION = 'native_resident_bridge/hanmo_world_bridge.gdextension'
RUNTIME_ROOTS = ('game/core', 'game/ui', 'game/storage', 'game/addons')
RUNTIME_FILES = ('game/main.tscn', 'game/project.godot', 'game/data/module_contracts.json',
                 'game/data/rule_capabilities.json', 'game/data/pack_runtime.json',
                 'game/data/presentation.schema.json', 'game/data/accepted_adult_appearances.json',
                 'game/data/adult_appearance_compositions.json', 'game/character_studio/src/studio_session.gd',
                 'game/assets/audio/ambience.wav', 'game/assets/audio/paper.wav', 'game/assets/audio/confirm.wav')
NATIVE_ROOTS = ('native/world/src', 'native/world/include', 'native/world/tools', 'native/godot',
                'native/world/CMakeLists.txt', 'native/world/resident.cmake', 'native/dependencies.lock.json')
EXPORT_HELPERS = {'plugin.cfg': 'tools/native_resident_export_plugin.cfg',
                  'plugin.gd': 'tools/native_resident_editor_plugin.gd',
                  'export_plugin.gd': 'tools/native_resident_export_plugin.gd'}
EXPORT_INPUTS = tuple(EXPORT_HELPERS.values()) + ('tools/native_resident_package_probe.gd',
    'tools/native_resident_profile.py', 'game/export_presets.cfg', 'docs/release/start.sh',
    'docs/release/PLAYER.md', 'docs/release/ENGINE-LICENSES.txt', 'tools/toolchain/godot-4.7.2.json')


def _fail(code: str, detail: str) -> None:
    """统一入口的失败协议，任何缺件均不得发布成功清单。"""
    raise RuntimeError('NativeResidentProfile/' + code + ': ' + detail)


def _sha(raw: bytes) -> str:
    """摘要只绑定真实字节。"""
    return hashlib.sha256(raw).hexdigest()


def _read_config(path: Path) -> dict:
    """读取中央维护的本章固定配置，拒绝无界路径、未知ABI与不完整身份。"""
    config = json.loads(_regular_path(path).read_bytes())
    if not isinstance(config, dict) or config.get('format') != 'HM-NATIVE-RESIDENT-RELEASE/1' or config.get('export_mode') != 'debug':
        _fail('Configuration', 'Expected HM-NATIVE-RESIDENT-RELEASE/1 with export_mode=debug')
    try:
        if config['engine']['version'] != '4.7.2.stable.official.ed1daf0bf':
            _fail('EngineVersion', str(config['engine']['version']))
        for value in [config['engine']['sha256'], config['native']['library_sha256'], config['content']['pack_set_digest']]:
            if not re.fullmatch(r'[0-9a-f]{64}', value):
                _fail('Configuration', 'Expected SHA256 digest')
        if not re.fullmatch(r'[0-9a-f]{40}', config['native']['source_commit']):
            _fail('Configuration', 'Expected native production commit')
        for key in ['profile', 'custom_user_dir_name']:
            if not re.fullmatch(r'[a-z][a-z0-9-]{0,63}', config['storage'][key]):
                _fail('StorageProfile', key)
        if not isinstance(config['content']['scenario_id'], str) or not config['content']['scenario_id']:
            _fail('Configuration', 'Expected a selected scenario')
    except (KeyError, TypeError) as error:
        _fail('Configuration', str(error))
    return config


def _entries(root: Path, commit: str, paths: tuple[str, ...]) -> dict:
    """从完整Git树枚举普通文件；不联网补对象，不依赖稀疏工作副本有全部资产。"""
    result = {}
    for row in _git(root, 'ls-tree', '-rz', commit, '--', *paths).split(b'\0'):
        if not row:
            continue
        metadata, name = row.split(b'\t', 1)
        mode, kind, blob = metadata.decode().split()
        name = name.decode()
        if mode not in {'100644', '100755'} or kind != 'blob' or '..' in Path(name).parts:
            _fail('GitEntry', name)
        result[name] = blob
    return result


def _resident_digest_fields(root: Path, commit: str) -> dict:
    """按既有CMake的resident固定列表/顺序复算，拒把旧只读桥摘要当成常驻库身份。"""
    text = _blob(root, commit, 'native/world/resident.cmake').decode()
    definitions = re.findall(r'set\(HANMO_RESIDENT_IDENTITY_FILES\s*\n(.*?)\n\)', text, re.S)
    if len(definitions) != 1:
        _fail('NativeIdentityDefinition', 'Expected one resident source list')
    names = [line.split('#', 1)[0].strip().strip('"') for line in definitions[0].splitlines()
             if line.split('#', 1)[0].strip()]
    cmake = _blob(root, commit, 'native/world/CMakeLists.txt').decode()
    tail = cmake.split('set(resident_bridge_identity ', 1)
    match = re.search(r'foreach\(path\s+([^)]*)\)', tail[1] if len(tail) == 2 else '')
    if not match:
        _fail('NativeIdentityDefinition', 'Expected resident bridge source list')
    bridge_names = match.group(1).split()
    for group in (names, bridge_names):
        if not group or len(group) > 10000 or len(group) != len(set(group)) or any(
                not re.fullmatch(r'[A-Za-z0-9_./-]+', name) or name.startswith('/')
                or '..' in Path(name).parts for name in group):
            _fail('NativeIdentityDefinition', 'Unsafe or ambiguous fixed source list')
    paths = ['native/world/' + name for name in names] + ['native/godot/' + name for name in bridge_names]
    # 一次只读Git批量获取固定源字节；不用工作副本或缓存路径替代提交内容。
    requests = ''.join(commit + ':' + path + '\n' for path in paths).encode()
    stream = io.BytesIO(subprocess.check_output(['git', '-C', str(root), 'cat-file', '--batch'], input=requests))
    hashes = {}
    for path in paths:
        row = stream.readline().decode().split()
        if len(row) != 3 or row[1] != 'blob' or not row[2].isdigit():
            _fail('NativeIdentitySource', path)
        raw = stream.read(int(row[2]))
        if len(raw) != int(row[2]) or stream.read(1) != b'\n':
            _fail('NativeIdentitySource', path)
        hashes[path] = _sha(raw)
    if stream.read():
        _fail('NativeIdentitySource', 'Unexpected batch tail')
    core = _sha(('test_faults=OFF\n' + ''.join(name + ':' + hashes['native/world/' + name] + '\n'
                                             for name in names)).encode())
    bridge = _sha(('enabled=ON\ncore=' + core + '\n' + ''.join(
        name + ':' + hashes['native/godot/' + name] + '\n' for name in bridge_names)).encode())
    return {'source_sha256': bridge, 'resident_core_source_sha256': core}


def _native_identity(root: Path, commit: str, library: Path, receipt_path: Path, config: dict) -> dict:
    """已审SO、恢复回执和当前生产源码须全部对应；允许只改UI/测试/文档的后继HEAD。"""
    native_commit = config['native']['source_commit']
    expected = config['native']['library_sha256']
    if sha256_file(library) != expected:
        _fail('NativeLibraryDigest', str(library))
    with library.open('rb') as stream:
        header = stream.read(20)
    if header[:6] != b'\x7fELF\x02\x01' or header[16:20] != b'\x03\x00\x3e\x00':
        _fail('NativeELF', str(library))
    symbols = subprocess.run(['nm', '-D', '--defined-only', '--format=posix', str(library)], capture_output=True, text=True)
    if symbols.returncode or {line.split()[0] for line in symbols.stdout.splitlines() if line.strip()} != {'hanmo_world_library_init'}:
        _fail('NativeExports', symbols.stderr or symbols.stdout)
    raw = receipt_path.read_bytes()
    receipt = json.loads(raw)
    _validate_build_receipt(receipt, native_commit, expected, config['engine']['sha256'])
    production = _entries(root, native_commit, NATIVE_ROOTS)
    if not production or production != _entries(root, commit, NATIVE_ROOTS):
        _fail('NativeSourceMismatch', 'Current production inputs differ from the reviewed native build')
    resident = receipt.get('bridge_identity', {})
    if (resident.get('format') != 'HM-RESIDENT-BRIDGE-BUILD/1' or resident.get('resident_enabled') is not True
            or resident.get('source_commit') != native_commit or resident.get('test_faults') is not False
            or any(resident.get(key) != value for key, value in _resident_digest_fields(root, native_commit).items())):
        _fail('NativeResidentIdentityMismatch', 'Receipt must match the actual resident CMake source identities')
    lock = _sha(_blob(root, commit, 'native/dependencies.lock.json'))
    if receipt['bridge_identity']['dependency_lock_sha256'] != lock:
        _fail('NativeLockMismatch', 'Build receipt differs from current lock')
    return {'source_commit': native_commit, 'library_sha256': expected, 'dependency_lock_sha256': lock,
            'production_files_sha256': _sha(content_stage.canonical(production)),
            'resident_core_source_sha256': receipt['bridge_identity']['resident_core_source_sha256'],
            'build_receipt_sha256': _sha(raw), 'library_path': 'res://' + LIBRARY,
            'extension_path': 'res://' + EXTENSION, 'godot_target': 'template_debug'}


def _validate_build_receipt(receipt: dict, source: str, library_sha: str, engine_sha: str) -> None:
    """新建与增量回执均须原生布尔标记和完整终态身份，不依赖驱动自报旧二进制可用。"""
    if (not isinstance(receipt, dict) or receipt.get('format') != 'HANMO-RECOVERY-NATIVE-BUILD/1'
            or receipt.get('status') != 'BUILD_COMPLETE_RUNTIME_NOT_RUN'
            or receipt.get('source_commit') != source or receipt.get('source_commit_after') != source
            or receipt.get('source_status_after') != '' or type(receipt.get('new_recovery_build')) is not bool
            or receipt.get('historical_artifacts_recreated') is not False
            or receipt.get('dependencies_unchanged_during_build') is not True
            or receipt.get('engine', {}).get('sha256') != engine_sha
            or receipt.get('configuration', {}).get('test_faults') is not False):
        _fail('BuildReceipt', 'Successful exact production source and engine identity required')
    stages = receipt.get('stages', [])
    if (not {'configure', 'build-hanmo_resident_journey', 'build-hanmo_world_bridge'} <= {stage.get('name') for stage in stages}
            or any(stage.get('exit_code') != 0 or not stage.get('completed_at') for stage in stages)):
        _fail('BuildReceipt', 'Native build stages must have completed successfully')
    completed = datetime.fromisoformat(receipt['completed_at'])
    if completed.tzinfo is None or completed >= datetime.now(timezone.utc):
        _fail('BuildReceipt', 'Native build must already have terminated')
    libraries = [item for item in receipt.get('artifacts', []) if Path(item.get('path', '')).name == 'libhanmo_world_bridge.so']
    if len(libraries) != 1 or libraries[0].get('sha256') != library_sha:
        _fail('BuildReceipt', 'SO bytes differ from the actual build artifact')


def _runtime_entries(root: Path, commit: str) -> dict:
    """完整枚举当前Main运行源码，新增或删减运行文件同样使旧stage失效。"""
    return {name: blob for name, blob in _entries(root, commit, RUNTIME_ROOTS + RUNTIME_FILES).items()
            if name in RUNTIME_FILES or Path(name).suffix in {'.gd', '.uid', '.cfg'}}


def _export_presets(raw: bytes) -> bytes:
    """复用原Linux preset并补齐本章JSON；Web节和debug模板选择保留原约定。"""
    text = raw.decode()
    if '[preset.0]' not in text or 'name="Linux"' not in text or 'binary_format/embed_pck=true' not in text:
        _fail('ExportPreset', 'Expected existing embedded Linux preset')
    before, after = text.split('[preset.0.options]', 1)
    before, count = re.subn(r'^include_filter="([^"]*)"$',
                           lambda match: 'include_filter="' + match.group(1) + ',data/pack_runtime.json,data/accepted_adult_appearances.json,data/adult_appearance_compositions.json,character_studio/src/studio_session.gd,assets/art/packs/**/*.json"', before, flags=re.M)
    if count != 1:
        _fail('ExportPreset', 'Expected one Linux include filter')
    return (before + '[preset.0.options]' + after).encode()


def _project_config(raw: bytes, config: dict, catalog: str) -> bytes:
    """只改隔离副本的本章入口和稳定存档键，保留原Main场景与其余配置。"""
    text = raw.decode()
    updates = {'config/storage_profile': config['storage']['profile'],
               'config/custom_user_dir_name': config['storage']['custom_user_dir_name'],
               'config/default_scenario': config['content']['scenario_id'], 'config/default_catalog': catalog}
    for key, value in updates.items():
        text, count = re.subn(r'^' + re.escape(key) + r'=.*$', key + '=' + json.dumps(value, ensure_ascii=False), text, flags=re.M)
        if count != 1:
            _fail('ProjectConfiguration', key)
    if '[development]' in text:
        _fail('ProjectConfiguration', 'Existing development section requires an explicit reviewed merge')
    return (text + '\n[development]\n\nnative_resident_world_enabled=true\n').encode()


def _enable_export_plugin(raw: bytes) -> bytes:
    """仅隔离输出启用标准EditorExportPlugin；原作者插件与主工程配置保持原样。"""
    text = raw.decode()
    section = re.search(r'(\[editor_plugins\]\s*\n)(.*?)(?=\n\[|\Z)', text, flags=re.S)
    if section is None:
        _fail('EditorPlugins', 'Expected original editor_plugins section')
    body, count = re.subn(r'^enabled=PackedStringArray\(([^\n]*)\)$',
        lambda match: 'enabled=PackedStringArray(' + match.group(1) + ', "res://addons/hanmo_resident_export/plugin.cfg")',
        section.group(2), flags=re.M)
    if count != 1:
        _fail('EditorPlugins', 'Expected original enabled plugin list')
    return (text[:section.start(2)] + body + text[section.end(2):]).encode()


def _native_license_bytes(root: Path, commit: str, receipt_path: Path) -> dict:
    """按编译依赖锁核四份实际分发许可原文，不用引擎总许可冒充原生库许可。"""
    lock = json.loads(_blob(root, commit, 'native/dependencies.lock.json'))
    receipt = json.loads(receipt_path.read_bytes())
    result = {}
    for name, relative in [('flecs', 'LICENSE'), ('json', 'LICENSE.MIT'), ('picosha2', 'LICENSE'), ('godot-cpp', 'LICENSE.md')]:
        source = _regular_path(Path(receipt['dependencies'][name]['root']) / relative)
        expected = (lock['godot_bridge']['binding'] if name == 'godot-cpp' else lock['libraries'][name])['files'][relative]
        raw = source.read_bytes()
        if not raw.strip() or _sha(raw) != expected:
            _fail('NativeLicenseMismatch', name)
        result[name + '-LICENSE.txt'] = raw
    return result


def _player_readme(config: dict) -> str:
    """当前试玩说明只描述已接入的启程、保存和继续，使用本章真实存档位置。"""
    return ('# 汉末行记 · 青川区域试玩\n\n'
        '解压后，在目录内执行 `bash start.sh`。无需另装Godot、Python或管理员权限。\n\n'
        '选择“启程”，选择本包的青川章节和人物，再确认外观进入旅途。游戏中的邀请、短工、采购、课程和回访均通过当前人物及菜单操作。\n\n'
        '从系统菜单保存；下次启动选择“继续”，读取此试玩保存的周目。保存后请保留同一用户数据目录。\n\n'
        '默认存档位于用户XDG数据目录的 `' + config['storage']['custom_user_dir_name'] + '/profiles/'
        + config['storage']['profile'] + '/saves`。可通过 `HANMO_STATE_ROOT=/绝对路径/状态目录 bash start.sh` 明确指定数据根。\n\n'
        '中文字体已内嵌，许可见FONT-LICENSE.txt。引擎许可见ENGINE-LICENSES.txt，原生依赖许可见NATIVE-LICENSES目录。\n\n'
        '本包为Linux x86_64试玩；具体运行与交互验收范围见BUILD.json及随附实际报告。\n')


def _files(directory: Path) -> dict:
    """绑定整个可交付成员集；所有链接、私人存档或未登记新增文件都不被默许。"""
    result = {}
    for path in sorted(directory.rglob('*')):
        if path.is_symlink():
            _fail('UnsafePath', str(path))
        if path.is_file():
            result[path.relative_to(directory).as_posix()] = sha256_file(path)
    return result


def _stage_content(root: Path, game: Path, selected: Path, config: dict) -> dict:
    """沿现有正式安装器安装唯一compose代及实际表现资源，不重新编造作者内容。"""
    record = content_stage.composed_set_record(selected, root)
    if record['pack_set_digest'] != config['content']['pack_set_digest']:
        _fail('ContentIdentity', 'Selected compose generation differs from the release profile')
    header = content_stage.read_pack(selected / 'catalog.hmcatalog.json')
    if config['content']['scenario_id'] not in header['catalog']['scenarios']:
        _fail('ScenarioMissing', config['content']['scenario_id'])
    generated = game / 'data/generated'
    generated.mkdir(parents=True, exist_ok=False)
    content_stage.write_schema_index(root, generated)
    (generated / 'content.schema.json').write_bytes((root / content_stage.SCHEMA).read_bytes())
    (generated / 'runtime_capabilities.json').write_bytes(content_stage.canonical(
        content_stage.runtime_declaration(content_stage.load_contract(root / content_stage.CAPABILITY), path=root / content_stage.CAPABILITY)))
    snapshots, members = content_stage.stage_installed_sets(root, generated, game.parent / 'no-previous-generation', [selected], foundation_sets=False)
    content_stage.installed_index_records(generated)
    installed = generated / record['directory']
    (game / 'data/presentation.json').write_bytes((installed / 'presentation.json').read_bytes())
    for name, path in content_stage.files_in(installed).items():
        if name.startswith('assets/'):
            target = game / name
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists() and target.read_bytes() != path.read_bytes():
                _fail('ContentCollision', name)
            target.write_bytes(path.read_bytes())
    if (content_stage.composed_set_record(selected, root) != record
            or any(path.read_bytes() != raw for path, raw in snapshots.items())
            or sorted(content_stage.files_in(selected)) != members[selected]):
        _fail('ContentChanged', str(selected))
    return {'pack_set_digest': record['pack_set_digest'], 'scenario_id': config['content']['scenario_id'],
            'catalog_path': 'res://data/generated/' + record['directory'] + '/catalog.hmcatalog.json', 'installed_set': record}


def prepare_profile(root: Path, output: Path, native_library: Path, content_sets=(), *,
                    native_build_receipt: Path, profile_config: Path | None = None,
                    font: Path | None = None, font_license: Path | None = None,
                    font_face_index: int = 0) -> dict:
    """在新目录冻结同一项目；准备成功只表示字节闭合，不代表已导出或实机通过。"""
    try:
        root = _regular_path(root); output = _regular_path(output)
        library = _regular_path(native_library); receipt = _regular_path(native_build_receipt)
        if output.exists() or root == output or root in output.parents or output in root.parents:
            _fail('OutputNotIsolated', str(output))
        if len(content_sets) != 1:
            _fail('ContentSelection', 'Exactly one reviewed compose generation is required')
        if (font is None) != (font_license is None) or (font is None and font_face_index != 0):
            _fail('FontInputs', '--font and --font-license must be supplied together')
        config_path = _regular_path(profile_config or root / CONFIG_PATH)
        config = _read_config(config_path)
        config_bytes = config_path.read_bytes()
        source = _clean_source(root); commit = source['commit']
        native = _native_identity(root, commit, library, receipt, config)
        entries = _runtime_entries(root, commit)
        if not set(RUNTIME_FILES) <= entries.keys():
            _fail('MissingRuntimeSource', ', '.join(sorted(set(RUNTIME_FILES) - entries.keys())))
        output.mkdir(parents=True, exist_ok=False)
        for name in entries:
            target = output / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(_blob(root, commit, name))
        game = output / 'game'
        content = _stage_content(root, game, _regular_path(Path(content_sets[0])), config)
        (game / 'project.godot').write_bytes(_enable_export_plugin(_project_config(
            _blob(root, commit, 'game/project.godot'), config, content['catalog_path'])))
        helper_blobs = _entries(root, commit, EXPORT_INPUTS)
        for name, source_path in EXPORT_HELPERS.items():
            destination = game / 'addons/hanmo_resident_export' / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(_blob(root, commit, source_path))
        (game / LIBRARY).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(library, game / LIBRARY)
        descriptor = _blob(root, commit, 'native/godot/hanmo_world.gdextension.in').replace(b'@HANMO_BRIDGE_LIBRARY@', ('res://' + LIBRARY).encode())
        if b'@' in descriptor or b'linux.debug.x86_64' not in descriptor or b'linux.release' in descriptor:
            _fail('ExtensionABI', 'Expected the reviewed debug-only Linux descriptor')
        (game / EXTENSION).write_bytes(descriptor)
        # 导出仍消费原发布preset；缺失Git对象是阻断，不临时生成替身。
        (game / 'export_presets.cfg').write_bytes(_export_presets(_blob(root, commit, 'game/export_presets.cfg')))
        for name in ['start.sh', 'PLAYER.md', 'ENGINE-LICENSES.txt']:
            destination = output / 'release-inputs' / name
            destination.parent.mkdir(exist_ok=True)
            destination.write_bytes(_blob(root, commit, 'docs/release/' + name))
        for name, raw in _native_license_bytes(root, commit, receipt).items():
            destination = output / 'release-inputs/native-licenses' / name
            destination.parent.mkdir(exist_ok=True)
            destination.write_bytes(raw)
        font_record = None
        if font is not None:
            try:
                from tools.prepare_font import prepare, prepared_font
            except ModuleNotFoundError:
                from prepare_font import prepare, prepared_font
            prepare(_regular_path(font), game, _regular_path(font_license), face_index=font_face_index)
            font_record = {key: str(value) if isinstance(value, Path) else value for key, value in prepared_font(game).items()}
        (output / 'native-build-receipt.json').write_bytes(receipt.read_bytes())
        (output / 'release-profile.json').write_bytes(config_bytes)
        if (config_path.read_bytes() != config_bytes or _clean_source(root) != source
                or _native_identity(root, commit, game / LIBRARY, receipt, config) != native):
            _fail('InputsChanged', 'Source or native artifact changed during preparation')
        manifest = {'format': FORMAT, 'profile': PROFILE, 'source_commit': commit,
                    'runtime_source_blobs': entries, 'export_input_blobs': helper_blobs, 'profile_sha256': _sha(config_bytes),
                    'configuration': config, 'content': content, 'native': native, 'font': font_record,
                    'status': {'stage': 'PASS', 'export': 'NOT_RUN', 'gameplay': 'NOT_RUN', 'gui': 'NOT_RUN'},
                    'files': _files(output)}
        manifest_path = output / MANIFEST
        manifest_path.write_bytes(content_stage.canonical(manifest))
        return {'game': str(game), 'manifest_path': str(manifest_path), 'manifest': manifest}
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        _fail('PreparationFailed', str(error))


def validate_profile(root: Path, staged_project: Path) -> dict:
    """只读核阶段清单与当前同源运行输入，允许Godot新导入缓存但不容许源码换代。"""
    root = _regular_path(root); game = _regular_path(staged_project); output = game.parent
    manifest = json.loads((output / MANIFEST).read_bytes())
    if game.name != 'game' or manifest.get('format') != FORMAT or manifest.get('profile') != PROFILE:
        _fail('Manifest', str(output))
    current = _clean_source(root)
    if _runtime_entries(root, current['commit']) != manifest['runtime_source_blobs']:
        _fail('RuntimeSourceChanged', current['commit'])
    if _entries(root, current['commit'], EXPORT_INPUTS) != manifest['export_input_blobs']:
        _fail('ExportInputsChanged', current['commit'])
    actual = _files(output)
    for name, digest in manifest['files'].items():
        if actual.get(name) != digest:
            _fail('StageChanged', name)
    for name in set(actual) - set(manifest['files']) - {MANIFEST}:
        if name.startswith('game/.godot/'):
            continue
        if name.endswith('.import') and name.removesuffix('.import') in manifest['files']:
            continue
        if name.endswith('.uid') and name.removesuffix('.uid') in manifest['files'] and re.fullmatch(rb'uid://[a-z0-9]{1,24}\n?', (output / name).read_bytes()):
            continue
        _fail('UnexpectedStageFile', name)
    config = _read_config(output / 'release-profile.json')
    if config != manifest['configuration'] or sha256_file(output / 'release-profile.json') != manifest['profile_sha256']:
        _fail('ProfileChanged', str(output))
    if _native_identity(root, current['commit'], game / LIBRARY, output / 'native-build-receipt.json', config) != manifest['native']:
        _fail('NativeChanged', str(output))
    content_stage.installed_index_records(game / 'data/generated')
    return {'game': str(game), 'manifest_path': str(output / MANIFEST), 'manifest': manifest}


def _export_prerequisites(root: Path, engine: str, config: dict) -> dict:
    """先核固定编辑器与实际安装的debug模板，缺模板不得创建成功导出目录。"""
    editor = _regular_path(Path(engine))
    if sha256_file(editor) != config['engine']['sha256']:
        _fail('EngineDigest', str(editor))
    version = build.run_checked([str(editor), '--version']).strip()
    if version != config['engine']['version']:
        _fail('EngineVersion', version)
    commit = _clean_source(root)['commit']
    toolchain = json.loads(_blob(root, commit, 'tools/toolchain/godot-4.7.2.json'))
    data_home = Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share')))
    templates = data_home / 'godot/export_templates' / toolchain['template_version']
    records = {}
    for name in ['linux_debug.x86_64', 'version.txt', 'icudt_godot.dat']:
        path = _regular_path(templates / name)
        expected = toolchain['archives']['templates']['entries']['templates/' + name]
        if not path.is_file() or path.stat().st_size != expected['bytes'] or sha256_file(path) != expected['sha256']:
            _fail('ExportTemplateMissingOrChanged', str(path))
        records[name] = {'path': str(path), 'sha256': expected['sha256']}
    return {'version': version, 'sha256': sha256_file(editor), 'templates': records}


def export_profile(root: Path, staged_project: Path, output: Path, engine: str) -> dict:
    """从已核stage执行真实debug导出并记录全部包成员；运行及GUI验收仍单独记录。"""
    try:
        from tools.prepare_font import prepared_font
    except ModuleNotFoundError:
        from prepare_font import prepared_font
    root = _regular_path(root); game = _regular_path(staged_project); output = _regular_path(output)
    if output.exists() or game.parent == output or game.parent in output.parents or output in game.parents:
        _fail('OutputExists', str(output))
    result = validate_profile(root, game); manifest = result['manifest']
    config = manifest['configuration']
    prerequisites = _export_prerequisites(root, engine, config)
    font = prepared_font(game)
    evidence = output.with_name(output.name + '-verification')
    if evidence.exists():
        _fail('EvidenceExists', str(evidence))
    evidence.mkdir(parents=True, exist_ok=False)
    # 仅创建新导入缓存；此步骤不运行Main、测试helper或世界规则。
    build.run_checked([engine, '--headless', '--audio-driver', 'Dummy', '--editor', '--import', '--path', str(game), '--quit'],
                      timeout=300, log=evidence / 'import.log')
    validate_profile(root, game)
    output.mkdir(parents=True, exist_ok=False)
    binary = output / 'hanmo.x86_64'
    command = [engine, '--headless', '--path', str(game), '--export-debug', 'Linux', str(binary)]
    build.run_checked(command, timeout=300, log=evidence / 'export-linux.log')
    with binary.open('rb') as stream:
        binary_header = stream.read(6)
    if binary_header != b'\x7fELF\x02\x01':
        _fail('ExportArtifact', 'Godot did not produce a Linux x86_64 executable')
    libraries = [path for path in output.rglob('*.so') if sha256_file(path) == manifest['native']['library_sha256']]
    if len(libraries) != 1:
        _fail('ExportedNativeLibrary', 'Expected exactly one exported copy of the reviewed Bridge')
    binary.chmod(0o755)
    launcher = output / 'start.sh'
    launcher.write_text((game.parent / 'release-inputs/start.sh').read_text().replace('@SAVE_PROFILE@', config['storage']['profile']))
    launcher.chmod(0o755)
    (output / 'README.md').write_text(_player_readme(config), encoding='utf-8')
    shutil.copyfile(game.parent / 'release-inputs/ENGINE-LICENSES.txt', output / 'ENGINE-LICENSES.txt')
    shutil.copyfile(font['license_path'], output / 'FONT-LICENSE.txt')
    shutil.copytree(game.parent / 'release-inputs/native-licenses', output / 'NATIVE-LICENSES')
    (output / 'RESIDENT-PLAYTEST.txt').write_text('汉末行记 · 青川区域Linux试玩\n\n运行 ./start.sh 后选择“启程”或“继续”。\n'
        '本包使用已审resident后端与独立存档目录；新局须确认外观，继续沿原菜单读取此试玩的已有存档。\n'
        '本次为匹配template_debug原生ABI的debug导出；图形操作及主机兼容性以实际验收记录为准。\n'
        '存档配置：' + config['storage']['profile'] + '\n', encoding='utf-8')
    validate_profile(root, game)
    packed_content = _inspect_package(root, game, binary, engine, evidence, manifest)
    record = {'format': 'HM-NATIVE-RESIDENT-LINUX-BUILD/1', 'profile': PROFILE, 'export_mode': 'debug',
              'source_commit': manifest['source_commit'], 'stage_manifest_sha256': sha256_file(game.parent / MANIFEST),
              'engine': prerequisites, 'native': manifest['native'], 'content': manifest['content'], 'storage': config['storage'],
              'font': {key: value for key, value in font.items() if key not in {'path', 'license_path'}},
              'export_command': command, 'export_log': str(evidence / 'export-linux.log'),
              'packed_content': packed_content,
              'acceptance': {'export': 'PASS', 'packed_content': 'PASS', 'start_continue': 'PENDING', 'gui': 'NOT_RUN'},
              'files': _files(output)}
    (output / 'BUILD.json').write_bytes(content_stage.canonical(record))
    return {'folder': str(output), 'binary': str(binary), 'manifest': record, 'manifest_path': str(output / 'BUILD.json')}


def _package_requirements(manifest: dict) -> dict:
    """完整installed成员（含隐藏marker和原PNG）与导入资源分别验，不能互相替代。"""
    files = {}
    resources = set()
    for name, digest in manifest['files'].items():
        if not name.startswith('game/'):
            continue
        relative = name.removeprefix('game/')
        if relative.startswith('data/') and relative.endswith('.json'):
            files['res://' + relative] = digest
        if Path(relative).suffix in {'.png', '.wav', '.ttf', '.ttc', '.otf', '.tres', '.gdextension'}:
            resources.add('res://' + relative)
    installed = manifest['content']['installed_set']
    for row in installed['files']:
        name = 'res://data/generated/' + installed['directory'] + '/' + row['path']
        files[name] = row['sha256']
    return {'files': [{'path': name, 'sha256': digest} for name, digest in sorted(files.items())],
            'resources': sorted(resources), 'catalog_path': manifest['content']['catalog_path'],
            'pack_set_digest': manifest['content']['pack_set_digest']}


def _inspect_package(root: Path, game: Path, binary: Path, engine: str, evidence: Path, manifest: dict) -> dict:
    """空工程调用真实InstalledContent.read并逐原成员核SHA，再核导入资源，不运行Main。"""
    expected = _package_requirements(manifest)
    expected_path = evidence / 'package-expected.json'
    expected_path.write_bytes(content_stage.canonical(expected))
    report_path = evidence / 'package-inspection.json'
    with tempfile.TemporaryDirectory(prefix='resident-package-probe-') as temporary:
        isolated = Path(temporary)
        (isolated / 'project.godot').write_text('config_version=5\n')
        probe = isolated / 'inspect.gd'
        probe.write_bytes(_blob(root, _clean_source(root)['commit'], 'tools/native_resident_package_probe.gd'))
        command = [engine, '--headless', '--path', str(isolated), '--script', str(probe), '--',
                   str(binary), str(expected_path), str(report_path)]
        build.run_checked(command, timeout=120, log=evidence / 'package-inspection.log')
    actual = json.loads(report_path.read_bytes())
    if (actual.get('ok') is not True or actual.get('files') != expected['files'] or actual.get('resources') != expected['resources']
            or actual.get('installed_content', {}).get('ok') is not True
            or actual['installed_content'].get('pack_set_digest') != expected['pack_set_digest']):
        _fail('PackedContent', 'Actual package differs from the frozen stage')
    return {'status': 'PASS', 'report_sha256': sha256_file(report_path), 'files': len(expected['files']),
            'resources': len(expected['resources']), 'installed_content': actual['installed_content'], 'command': command}


def package_profile(folder: Path, archive: Path | None = None) -> Path:
    """重核完整目录后调用已有ZIP打包器，拒绝新增、损坏、私人存档与覆盖旧包。"""
    try:
        from tools.release_linux import package_directory
    except ModuleNotFoundError:
        from release_linux import package_directory
    folder = _regular_path(folder); archive = _regular_path(archive or folder.with_suffix('.zip'))
    if archive.exists() or archive.with_suffix('.zip.tmp').exists() or folder in archive.parents:
        _fail('ArchiveExistsOrUnsafe', str(archive))
    manifest = json.loads((folder / 'BUILD.json').read_bytes())
    if manifest.get('format') != 'HM-NATIVE-RESIDENT-LINUX-BUILD/1' or manifest.get('export_mode') != 'debug':
        _fail('BuildManifest', str(folder))
    actual = _files(folder)
    actual.pop('BUILD.json')
    if actual != manifest['files']:
        _fail('PackageChanged', str(folder))
    if manifest.get('acceptance', {}).get('packed_content') != 'PASS':
        _fail('PackedContent', 'Actual package inspection is required before ZIP creation')
    if any(not os.access(folder / name, os.X_OK) for name in ['start.sh', 'hanmo.x86_64']):
        _fail('ExecutablePermission', str(folder))
    package_directory(folder, archive)
    return archive
