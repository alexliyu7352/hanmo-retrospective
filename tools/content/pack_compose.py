"""以冻结包快照限定声明引用并组合只读目录；作者源与正式生成目录始终只读。"""
from __future__ import annotations

from copy import deepcopy
import hashlib
import json
import os
import shutil
import tempfile
from pathlib import Path
import re

from .io import canonical, _unique_pairs, _reject_constant, _check_tree
from .pack_errors import PackError
from .adult_composition_resources import validate_resources as validate_composition_resources
from .packs import (PACK_SET_SCHEMA, SELECTION_VALIDATOR,
                    discover_packs, load_pack_runtime, resolve_pack_set)
from .references import _iter_annotated, iter_key_references
from .validation import ROOT, SCHEMA, SCHEMA_BYTES, TABLES, StrictValidator, _bounded, pointer, validate_pack

LOCAL_ID = re.compile(r'[a-z][a-z0-9_.-]{0,79}\Z')


def _partial_content_schema() -> dict:
    """只降低根集合下限，记录字段及所有类型约束继续取同一内容Schema。"""
    schema = deepcopy(SCHEMA)
    for table in TABLES:
        schema['properties'][table]['minItems'] = 0
    schema['properties']['texts']['minProperties'] = 0
    schema['properties']['required_capabilities']['minItems'] = 0
    return schema


PARTIAL_CONTENT_SCHEMA = _partial_content_schema()
PARTIAL_CONTENT_VALIDATOR = StrictValidator(PARTIAL_CONTENT_SCHEMA)


def validate_pack_source(source: dict, location: str = 'content.json') -> None:
    """部分包入口只查结构和本地定义身份，跨文件引用语义延后到组合后。"""
    if not _bounded(source):
        raise PackError('InvalidPackSource', '入口超过既有结构边界', {'source': location})
    errors = sorted(PARTIAL_CONTENT_VALIDATOR.iter_errors(source), key=lambda error: str(list(error.absolute_path)))
    if errors:
        raise PackError('InvalidPackSource', '入口结构不符合共享内容Schema', {'source': location, 'issues': [
            {'code': 'SCHEMA', 'path': pointer(error.absolute_path), 'message': error.message[:600],
             'hint': '保留记录字段约束；跨包引用须显式声明直接依赖'} for error in errors]})
    definitions = [('id', source['id'])]
    for table in TABLES:
        definitions.extend((pointer((table, index, 'id')), record['id']) for index, record in enumerate(source.get(table, [])))
    for table in ('texts', 'flags'):
        definitions.extend((pointer((table, key)), key) for key in source[table])
    # 嵌套全局身份只由受信Schema显式声明；剧本覆盖重复同一身份不是新定义冲突。
    definitions.extend((pointer(path), identifier) for path, _scope, identifier in
                       _iter_annotated(source, annotation='x-hanmo-global-definition')
                       if not (_initial_unit_override_path(path) and '::' in identifier))
    for path, identifier in definitions:
        if not LOCAL_ID.fullmatch(identifier):
            raise PackError('QualifiedPackDefinition', '新包自定义ID必须是原80字符本地ID', {'source': location, 'path': path, 'id': identifier, 'hint': '仅引用可写namespace::local_id；定义不加前缀'})


def _source(pack: dict, file: str, record_id: str, field_path: tuple, chain=None) -> dict:
    """生成无机器路径的作者定位，引用链只描述静态限定关系。"""
    return {'pack_id': pack['manifest']['id'], 'file': file, 'record_id': record_id,
            'field_path': pointer(field_path), 'reference_chain': list(chain or [])}


def _assign(value, path: tuple, replacement) -> None:
    """沿已经通过Schema的字段路径赋值，不对普通字符串做替换。"""
    cursor = value
    for part in path[:-1]:
        cursor = cursor[part]
    cursor[path[-1]] = replacement


def _qualified(reference: str, pack: dict, selected: dict[str, dict], location: str) -> str:
    """本地引用只进本包；跨namespace必须已选直接依赖，旧ID需显式映射。"""
    if not reference:
        return reference
    manifest = pack['manifest']
    own = manifest['namespace']
    if '::' not in reference:
        return own + '::' + reference
    namespace, _ = reference.split('::', 1)
    if namespace == own:
        return reference
    dependency = selected.get(namespace)
    direct = {item['id'] for item in manifest['dependencies']}
    if dependency is None or dependency['manifest']['id'] not in direct:
        raise PackError('UndeclaredPackDependency', '跨包引用必须指向已选直接依赖', {'source': location, 'pack_id': manifest['id'], 'reference': reference,
            'target_pack': dependency['manifest']['id'] if dependency else None, 'hint': '在manifest.dependencies与selection中明确加入目标包'})
    if 'legacy_source' in dependency['manifest']:
        if reference not in manifest.get('legacy_imports', {}):
            raise PackError('MissingLegacyImport', '旧包引用须经legacy_imports明确导入', {'source': location, 'reference': reference, 'hint': '提供限定引用到原ID的显式映射'})
        return manifest['legacy_imports'][reference]
    return reference


def _initial_unit_override_path(path: tuple) -> bool:
    """只识别情景驻军初态的单位ID；其他嵌套定义仍须本包local身份。"""
    return (len(path) == 8 and path[0] == 'scenarios' and type(path[1]) is int
            and path[2:4] == ('initial', 'garrisons') and type(path[4]) is int
            and path[5] == 'units' and type(path[6]) is int and path[7] == 'id')


