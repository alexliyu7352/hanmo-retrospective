"""复核政治作者小增量的精确来源和旧情景隔离；只产生候选，不编译或发布内容包。"""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path

from jsonschema import Draft202012Validator


def _module(path, name):
    """只借指定的现有领域检查器，不复制其人物/资格规则。"""
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _parts(pointer):
    """JSON pointer按字段解析，路径中的作者字符串从不作为代码执行。"""
    if not pointer.startswith('/'):
        raise ValueError('Expected absolute JSON pointer')
    return [part.replace('~1', '/').replace('~0', '~') for part in pointer[1:].split('/')]


def _read(document, pointer):
    """源引用必须实际存在，缺失内容不补空表。"""
    value = document
    for part in _parts(pointer):
        value = value[int(part)] if isinstance(value, list) else value[part]
    return value


def apply_candidate(catalog, operations):
    """仅支持本候选明示的copy/add/replace，add不得悄悄覆盖既有对象。"""
    result = copy.deepcopy(catalog)
    for operation in operations:
        kind = operation['op']
        if kind not in ('copy', 'add', 'replace'):
            raise ValueError('Unsupported candidate operation: ' + kind)
        value = copy.deepcopy(_read(result, operation['from']) if kind == 'copy' else operation['value'])
        parts = _parts(operation['path'])
        target = result
        for part in parts[:-1]:
            target = target[int(part)] if isinstance(target, list) else target[part]
        key = parts[-1]
        if isinstance(target, list):
            if kind != 'add' or key != '-' or value in target:
                raise ValueError('Candidate array additions must be unique appends')
            target.append(value)
        else:
            if (kind == 'replace') != (key in target):
                raise ValueError('Candidate object existence mismatch: ' + operation['path'])
            target[key] = value
    return result


def verify_candidate(base_path, delta_path, appearance_checker):
    """原件OID、角色/资产未复制、政治权限及S4名单政策分别走真实检查器。"""
    raw = Path(base_path).read_bytes()
    base = json.loads(raw)['catalog']
    delta = json.loads(Path(delta_path).read_text())
    oid = hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()
    if oid != delta['base_blob_oid']:
        raise ValueError('Author base GitOID mismatch')
    candidate = apply_candidate(base, delta['operations'])
    original_id, new_id = delta['base_scenario_id'], delta['scenario_id']
    if original_id == new_id or candidate['scenarios'][original_id] != base['scenarios'][original_id]:
        raise ValueError('Original scenario must remain byte-equivalent as a value')
    new = candidate['scenarios'][new_id]
    if new['id'] != new_id or new['initial'] != base['scenarios'][original_id]['initial']:
        raise ValueError('Candidate cannot create initial people/assets')
    for table in ('actors', 'wallets', 'inventories', 'places', 'settlements', 'organizations'):
        if candidate[table] != base[table]:
            raise ValueError('Unexpected shared source change: ' + table)
    if new['dynamic_world']['founder_policy'] != 'selected_existing_actor_roster_v1':
        raise ValueError('New named roster requires explicit v2 policy')
    pack = copy.deepcopy(candidate)
    pack['scenarios'] = list(candidate['scenarios'].values())
    root = Path(__file__).resolve().parent
    politics = _module(root / 'politics_links.py', 'candidate_politics')
    appearance = _module(Path(appearance_checker), 'candidate_s4_appearance')
    issues = politics.check_politics(pack, candidate) + politics.check_political_fiscal(pack, candidate)
    issues += politics.check_political_career(pack, candidate)
    issues += appearance.check_appearance_choices(pack, candidate)
    if issues:
        raise ValueError(str(issues))
    schema = json.loads((root / 'politics_schema.json').read_text())
    checks = [(schema, new['dynamic_world']['politics'])]
    checks += [({'$ref': '#/$defs/political_career_policy', '$defs': schema['$defs']}, row)
               for row in delta['political_career_profiles']]
    checks += [({'$ref': '#/$defs/politics_career_policy', '$defs': schema['$defs']}, policy['political_career'])
               for policy in new['dynamic_world']['director'].get('policies', {}).values() if 'political_career' in policy]
    checks.append(({'$ref': '#/$defs/political_fiscal_catalog', '$defs': schema['$defs']},
                   new['dynamic_world']['political_fiscal']))
    for shape, value in checks:
        Draft202012Validator(shape).validate(value)
    return candidate, {'base_blob_oid': oid, 'base_scenario_id': original_id, 'scenario_id': new_id,
                       'delta_operations': len(delta['operations']), 'owned_schema_errors': 0,
                       'politics_semantic_errors': 0, 'fiscal_semantic_errors': 0, 'political_director_semantic_errors': 0,
                       's4_appearance_roster_errors': 0, 'old_scenario_unchanged': True,
                       'initial_assets_and_people_unchanged': True, 'runtime_verified': False,
                       'required_native_module': 'selected_existing_actor/2',
                       'appearance_checker_sha256': hashlib.sha256(Path(appearance_checker).read_bytes()).hexdigest()}


def main():
    """默认只打印候选检查结果；输出路径须由调用者明确给出。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True)
    parser.add_argument('--delta', required=True)
    parser.add_argument('--appearance-checker', required=True)
    parser.add_argument('--output')
    parser.add_argument('--evidence')
    args = parser.parse_args()
    candidate, evidence = verify_candidate(args.base, args.delta, args.appearance_checker)
    if args.output:
        Path(args.output).write_text(json.dumps(candidate, ensure_ascii=False, indent=2) + '\n')
    if args.evidence:
        Path(args.evidence).write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(evidence, ensure_ascii=False))


if __name__ == '__main__':
    main()
