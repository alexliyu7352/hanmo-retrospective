"""核真实1.5作者垫款的完整组合与坏政策；静态通过不等于World或Main验收。"""
from __future__ import annotations
from copy import deepcopy
from pathlib import Path
import argparse
import json
import subprocess
import sys


def run(repo: Path, tool_root: Path, parent: Path, contacts: Path) -> dict:
    """走既有包依赖与严格语义入口，保持原人物钱货及全部旧情景不变。"""
    sys.path.insert(0, str(tool_root))
    from tools.content.packs import discover_packs, load_pack_runtime, resolve_pack_set
    from tools.content.pack_compose import _merge_content
    from tools.content.validation import validate_pack
    packs = discover_packs([parent, contacts])
    selection = [{'id': 'hanmo.local_society', 'version': '1.11.0'}, {'id': 'hanmo.task_contacts', 'version': '1.5.0'}]
    resolved = resolve_pack_set(packs, selection, load_pack_runtime())
    merged, _sources = _merge_content(resolved['packs'])
    chosen_id = 'contacts::scenario.qingchuan_task_advances'
    chosen = next(row for row in merged['scenarios'] if row['id'] == chosen_id)
    policies = chosen['dynamic_world']['task_personal_advances']['policies']
    assert len(policies) == 2 and {row['task_template_id'] for row in policies} == {'society::template.city_grain', 'society::template.sect_grain'}
    assert all(row['maximum_cash'] == 40 and row['repayment_policy'] == 'authorized_advance_first_v1' for row in policies)
    source = json.loads((contacts / 'content.json').read_text())
    original = json.loads(subprocess.check_output(['git', 'show', 'b97055ec922de482ad2e843a1983f88684897b46:content/packs/authored/task_contacts_1_4/content.json'], cwd=repo))
    for table in ('actors', 'wallets', 'inventories', 'task_templates', 'offers', 'roles', 'items', 'organizations'):
        assert source[table] == original[table], table
    assert source['scenarios'][:-1] == original['scenarios']
    assert all('task_personal_advances' not in row.get('dynamic_world', {}) for row in source['scenarios'][:-1])
    rejected = {}
    for kind in ('unselected_template', 'duplicate_template', 'boolean_amount', 'zero_amount', 'unknown_repayment', 'missing_capability'):
        bad = deepcopy(merged)
        scenario = next(row for row in bad['scenarios'] if row['id'] == chosen_id)
        block = scenario['dynamic_world']['task_personal_advances']
        if kind == 'unselected_template':
            scenario['enabled']['task_templates'].remove(block['policies'][0]['task_template_id'])
        elif kind == 'duplicate_template':
            block['policies'][1]['task_template_id'] = block['policies'][0]['task_template_id']
        elif kind == 'boolean_amount':
            block['policies'][0]['maximum_cash'] = True
        elif kind == 'zero_amount':
            block['policies'][0]['maximum_cash'] = 0
        elif kind == 'unknown_repayment':
            block['policies'][0]['repayment_policy'] = 'keep_reward_first'
        elif kind == 'missing_capability':
            bad['required_capabilities'].remove('tasks.procurement')
        report = validate_pack(bad)
        assert not report.ok, kind
        rejected[kind] = sorted({issue.code for issue in report.issues})
    return {'status': 'STRUCTURE_VALID', 'runtime_verified': False, 'selection': selection,
            'scenario_id': chosen_id, 'policies': policies, 'original_assets_and_scenarios_unchanged': True,
            'negative_cases': rejected}


def main() -> int:
    """显式给定冻结工具根，不从活动构建借Schema或改正式舞台。"""
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('repo', 'tool-root', 'parent-pack', 'contacts-pack'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args.repo.resolve(), args.tool_root.resolve(), args.parent_pack.resolve(), args.contacts_pack.resolve()), ensure_ascii=False, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