def _validate_initial_unit_override(source: dict, pack: dict, selected: dict,
                                    path: tuple, identifier: str, location: str) -> None:
    """跨包初态只能覆盖直接依赖中同一驻军的既有单位，不定义或改名另一部队。"""
    if not _initial_unit_override_path(path) or '::' not in identifier:
        raise PackError('QualifiedPackDefinition', '仅驻军单位初态允许已声明依赖的既有身份',
                        {'source': location, 'path': pointer(path), 'id': identifier})
    namespace, local_unit = identifier.split('::', 1)
    if namespace == pack['manifest']['namespace']:
        raise PackError('QualifiedPackDefinition', '本包自定义单位仍使用local身份',
                        {'source': location, 'path': pointer(path), 'id': identifier})
    _qualified(identifier, pack, selected, location + '#' + pointer(path))
    dependency = selected[namespace]
    if 'legacy_source' in dependency['manifest']:
        raise PackError('UnsupportedInitialUnitOverride', '旧导入包未提供此单位覆盖合同',
                        {'source': location, 'path': pointer(path), 'id': identifier})
    camp = source['scenarios'][path[1]]['initial']['garrisons'][path[4]]
    garrison_id = _qualified(camp['id'], pack, selected, location + '#' + pointer(path[:5]))
    if not garrison_id.startswith(namespace + '::'):
        raise PackError('UnknownInitialUnitOverride', '单位须来自所覆盖驻军的同一依赖包',
                        {'source': location, 'path': pointer(path), 'id': identifier, 'garrison_id': garrison_id})
    local_camp = garrison_id.split('::', 1)[1]
    matches = []
    for entry in dependency['manifest']['content_entries']:
        content = json.loads(dependency['files'][entry])
        for defined in content.get('garrisons', []):
            if defined['id'] == local_camp:
                matches.extend(unit for unit in defined['units'] if unit['id'] == local_unit)
    if len(matches) != 1:
        raise PackError('UnknownInitialUnitOverride', '直接依赖的同一驻军须唯一声明该单位',
                        {'source': location, 'path': pointer(path), 'id': identifier, 'garrison_id': garrison_id})


def _qualify_content(source: dict, pack: dict, file: str, selected: dict[str, dict]) -> tuple[dict, dict]:
    """只限定实体、显式嵌套全局身份及注解引用，语义数组和真正局部ID保持原序。"""
    validate_pack_source(source, pack['manifest']['id'] + '/' + file)
    for path, _scope, identifier in _iter_annotated(source, annotation='x-hanmo-global-definition'):
        if '::' in identifier:
            _validate_initial_unit_override(source, pack, selected, path, identifier, file)
    result = deepcopy(source)
    namespace = pack['manifest']['namespace']
    chains = {}
    for annotation in ('x-hanmo-ref', 'x-hanmo-presentation',
                       'x-hanmo-global-definition', 'x-hanmo-global-ref'):
        for path, _target, reference in _iter_annotated(source, annotation=annotation):
            qualified = _qualified(reference, pack, selected, file + '#' + pointer(path))
            _assign(result, path, qualified)
            chains[path] = [reference, qualified] if reference != qualified else [reference]
    # 先完成值路径，再由内至外改身份键，避免父键改名使未处理的子路径悬空。
    keyed = sorted(iter_key_references(source), key=lambda entry: len(entry[0]), reverse=True)
    for path, _table, reference in keyed:
        qualified = _qualified(reference, pack, selected, file + '#' + pointer(path + (reference,)))
        cursor = result
        for part in path:
            cursor = cursor[part]
        if qualified != reference and qualified in cursor:
            raise PackError('DuplicatePackDefinition', '限定身份键发生冲突', {'source': file, 'path': pointer(path), 'id': qualified})
        if qualified != reference:
            replacement = {qualified if key == reference else key: value for key, value in cursor.items()}
            cursor.clear()
            cursor.update(replacement)
            old_prefix, new_prefix = path + (reference,), path + (qualified,)
            for location, chain in list(chains.items()):
                if location[:len(old_prefix)] == old_prefix:
                    chains[new_prefix + location[len(old_prefix):]] = chain
                    del chains[location]
        chains[path + (qualified,)] = [reference, qualified]
    for table in TABLES:
        for record in result.get(table, []):
            record['id'] = namespace + '::' + record['id']
    for table in ('texts', 'flags'):
        result[table] = {namespace + '::' + key: value for key, value in result[table].items()}
    return result, chains


def _leaves(value, path=()):
    """枚举所有产物叶子和空容器，来源映射不遗漏作者可定位字段。"""
    if isinstance(value, dict) and value:
        for key, child in value.items():
            yield from _leaves(child, path + (key,))
    elif isinstance(value, list) and value:
        for index, child in enumerate(value):
            yield from _leaves(child, path + (index,))
    else:
        yield path


