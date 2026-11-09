"""准备闭合profile的显式原生开发工程；共用固定来源校验，不导入、不编译、不接管旧目录。"""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import uuid

try:
    from tools import build, content_stage
    from tools.verification_report import source_identity, sha256_file
except ModuleNotFoundError:
    import build
    import content_stage
    from verification_report import source_identity, sha256_file

FORMAT = 'HM-NATIVE-GROWTH-STAGE/1'
MANIFEST = 'native-growth-stage.json'
REVIEW = 'verification/native-legacy-godot-bridge-20261004.json'
LIBRARY_SHA256 = '991c5e09d20b1deb07b15741d839c32259ee888a1af7411d4fe342f9a65546f2'
SOURCE_ROOTS = ('game', 'tools', 'content', 'schemas')
EXTENSION = 'native/hanmo_world.gdextension'
LIBRARY = 'native/libhanmo_world_bridge.so'
REPRESENTATIVE_TEST = 'game/tests/test_native_growth_profile.gd'
PROJECT_IDENTITY = 'unique_sections_keys_exact_value_text'
TRAVEL_BUILD_KEY = 'native_travel_schedule_expected_build'
TRAVEL_BUILD_FIELDS = {'profile', 'source_commit', 'source_sha256', 'core_source_sha256', 'api_sha256', 'dependency_lock_sha256'}


def _profile_spec(profile: str) -> dict:
    """只允许两种已审组合，名称不能变成任意库、来源或脚本路径。"""
    if profile == 'native-growth':
        return {'format': FORMAT, 'manifest': MANIFEST, 'review': REVIEW, 'library_sha256': LIBRARY_SHA256,
                'source_commit': 'f1c907d7c42ef6ecd78f3d68bf731a85f65eaa68', 'representative_test': REPRESENTATIVE_TEST}
    if profile == 'native-travel':
        return {'format': 'HM-NATIVE-TRAVEL-STAGE/1', 'manifest': 'native-travel-stage.json',
                'review': 'verification/native-travel-schedule-20261004.json',
                'library_sha256': '65ada2192d00429b0edd77b0b16038c9247a6fff06d0828a2573d967c2f60fd1',
                'source_commit': '62654abb3238de1f4ad23037dbd250734c399608',
                'representative_test': 'game/tests/test_native_travel_profile.gd'}
    _fail('UnknownProfile', str(profile))


def _fail(code: str, detail: str) -> None:
    """所有准备/复核失败均显式阻断，不返回半成品成功对象。"""
    raise RuntimeError('NativeGrowthProfile/' + code + ': ' + detail)


def _git(root: Path, *args: str) -> bytes:
    """仅查询本地Git对象；禁用按需网络取对象，缺失原件直接失败。"""
    import os
    result = subprocess.run(['git', *args], cwd=root, capture_output=True,
                            env={**os.environ, 'GIT_NO_LAZY_FETCH': '1', 'GIT_TERMINAL_PROMPT': '0'})
    if result.returncode:
        _fail('GitSourceUnavailable', result.stderr.decode(errors='replace').strip())
    return result.stdout


def _blob(root: Path, commit: str, name: str) -> bytes:
    """从固定提交取原字节，稀疏未物化文件同样参与准备和核验。"""
    return _git(root, 'show', commit + ':' + name)


def _regular_path(path: Path) -> Path:
    """拒绝任一层符号链接，避免输出和输入的实际归属被路径别名改变。"""
    path = path.absolute()
    if any(part.is_symlink() for part in (path, *path.parents)):
        _fail('UnsafePath', str(path))
    return path.resolve()


def _clean_source(root: Path) -> dict:
    """沿现有身份助手拒绝脏源码；不能把工作副本改动冒充固定Git提交。"""
    identity = source_identity(root)
    if identity['dirty_paths']:
        _fail('DirtySource', ', '.join(row['path'] for row in identity['dirty_paths']))
    return identity


