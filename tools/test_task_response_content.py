"""在固定工具候选中核真实父包与回应作者；只读资源并做内存组合，不创建安装目录或World。"""
from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def run(repo: Path, tool_root: Path, parent: Path, contacts: Path) -> dict:
    """实际走discover、依赖选择和完整内容语义；额外核两份原合同及告辞不被回应功能改写。"""
    sys.path.insert(0, str(tool_root))
    from tools.content.packs import discover_packs, load_pack_runtime, resolve_pack_set
    from tools.content.pack_compose import _merge_content
    from tools.content.validation import validate_pack
    paths = [parent, contacts]
    packs = discover_packs(paths)
    selection = [{'id': 'hanmo.local_society', 'version': '1.11.0'}, {'id': 'hanmo.task_contacts', 'version': '1.1.0'}]
    resolved = resolve_pack_set(packs, selection, load_pack_runtime())
    merged, _sources = _merge_content(resolved['packs'])
    assert validate_pack(merged).ok
    chosen = next(row for row in merged['scenarios'] if row['id'] == 'contacts::scenario.qingchuan_responses')
    assert chosen['task_offer_response_profile'] == 'known_offers_v1'
    old_scenario = next(row for row in merged['scenarios'] if row['id'] == 'society::scenario.qingchuan_spring_reserve')
    assert 'task_offer_response_profile' not in old_scenario

    source = json.loads((contacts / 'content.json').read_text())
    original = json.loads(subprocess.check_output(['git', 'show', '4f39ecf4c2b72f213cd92051c1eb604550f1769e:content/packs/authored/task_contacts_1_0/content.json'], cwd=repo))
    for field in ('roles', 'offers', 'items', 'personal_objectives'):
        assert source[field] == original[field], field
    assert source['scenarios'][0]['initial'] == original['scenarios'][0]['initial']
    sender = next(row for row in source['dialogues'] if row['id'] == 'dialog.sender')
    previous = next(row for row in original['dialogues'] if row['id'] == 'dialog.sender')
    nodes = {row['id']: row for row in sender['nodes']}
    old_nodes = {row['id']: row for row in previous['nodes']}
    for ident, value in old_nodes.items():
        if ident != 'menu':
            assert nodes[ident] == value
    menu = {row['id']: row for row in nodes['menu']['options']}
    for value in old_nodes['menu']['options']:
        assert menu[value['id']] == value
    for suffix in ('letter', 'escort'):
        ident = 'decline_' + suffix
        assert menu[ident]['conditions'] == [{'op': 'task_state', 'offer_id': 'offer.' + suffix, 'state': 'known'}]
        assert nodes[ident]['command'] == {'kind': 'decline_offer', 'offer_id': 'offer.' + suffix}
        assert nodes[ident]['on_failure'] == 'unavailable'
        assert nodes[nodes[ident]['on_success']]['next'] == 'menu'
    assert len(nodes) == len(old_nodes) + 4 and len(menu) == len(old_nodes['menu']['options']) + 2

    # 反例仍调用原完整校验：移除必需能力、未知profile与无权限岗位都必须失败。
    rejected = {}
    bad = deepcopy(merged)
    bad['required_capabilities'].remove('task_offer_responses_v1')
    rejected['missing_capability'] = [issue.code for issue in validate_pack(bad).issues]
    bad = deepcopy(merged)
    next(row for row in bad['scenarios'] if row['id'] == chosen['id'])['task_offer_response_profile'] = 'permanent_refusal_v1'
    rejected['unknown_profile'] = [issue.code for issue in validate_pack(bad).issues]
    bad = deepcopy(merged)
    next(row for row in bad['roles'] if row['id'] == 'contacts::role.sender')['services'].remove('accept_offer')
    rejected['wrong_service'] = [issue.code for issue in validate_pack(bad).issues]
    assert all(rejected.values())
    return {'ok': True, 'status': 'STRUCTURE_VALID', 'runtime_verified': False,
            'scenario_id': chosen['id'], 'selection': selection, 'old_scenario_profile_absent': True,
            'original_contracts_unchanged': True, 'worlds_created': 0, 'resource_files_copied': 0,
            'negative_cases': rejected, 'packs': [{key: pack[key] for key in ('manifest_sha256', 'content_sha256', 'resources_sha256')} | {'id': pack['manifest']['id'], 'required_resources_verified': len(pack['resource_paths'])} for pack in packs],
            'author_files': {name: hashlib.sha256((contacts / name).read_bytes()).hexdigest() for name in ('content.json', 'pack.json')}}


def main() -> int:
    """显式指定已冻结工具和现有父包，不从活动生产舞台隐式借Schema。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--tool-root', required=True, type=Path)
    parser.add_argument('--parent-pack', required=True, type=Path)
    parser.add_argument('--contacts-pack', required=True, type=Path)
    args = parser.parse_args()
    result = run(args.repo.resolve(), args.tool_root.resolve(), args.parent_pack.resolve(), args.contacts_pack.resolve())
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