def _merge_content(packs: list[dict]) -> tuple[dict, dict]:
    """按限定ID确定合并并保留逐字段来源，重复定义或缺声明立即失败。"""
    selected = {pack['manifest']['namespace']: pack for pack in packs}
    merged = {'format': 'HM-CONTENT', 'schema_version': 1, 'id': 'packset.pending', 'version': '1.0.0',
              'required_capabilities': [], 'texts': {}, 'flags': {}}
    merged.update({table: [] for table in TABLES})
    source_map, all_ids, capabilities = {}, {}, set()
    for pack in packs:
        manifest = pack['manifest']
        legacy = 'legacy_source' in manifest
        declared = set(manifest['required_capabilities'])
        capabilities.update(declared)
        entries = ([file for file in pack['content_paths'] if file.endswith('/' + manifest['legacy_source'] + '.json')]
                   if legacy else manifest['content_entries'])
        for file in sorted(entries):
            original = json.loads(pack['files'][file])
            qualified, chains = ((deepcopy(original), {}) if legacy else _qualify_content(original, pack, file, selected))
            if set(original['required_capabilities']) - declared:
                raise PackError('MissingPackCapabilityDeclaration', '包manifest须声明入口要求的全部能力', {'pack_id': manifest['id'], 'file': file, 'missing_capabilities': sorted(set(original['required_capabilities']) - declared)})
            for table in TABLES:
                for index, record in enumerate(qualified.get(table, [])):
                    identifier = record['id']
                    origin = _source(pack, file, original[table][index]['id'], (table, index))
                    if identifier in all_ids:
                        raise PackError('DuplicatePackDefinition', '限定实体ID全表必须唯一', {'id': identifier, 'sources': [all_ids[identifier], origin]})
                    all_ids[identifier] = origin
                    merged[table].append(record)
                    for leaf in _leaves(record):
                        author_path = (table, index) + leaf
                        original_path = list(author_path)
                        for length in range(1, len(author_path) + 1):
                            chain = chains.get(author_path[:length])
                            if chain and len(chain) == 2 and author_path[length - 1] == chain[1]:
                                original_path[length - 1] = chain[0]
                        source_map[pointer(('catalog', table, identifier) + leaf)] = _source(pack, file, original[table][index]['id'], tuple(original_path), chains.get(author_path))
            for table in ('texts', 'flags'):
                for local, raw in original[table].items():
                    key = local if legacy else manifest['namespace'] + '::' + local
                    if key in merged[table]:
                        raise PackError('DuplicatePackDefinition', '限定字典键不能重复', {'table': table, 'id': key})
                    merged[table][key] = raw
                    source_map[pointer(('catalog', table, key))] = _source(pack, file, local, (table, local))
    merged['required_capabilities'] = sorted(capabilities)
    for table in TABLES:
        merged[table].sort(key=lambda record: record['id'])
    # 被动旧包不追加空卡片表，旧作者目录仍保持原字段集合。
    if not merged.get('insight_cards'):
        merged.pop('insight_cards', None)
    if not merged.get('cards'):
        merged.pop('cards', None)
    if not merged.get('legacy_control_consent_requests'):
        merged.pop('legacy_control_consent_requests', None)
    if not merged.get('succession_consent_requests'):
        merged.pop('succession_consent_requests', None)
    if not merged.get('adult_adoption_requests'):
        merged.pop('adult_adoption_requests', None)
    if not merged.get('teaching_requests'):
        merged.pop('teaching_requests', None)
    if not merged.get('study_teaching_requests'):
        merged.pop('study_teaching_requests', None)
    if not merged.get('actor_cards'):
        merged.pop('actor_cards', None)
    report = validate_pack(merged, 'composed')
    if not report.ok:
        issues = []
        for issue in report.issues:
            origin = all_ids.get(issue.record_id)
            issues.append({'code': issue.code, 'path': issue.path, 'message': issue.message,
                           'source': origin, 'hint': '按来源修正定义或引用；组合失败不发布任何新代'})
        raise PackError('InvalidComposedContent', '组合内容未通过完整共享语义校验', {'issues': issues})
    return merged, source_map


MAX_CATALOG_BYTES = 4 * 1024 * 1024
GENERATION_FILE = '.pack-generation.json'
LEGACY_BASELINE_ROOT = ROOT / 'content/packs/legacy/archive'


def _sha(raw: bytes) -> str:
    """只对冻结字节求摘要，绝不在哈希阶段重读作者源。"""
    return hashlib.sha256(raw).hexdigest()


def _trusted_inputs() -> dict[Path, bytes]:
    """冻结工程提供的Schema、规则能力及API声明，最终发布前逐字节复核。"""
    paths = [ROOT / 'schemas/content.schema.json', ROOT / 'game/data/presentation.schema.json',
             ROOT / 'game/data/rule_capabilities.json', ROOT / 'game/data/pack_runtime.json',
             ROOT / 'game/data/adult_appearance_compositions.json', ROOT / 'game/data/accepted_adult_appearances.json']
    result = {path: path.read_bytes() for path in paths}
    if result[paths[0]] != SCHEMA_BYTES:
        raise PackError('SourceChanged', '已加载Schema与当前工程字节不同，须重新加载工具')
    return result


def _input_snapshots(packs: list[dict], trusted: dict[Path, bytes]) -> dict[Path, bytes | None]:
    """保留所有参与输入的原字节及可选缺件，不把绝对路径写入产物。"""
    snapshots = dict(trusted)
    for pack in packs:
        current = {pack['manifest_path']: pack['manifest_bytes']}
        current.update({path: pack['files'].get(name) for name, path in pack['file_paths'].items()})
        for path, raw in current.items():
            if path in snapshots and snapshots[path] != raw:
                raise PackError('SourceChanged', '同一输入路径在冻结过程中出现不同字节', {'source': str(path)})
            snapshots[path] = raw
    return snapshots


def _unchanged(snapshots: dict[Path, bytes | None]) -> None:
    """删除、改字节、变链接或可选缺件突然出现都阻止发布新代。"""
    for path, expected in sorted(snapshots.items(), key=lambda pair: str(pair[0])):
        try:
            if any(candidate.is_symlink() for candidate in (path, *path.parents)):
                raise OSError('source became a link')
            if expected is None:
                if path.exists():
                    raise OSError('optional resource appeared')
                continue
            with path.open('rb') as stream:
                if stream.read(len(expected) + 1) != expected:
                    raise OSError('source bytes changed')
        except OSError as exc:
            raise PackError('SourceChanged', '编译期间输入发生变化，未发布新代', {'source': str(path), 'hint': '重新加载最新作者源后再编译'}) from exc