def _source_entries(root: Path, commit: str, *, profile: str = 'native-growth') -> dict[str, str]:
    """以Git完整树列普通源码/资源；生成内容与引擎缓存不属于原件。"""
    result = {}
    representative = _profile_spec(profile)['representative_test']
    raw = _git(root, 'ls-tree', '-r', '-z', commit, *SOURCE_ROOTS)
    for row in raw.split(b'\0'):
        if not row:
            continue
        metadata, encoded_name = row.split(b'\t', 1)
        mode, kind, digest = metadata.decode().split()
        name = encoded_name.decode('utf-8')
        if mode not in {'100644', '100755'} or kind != 'blob':
            _fail('UnsupportedGitEntry', name)
        if name.startswith(('game/data/generated/', 'game/.godot/')):
            continue
        if name.startswith('game/tests/') and name not in {representative, representative + '.uid'}:
            continue
        if Path(name).is_absolute() or '..' in Path(name).parts:
            _fail('UnsafeGitPath', name)
        result[name] = digest
    if 'game/main.tscn' not in result or 'game/project.godot' not in result:
        _fail('MissingGameEntry', commit)
    if representative not in result:
        _fail('MissingRepresentativeTest', commit)
    return result


def _same_runtime_source(root: Path, staged_commit: str, current_commit: str, *, profile: str = 'native-growth') -> dict[str, str]:
    """固定代仍绑原提交；仅文档/证据后继可复用，当前任何受控运行原件变更均拒绝。"""
    if not isinstance(staged_commit, str) or not re.fullmatch(r'[0-9a-f]{40}', staged_commit):
        _fail('SourceCommitMismatch', 'invalid fixed commit')
    entries = _source_entries(root, staged_commit, profile=profile)
    if entries != _source_entries(root, current_commit, profile=profile):
        _fail('SourceCommitMismatch', 'current runtime sources differ from ' + staged_commit)
    return entries


def _cmake_identity(root: Path, commit: str, directory: str, marker: str) -> tuple[str, list[str]]:
    """读取已有CMake固定身份列表，用同一原字节/顺序计算编译源摘要。"""
    text = _blob(root, commit, directory + '/CMakeLists.txt').decode()
    suffix = text.split(marker, 1)
    match = re.search(r'foreach\(path\s+([^)]*)\)', suffix[1] if len(suffix) == 2 else '')
    if not match:
        _fail('NativeIdentityDefinition', directory)
    names = match.group(1).split()
    identity = ''.join(name + ':' + hashlib.sha256(_blob(root, commit, directory + '/' + name)).hexdigest() + '\n'
                       for name in names)
    return hashlib.sha256(identity.encode()).hexdigest(), [directory + '/' + name for name in names]


