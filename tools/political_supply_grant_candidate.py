#!/usr/bin/env python3
"""仅产生两片Grant声明候选，中央仍是共享叶唯一writer。"""
import argparse
import difflib
import hashlib
import json
import subprocess
from pathlib import Path

BASE = '9effd89a84873a7cec0e30bb3a5761011c62151e'


def main():
    """固定9eff锚点，可承接同基准已产生的S5候选，拒绝重复或漂移覆盖。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--base', default=BASE)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.base != BASE or args.output.exists():
        raise SystemExit('Fixed 9eff base and fresh output required')
    args.output.mkdir(parents=True)
    records, patches = [], []

    def replace(source, old, new):
        """每片锚点必须唯一，不能静默覆盖其他owner变化。"""
        if source.count(old) != 1:
            raise SystemExit('Anchor mismatch: ' + old[:100])
        return source.replace(old, new)

    for path in ['native/world/src/resident/kernel/world_bundle.cpp', 'tools/content/dynamic_world_links.py']:
        incoming = args.source / path if args.source else None
        before = incoming.read_text() if incoming and incoming.exists() else subprocess.check_output(
            ['git', 'show', BASE + ':' + path], cwd=args.repo, text=True)
        after = before
        if path.endswith('.cpp'):
            after = replace(after, '#include "occupation_fiscal_integration.hpp"',
                            '#include "occupation_fiscal_integration.hpp"\n#include "resident/domains/politics/supply/politics_supply_authority.hpp"')
            lines = [line for line in after.splitlines() if 'OrganizationProspectiveGrantRead prospective=' in line]
            if len(lines) != 1 or 'political_supply_grant_declares_resource' in lines[0]:
                raise SystemExit('Unexpected prospective Grant anchor')
            old = lines[0]
            prefix, inner = old.split('return political_takeover_enabled()&&(', 1)
            if not inner.endswith(');};'):
                raise SystemExit('Unexpected prospective Grant terminator')
            inner = inner[:-4]
            extra = '||(economic_supply_policies_enabled()&&political_supply_grant_declares_resource(*catalog->common,*catalog->organization,*catalog->politics,*catalog->military,*catalog->political_takeover,true,grant.id,resource))'
            after = replace(after, old, prefix + 'return political_takeover_enabled()&&(' + inner + extra + ');};')
        else:
            after = replace(after, 'from .political_occupation_fiscal_links import occupation_fiscal_grant_declares_assets',
                            'from .political_occupation_fiscal_links import occupation_fiscal_grant_declares_assets\nfrom .political_supply_links import political_supply_grant_declares_assets')
            after = replace(after, 'not occupation_fiscal_grant_declares_assets(scenario,index,grant) and (any(',
                            'not occupation_fiscal_grant_declares_assets(scenario,index,grant) and not political_supply_grant_declares_assets(scenario,index,grant) and (any(')
            command = "                allowed.add('political_issue_supply_policy')"
            if command not in after:
                anchor = "            if dynamic.get('political_occupation_fiscal', {}).get('enabled'):\n                allowed.add('political_set_occupation_tax')"
                addition = "\n            supply = dynamic.get('economic_supply_policies', {})\n            if type(supply.get('version')) is int and supply['version'] == 1:\n" + command
                after = replace(after, anchor, anchor + addition)
        target = args.output / 'candidate' / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(after)
        def oid(value):
            """直接取实际字节的Git对象名，不伪造共享提交。"""
            return subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=args.repo, input=value, text=True).strip()
        records.append({'path': path, 'before_oid': oid(before), 'after_oid': oid(after),
                        'sha256': hashlib.sha256(after.encode()).hexdigest()})
        patches.extend(difflib.unified_diff(before.splitlines(True), after.splitlines(True), 'a/' + path, 'b/' + path))
    (args.output / 'candidate.patch').write_text(''.join(patches))
    (args.output / 'manifest.json').write_text(json.dumps({'base': BASE, 'status': 'SOURCE_ONLY_PENDING_S5_COMPOSITE_NATIVE_SYNTAX',
        'shared': records, 'owned_dependency': 'tools/content/political_supply_links.py plus existing native politics_supply_authoring.cpp',
        'initial_path': 'native_initial_state make_initial_organization only copies state; ResidentWorld initialize then invokes same WorldBundle.validate prospective callback',
        'cmake': 'central unique writer'}, indent=2) + '\n')


if __name__ == '__main__':
    main()