def _lock(packs: list[dict], rules_api: dict, catalog: dict, presentation: bytes,
          schema_sha256: str, capability_sha256: str) -> dict:
    """锁只含包摘要与目录数据摘要，不含路径、时间或自身digest，避免哈希环。"""
    fields = ('manifest_sha256', 'content_sha256', 'resources_sha256')
    result = {'format': 'HM-PACK-SET', 'format_version': 1,
              'packs': [{**{field: pack['manifest'][field] for field in ('id', 'namespace', 'version')},
                         **{field: pack[field] for field in fields}} for pack in packs],
              'rules_api': dict(rules_api), 'rule_capabilities_sha256': capability_sha256,
              'schema_sha256': schema_sha256, 'catalog_sha256': _sha(canonical(catalog)),
              'presentation_sha256': _sha(presentation)}
    errors = list(StrictValidator(PACK_SET_SCHEMA).iter_errors(result))
    if errors:
        raise PackError('InvalidComposedLock', '内部组合锁不符合冻结合同', {'errors': [error.message for error in errors]})
    return result


def _catalog(merged: dict) -> dict:
    """沿现有只读索引形状生成目录数据，固定表顺序和记录ID顺序。"""
    result = {table: {row['id']: row for row in merged[table]} for table in TABLES if table in merged}
    result.update(texts=merged['texts'], flags=merged['flags'], required_capabilities=merged['required_capabilities'])
    return result


def _new_header(catalog: dict, lock: dict) -> dict:
    """先有目录数据和锁再写header身份，旧接口使用packset兼容ID。"""
    digest = _sha(canonical(lock))
    return {'format': 'HM-CONTENT-INDEX', 'format_version': 1, 'status': 'AUTHORING_ONLY',
            'runtime_verified': False, 'content_id': 'packset.' + digest, 'content_version': '1.0.0',
            'content_abi': 1, 'source_sha256': _sha(canonical(lock['packs'])),
            'schema_sha256': lock['schema_sha256'], 'rule_capabilities_sha256': lock['rule_capabilities_sha256'],
            'catalog_sha256': lock['catalog_sha256'], 'pack_set_digest': digest, 'catalog': catalog}


def _artifact_json(raw: bytes) -> dict:
    """受限读取本工具产物与旧目录原件，继续拒绝重复键和非法JSON值。"""
    if len(raw) > MAX_CATALOG_BYTES:
        raise PackError('PackArtifactLimit', '目录或生成清单超过4MiB')
    try:
        value = json.loads(raw.decode('utf8'), object_pairs_hook=_unique_pairs, parse_constant=_reject_constant)
        _check_tree(value)
        if not isinstance(value, dict):
            raise ValueError('object expected')
        return value
    except (ValueError, UnicodeError, RecursionError) as exc:
        raise PackError('InvalidPackArtifact', '生成产物不是合法受限JSON对象') from exc


def _protected_output(output: Path, roots: list[Path]) -> Path:
    """组合目标不得覆盖任何作者输入或工程源码；已有代只准由所有权清单替换。"""
    output = output.absolute()
    if any(path.is_symlink() for path in (output, *output.parents)):
        raise PackError('ProtectedPackOutput', '组合目标不得经过链接')
    target = output.resolve()
    protected = [ROOT / name for name in ('content', 'schemas', 'game', 'tools', 'tests')]
    protected.extend(Path(root).resolve() for root in roots)
    for path in protected:
        if target == path or target.is_relative_to(path) or path.is_relative_to(target):
            raise PackError('ProtectedPackOutput', '组合目标与作者源或工程保护目录重叠', {'output': str(output)})
    if output.exists() and not output.is_dir():
        raise PackError('ProtectedPackOutput', '组合目标不是目录', {'output': str(output)})
    return output


def _generation_files(directory: Path) -> dict[str, Path]:
    """枚举生成代内的普通文件；链接和特殊文件不能获得替换授权。"""
    result = {}
    for path in sorted(directory.rglob('*')):
        if path.is_symlink() or not (path.is_file() or path.is_dir()):
            raise PackError('UnownedPackOutput', '生成目录含链接或特殊文件')
        if path.is_file():
            result[path.relative_to(directory).as_posix()] = path
    return result


def _verify_generation(directory: Path) -> dict:
    """只有本工具完整持有且逐字摘要匹配的旧代可以整体替换。"""
    actual = _generation_files(directory)
    if GENERATION_FILE not in actual:
        raise PackError('UnownedPackOutput', '已有输出目录缺少本工具所有权清单')
    marker = _artifact_json(actual[GENERATION_FILE].read_bytes())
    if (set(marker) != {'format', 'files'} or marker['format'] != 'HM-PACK-GENERATION/1'
            or not isinstance(marker['files'], dict) or set(actual) != set(marker['files']) | {GENERATION_FILE}):
        raise PackError('UnownedPackOutput', '已有输出含清单外文件或所有权清单无效')
    for name, digest in marker['files'].items():
        if _sha(actual[name].read_bytes()) != digest:
            raise PackError('UnownedPackOutput', '已有生成文件经过外部修改，保留原目录', {'file': name})
    return marker