def _native_identity(root: Path, commit: str, library: Path, *, profile: str = 'native-growth') -> dict:
    """按profile核唯一已审ELF/C入口/来源；GD当前提交与库编译提交明确分开。"""
    spec = _profile_spec(profile)
    if platform.system() != 'Linux' or platform.machine().lower() not in {'x86_64', 'amd64'}:
        _fail('UnsupportedPlatform', platform.platform())
    if not library.is_file() or sha256_file(library) != spec['library_sha256']:
        _fail('UnreviewedNativeLibrary', str(library))
    with library.open('rb') as stream:
        header = stream.read(20)
    if header[:6] != b'\x7fELF\x02\x01' or int.from_bytes(header[16:18], 'little') != 3 or int.from_bytes(header[18:20], 'little') != 62:
        _fail('InvalidNativeELF', str(library))
    symbols = subprocess.run(['nm', '-D', '--defined-only', '--format=posix', str(library)], capture_output=True, text=True)
    if symbols.returncode or {line.split()[0] for line in symbols.stdout.splitlines() if line.strip()} != {'hanmo_world_library_init'}:
        _fail('InvalidNativeExports', symbols.stderr or symbols.stdout)
    reviewed_raw = _blob(root, commit, spec['review'])
    reviewed = json.loads(reviewed_raw)
    if reviewed.get('library_sha256') != spec['library_sha256'] or reviewed.get('source_commit') != spec['source_commit']:
        _fail('NativeReviewMismatch', spec['review'])
    native_commit = reviewed['source_commit']
    lock_raw = _blob(root, commit, 'native/dependencies.lock.json')
    if lock_raw != _blob(root, native_commit, 'native/dependencies.lock.json'):
        _fail('NativeLockMismatch', 'native/dependencies.lock.json')
    lock = json.loads(lock_raw)['godot_bridge']
    bridge_sha, bridge_files = _cmake_identity(root, native_commit, 'native/godot', 'set(identity "")')
    core_sha, core_files = _cmake_identity(root, native_commit, 'native/world', 'set(source_identity "")')
    for name in bridge_files + core_files:
        if _blob(root, commit, name) != _blob(root, native_commit, name):
            _fail('NativeSourceMismatch', name)
    expected = {'source_commit': native_commit, 'source_sha256': bridge_sha,
                'core_source_sha256': core_sha, 'dependency_lock_sha256': hashlib.sha256(lock_raw).hexdigest(),
                'godot_cpp_commit': lock['binding']['commit'], 'api_sha256': lock['sdk']['files']['extension_api.json'],
                'interface_sha256': lock['sdk']['files']['gdextension_interface.json'], 'profile_sha256': lock['profile']['sha256'],
                'target': lock['abi']['target'], 'godot_target': lock['abi']['godot_target'], 'precision': lock['abi']['precision'],
                'real_t_bits': lock['abi']['real_t_bits'], 'pointer_bits': lock['abi']['pointer_bits'],
                'builtin_configuration': lock['abi']['builtin_configuration'], 'cpp_standard': '17'}
    result = {'source_commit': native_commit, 'library_sha256': spec['library_sha256'],
            'library_path': 'res://' + LIBRARY, 'extension_path': 'res://' + EXTENSION,
            'dependency_lock_sha256': expected['dependency_lock_sha256'], 'bridge_audit_expected': expected,
            'review_record': {'path': spec['review'], 'sha256': hashlib.sha256(reviewed_raw).hexdigest()},
            'sdk_evidence': {'source': 'native/dependencies.lock.json', 'sdk': lock['sdk'], 'binding_commit': lock['binding']['commit']}}
    if profile == 'native-travel':
        result['travel_build_expected'] = {key: expected[key] for key in TRAVEL_BUILD_FIELDS - {'profile'}}
        result['travel_build_expected']['profile'] = 'native_travel_schedule'
    return result


def _storage(output: Path, commit: str, instance_nonce: str, *, profile: str = 'native-growth') -> dict:
    """独立输出、固定提交和非秘密本次nonce共同隔离存储，同路径重建也不复用旧存档。"""
    if not isinstance(instance_nonce, str) or not re.fullmatch(r'[0-9a-f]{32}', instance_nonce):
        _fail('StorageMismatch', 'invalid build-instance nonce')
    suffix = hashlib.sha256((str(output) + '\n' + commit + '\n' + instance_nonce).encode()).hexdigest()[:24]
    _profile_spec(profile)
    name = profile + '-' + suffix
    return {'profile': name, 'custom_user_dir_name': 'hanmo-' + name, 'instance_nonce': instance_nonce}


