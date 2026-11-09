"""验证内容staging的精确清单和来源；所有权仅限受识别产物，不替代运行时认证。"""
from __future__ import annotations

import hashlib
from pathlib import Path
import re

try:
    from tools.content.io import canonical, read_pack
    from tools.content.capability_contract import load_contract, runtime_declaration
except ModuleNotFoundError:
    from content.io import canonical, read_pack
    from content.capability_contract import load_contract, runtime_declaration

MANIFEST = 'content-stage.json'
SCHEMA = 'schemas/content.schema.json'
CAPABILITY = 'game/data/rule_capabilities.json'
CATALOG_NAME = re.compile(r'[^/\\\x00]+\.hmcatalog\.json\Z')
SCHEMA_INDEX = 'schema-index.json'
SCHEMA_HISTORY = 'schemas/content-history'
INSTALLED_INDEX = 'installed-content.json'
SET_DIRECTORY = 'content-sets'
PACK_SET_SCHEMA = 'pack-set.schema.json'
FOUNDATION_ARCHIVE = 'content/packs/legacy/archive'
FOUNDATION_BINDINGS = FOUNDATION_ARCHIVE + '/identity-bindings.json'
PNG_IMPORT_PARAMETERS = '''compress/mode=0
compress/high_quality=false
compress/lossy_quality=0.7
compress/uastc_level=0
compress/rdo_quality_loss=0.0
compress/hdr_compression=1
compress/normal_map=0
compress/channel_pack=0
mipmaps/generate=false
mipmaps/limit=-1
roughness/mode=0
roughness/src_normal=""
process/channel_remap/red=0
process/channel_remap/green=1
process/channel_remap/blue=2
process/channel_remap/alpha=3
process/fix_alpha_border=true
process/premult_alpha=false
process/normal_map_invert_y=false
process/hdr_as_srgb=false
process/hdr_clamp_exposure=false
process/size_limit=0
detect_3d/compress_to=1
'''