def _publish(output: Path, artifacts: dict[str, bytes], snapshots: dict[Path, bytes | None], validate=None) -> list[str]:
    """干净临时代回验后替换整代；失败回滚旧目录，拒绝覆盖未知文件。"""
    output.parent.mkdir(parents=True, exist_ok=True)
    lock_path = output.parent / ('.' + output.name + '.compose.lock')
    try:
        descriptor = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    except FileExistsError as exc:
        raise PackError('PackComposeBusy', '同一输出已有组合任务') from exc
    os.close(descriptor)
    stage = backup = None
    warnings = []
    try:
        previous = _verify_generation(output) if output.exists() else None
        stage = Path(tempfile.mkdtemp(prefix='.' + output.name + '.stage-', dir=output.parent))
        for name, raw in sorted(artifacts.items()):
            target = stage / name
            if not target.resolve().is_relative_to(stage.resolve()) or target.name == GENERATION_FILE:
                raise PackError('InvalidPackArtifact', '生成文件路径不属于本代')
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open('wb') as stream:
                stream.write(raw)
                stream.flush()
                os.fsync(stream.fileno())
        if validate is not None:
            validate(stage)
        marker = {'format': 'HM-PACK-GENERATION/1', 'files': {name: _sha(raw) for name, raw in sorted(artifacts.items())}}
        marker_raw = canonical(marker)
        _artifact_json(marker_raw)
        (stage / GENERATION_FILE).write_bytes(marker_raw)
        _verify_generation(stage)
        _unchanged(snapshots)
        if output.exists():
            if previous != _verify_generation(output):
                raise PackError('PackOutputChanged', '输出代在编译期间已改变')
            backup = Path(tempfile.mkdtemp(prefix='.' + output.name + '.previous-', dir=output.parent))
            backup.rmdir()
            os.replace(output, backup)
        try:
            os.replace(stage, output)
            stage = None
        except OSError:
            if backup is not None:
                try:
                    os.replace(backup, output)
                    backup = None
                except OSError as rollback_error:
                    raise PackError('PackPublishRollbackRequired', '目录切换和自动回滚均失败；原代保存在备份目录', {'output': str(output), 'backup': str(backup)}) from rollback_error
            raise
        if backup is not None:
            try:
                shutil.rmtree(backup)
                backup = None
            except OSError:
                warnings.append('PreviousGenerationCleanupPending')
    except OSError as exc:
        raise PackError('PackPublishFailed', '无法发布整代内容；可用旧代已保留', {'output': str(output)}) from exc
    finally:
        if stage is not None and stage.exists():
            shutil.rmtree(stage)
        try:
            lock_path.unlink(missing_ok=True)
        except OSError:
            warnings.append('ComposeLockCleanupPending')
    return warnings


def _read_frozen(path: Path, snapshots: dict[Path, bytes | None]) -> bytes:
    """读取受信归档的有界普通文件并加入发布前复核，不接受调用方自报路径。"""
    try:
        if any(candidate.is_symlink() for candidate in (path, *path.parents)):
            raise OSError('archive link')
        with path.open('rb') as stream:
            raw = stream.read(MAX_CATALOG_BYTES + 1)
        _artifact_json(raw)
        snapshots[path] = raw
        return raw
    except OSError as exc:
        raise PackError('LegacyArtifactRequired', '缺少已核原字节的受信旧产物归档', {'file': path.name, 'hint': '由工程安装已核旧目录，不能用当前源伪造历史'}) from exc


def _legacy_header(pack: dict, snapshots: dict[Path, bytes | None]) -> tuple[bytes, bytes]:
    """核对真实旧目录、源和Schema三者字节；源已改变时绝不继承旧身份。"""
    name = pack['manifest']['legacy_source']
    index = _artifact_json(_read_frozen(LEGACY_BASELINE_ROOT / 'manifest.json', snapshots))
    if index.get('format') != 'HM-A2-OBSERVED-LEGACY-BASELINE/1' or name not in index.get('records', {}):
        raise PackError('InvalidLegacyArtifact', '受信旧产物索引不含所选旧包')
    expected = index['records'][name]
    raw = _read_frozen(LEGACY_BASELINE_ROOT / (name + '.hmcatalog.json'), snapshots)
    source_raw = _read_frozen(LEGACY_BASELINE_ROOT / (name + '.source.json'), snapshots)
    schema_digest = expected['schema_sha256']
    if not re.fullmatch(r'[0-9a-f]{64}', schema_digest):
        raise PackError('InvalidLegacyArtifact', '归档Schema摘要格式错误')
    schema_raw = _read_frozen(LEGACY_BASELINE_ROOT / (schema_digest + '.schema.json'), snapshots)
    header, source, schema = _artifact_json(raw), _artifact_json(source_raw), _artifact_json(schema_raw)
    if (_sha(raw) != expected['catalog_file_sha256'] or _sha(source_raw) != expected['source_sha256'] or
            _sha(schema_raw) != schema_digest or header.get('schema_sha256') != schema_digest or
            header.get('source_sha256') != expected['source_sha256'] or
            header.get('content_id') != expected['content_id'] or header.get('content_version') != expected['content_version'] or
            header.get('catalog_sha256') != expected['catalog_sha256'] or
            _sha(canonical(header.get('catalog'))) != expected['catalog_sha256'] or
            header.get('format') != 'HM-CONTENT-INDEX' or header.get('format_version') != 1 or
            'pack_set_digest' in header or not StrictValidator(schema).is_valid(source)):
        raise PackError('InvalidLegacyArtifact', '旧产物、源或Schema字节与受信索引不符')
    source_file = next((file for file in pack['content_paths'] if file.endswith('/' + name + '.json')), None)
    if source_file is None or pack['files'][source_file] != source_raw:
        raise PackError('LegacySourceChanged', '当前旧包作者源已不同于历史原件，不能沿用旧目录身份', {'id': pack['manifest']['id'], 'version': pack['manifest']['version'], 'expected_source_sha256': expected['source_sha256'], 'hint': '将当前源编译为新内容集；旧档继续使用精确归档'})
    expected_data = {table: {row['id']: row for row in source[table]} for table in TABLES if table in source}
    expected_data.update(texts=source['texts'], flags=source['flags'], required_capabilities=source['required_capabilities'])
    if header['catalog'] != expected_data:
        raise PackError('InvalidLegacyArtifact', '旧目录数据与所记录的作者源不符')
    return raw, schema_raw