def _project_config(raw: bytes, storage: dict, commit: str, *, profile: str = 'native-growth', expected_build: dict | None = None) -> bytes:
    """仅改本输出的开发开关/来源与独立存储键，原主场景和表现配置保持原字节顺序。"""
    _profile_spec(profile)
    text = raw.decode('utf-8')
    replacements = {'config/storage_profile': json.dumps(storage['profile']),
                    'config/custom_user_dir_name': json.dumps(storage['custom_user_dir_name']),
                    'config/use_custom_user_dir': 'true'}
    for key, value in replacements.items():
        text, count = re.subn(r'^' + re.escape(key) + r'=.*$', key + '=' + value, text, flags=re.M)
        if count != 1:
            _fail('ProjectConfiguration', key)
    if '[development]' in text or 'native_growth_action_enabled' in text:
        _fail('DefaultNativeAlreadyEnabled', 'game/project.godot')
    text += '\n[development]\n\nnative_growth_action_enabled=' + ('true' if profile == 'native-growth' else 'false')
    text += '\nnative_projection_source_commit="' + commit + '"\n'
    if profile == 'native-travel':
        _check_travel_build(expected_build)
        text += 'native_travel_schedule_enabled=true\n' + TRAVEL_BUILD_KEY + '='
        text += json.dumps(expected_build, ensure_ascii=False, sort_keys=True, separators=(',', ':')) + '\n'
    return text.encode('utf-8')


def _check_travel_build(value: dict) -> None:
    """唯一配置字典只承载原provider六个String字段，不允许嵌套Variant或扩大键集合。"""
    if not isinstance(value, dict) or set(value) != TRAVEL_BUILD_FIELDS or any(not isinstance(item, str) for item in value.values()):
        _fail('ConfigurationSyntax', 'travel build requires exactly six String fields')
    if value['profile'] != 'native_travel_schedule' or not re.fullmatch(r'[0-9a-f]{40}', value['source_commit']):
        _fail('ConfigurationSyntax', 'invalid travel build profile or source commit')
    if any(not re.fullmatch(r'[0-9a-f]{64}', value[key]) for key in TRAVEL_BUILD_FIELDS - {'profile', 'source_commit'}):
        _fail('ConfigurationSyntax', 'invalid travel build digest')


def _travel_build_pairs(pairs: list) -> dict:
    """配置字典的重复键必须失败，不能被JSON最后一项覆盖而隐藏身份变化。"""
    result = {}
    for key, value in pairs:
        if key in result:
            _fail('ConfigurationSyntax', 'duplicate travel build key: ' + key)
        result[key] = value
    return result


def _project_lines(raw: bytes):
    """只合并development唯一build键的有限多行字典，其它多行语法仍由原解析器拒绝。"""
    lines = raw.decode('utf-8').splitlines(); index = 0; section = ''
    while index < len(lines):
        line = lines[index]; number = index + 1; index += 1
        header = re.fullmatch(r'\[([A-Za-z0-9_/-]+)\]', line.strip())
        if header:
            section = header.group(1)
        key, separator, value = line.partition('=')
        if section == 'development' and key.strip() == TRAVEL_BUILD_KEY and separator:
            while not value.rstrip().endswith('}'):
                if index >= len(lines) or index - number >= 16 or len(value) > 4096:
                    _fail('ConfigurationSyntax', 'unbounded or incomplete travel build dictionary')
                value += '\n' + lines[index]; index += 1
            if len(value) > 4096:
                _fail('ConfigurationSyntax', 'travel build dictionary exceeds its bounded syntax')
            line = key + '=' + value
        yield number, line


