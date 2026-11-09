#!/usr/bin/env python3
"""由固定原继位作者派生明确生前交接入口，保真实收养/本人换控且不注入任何新公职事实。"""
import argparse, copy, json, subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(); parser.add_argument('--base', required=True); parser.add_argument('--repo', type=Path, default=Path('.')); args = parser.parse_args()
    def read(path):
        return json.loads(subprocess.check_output(['git', 'show', args.base + ':' + path], cwd=args.repo))
    root = 'content/packs/authored/political_successions_1_0/'
    content, manifest = read(root + 'content.json'), read(root + 'pack.json')
    content['id'] = 'political_head_handovers'; scenario = content['scenarios'][0]
    scenario['id'] = 'scenario.qingchuan_living_handover'
    content['texts'][scenario['name_key']] = '青川首職生前交接'
    content['texts']['succession.consent'] = '若已有合法成年收养关系，我愿接受这次零私资的本人控制接续。你的原首职仍由你本人持有，我须另行亲自接受明确的让职意愿，才可交接公职。'
    content['required_capabilities'].append('political_head_handovers_v1')
    dynamic = scenario['dynamic_world']; org = dynamic['organization']
    grant = copy.deepcopy(org['grants']['grant.first_successor'])
    grant.update(id='grant.living_handover', allowed_command_kinds=['political_offer_head_handover', 'political_withdraw_head_handover'])
    org['grants'][grant['id']] = grant
    org['offices']['office.city_steward']['grant_ids'].append(grant['id'])
    dynamic['political_head_handovers'] = {'version': 1, 'policies': {'policy.living_handover': {'id': 'policy.living_handover', 'succession_policy_id': 'policy.first_successor', 'handover_grant_id': grant['id'], 'response_ticks': 80}}}
    for kind in grant['allowed_command_kinds']:
        key = 'handover.command.' + kind; content['texts'][key] = kind; dynamic['politics']['command_name_keys'][kind] = key
    manifest.update(id='hanmo.political_head_handovers', namespace='headhandover', version='1.0.0', required_capabilities=content['required_capabilities'])
    out = args.repo / 'content/packs/authored/political_head_handovers_1_0'; out.mkdir(parents=True, exist_ok=True)
    for name, value in [('content.json', content), ('pack.json', manifest)]:
        (out / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    selection = read('content/packs/selections/political-successions-1.0.0.json')
    selection['packs'][-1] = {'id': manifest['id'], 'version': '1.0.0'}
    (args.repo / 'content/packs/selections/political-head-handovers-1.0.0.json').write_text(json.dumps(selection, ensure_ascii=False, indent=2) + '\n')


if __name__ == '__main__': main()