def _check_presentation_references(merged: dict, presentation: dict) -> None:
    """按共享typed注解检查真实表现引用，空值仅沿原允许的旧默认分支。"""
    if (any(row.get('dynamic_world', {}).get('founder_policy') in ('fixed_actor_appearance_composition_v1', 'selected_existing_actor_v1')
            or (row.get('dynamic_world', {}).get('founder_policy') == 'selected_existing_actor_roster_v1'
                and any(policy.get('policy') == 'fixed_actor_appearance_composition_v1'
                        for policy in row.get('playable_actor_appearance_policies', [])))
            for row in merged.get('scenarios', [])) and 'adult_appearance_composition' not in presentation):
        raise PackError('MissingPackPresentationReference', '成人组合政策必须登记完整组合资源',
                        {'group': 'adult_appearance_composition'})
    for path, group, reference in _iter_annotated(merged, annotation='x-hanmo-presentation'):
        if reference == '' and group in ('backgrounds', 'portraits'):
            continue
        if reference not in presentation.get(group, {}):
            raise PackError('MissingPackPresentationReference', '组合内容引用的表现定义不存在', {'path': pointer(path), 'group': group, 'reference': reference, 'hint': '加入提供该表现定义的包并声明直接依赖'})


def _schema_artifacts(raw: bytes) -> dict[str, bytes]:
    """新代自带所需确切Schema及无任意路径的摘要索引。"""
    digest = _sha(raw)
    return {'schemas/' + digest + '.json': raw,
            'schema-index.json': canonical({'format': 'HM-CONTENT-SCHEMA-INDEX/1', 'schemas': {digest: 'schemas/' + digest + '.json'}})}


def _compose_pack_set(roots: list[Path], selection: dict, output: Path) -> dict:
    """冻结选包到完整临时代并回验输入后发布；失败不改变旧代或作者源。"""
    selection = deepcopy(selection)
    if not _bounded(selection) or not SELECTION_VALIDATOR.is_valid(selection):
        raise PackError('InvalidPackSelection', '选择文件须为HM-PACK-SELECTION/1精确版本列表')
    output = _protected_output(Path(output), roots)
    trusted = _trusted_inputs()
    resolved = resolve_pack_set(discover_packs(roots), selection['packs'], load_pack_runtime())
    packs = resolved['packs']
    snapshots = _input_snapshots(packs, trusted)
    presentation_schema = trusted[ROOT / 'game/data/presentation.schema.json']
    presentation_raw, assets, presentation_origins = _merge_presentation(packs, json.loads(presentation_schema))
    merged, source_map = _merge_content(packs)
    source_map.update(presentation_origins)
    _check_presentation_references(merged, _artifact_json(presentation_raw))
    _check_presentation_content_references(merged, _artifact_json(presentation_raw), json.loads(presentation_schema))
    capability_sha = _sha(trusted[ROOT / 'game/data/rule_capabilities.json'])
    if len(packs) == 1 and 'legacy_source' in packs[0]['manifest']:
        header_raw, schema_raw = _legacy_header(packs[0], snapshots)
        header = _artifact_json(header_raw)
        if header['rule_capabilities_sha256'] != capability_sha:
            raise PackError('LegacyRuleCapabilitiesChanged', '旧原件规则能力摘要与当前工程不一致')
        lock = _lock(packs, resolved['rules_api'], header['catalog'], presentation_raw, _sha(schema_raw), capability_sha)
    else:
        schema_raw = trusted[ROOT / 'schemas/content.schema.json']
        lock = _lock(packs, resolved['rules_api'], _catalog(merged), presentation_raw, _sha(schema_raw), capability_sha)
        header = _new_header(_catalog(merged), lock)
        header_raw = canonical(header)
    _artifact_json(header_raw)
    lock_digest = _sha(canonical(lock))
    artifacts = {'catalog.hmcatalog.json': header_raw, 'presentation.json': presentation_raw,
                 'pack-set.lock.json': canonical(lock), 'source-map.json': canonical({'format': 'HM-CONTENT-SOURCE-MAP', 'format_version': 1, 'entries': source_map}),
                 **assets, **_schema_artifacts(schema_raw)}
    if 'pack_set_digest' not in header:
        artifacts['legacy-identity.json'] = canonical({'format': 'HM-LEGACY-CONTENT-IDENTITY', 'format_version': 1,
            'content_id': header['content_id'], 'content_version': header['content_version'], 'catalog_sha256': header['catalog_sha256'],
            'catalog_file_sha256': _sha(header_raw), 'pack_set_digest': lock_digest})
    warnings = _publish(output, artifacts, snapshots, lambda stage: _validate_products(stage, merged, presentation_schema))
    return {'warnings': warnings, 'status': 'STRUCTURE_VALID', 'runtime_verified': False, 'pack_set_digest': lock_digest,
            'content_id': header['content_id'], 'content_version': header['content_version'],
            'catalog_sha256': header['catalog_sha256'], 'files': sorted(artifacts),
            'missing_resources': [{'pack_id': pack['manifest']['id'], 'path': path} for pack in packs for path in pack['missing_resources']]}


PRESENTATION_GROUPS = ('backgrounds', 'portraits', 'components', 'character_profiles', 'scene_templates')


def _annotated_schema(value, schema: dict, root: dict, path=()):
    """跟随受信Schema目标定义枚举typed引用，不把普通ID字段视为全局引用。"""
    if '$ref' in schema:
        yield from _annotated_schema(value, root['$defs'][schema['$ref'].split('/')[-1]], root, path)
        return
    for union in ('oneOf', 'anyOf'):
        if union in schema:
            for option in schema[union]:
                if StrictValidator({'$defs': root['$defs'], **option}).is_valid(value):
                    yield from _annotated_schema(value, option, root, path)
                    if union == 'oneOf':
                        break
    for annotation in ('x-hanmo-ref', 'x-hanmo-asset-ref', 'x-hanmo-content-ref', 'x-hanmo-content-ref-by-kind'):
        if annotation in schema and isinstance(value, str):
            yield path, annotation, schema[annotation], value
    if isinstance(value, dict):
        for key, child in value.items():
            child_schema = schema.get('properties', {}).get(key, schema.get('additionalProperties', {}))
            if isinstance(child_schema, dict):
                yield from _annotated_schema(child, child_schema, root, path + (key,))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            yield from _annotated_schema(child, schema.get('items', {}), root, path + (index,))