def _project_semantics(raw: bytes) -> dict:
    """仅识别当前单行Godot配置，忽略节/键排列；值文本精确保留，重复或未知语法直接拒绝。"""
    sections: dict[str, dict] = {'': {}}
    section = ''
    number = r'[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?'
    for index, line in _project_lines(raw):
        stripped = line.strip()
        if not stripped or stripped.startswith(';'):
            continue
        header = re.fullmatch(r'\[([A-Za-z0-9_/-]+)\]', stripped)
        if header:
            section = header.group(1)
            if section in sections:
                _fail('ConfigurationSyntax', 'duplicate section at line ' + str(index))
            sections[section] = {}
            continue
        key, separator, value = line.partition('=')
        key = key.strip()
        if not separator or not re.fullmatch(r'[A-Za-z0-9_./-]+', key) or key in sections[section]:
            _fail('ConfigurationSyntax', 'invalid or duplicate key at line ' + str(index))
        if section == 'development' and key == TRAVEL_BUILD_KEY:
            try:
                parsed = json.loads(value, object_pairs_hook=_travel_build_pairs)
            except ValueError:
                _fail('ConfigurationSyntax', 'invalid travel build dictionary at line ' + str(index))
            _check_travel_build(parsed)
            sections[section][key] = parsed
            continue
        token = value.strip()
        supported = token in {'true', 'false', 'null'} or re.fullmatch(number, token) is not None
        try:
            if token.startswith('"'):
                supported = isinstance(json.loads(token), str)
            elif token.startswith('PackedStringArray(') and token.endswith(')'):
                values = json.loads('[' + token[len('PackedStringArray('):-1] + ']')
                supported = isinstance(values, list) and all(isinstance(item, str) for item in values)
            elif token.startswith('Color(') and token.endswith(')'):
                values = token[len('Color('):-1].split(',')
                supported = len(values) in {3, 4} and all(re.fullmatch(number, item.strip()) for item in values)
        except ValueError:
            supported = False
        if not supported:
            _fail('ConfigurationSyntax', 'unsupported single-line value at line ' + str(index))
        sections[section][key] = value
    return sections


def _project_semantic_sha256(raw: bytes) -> str:
    """只对此配置按唯一节/键映射摘要，其他运行源码和资源仍绑定原字节。"""
    return hashlib.sha256(content_stage.canonical(_project_semantics(raw))).hexdigest()


def _materialize(root: Path, commit: str, output: Path, entries: dict) -> None:
    """按固定Git流式取出完整真实资源；缺对象失败保现场，不复制稀疏工作副本。"""
    import os
    process = subprocess.Popen(['git', 'archive', commit, *SOURCE_ROOTS], cwd=root, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, env={**os.environ, 'GIT_NO_LAZY_FETCH': '1', 'GIT_TERMINAL_PROMPT': '0'})
    try:
        with tarfile.open(fileobj=process.stdout, mode='r|') as archive:
            for member in archive:
                if member.isdir():
                    continue
                if not member.isfile():
                    _fail('UnsupportedGitEntry', member.name)
                if member.name not in entries:
                    continue
                target = output / member.name
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(member) as source, target.open('xb') as destination:
                    shutil.copyfileobj(source, destination)
        error = process.stderr.read().decode(errors='replace')
        if process.wait():
            _fail('GitSourceUnavailable', error)
    finally:
        if process.poll() is None:
            process.kill(); process.wait()


def _content_identity(output: Path, selected: list[dict]) -> dict:
    """复用正式stage验证/清单；代表入口来自完整作者目录或显式compose首代。"""
    folder = _regular_path(output / 'game/data/generated')
    content_stage.validate_stage(output, build.discover_author_sources(output), folder)
    installed = content_stage.read_pack(folder / content_stage.INSTALLED_INDEX)['sets']
    if (not isinstance(selected, list) or any(not isinstance(row, dict) or row not in installed for row in selected)
            or len({row['pack_set_digest'] for row in selected}) != len(selected)):
        _fail('SelectedContentMismatch', 'explicit_sets')
    relative = selected[0]['directory'] + '/catalog.hmcatalog.json' if selected else 'alpha_life.hmcatalog.json'
    catalog = content_stage.read_pack(folder / relative)
    scenarios = catalog['catalog']['scenarios']
    if not isinstance(scenarios, dict) or not scenarios:
        _fail('RepresentativeContentMissing', relative + ' has no scenarios')
    scenario_id = 'scenario.alpha_life' if not selected else next(iter(scenarios))
    if scenario_id not in scenarios or (not selected and catalog['content_id'] != 'foundation.alpha_life'):
        _fail('RepresentativeContentMissing', relative)
    entry = {'catalog_path': 'res://data/generated/' + relative, 'content_id': catalog['content_id'], 'scenario_id': scenario_id}
    if 'pack_set_digest' in catalog:
        entry['pack_set_digest'] = catalog['pack_set_digest']
    return {'entry': entry, 'mode': 'composed_content_set' if selected else 'current_prepared_catalog', 'foundation_sets': False,
            'stage_manifest_sha256': sha256_file(folder / content_stage.MANIFEST),
            'installed_index_sha256': sha256_file(folder / content_stage.INSTALLED_INDEX), 'explicit_sets': selected}


