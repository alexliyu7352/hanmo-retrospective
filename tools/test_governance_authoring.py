"""用指定同源作者工具核真实安民addon及坏字段；正式compose仍由统一入口执行。"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys


def main():
    """正常部分包准入/引用限定后跑全links，并保精确输入摘要与局部反例。"""
    parser = argparse.ArgumentParser()
    for field in ('source_root', 'pack_root', 'catalog', 'output'):
        parser.add_argument('--' + field.replace('_', '-'), type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.source_root))
    from tools.content.pack_compose import _qualify_content
    from tools.content.links import check_links
    from tools.content.politics_governance_links import check_politics_governance
    from tools.content.public_relief_links import check_public_relief
    raw = (args.pack_root / 'content.json').read_bytes()
    manifest = json.loads((args.pack_root / 'pack.json').read_text())
    source = json.loads(raw)
    # 此处只消费原限定函数；给出已验证父包的真实namespace/包ID，不伪造内容定义。
    qualified, _ = _qualify_content(source, {'manifest': manifest}, 'content.json',
                                   {'society': {'manifest': {'id': 'hanmo.local_society', 'namespace': 'society'}}})
    index = copy.deepcopy(json.loads(args.catalog.read_text())['data'])
    index['texts'].update(qualified['texts'])
    index['scenarios'].update({s['id']: s for s in qualified['scenarios']})
    issues = check_links(qualified, index)
    cases = []

    def negative(label, mutate, expected):
        """每个副本只改一个明确作者前件，保持原正式输入不变。"""
        bad = copy.deepcopy(qualified)
        mutate(bad['scenarios'][-1]['dynamic_world'])
        found = check_politics_governance(bad, index) + check_public_relief(bad, index)
        cases.append({'name': label, 'passed': any(x[0] == expected for x in found),
                      'expected_code': expected, 'actual_codes': [x[0] for x in found]})

    negative('partial group', lambda d: d.pop('public_relief'), 'GOVERNANCE_GROUP_PROFILE')
    negative('missing initial support', lambda d: d['politics_governance']['initial_support'].clear(), 'GOVERNANCE_INITIAL_SUPPORT')
    negative('bool initial support', lambda d: d['politics_governance']['initial_support'].update({'jurisdiction.qingchuan': True}), 'GOVERNANCE_INITIAL_SUPPORT')
    negative('too short completion', lambda d: d['politics_governance']['commission_policies']['commission.qingchuan_appease'].update({'completion_deadline_ticks': 5}), 'GOVERNANCE_COMMISSION_TIME')
    negative('short actual cash', lambda d: d['public_relief']['policies']['relief.qingchuan_households']['recipients'][0].update({'cash': 119}), 'RELIEF_FIXED_COST')
    negative('missing rule text', lambda d: d['politics_governance']['appeasement_rules']['governance.qingchuan_appease'].update({'name_key': 'missing'}), 'GOVERNANCE_RULE_SOURCE')
    result = {'source_content_sha256': hashlib.sha256(raw).hexdigest(),
              'schema_sha256': hashlib.sha256((args.source_root / 'schemas/content.schema.json').read_bytes()).hexdigest(),
              'normal_partial_source': True, 'normal_schema_qualification': True,
              'complete_links_errors': issues, 'negative_cases': cases,
              'formal_compose': False, 'runtime_verified': False}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(result, ensure_ascii=False))
    return 1 if issues or not all(case['passed'] for case in cases) else 0


if __name__ == '__main__':
    raise SystemExit(main())
