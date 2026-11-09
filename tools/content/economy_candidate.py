"""只在精确政治候选上叠加既有产能管理，不修改旧情景或增造初始资产。"""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path

from jsonschema import Draft202012Validator


def _module(path, name):
    """复用明确给定的现有owner工具，不复制政治delta解析或产权规则。"""
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _load_exact(path, expected):
    """核原件GitOID后才读内容，路径存在不能替代版本身份。"""
    raw = Path(path).read_bytes()
    oid = hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()
    if oid != expected:
        raise ValueError('Candidate source GitOID mismatch: ' + str(path))
    return json.loads(raw)


def verify_candidate(base_path, politics_delta, economic_delta, politics_tool, schema_path):
    """原始生产、劳力、供养与资产保持同值；只验新管理块和父子两份显式授权。"""
    delta = json.loads(Path(economic_delta).read_text())
    base = _load_exact(base_path, delta['base_blob_oid'])['catalog']
    political = _load_exact(politics_delta, delta['prerequisite']['blob_oid'])
    apply = _module(politics_tool, 'economic_political_candidate').apply_candidate
    before = apply(base, political['operations'])
    candidate = apply(before, delta['operations'])
    target = delta['scenario_id']
    if target != political['scenario_id'] or candidate['required_capabilities'] != before['required_capabilities']:
        raise ValueError('Economic candidate cannot change shared capability identity')
    for key, value in before['scenarios'].items():
        if key != target and candidate['scenarios'][key] != value:
            raise ValueError('Unexpected original scenario change: ' + key)
    for table in ('actors', 'wallets', 'inventories', 'places', 'settlements', 'organizations'):
        if before[table] != candidate[table]:
            raise ValueError('Unexpected world source change: ' + table)
    old, new = before['scenarios'][target], candidate['scenarios'][target]
    if old['initial'] != new['initial']:
        raise ValueError('Initial people and resources must remain identical')
    old_e = old['dynamic_world']['local_economy']
    new_e = copy.deepcopy(new['dynamic_world']['local_economy'])
    management = new_e.pop('production_management')
    if new_e != old_e:
        raise ValueError('Original labor/production/obligation state changed')
    for key, plan in management['plans'].items():
        unit = old_e['production_units'][key]
        if len(plan['asset_uses']) != 1 or plan['season_profile_id']:
            raise ValueError('Candidate must retain original recipe factors')
        use = plan['asset_uses'][0]
        asset = management['assets'][use['asset_id']]
        if use['batches_per_unit'] != 1 or use['units'] != unit['capacity'] or asset['capacity_units'] != unit['capacity']:
            raise ValueError('Candidate capacity differs from original unit')
    pack = copy.deepcopy(candidate)
    pack['scenarios'] = list(candidate['scenarios'].values())
    root = Path(__file__).resolve().parents[2]
    issues = _module(root / 'tools/content/production_management_links.py', 'economic_management_links').check_production_management(pack, candidate)
    if issues:
        raise ValueError(str(issues))
    schema = json.loads(Path(schema_path).read_text())
    fragment = json.loads((root / 'schemas/fragments/production-management-v1.json').read_text())
    definitions = copy.deepcopy(schema['$defs'])
    definitions.update(fragment['$defs'])
    Draft202012Validator({'$defs': definitions, '$ref': '#/$defs/dynamic_production_management'}).validate(management)
    grants = new['dynamic_world']['organization']['grants']
    accounts = {'society::wallet.city', 'society::wallet.city_wages'}
    for key in ('grant.qingchuan_appointment', 'grant.qingchuan_governance'):
        grant = grants[key]
        if 'configure_local_production' not in grant['allowed_command_kinds'] or not accounts.issubset(grant['wallet_ids']) or not accounts.issubset({s['id'] for s in grant['scopes'] if s['kind'] == 'account'}):
            raise ValueError('Both appointment and delegated grants need explicit economic scope')
    return candidate, {'scenario_id': target, 'delta_operations': len(delta['operations']), 'management_schema_errors': 0, 'management_semantic_errors': 0, 'old_scenarios_unchanged': True, 'initial_assets_and_people_unchanged': True, 'original_production_and_labor_unchanged': True, 'runtime_verified': False, 'political_prerequisite_oid': delta['prerequisite']['blob_oid']}


def main():
    """生成可供原native author入口接收的候选索引；不发布内容包或改变原件。"""
    parser = argparse.ArgumentParser()
    for name in ('base', 'politics_delta', 'economic_delta', 'politics_tool', 'schema', 'output', 'receipt'):
        parser.add_argument('--' + name.replace('_', '-'), required=True)
    args = parser.parse_args()
    candidate, receipt = verify_candidate(args.base, args.politics_delta, args.economic_delta, args.politics_tool, args.schema)
    Path(args.output).write_text(json.dumps(candidate, ensure_ascii=False, indent=2) + '\n')
    receipt['output_sha256'] = hashlib.sha256(Path(args.output).read_bytes()).hexdigest()
    Path(args.receipt).write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(receipt, ensure_ascii=False))


if __name__ == '__main__':
    main()