def _source_check(root: Path, commit: str, output: Path, entries: dict, storage: dict, *, profile: str = 'native-growth', expected_build: dict | None = None) -> str:
    """逐Git blob核实际运行原件；合法导入缓存不算源码，新增可执行源码不能漏检。"""
    rows = []
    for name, expected in entries.items():
        path = _regular_path(output / name)
        if path.is_symlink() or not path.is_file():
            _fail('StagedSourceMissing', name)
        raw = path.read_bytes()
        if name == 'game/project.godot':
            if _project_semantics(raw) != _project_semantics(_project_config(_blob(root, commit, name), storage, commit, profile=profile, expected_build=expected_build)):
                _fail('ConfigurationMismatch', name)
            rows.append({'path': name, 'semantic_sha256': _project_semantic_sha256(raw)})
            continue
        elif hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest() != expected:
            _fail('StagedSourceMismatch', name)
        rows.append({'path': name, 'sha256': hashlib.sha256(raw).hexdigest()})
    for prefix in SOURCE_ROOTS:
        for path in _regular_path(output / prefix).rglob('*'):
            name = path.relative_to(output).as_posix()
            if path.is_symlink():
                _fail('UnsafePath', name)
            if not path.is_file() or name in entries or name.startswith(('game/.godot/', 'game/data/generated/')):
                continue
            if name in {'game/' + LIBRARY, 'game/' + EXTENSION}:
                continue
            if prefix == 'tools' and '__pycache__' in path.parts and path.suffix in {'.pyc', '.pyo'}:
                continue
            if prefix == 'game' and path.suffix == '.import' and name.removesuffix('.import') in entries:
                continue
            # 正常GUI给固定脚本和唯一生成扩展描述补UID；限定对应原件与文本，不准入任意载荷。
            script_uid = name.endswith('.gd.uid') and name.removesuffix('.uid') in entries
            extension_uid = name == 'game/' + EXTENSION + '.uid' and (output / 'game' / EXTENSION).is_file()
            if (prefix == 'game' and (script_uid or extension_uid)
                    and re.fullmatch(rb'uid://[a-z0-9]{1,24}\n?', path.read_bytes())):
                continue
            _fail('UnexpectedStagedSource', name)
    return hashlib.sha256(content_stage.canonical(rows)).hexdigest()


def _prepare_profile(root: Path, output: Path, native_library: Path, content_sets=(), *, profile: str = 'native-growth') -> dict:
    """准备全新固定来源开发工程；末尾才发布manifest，失败现场保持且从不导入。"""
    _profile_spec(profile)
    root = _regular_path(Path(root)); output = _regular_path(Path(output)); library = _regular_path(Path(native_library))
    if output.exists() or root == output or root in output.parents or output in root.parents:
        _fail('OutputNotIsolated', str(output))
    source = _clean_source(root); commit = source['commit']
    entries = _source_entries(root, commit, profile=profile)
    _native_identity(root, commit, library, profile=profile)
    for path in content_sets:
        content_stage.composed_set_record(_regular_path(Path(path)), root)
    output.mkdir(parents=True, exist_ok=False)
    _materialize(root, commit, output, entries)
    return _complete_profile(root, output, library, content_sets, expected_source=source, profile=profile)