def _presentation_source_schema(schema: dict) -> dict:
    """表现入口允许只提供新增组，定义内部字段和局部身份约束一律保留。"""
    partial = deepcopy(schema)
    partial['required'] = ['version']
    for group in ('group', 'background_group'):
        if group in partial.get('$defs', {}):
            partial['$defs'][group]['minProperties'] = 0
    return partial


def _pack_resources(pack: dict) -> tuple[dict[str, str], dict[str, bytes]]:
    """资源只按声明路径复制到namespace隔离位置，绝不读取表现清单指向的宿主路径。"""
    locations, assets = {}, {}
    prefix = 'assets/art/packs/' + pack['manifest']['namespace'] + '/'
    for resource in pack['manifest']['resources']:
        logical = resource['path']
        target = prefix + logical
        locations[logical] = 'res://' + target
        if logical in pack['files']:
            assets[target] = pack['files'][logical]
    return locations, assets


def _qualify_presentation(value: dict, pack: dict, file: str, selected: dict[str, dict], schema: dict) -> tuple[dict, dict, dict]:
    """只限定表现全局键和typed引用，局部age/attire/layer与语义标签保持原样。"""
    source_schema = _presentation_source_schema(schema)
    if not _bounded(value) or not StrictValidator(source_schema).is_valid(value):
        raise PackError('InvalidPackPresentation', '表现入口结构不符合共享Schema', {'pack_id': pack['manifest']['id'], 'file': file})
    # 该固定模板只服务标准Alpha的显式全局版本，内容包不能静默丢弃或跨namespace套用。
    if value.get('battle_pose_profiles'):
        raise PackError('UnsupportedPackBattlePoseProfile', '当前完整刀姿只支持标准Alpha显式全局表现，不支持内容包导入', {'pack_id': pack['manifest']['id'], 'file': file})
    result = deepcopy(value)
    locations, assets = _pack_resources(pack)
    chains = {}
    for path, annotation, _group, reference in _annotated_schema(value, source_schema, source_schema):
        if annotation == 'x-hanmo-asset-ref':
            if '/' not in reference:
                raise PackError('InvalidPackPresentation', '表现资产引用缺少组前缀', {'reference': reference})
            group, local = reference.split('/', 1)
            qualified = group + '/' + _qualified(local, pack, selected, file + '#' + pointer(path))
        else:
            qualified = _qualified(reference, pack, selected, file + '#' + pointer(path))
        _assign(result, path, qualified)
        chains[path] = [reference, qualified] if reference != qualified else [reference]
    namespace = pack['manifest']['namespace']
    composition = result.get('adult_appearance_composition')
    if composition is not None:
        validate_composition_resources(composition, pack, _artifact_json,
            _artifact_json((ROOT / 'game/data/adult_appearance_compositions.json').read_bytes()),
            _artifact_json((ROOT / 'game/data/accepted_adult_appearances.json').read_bytes())['records'])
        # 正式组合只从本包已冻结资源取目录与原图；共享脚本属于引擎，不从内容包执行代码。
        if composition['resource_root'] != 'res://character_studio/':
            raise PackError('InvalidPackPresentation', '组合作者源必须使用明确工坊逻辑根', {'file': file})
        for field in ('catalog', 'fittings', 'representatives'):
            logical = composition[field].removeprefix('res://')
            resource = next((entry for entry in pack['manifest']['resources'] if entry['path'] == logical), None)
            if resource is None or resource['kind'] != 'data' or logical not in pack['files']:
                raise PackError('UndeclaredPackResource', '组合目录须在当前包声明并保全', {'file': file, 'path': logical})
            composition[field] = locations[logical]
        composition['resource_root'] = 'res://assets/art/packs/' + namespace + '/character_studio/'
    for group in PRESENTATION_GROUPS:
        updated = {}
        for local, record in result.get(group, {}).items():
            if not LOCAL_ID.fullmatch(local) or ('id' in record and record['id'] != local):
                raise PackError('QualifiedPackDefinition', '表现定义键和记录ID须为相同本地ID', {'group': group, 'id': local, 'file': file})
            identifier = namespace + '::' + local
            if group in ('components', 'character_profiles', 'scene_templates'):
                record['id'] = identifier
            if group in ('backgrounds', 'portraits'):
                logical = record['path'].removeprefix('res://')
                resource = next((entry for entry in pack['manifest']['resources'] if entry['path'] == logical), None)
                if resource is None or resource['kind'] != 'image':
                    raise PackError('UndeclaredPackResource', '表现资源必须在当前包resources声明为image', {'file': file, 'path': record['path']})
                record['path'] = locations[logical]
            updated[identifier] = record
        if group in result:
            result[group] = updated
    return result, chains, assets