def digest(path: Path) -> str:
    """摘要绑定文件原字节，缺失文件直接阻断当前代的发布。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def input_record(root: Path, path: str) -> dict:
    """生成仓库相对输入记录，不把checkout路径或时钟写进产物。"""
    return {'path': path, 'sha256': digest(root / path)}


def schema_sources(root: Path) -> dict[str, Path]:
    """只枚举受信当前Schema与摘要命名的原字节归档，不从包声明接收路径。"""
    current = root / SCHEMA
    if current.is_symlink() or not current.is_file():
        raise ValueError('Invalid current schema source')
    result = {digest(current): current}
    history = root / SCHEMA_HISTORY
    if history.is_symlink():
        raise ValueError('Schema history must not be a symlink')
    if history.exists():
        for path in sorted(history.iterdir()):
            if (path.is_symlink() or not path.is_file()
                    or not re.fullmatch(r'[0-9a-f]{64}\.json', path.name)
                    or digest(path) != path.stem):
                raise ValueError('Schema archive digest mismatch: ' + path.name)
            result[path.stem] = path
    return dict(sorted(result.items()))


def write_schema_index(root: Path, directory: Path) -> None:
    """向本次新代写入精确归档和确定索引，历史Schema不升级或重格式化。"""
    sources = schema_sources(root)
    (directory / 'schemas').mkdir(parents=True, exist_ok=True)
    for sha, source in sources.items():
        (directory / 'schemas' / (sha + '.json')).write_bytes(source.read_bytes())
    index = {'format': 'HM-CONTENT-SCHEMA-INDEX/1',
             'schemas': {sha: 'schemas/' + sha + '.json' for sha in sources}}
    (directory / SCHEMA_INDEX).write_bytes(canonical(index))


def schema_index_records(directory: Path) -> list[dict]:
    """核索引的固定相对路径与实际归档摘要，拒绝自报路径和缺失或篡改字节。"""
    index = read_pack(directory / SCHEMA_INDEX)
    if (set(index) != {'format', 'schemas'} or index['format'] != 'HM-CONTENT-SCHEMA-INDEX/1'
            or not isinstance(index['schemas'], dict) or not index['schemas']):
        raise ValueError('Invalid trusted schema index')
    rows = [{'path': SCHEMA_INDEX, 'sha256': digest(directory / SCHEMA_INDEX)}]
    for sha, name in sorted(index['schemas'].items()):
        if (not re.fullmatch('[0-9a-f]{64}', sha) or name != 'schemas/' + sha + '.json'
                or (directory / name).is_symlink() or digest(directory / name) != sha):
            raise ValueError('Indexed schema digest or path mismatch')
        rows.append({'path': name, 'sha256': sha})
    return rows


def files_in(directory: Path) -> dict[str, Path]:
    """列出真实普通文件；拒绝链接和特殊文件，避免复制或删除越过所有权边界。"""
    if directory.is_symlink() or (directory.exists() and not directory.is_dir()):
        raise ValueError('Content stage is not a regular directory: ' + str(directory))
    result = {}
    for path in sorted(directory.rglob('*')):
        if path.is_symlink() or not (path.is_file() or path.is_dir()):
            raise ValueError('Unsupported content stage entry: ' + str(path))
        if path.is_file():
            result[path.relative_to(directory).as_posix()] = path
    return result


def validate_catalog(path: Path, source: dict, schema_sha: str, capability_sha: str, authored: dict | None = None) -> None:
    """验证实际索引身份、原始输入和规范目录摘要；成功退出码不等于有效产物。"""
    catalog = read_pack(path)
    if (catalog.get('format') != 'HM-CONTENT-INDEX' or type(catalog.get('format_version')) is not int
            or catalog['format_version'] != 1 or catalog.get('status') != 'AUTHORING_ONLY'
            or catalog.get('runtime_verified') is not False or type(catalog.get('content_abi')) is not int
            or catalog['content_abi'] != 1 or not isinstance(catalog.get('catalog'), dict)
            or not isinstance(catalog.get('content_id'), str) or not catalog['content_id']
            or not isinstance(catalog.get('content_version'), str) or not catalog['content_version']):
        raise ValueError('Invalid prepared catalog identity: ' + path.name)
    expected = {'source_sha256': source['sha256'], 'schema_sha256': schema_sha,
                'rule_capabilities_sha256': capability_sha,
                'catalog_sha256': hashlib.sha256(canonical(catalog['catalog'])).hexdigest()}
    if authored is not None:
        expected.update(content_id=authored['id'], content_version=authored['version'], content_abi=authored['schema_version'])
    if any(catalog.get(key) != value for key, value in expected.items()):
        raise ValueError('Prepared catalog provenance mismatch: ' + path.name)


def make_manifest(root: Path, directory: Path, sources: list[Path]) -> dict:
    """从本次全新产物构造确定性清单，每份索引均绑定对应源及共享合同。"""
    schema = input_record(root, SCHEMA); capability = input_record(root, CAPABILITY)
    rows = []
    for source in sorted(sources):
        name = source.stem + '.hmcatalog.json'
        if not CATALOG_NAME.fullmatch(name):
            raise ValueError('Unsupported author filename: ' + source.name)
        provenance = input_record(root, source.relative_to(root).as_posix())
        validate_catalog(directory / name, provenance, schema['sha256'], capability['sha256'], read_pack(source))
        rows.append({'path': name, 'sha256': digest(directory / name), 'source': provenance})
    rows.extend({'path': name, 'sha256': digest(directory / name)} for name in ('content.schema.json', 'runtime_capabilities.json'))
    index = read_pack(directory / SCHEMA_INDEX)
    expected = {sha: 'schemas/' + sha + '.json' for sha in schema_sources(root)}
    if index.get('schemas') != expected:
        raise ValueError('Schema index does not match trusted source archives')
    rows.extend(schema_index_records(directory))
    if (directory / INSTALLED_INDEX).exists():
        if (directory / PACK_SET_SCHEMA).read_bytes() != (root / 'schemas' / PACK_SET_SCHEMA).read_bytes():
            raise ValueError('Installed pack lock schema differs from trusted source')
        rows.extend(installed_index_records(directory))
    return {'format': 'HM-CONTENT-STAGE/1', 'schema': schema, 'capability': capability,
            'files': sorted(rows, key=lambda row: row['path'])}


def _record(value, expected_path: str) -> None:
    """限定记录键和相对路径，禁止manifest借路径穿越扩大清理范围。"""
    if (not isinstance(value, dict) or set(value) != {'path', 'sha256'} or value.get('path') != expected_path
            or not isinstance(value.get('sha256'), str) or not re.fullmatch('[0-9a-f]{64}', value['sha256'])):
        raise ValueError('Invalid content provenance record: ' + expected_path)


def read_manifest(directory: Path) -> dict:
    """核验上一代清单及受识别产物，不能凭任意新增路径获得删除用户文件的权限。"""
    actual = files_in(directory)
    manifest = read_pack(directory / MANIFEST)
    if set(manifest) != {'format', 'schema', 'capability', 'files'} or manifest.get('format') != 'HM-CONTENT-STAGE/1':
        raise ValueError('Invalid content staging manifest')
    _record(manifest['schema'], SCHEMA); _record(manifest['capability'], CAPABILITY)
    rows = manifest['files']
    if not isinstance(rows, list) or not rows:
        raise ValueError('Empty content staging manifest')
    names = []
    archive_rows = {row['path']: row for row in schema_index_records(directory)} if SCHEMA_INDEX in actual else {}
    installed_rows = {row['path']: row for row in installed_index_records(directory)} if INSTALLED_INDEX in actual else {}
    for row in rows:
        if not isinstance(row, dict) or not isinstance(row.get('path'), str):
            raise ValueError('Invalid content manifest entry')
        name = row['path']; names.append(name)
        _record({key: row.get(key) for key in ('path', 'sha256')}, name)
        if name not in actual or digest(actual[name]) != row['sha256']:
            raise ValueError('Content stage bytes differ from manifest: ' + name)
        if CATALOG_NAME.fullmatch(name):
            if set(row) != {'path', 'sha256', 'source'}:
                raise ValueError('Catalog ownership requires source: ' + name)
            _record(row['source'], 'content/foundation/' + name.removesuffix('.hmcatalog.json') + '.json')
            validate_catalog(actual[name], row['source'], manifest['schema']['sha256'], manifest['capability']['sha256'])
        elif name in installed_rows and row == installed_rows[name]:
            pass
        elif name in archive_rows and row == archive_rows[name]:
            pass
        elif name in ('content.schema.json', 'runtime_capabilities.json') and set(row) == {'path', 'sha256'}:
            if name == 'content.schema.json' and row['sha256'] != manifest['schema']['sha256']:
                raise ValueError('Staged schema digest mismatch')
            if name == 'runtime_capabilities.json':
                declaration = read_pack(actual[name])
                if (declaration.get('format') != 'HM-RUNTIME-CAPABILITIES' or declaration.get('runtime_verified') is not False
                        or declaration.get('rule_capabilities_sha256') != manifest['capability']['sha256']):
                    raise ValueError('Staged runtime declaration mismatch')
        else:
            raise ValueError('Unrecognized content ownership: ' + name)
    if names != sorted(set(names)) or not {'content.schema.json', 'runtime_capabilities.json'} < set(names):
        raise ValueError('Incomplete or duplicate content ownership')
    if archive_rows and (not set(archive_rows) <= set(names)
                         or 'schemas/' + manifest['schema']['sha256'] + '.json' not in archive_rows):
        raise ValueError('Incomplete trusted schema archive ownership')
    if not set(installed_rows) <= set(names):
        raise ValueError('Incomplete installed content ownership')
    return manifest


def composed_set_record(directory: Path, trusted_root: Path | None = None, *, lock_schema: dict | None = None,
                        allow_import_sidecars: bool = False) -> dict:
    """核验compose原代全部字节和身份；安装来源不能以自报摘要替代目录、锁或Schema。"""
    try:
        from tools.content.packs import MAX_MEMBERS, MAX_TOTAL_BYTES, RESOURCE_EXTENSIONS, _relative
        from tools.content.pack_compose import _artifact_json
        from tools.content.validation import StrictValidator
    except ModuleNotFoundError:
        from content.packs import MAX_MEMBERS, MAX_TOTAL_BYTES, RESOURCE_EXTENSIONS, _relative
        from content.pack_compose import _artifact_json
        from content.validation import StrictValidator
    actual = files_in(directory)
    if len(actual) > MAX_MEMBERS or sum(path.stat().st_size for path in actual.values()) > MAX_TOTAL_BYTES:
        raise ValueError('Installed content set exceeds compose limits')
    required = {'catalog.hmcatalog.json', 'pack-set.lock.json', 'presentation.json',
                'source-map.json', SCHEMA_INDEX, '.pack-generation.json'}
    if not required <= set(actual):
        raise ValueError('Incomplete composed content set')
    raw = {name: path.read_bytes() for name, path in actual.items()}
    marker = _artifact_json(raw['.pack-generation.json'])
    if set(marker) != {'format', 'files'} or marker['format'] != 'HM-PACK-GENERATION/1' or not isinstance(marker['files'], dict):
        raise ValueError('Composed generation ownership or bytes mismatch')
    if allow_import_sidecars:
        lock_sha = hashlib.sha256(raw['pack-set.lock.json']).hexdigest()
        derived = _verified_png_imports(raw, marker['files'], lock_sha)
        raw = {name: data for name, data in raw.items() if name not in derived}
        actual = {name: path for name, path in actual.items() if name not in derived}
    hashes = {name: hashlib.sha256(data).hexdigest() for name, data in raw.items()}
    if (set(marker) != {'format', 'files'} or marker['format'] != 'HM-PACK-GENERATION/1'
            or marker['files'] != {name: sha for name, sha in hashes.items() if name != '.pack-generation.json'}):
        raise ValueError('Composed generation ownership or bytes mismatch')
    lock = _artifact_json(raw['pack-set.lock.json'])
    contract_root = trusted_root or Path(__file__).resolve().parents[1]
    lock_schema = lock_schema if lock_schema is not None else read_pack(contract_root / 'schemas' / PACK_SET_SCHEMA)
    if not StrictValidator(lock_schema).is_valid(lock) or raw['pack-set.lock.json'] != canonical(lock):
        raise ValueError('Invalid composed pack lock')
    lock_sha = hashes['pack-set.lock.json']
    header = _artifact_json(raw['catalog.hmcatalog.json'])
    if not isinstance(header.get('source_sha256'), str) or not re.fullmatch(r'[0-9a-f]{64}', header['source_sha256']):
        raise ValueError('Invalid composed source provenance')
    validate_catalog(directory / 'catalog.hmcatalog.json', {'sha256': header.get('source_sha256')},
                     lock['schema_sha256'], lock['rule_capabilities_sha256'])
    schema_rows = schema_index_records(directory)
    expected_schema = {'format': 'HM-CONTENT-SCHEMA-INDEX/1',
                       'schemas': {lock['schema_sha256']: 'schemas/' + lock['schema_sha256'] + '.json'}}
    if (_artifact_json(raw[SCHEMA_INDEX]) != expected_schema
            or header['catalog_sha256'] != lock['catalog_sha256']
            or hashes['presentation.json'] != lock['presentation_sha256']):
        raise ValueError('Composed content lock does not match actual artifacts')
    if trusted_root is not None:
        trusted_schemas = schema_sources(trusted_root)
        if (lock['schema_sha256'] not in trusted_schemas
                or digest(trusted_root / CAPABILITY) != lock['rule_capabilities_sha256']):
            raise ValueError('Composed content requires an unregistered schema or rule capability contract')
    if 'pack_set_digest' in header:
        if (header['pack_set_digest'] != lock_sha or 'legacy-identity.json' in actual
                or header['content_id'] != 'packset.' + lock_sha or header['content_version'] != '1.0.0'
                or header['source_sha256'] != hashlib.sha256(canonical(lock['packs'])).hexdigest()):
            raise ValueError('Composed content pack identity mismatch')
    else:
        expected_legacy = {'format': 'HM-LEGACY-CONTENT-IDENTITY', 'format_version': 1,
                           'content_id': header['content_id'], 'content_version': header['content_version'],
                           'catalog_sha256': header['catalog_sha256'],
                           'catalog_file_sha256': hashes['catalog.hmcatalog.json'], 'pack_set_digest': lock_sha}
        if 'legacy-identity.json' not in raw or _artifact_json(raw['legacy-identity.json']) != expected_legacy:
            raise ValueError('Legacy content set requires its exact identity sidecar')
    _verify_resource_digests(raw, lock['packs'], contract_root)
    allowed = required | {'legacy-identity.json'} | {row['path'] for row in schema_rows}
    extensions = set().union(*RESOURCE_EXTENSIONS.values())
    for name in actual:
        _relative(name, str(directory))
        if name not in allowed and not (name.startswith('assets/') and Path(name).suffix.lower() in extensions):
            raise ValueError('Unrecognized composed content file: ' + name)
    return {'pack_set_digest': lock_sha, 'directory': SET_DIRECTORY + '/' + lock_sha,
            'files': [{'path': name, 'sha256': hashes[name]} for name in sorted(actual)]}


def _verified_png_imports(raw: dict[str, bytes], owned: dict, lock_sha: str) -> set[str]:
    """只识别实测Godot 4.7.2默认PNG派生元数据；不授予其清理所有权或忽略未知.import。"""
    result = set()
    for name in set(raw) - set(owned) - {'.pack-generation.json'}:
        source = name.removesuffix('.import')
        if not name.endswith('.png.import') or source not in owned:
            continue
        uid = re.search(rb'^uid="(uid://[a-z0-9]{1,32})"$', raw[name], re.MULTILINE)
        if uid is None:
            continue
        resource = 'res://data/generated/' + SET_DIRECTORY + '/' + lock_sha + '/' + source
        target = 'res://.godot/imported/' + Path(source).name + '-' + hashlib.md5(resource.encode('utf8')).hexdigest() + '.ctex'
        expected = ('[remap]\n\nimporter="texture"\ntype="CompressedTexture2D"\nuid="'
                    + uid.group(1).decode('ascii') + '"\npath="' + target
                    + '"\nmetadata={\n"vram_texture": false\n}\n\n[deps]\n\nsource_file="'
                    + resource + '"\ndest_files=["' + target + '"]\n\n[params]\n\n' + PNG_IMPORT_PARAMETERS)
        if raw[name] == expected.encode('utf8'):
            result.add(name)
    return result


def _legacy_manifest_sources(root: Path) -> dict[Path, bytes]:
    """只读取仓内五个固定legacy适配声明；包目录和锁不能提供替代受信来源。"""
    try:
        from tools.content.packs import LEGACY_SOURCES
    except ModuleNotFoundError:
        from content.packs import LEGACY_SOURCES
    result = {}
    for name in sorted(LEGACY_SOURCES):
        path = root / 'content/packs/legacy' / name / 'pack.json'
        if not path.exists():
            continue
        if any(part.is_symlink() for part in (path, *path.parents)) or not path.is_file():
            raise ValueError('Invalid trusted legacy manifest')
        if read_pack(path).get('legacy_source') != name:
            raise ValueError('Invalid trusted legacy source mapping')
        result[path] = path.read_bytes()
    return result


def _verify_resource_digests(raw: dict[str, bytes], packs: list[dict], root: Path) -> None:
    """从既有composer路径规则恢复资源清单并核锁摘要，legacy只由受信manifest原字节识别。"""
    try:
        from tools.content.packs import _digest_files
        from tools.content.pack_compose import _artifact_json
    except ModuleNotFoundError:
        from content.packs import _digest_files
        from content.pack_compose import _artifact_json
    trusted = {hashlib.sha256(data).hexdigest(): _artifact_json(data)
               for data in _legacy_manifest_sources(root).values()}
    resources = {name: data for name, data in raw.items() if name.startswith('assets/')}
    legacy, regular, claimed = [], [], set()
    for pack in packs:
        manifest = trusted.get(pack['manifest_sha256'])
        if manifest is not None:
            if any(pack[key] != manifest[key] for key in ('id', 'namespace', 'version')):
                raise ValueError('UntrustedLegacyArtifact: manifest identity mismatch')
            legacy.append(pack)
        else:
            regular.append(pack)
    for pack in regular:
        prefix = 'assets/art/packs/' + pack['namespace'] + '/'
        original = {name.removeprefix(prefix): data for name, data in resources.items() if name.startswith(prefix)}
        if _digest_files(original, list(original)) != pack['resources_sha256']:
            raise ValueError('Installed resource digest differs from pack lock')
        claimed.update(name for name in resources if name.startswith(prefix))
    remaining = set(resources) - claimed
    if legacy:
        original = {'game/' + name: resources[name] for name in remaining}
        expected = _digest_files(original, list(original))
        if any(pack['resources_sha256'] != expected for pack in legacy):
            raise ValueError('Installed legacy resource digest differs from pack lock')
    elif remaining:
        raise ValueError('UntrustedLegacyArtifact: resources lack a trusted pack manifest')


def installed_index_records(directory: Path) -> list[dict]:
    """只从固定受管索引枚举摘要目录；存档、作者manifest及文件名不能提供安装位置。"""
    path = directory / INSTALLED_INDEX
    if any(candidate.is_symlink() for candidate in (directory, directory / SET_DIRECTORY, path)):
        raise ValueError('Installed content index must not traverse symlinks')
    index = read_pack(path)
    if (set(index) != {'format', 'format_version', 'sets'} or index['format'] != 'HM-INSTALLED-CONTENT'
            or type(index['format_version']) is not int or index['format_version'] != 1
            or not isinstance(index['sets'], list)):
        raise ValueError('Invalid installed content index')
    lock_schema_path = directory / PACK_SET_SCHEMA
    if lock_schema_path.is_symlink():
        raise ValueError('Installed pack lock schema must not be a symlink')
    lock_schema = read_pack(lock_schema_path)
    records = [{'path': INSTALLED_INDEX, 'sha256': digest(path)},
               {'path': PACK_SET_SCHEMA, 'sha256': digest(lock_schema_path)}]
    identities = []
    for entry in index['sets']:
        if (not isinstance(entry, dict) or set(entry) != {'pack_set_digest', 'directory', 'files'}
                or not isinstance(entry['pack_set_digest'], str)
                or not re.fullmatch(r'[0-9a-f]{64}', entry['pack_set_digest'])
                or entry['directory'] != SET_DIRECTORY + '/' + entry['pack_set_digest']):
            raise ValueError('Invalid installed content directory or digest')
        if entry != composed_set_record(directory / entry['directory'], lock_schema=lock_schema, allow_import_sidecars=True):
            raise ValueError('Installed content index differs from actual set bytes')
        identities.append(entry['pack_set_digest'])
        records.extend({'path': entry['directory'] + '/' + row['path'], 'sha256': row['sha256']}
                       for row in entry['files'])
    if identities != sorted(set(identities)):
        raise ValueError('Installed content sets must be unique and sorted')
    return records


def stage_installed_sets(root: Path, prepared: Path, previous: Path, sources: list[Path], *,
                         foundation_sets: bool = True) -> tuple[dict, dict]:
    """把已受管代与显式compose输入按锁摘要并存复制，返回发布前必须复核的源快照。"""
    snapshots, memberships, entries = {}, {}, {}
    snapshots.update(_legacy_manifest_sources(root))
    schema_path = root / 'schemas' / PACK_SET_SCHEMA
    snapshots[schema_path] = schema_path.read_bytes()
    (prepared / PACK_SET_SCHEMA).write_bytes(snapshots[schema_path])
    selected = []
    if (previous / MANIFEST).is_file():
        read_manifest(previous)
        if (previous / INSTALLED_INDEX).is_file():
            index_path = previous / INSTALLED_INDEX
            snapshots[index_path] = index_path.read_bytes()
            selected.extend((previous / entry['directory'], False)
                            for entry in read_pack(index_path)['sets'])
    selected.extend((source, True) for source in sources)
    for source, incoming in selected:
        if any(part.is_symlink() for part in (source, *source.parents)):
            raise ValueError('Installed content input must not use symlinks')
        files = files_in(source)
        memberships[source] = sorted(files)
        frozen = {name: path.read_bytes() for name, path in files.items()}
        record = composed_set_record(source, root if incoming else None, allow_import_sidecars=not incoming)
        if read_pack(source / 'pack-set.lock.json')['rule_capabilities_sha256'] != digest(root / CAPABILITY):
            raise ValueError('InstalledRuleCapabilitiesMismatch: installed set requires a different rule descriptor')
        for name, raw in frozen.items():
            path = files[name]
            if path in snapshots and snapshots[path] != raw:
                raise ValueError('Installed content input changed during snapshot')
            snapshots[path] = raw
        identity = record['pack_set_digest']
        if identity in entries and entries[identity] != record:
            raise ValueError('Same content lock has different installed artifact bytes')
        target = prepared / record['directory']
        for name in (row['path'] for row in record['files']):
            raw = frozen[name]
            destination = target / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(raw)
        if composed_set_record(target) != record:
            raise ValueError('Installed content source changed while copying')
        entries[identity] = record
    if foundation_sets:
        for path, raw in stage_foundation_sets(root, prepared, entries).items():
            if path in snapshots and snapshots[path] != raw:
                raise ValueError('Foundation binding inputs changed during snapshot')
            snapshots[path] = raw
    index = {'format': 'HM-INSTALLED-CONTENT', 'format_version': 1,
             'sets': [entries[identity] for identity in sorted(entries)]}
    (prepared / INSTALLED_INDEX).write_bytes(canonical(index))
    return snapshots, memberships


def _bound_bytes(path: Path, expected: str, snapshots: dict) -> bytes:
    """只读取固定受信绑定指向的原件，缺件、链接或摘要变化都阻止发布并保留旧代。"""
    if not re.fullmatch(r'[0-9a-f]{64}', expected):
        raise ValueError('IncompleteLegacyBinding: invalid digest')
    if any(part.is_symlink() for part in (path, *path.parents)) or not path.is_file():
        raise ValueError('MissingLegacyBinding: ' + path.name)
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != expected:
        raise ValueError('IncompleteLegacyBinding: changed original ' + path.name)
    if path in snapshots and snapshots[path] != raw:
        raise ValueError('Foundation binding inputs changed during snapshot')
    snapshots[path] = raw
    return raw


def _legacy_identity(header: dict) -> tuple:
    """v1只按明确内容ID、版本和数据摘要定位，文件名、源Schema或美术默认值不参与猜配。"""
    return tuple(header[key] for key in ('content_id', 'content_version', 'catalog_sha256'))


def stage_foundation_sets(root: Path, prepared: Path, installed: dict[str, dict]) -> dict[Path, bytes]:
    """把当前五份作者源安装为新packset；保留原ID数据，不沿用历史header、sidecar或旧规则锁。"""
    try:
        from tools.content.pack_compose import _artifact_json, _lock, _leaves, _source, _schema_artifacts, _new_header
        from tools.content.packs import LEGACY_SOURCES, _digest_files, resolve_pack_set
        from tools.presentation import project_inputs, validate_references
    except ModuleNotFoundError:
        from content.pack_compose import _artifact_json, _lock, _leaves, _source, _schema_artifacts, _new_header
        from content.packs import LEGACY_SOURCES, _digest_files, resolve_pack_set
        from presentation import project_inputs, validate_references
    manifests = _legacy_manifest_sources(root)
    if not manifests:
        return {}
    if len(manifests) != len(LEGACY_SOURCES):
        raise ValueError('IncompleteCurrentFoundation: expected five trusted adapters')
    paths = project_inputs(root) + [root / SCHEMA, root / CAPABILITY, root / 'game/data/pack_runtime.json']
    if any(part.is_symlink() for path in paths for part in (path, *path.parents)):
        raise ValueError('Current foundation inputs must not traverse symlinks')
    snapshots = {path: path.read_bytes() for path in paths}
    snapshots.update(manifests)
    schema_raw = snapshots[root / SCHEMA]
    schema_sha = hashlib.sha256(schema_raw).hexdigest()
    capability_sha = hashlib.sha256(snapshots[root / CAPABILITY]).hexdigest()
    presentation_raw = snapshots[root / 'game/data/presentation.json']
    presentation = _artifact_json(presentation_raw)
    presentation_output = canonical(presentation)
    resource_bytes = {path.relative_to(root).as_posix(): raw for path, raw in snapshots.items()
                      if path.is_relative_to(root / 'game/assets')}
    runtime = {'rules_api': _artifact_json(snapshots[root / 'game/data/pack_runtime.json']),
               'capabilities': [row['capability_id'] for row in load_contract(root / CAPABILITY)['capabilities']]}
    schema = _artifact_json(schema_raw)
    for manifest_path, manifest_raw in sorted(manifests.items()):
        manifest = _artifact_json(manifest_raw)
        name = manifest['legacy_source']
        source_path = root / 'content/foundation' / (name + '.json')
        if any(part.is_symlink() for part in (source_path, *source_path.parents)):
            raise ValueError('Current foundation source must not traverse symlinks')
        source_raw = source_path.read_bytes(); snapshots[source_path] = source_raw
        source = _artifact_json(source_raw)
        if any(manifest[key] != source[key] for key in ('id', 'version')):
            raise ValueError('CurrentFoundationIdentityMismatch: ' + name)
        if set(source['required_capabilities']) - set(manifest['required_capabilities']):
            raise ValueError('MissingCurrentFoundationCapability: ' + name)
        header_path = prepared / (name + '.hmcatalog.json')
        validate_catalog(header_path, {'sha256': hashlib.sha256(source_raw).hexdigest()},
                         schema_sha, capability_sha, source)
        catalog = read_pack(header_path)['catalog']
        projected = {table: {row['id']: row for row in source[table]}
                     for table in schema['x-hanmo-tables'] if table in source}
        projected.update({key: source[key] for key in ('texts', 'flags', 'required_capabilities')})
        if catalog != projected:
            raise ValueError('CurrentFoundationCatalogMismatch: ' + name)
        if validate_references(presentation, source):
            raise ValueError('Invalid current foundation presentation references: ' + name)
        files = {source_path.relative_to(root).as_posix(): source_raw,
                 'game/data/presentation.json': presentation_raw}
        pack = {'manifest': manifest, 'manifest_path': manifest_path,
                'manifest_sha256': hashlib.sha256(manifest_raw).hexdigest(),
                'content_sha256': _digest_files(files, list(files)),
                'resources_sha256': _digest_files(resource_bytes, list(resource_bytes))}
        resolved = resolve_pack_set([pack], [{key: manifest[key] for key in ('id', 'version')}], runtime)
        lock = _lock([pack], resolved['rules_api'], catalog, presentation_output, schema_sha, capability_sha)
        lock_raw = canonical(lock); identity = hashlib.sha256(lock_raw).hexdigest()
        origins = {}
        for path in _leaves(catalog):
            author = path
            if len(path) > 1 and path[0] in schema['x-hanmo-tables']:
                author = (path[0], next(i for i, row in enumerate(source[path[0]]) if row['id'] == path[1])) + path[2:]
            origins['/catalog' + ''.join('/' + str(part).replace('~', '~0').replace('/', '~1') for part in path)] = _source(
                pack, source_path.relative_to(root).as_posix(), str(path[1]) if len(path) > 1 else '', author)
        for path in _leaves(presentation):
            origins['/presentation' + ''.join('/' + str(part).replace('~', '~0').replace('/', '~1') for part in path)] = _source(
                pack, 'game/data/presentation.json', str(path[1]) if len(path) > 1 else '', path)
        artifacts = {'catalog.hmcatalog.json': canonical(_new_header(catalog, lock)),
                     'pack-set.lock.json': lock_raw, 'presentation.json': presentation_output,
                     **{path.removeprefix('game/'): raw for path, raw in resource_bytes.items()},
                     **_schema_artifacts(schema_raw),
                     'source-map.json': canonical({'format': 'HM-CONTENT-SOURCE-MAP', 'format_version': 1, 'entries': origins})}
        artifacts['.pack-generation.json'] = canonical({'format': 'HM-PACK-GENERATION/1',
            'files': {path: hashlib.sha256(raw).hexdigest() for path, raw in sorted(artifacts.items())}})
        destination = prepared / SET_DIRECTORY / identity
        if identity in installed:
            if any((destination / path).read_bytes() != raw for path, raw in artifacts.items()):
                raise ValueError('CurrentFoundationBindingConflict: same lock has different artifacts')
            continue
        for path, raw in artifacts.items():
            target = destination / path; target.parent.mkdir(parents=True, exist_ok=True); target.write_bytes(raw)
        installed[identity] = composed_set_record(destination, root)
    return snapshots


def stage_archived_foundation_sets(root: Path, prepared: Path, installed: dict[str, dict]) -> dict[Path, bytes]:
    """独立核验六个历史原件的canonical锁；不加入正常当前安装索引，不授予当前运行兼容。"""
    try:
        from tools.content.pack_compose import _artifact_json, _lock, _leaves, _source, _schema_artifacts
        from tools.content.packs import LEGACY_SOURCES, _digest_files, _relative
        from tools.content.validation import StrictValidator
    except ModuleNotFoundError:
        from content.pack_compose import _artifact_json, _lock, _leaves, _source, _schema_artifacts
        from content.packs import LEGACY_SOURCES, _digest_files, _relative
        from content.validation import StrictValidator
    binding_path = root / FOUNDATION_BINDINGS
    if not binding_path.exists():
        if (root / FOUNDATION_ARCHIVE).exists():
            raise ValueError('MissingLegacyBinding: identity-bindings.json')
        return {}
    if any(part.is_symlink() for part in (binding_path, *binding_path.parents)):
        raise ValueError('MissingLegacyBinding: linked identity-bindings.json')
    binding_raw = binding_path.read_bytes()
    binding = _artifact_json(binding_raw)
    if (set(binding) != {'format', 'format_version', 'baseline_commit', 'rules_api',
                        'rule_capabilities_sha256', 'presentation_sha256', 'resources', 'entries'}
            or binding['format'] != 'HM-FOUNDATION-IDENTITY-BINDINGS'
            or type(binding['format_version']) is not int or binding['format_version'] != 1):
        raise ValueError('IncompleteLegacyBinding: invalid fixed index')
    snapshots = {binding_path: binding_raw}
    observed_path = root / FOUNDATION_ARCHIVE / 'manifest.json'
    if any(part.is_symlink() for part in (observed_path, *observed_path.parents)) or not observed_path.is_file():
        raise ValueError('MissingLegacyBinding: original archive index')
    observed_raw = observed_path.read_bytes(); snapshots[observed_path] = observed_raw
    observed = _artifact_json(observed_raw)
    if (observed.get('format') != 'HM-A2-OBSERVED-LEGACY-BASELINE/1'
            or observed.get('observed_central_commit') != binding['baseline_commit']):
        raise ValueError('IncompleteLegacyBinding: observed archive baseline changed')
    capability_raw = _bound_bytes(root / FOUNDATION_ARCHIVE / (binding['rule_capabilities_sha256'] + '.rule-capabilities.json'),
                                  binding['rule_capabilities_sha256'], snapshots)
    runtime_path = root / 'game/data/pack_runtime.json'
    runtime_raw = runtime_path.read_bytes(); snapshots[runtime_path] = runtime_raw
    if _artifact_json(runtime_raw) != binding['rules_api']:
        raise ValueError('IncompleteLegacyBinding: rules API changed')
    presentation_raw = _bound_bytes(root / FOUNDATION_ARCHIVE / 'presentation.json', binding['presentation_sha256'], snapshots)
    presentation = _artifact_json(presentation_raw)
    expected_resources = sorted({'game/' + asset['path'].removeprefix('res://')
                                 for group in ('backgrounds', 'portraits') for asset in presentation[group].values()})
    resources = binding['resources']
    if not isinstance(resources, list) or [row.get('path') for row in resources] != expected_resources:
        raise ValueError('IncompleteLegacyBinding: presentation resource membership')
    for row in resources:
        _record(row, row['path']); _relative(row['path'], FOUNDATION_BINDINGS)
        if not row['path'].startswith('game/assets/'):
            raise ValueError('IncompleteLegacyBinding: resource outside game assets')
    entries = binding['entries']
    slots = set(LEGACY_SOURCES) | {'alpha_life.current'}
    if (not isinstance(entries, list) or len(entries) != len(slots)
            or {row.get('slot') for row in entries} != slots):
        raise ValueError('IncompleteLegacyBinding: expected five originals and current Alpha')
    by_identity = {}
    for row in entries:
        if (set(row) != {'slot', 'source_name', 'content_id', 'content_version', 'catalog_sha256',
                         'catalog_file_sha256', 'source_sha256', 'schema_sha256', 'manifest_sha256', 'pack_set_digest'}
                or row['source_name'] != row['slot'].removesuffix('.current')
                or not re.fullmatch(r'[0-9a-f]{64}', row['pack_set_digest'])):
            raise ValueError('IncompleteLegacyBinding: invalid canonical identity')
        identity = _legacy_identity(row)
        if identity in by_identity:
            raise ValueError('IncompleteLegacyBinding: duplicate v1 identity')
        by_identity[identity] = row
        if not row['slot'].endswith('.current'):
            original = observed.get('records', {}).get(row['source_name'], {})
            if any(original.get(key) != row[key] for key in ('content_id', 'content_version', 'catalog_sha256',
                       'catalog_file_sha256', 'source_sha256', 'schema_sha256')):
                raise ValueError('IncompleteLegacyBinding: original observed identity changed')
    for record in installed.values():
        if any(row['path'] == 'legacy-identity.json' for row in record['files']):
            header = read_pack(prepared / record['directory'] / 'catalog.hmcatalog.json')
            fixed = by_identity.get(_legacy_identity(header))
            if fixed and fixed['pack_set_digest'] != record['pack_set_digest']:
                raise ValueError('LegacyBindingConflict: same v1 identity has a different lock')
    resource_bytes = None
    for entry in entries:
        slot = entry['slot']; name = entry['source_name']
        header_raw = _bound_bytes(root / FOUNDATION_ARCHIVE / (slot + '.hmcatalog.json'), entry['catalog_file_sha256'], snapshots)
        source_raw = _bound_bytes(root / FOUNDATION_ARCHIVE / (slot + '.source.json'), entry['source_sha256'], snapshots)
        schema_raw = _bound_bytes(root / FOUNDATION_ARCHIVE / (entry['schema_sha256'] + '.schema.json'), entry['schema_sha256'], snapshots)
        manifest_raw = _bound_bytes(root / 'content/packs/legacy' / name / 'pack.json', entry['manifest_sha256'], snapshots)
        header, source, schema, manifest = [_artifact_json(raw) for raw in (header_raw, source_raw, schema_raw, manifest_raw)]
        projected = {table: {row['id']: row for row in source[table]} for table in schema['x-hanmo-tables'] if table in source}
        projected.update({key: source[key] for key in ('texts', 'flags', 'required_capabilities')})
        validate_catalog(root / FOUNDATION_ARCHIVE / (slot + '.hmcatalog.json'),
                         {'sha256': entry['source_sha256']}, entry['schema_sha256'], binding['rule_capabilities_sha256'], source)
        if (not StrictValidator(schema).is_valid(source) or header['catalog'] != projected
                or _legacy_identity(header) != _legacy_identity(entry) or 'pack_set_digest' in header
                or manifest.get('legacy_source') != name or manifest['id'] != header['content_id']
                or manifest['version'] != header['content_version']):
            raise ValueError('IncompleteLegacyBinding: original header, source, schema or manifest mismatch')
        files = {'content/foundation/' + name + '.json': source_raw, 'game/data/presentation.json': presentation_raw}
        pack = {'manifest': manifest, 'manifest_sha256': entry['manifest_sha256'],
                'content_sha256': _digest_files(files, list(files)),
                'resources_sha256': hashlib.sha256(canonical(resources)).hexdigest()}
        presentation_output = canonical(presentation)
        lock = _lock([pack], binding['rules_api'], projected, presentation_output,
                     entry['schema_sha256'], hashlib.sha256(capability_raw).hexdigest())
        lock_raw = canonical(lock); identity = hashlib.sha256(lock_raw).hexdigest()
        if identity != entry['pack_set_digest']:
            raise ValueError('IncompleteLegacyBinding: canonical lock changed')
        if identity in installed:
            original = next(row['sha256'] for row in installed[identity]['files'] if row['path'] == 'catalog.hmcatalog.json')
            if original != entry['catalog_file_sha256']:
                raise ValueError('LegacyBindingConflict: canonical header bytes changed')
            continue
        if resource_bytes is None:
            resource_bytes = {}
            cached = next((prepared / installed[row['pack_set_digest']]['directory'] for row in entries
                           if row['pack_set_digest'] in installed), None)
            for resource in resources:
                logical = resource['path'].removeprefix('game/')
                path = cached / logical if cached is not None else root / resource['path']
                resource_bytes[logical] = _bound_bytes(path, resource['sha256'], snapshots)
        origins = {}
        for path in _leaves(projected):
            author = path
            if len(path) > 1 and path[0] in schema['x-hanmo-tables']:
                author = (path[0], next(i for i, row in enumerate(source[path[0]]) if row['id'] == path[1])) + path[2:]
            origins['/catalog' + ''.join('/' + str(part).replace('~', '~0').replace('/', '~1') for part in path)] = _source(
                pack, FOUNDATION_ARCHIVE + '/' + slot + '.source.json', str(path[1]) if len(path) > 1 else '', author)
        for path in _leaves(presentation):
            origins['/presentation' + ''.join('/' + str(part).replace('~', '~0').replace('/', '~1') for part in path)] = _source(
                pack, FOUNDATION_ARCHIVE + '/presentation.json', str(path[1]) if len(path) > 1 else '', path)
        artifacts = {'catalog.hmcatalog.json': header_raw, 'pack-set.lock.json': lock_raw,
                     'presentation.json': presentation_output, **resource_bytes, **_schema_artifacts(schema_raw),
                     'source-map.json': canonical({'format': 'HM-CONTENT-SOURCE-MAP', 'format_version': 1, 'entries': origins}),
                     'legacy-identity.json': canonical({'format': 'HM-LEGACY-CONTENT-IDENTITY', 'format_version': 1,
                         **{key: header[key] for key in ('content_id', 'content_version', 'catalog_sha256')},
                         'catalog_file_sha256': entry['catalog_file_sha256'], 'pack_set_digest': identity})}
        artifacts['.pack-generation.json'] = canonical({'format': 'HM-PACK-GENERATION/1',
            'files': {path: hashlib.sha256(raw).hexdigest() for path, raw in sorted(artifacts.items())}})
        destination = prepared / SET_DIRECTORY / identity
        for path, raw in artifacts.items():
            target = destination / path; target.parent.mkdir(parents=True, exist_ok=True); target.write_bytes(raw)
        installed[identity] = composed_set_record(destination)
    return snapshots


def validate_stage(root: Path, sources: list[Path], directory: Path | None = None, *, allow_unowned: bool = False) -> dict:
    """验证当前源集合与完整产物清单；发行默认拒绝任何未受管文件，防止隐式打包。"""
    directory = directory or root / 'game/data/generated'
    try:
        manifest = read_manifest(directory)
        expected = make_manifest(root, directory, sources)
        if manifest != expected:
            raise ValueError('Content staging manifest does not match current sources')
        declaration = runtime_declaration(load_contract(root / CAPABILITY), path=root / CAPABILITY)
        if read_pack(directory / 'runtime_capabilities.json') != declaration:
            raise ValueError('Content declaration does not match trusted capability contract')
        owned = {row['path'] for row in manifest['files']} | {MANIFEST}
        extra = set(files_in(directory)) - owned
        if (directory / INSTALLED_INDEX).is_file():
            # read_manifest已逐字核验唯一可识别的派生sidecar；只保留，不纳删除所有权。
            index = read_pack(directory / INSTALLED_INDEX)
            derived = {entry['directory'] + '/' + row['path'] + '.import'
                       for entry in index['sets'] for row in entry['files'] if row['path'].endswith('.png')}
            extra -= derived
        if extra and not allow_unowned:
            raise ValueError('Unowned content blocks export: ' + ', '.join(sorted(extra)))
        return manifest
    except (OSError, ValueError, KeyError, TypeError) as exc:
        raise RuntimeError('Invalid content stage: ' + str(exc)) from exc


def package_files(root: Path, manifest: dict) -> list[dict]:
    """列出包内必须逐字保留的资源，额外绑定清单自身及生成目录外的受信描述。"""
    rows = [{'path': 'res://data/generated/' + row['path'], 'sha256': row['sha256']} for row in manifest['files']]
    rows += [{'path': 'res://data/generated/' + MANIFEST, 'sha256': digest(root / 'game/data/generated' / MANIFEST)},
             {'path': 'res://data/rule_capabilities.json', 'sha256': manifest['capability']['sha256']},
             {'path': 'res://data/module_contracts.json', 'sha256': digest(root / 'game/data/module_contracts.json')}]
    return sorted(rows, key=lambda row: row['path'])