def _complete_profile(root: Path, output: Path, native_library: Path, content_sets=(), *, expected_source: dict | None = None, profile: str = 'native-growth') -> dict:
    """复用已物化固定原件的同一尾流程；续准备只限调用方已保全的失败目录，不提供CLI覆盖入口。"""
    spec = _profile_spec(profile)
    root = _regular_path(Path(root)); output = _regular_path(Path(output)); library = _regular_path(Path(native_library))
    if (not output.is_dir() or any((output / _profile_spec(name)['manifest']).exists() for name in ('native-growth', 'native-travel')) or root == output
            or root in output.parents or output in root.parents):
        _fail('OutputNotIsolated', str(output))
    source = _clean_source(root)
    if expected_source is not None and source != expected_source:
        _fail('SourceChangedDuringStage', str(root))
    commit = source['commit']; entries = _source_entries(root, commit, profile=profile); native = _native_identity(root, commit, library, profile=profile)
    selected_paths = [_regular_path(Path(path)) for path in content_sets]
    selected = [content_stage.composed_set_record(path, root) for path in selected_paths]
    storage = _storage(output, commit, uuid.uuid4().hex, profile=profile)
    expected_build = native.get('travel_build_expected')
    game = output / 'game'; (game / 'project.godot').write_bytes(_project_config(_blob(root, commit, 'game/project.godot'), storage, commit, profile=profile, expected_build=expected_build))
    _source_check(root, commit, output, entries, storage, profile=profile, expected_build=expected_build)
    # 此显式开发profile只准备当前完整作者；历史canonical六锁仍留在原默认stage职责中。
    build.stage_content(output, content_sets=selected_paths, expected_content_sets=selected, foundation_sets=False)
    (game / LIBRARY).parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(library, game / LIBRARY)
    extension = _blob(root, commit, 'native/godot/hanmo_world.gdextension.in').replace(b'@HANMO_BRIDGE_LIBRARY@', ('res://' + LIBRARY).encode())
    (game / EXTENSION).write_bytes(extension)
    content = _content_identity(output, selected)
    source_hash = _source_check(root, commit, output, entries, storage, profile=profile, expected_build=expected_build)
    if _clean_source(root) != source:
        _fail('SourceChangedDuringStage', str(root))
    if _native_identity(root, commit, game / LIBRARY, profile=profile) != native:
        _fail('NativeChangedDuringStage', str(library))
    manifest = {'format': spec['format'], 'profile': profile, 'game': str(game),
                'gd_source': {'commit': commit, 'tree': _git(root, 'rev-parse', commit + '^{tree}').decode().strip(),
                              'file_count': len(entries), 'staged_files_sha256': source_hash,
                              'exclude_other_game_tests': True, 'representative_test': spec['representative_test'],
                              'project_configuration_identity': PROJECT_IDENTITY},
                'native': native, 'engine': native['sdk_evidence']['sdk'], 'content': content, 'storage': storage,
                'configuration': {'project_sha256': sha256_file(game / 'project.godot'),
                                  'project_semantic_sha256': _project_semantic_sha256((game / 'project.godot').read_bytes()),
                                  'extension_sha256': sha256_file(game / EXTENSION), 'imported': False,
                                  'launch_arguments': ['--content=' + content['entry']['catalog_path'],
                                                       '--scenario=' + content['entry']['scenario_id']]}}
    manifest_path = output / spec['manifest']
    manifest_path.write_bytes(content_stage.canonical(manifest))
    return {'game': str(game), 'manifest_path': str(manifest_path), 'manifest': manifest}


def prepare_profile(root: Path, output: Path, native_library: Path, content_sets=(), *, profile: str = 'native-growth') -> dict:
    """对共享CLI统一RuntimeError失败协议；现场可保全，但失败永不产生成功返回。"""
    try:
        return _prepare_profile(root, output, native_library, content_sets, profile=profile)
    except RuntimeError as exc:
        if str(exc).startswith('NativeGrowthProfile/'):
            raise
        _fail('PreparationFailed', str(exc))
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError, tarfile.TarError) as exc:
        _fail('PreparationFailed', str(exc))