def _merge_presentation(packs: list[dict], schema: dict) -> tuple[bytes, dict[str, bytes], dict]:
    """按全局ID确定合并表现，默认入口冲突不得靠后写覆盖，所有资源均来自冻结声明。"""
    selected = {pack['manifest']['namespace']: pack for pack in packs}
    merged, assets, origins = {'version': 1}, {}, {}
    merged.update({group: {} for group in PRESENTATION_GROUPS})
    for pack in packs:
        legacy = 'legacy_source' in pack['manifest']
        if not legacy:
            _locations, declared_assets = _pack_resources(pack)
            assets.update(declared_assets)
        files = ([name for name in pack['content_paths'] if name.endswith('/presentation.json')]
                 if legacy else sorted(pack['manifest']['presentation_entries']))
        for file in files:
            original = _artifact_json(pack['files'][file])
            if legacy:
                qualified, chains = deepcopy(original), {}
                resource_files = {name.removeprefix('game/'): pack['files'][name] for name in pack['resource_paths']}
            else:
                qualified, chains, resource_files = _qualify_presentation(original, pack, file, selected, schema)
            merged['version'] = max(merged['version'], qualified['version'])
            composition = qualified.get('adult_appearance_composition')
            if composition is not None:
                if 'adult_appearance_composition' in merged and merged['adult_appearance_composition'] != composition:
                    raise PackError('PackPresentationDefaultConflict', '正式组合资源根不得由第二包覆盖', {'field': 'adult_appearance_composition'})
                merged['adult_appearance_composition'] = deepcopy(composition)
            for default in ('default_background', 'default_portrait'):
                if default not in qualified:
                    continue
                if default in merged and merged[default] != qualified[default]:
                    raise PackError('PackPresentationDefaultConflict', '多个表现包不能隐式覆盖默认入口', {'field': default, 'existing': merged[default], 'incoming': qualified[default]})
                merged[default] = qualified[default]
                origins[pointer(('presentation', default))] = _source(pack, file, '', (default,), chains.get((default,)))
            for group in PRESENTATION_GROUPS:
                for identifier, record in qualified.get(group, {}).items():
                    local = identifier if legacy else identifier.split('::', 1)[1]
                    if identifier in merged[group]:
                        raise PackError('DuplicatePackPresentation', '表现全局ID不能重复', {'group': group, 'id': identifier})
                    merged[group][identifier] = record
                    for leaf in _leaves(record):
                        path = (group, local) + leaf
                        origins[pointer(('presentation', group, identifier) + leaf)] = _source(pack, file, local, path, chains.get(path))
            for name, raw in resource_files.items():
                if name in assets and assets[name] != raw:
                    raise PackError('PackResourceConflict', '发布资源路径存在不同字节', {'path': name})
                assets[name] = raw
    if merged['version'] == 1:
        for group in ('components', 'character_profiles', 'scene_templates'):
            if merged[group]:
                raise PackError('InvalidPackPresentation', 'v1表现不能包含v2定义组')
            del merged[group]
    if not StrictValidator(schema).is_valid(merged):
        raise PackError('InvalidComposedPresentation', '组合后的完整表现不符合当前受信Schema', {'hint': '确认默认入口及全局限定引用由同一受信Schema支持'})
    return canonical(merged), assets, origins


def _validate_products(stage: Path, merged: dict, presentation_schema: bytes) -> None:
    """回读新代的锁和目录，并复用表现工具真实解码及语义校验，禁止只信自报摘要。"""
    try:
        from tools.presentation import validate_registry, validate_references
    except ModuleNotFoundError:
        from presentation import validate_registry, validate_references
    header = _artifact_json((stage / 'catalog.hmcatalog.json').read_bytes())
    lock = _artifact_json((stage / 'pack-set.lock.json').read_bytes())
    presentation_raw = (stage / 'presentation.json').read_bytes()
    presentation = _artifact_json(presentation_raw)
    schema_index = _artifact_json((stage / 'schema-index.json').read_bytes())
    expected_index = {'format': 'HM-CONTENT-SCHEMA-INDEX/1', 'schemas': {lock['schema_sha256']: 'schemas/' + lock['schema_sha256'] + '.json'}}
    if (not StrictValidator(PACK_SET_SCHEMA).is_valid(lock) or schema_index != expected_index or
            header.get('schema_sha256') != lock['schema_sha256'] or
            header.get('rule_capabilities_sha256') != lock['rule_capabilities_sha256'] or
            lock['catalog_sha256'] != _sha(canonical(header['catalog'])) or
            header['catalog_sha256'] != lock['catalog_sha256'] or
            lock['presentation_sha256'] != _sha(presentation_raw) or
            lock['schema_sha256'] != _sha((stage / 'schemas' / (lock['schema_sha256'] + '.json')).read_bytes()) or
            ('pack_set_digest' in header and header['pack_set_digest'] != _sha(canonical(lock)))):
        raise PackError('ComposedArtifactMismatch', '写入新代后回读摘要不匹配')
    schema_path = stage / 'data/presentation.schema.json'
    schema_path.parent.mkdir()
    schema_path.write_bytes(presentation_schema)
    try:
        errors = validate_registry(presentation, stage) or validate_references(presentation, merged)
        if errors:
            raise PackError('InvalidComposedPresentation', '组合表现未通过真实资源或语义校验', {'issues': errors})
    finally:
        schema_path.unlink()
        schema_path.parent.rmdir()


def compose_pack_set(roots: list[Path], selection: dict, output: Path) -> dict:
    """公开组合入口统一文件系统失败为可诊断错误，不下载或隐式重用旧generated。"""
    try:
        return _compose_pack_set(roots, selection, output)
    except PackError:
        raise
    except (OSError, UnicodeError, json.JSONDecodeError, RecursionError) as exc:
        raise PackError('PackComposeIO', '组合输入或输出读取失败，不能认定已发布', {'output': str(output), 'error': str(exc)}) from exc


def _value_at(value, path: tuple):
    """只沿Schema已核字段读取父记录，动态target kind不从字符串猜测。"""
    for part in path:
        value = value[part]
    return value


def _check_presentation_content_references(merged: dict, presentation: dict, schema: dict) -> None:
    """场景目标和选择器只按typed注解核实际内容定义，禁止遗漏隐藏在anyOf旁的引用。"""
    indexes = {table: {record['id'] for record in merged.get(table, [])} for table in TABLES}
    for path, annotation, target, reference in _annotated_schema(presentation, schema, schema):
        if annotation == 'x-hanmo-content-ref-by-kind':
            target = target.get(_value_at(presentation, path[:-1]).get('kind'))
        elif annotation != 'x-hanmo-content-ref':
            continue
        if target not in indexes or reference not in indexes[target]:
            raise PackError('MissingPackSceneReference', '表现模板引用的内容定义不存在', {'path': pointer(path), 'table': target, 'reference': reference, 'hint': '按场景typed目标修正actor/place/prop/estate/role引用'})