def _manifest_profile(output: Path, requested: str | None) -> str:
    """只识别唯一已知manifest；CLI显式mode不能误消费另一种profile或歧义目录。"""
    if requested is not None:
        _profile_spec(requested)
    found = [name for name in ('native-growth', 'native-travel') if (output / _profile_spec(name)['manifest']).exists()]
    if len(found) != 1 or (requested is not None and requested != found[0]):
        _fail('ProfileManifestMismatch', str(output))
    return found[0]


def validate_profile(root: Path, staged_project: Path, *, profile: str | None = None) -> dict:
    """只读重核固定GD/库/锁/完整内容/配置；导入是否可运行由显式verify另判。"""
    root = _regular_path(Path(root)); game = _regular_path(Path(staged_project)); output = game.parent
    try:
        profile = _manifest_profile(output, profile); spec = _profile_spec(profile)
        manifest_path = _regular_path(output / spec['manifest'])
        manifest = content_stage.read_pack(manifest_path)
        source = _clean_source(root)
        if (set(manifest) != {'format', 'profile', 'game', 'gd_source', 'native', 'engine', 'content', 'storage', 'configuration'}
                or manifest['format'] != spec['format'] or manifest['profile'] != profile or manifest['game'] != str(game) or game.name != 'game'):
            _fail('ManifestMismatch', str(manifest_path))
        commit = manifest['gd_source']['commit']
        entries = _same_runtime_source(root, commit, source['commit'], profile=profile)
        storage = _storage(output, commit, manifest['storage']['instance_nonce'], profile=profile)
        expected_build = manifest['native'].get('travel_build_expected')
        if storage != manifest['storage']:
            _fail('StorageMismatch', str(game))
        identity = {'commit': commit, 'tree': _git(root, 'rev-parse', commit + '^{tree}').decode().strip(),
                    'file_count': len(entries), 'staged_files_sha256': _source_check(root, commit, output, entries, storage, profile=profile, expected_build=expected_build),
                    'exclude_other_game_tests': True, 'representative_test': spec['representative_test'],
                    'project_configuration_identity': PROJECT_IDENTITY}
        if identity != manifest['gd_source']:
            _fail('StagedSourceMismatch', str(game))
        native = _native_identity(root, commit, game / LIBRARY, profile=profile)
        if (native != manifest['native'] or native['sdk_evidence']['sdk'] != manifest['engine']
                or _native_identity(root, source['commit'], game / LIBRARY, profile=profile) != native):
            _fail('NativeIdentityMismatch', str(game))
        extension = _blob(root, commit, 'native/godot/hanmo_world.gdextension.in').replace(b'@HANMO_BRIDGE_LIBRARY@', ('res://' + LIBRARY).encode())
        if (game / EXTENSION).read_bytes() != extension:
            _fail('ExtensionMismatch', str(game / EXTENSION))
        if _content_identity(output, manifest['content']['explicit_sets']) != manifest['content']:
            _fail('ContentMismatch', str(game))
        stage_project = _project_config(_blob(root, commit, 'game/project.godot'), storage, commit, profile=profile, expected_build=expected_build)
        expected = {'project_sha256': hashlib.sha256(stage_project).hexdigest(),
                    'project_semantic_sha256': _project_semantic_sha256((game / 'project.godot').read_bytes()),
                    'extension_sha256': sha256_file(game / EXTENSION), 'imported': False,
                    'launch_arguments': ['--content=' + manifest['content']['entry']['catalog_path'],
                                         '--scenario=' + manifest['content']['entry']['scenario_id']]}
        if expected != manifest['configuration'] or _clean_source(root) != source:
            _fail('ConfigurationMismatch', str(game))
    except RuntimeError as exc:
        if str(exc).startswith('NativeGrowthProfile/'):
            raise
        _fail('InvalidStage', str(exc))
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as exc:
        _fail('InvalidStage', str(exc))
    return {'game': str(game), 'manifest_path': str(manifest_path), 'manifest': manifest}
