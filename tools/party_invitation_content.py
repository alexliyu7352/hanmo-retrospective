"""从已验回应包生成具名当面随行邀请；保原人物、钱粮和Director计划，不创建世界。"""
from __future__ import annotations

import argparse
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import subprocess

BASE = '434d7f35f21710d24e466a27109e3142a9e924e2'
BASE_PACK = 'content/packs/authored/task_contacts_1_1'
CAPABILITIES = {'native_dynamic_world', 'multi_actor', 'dialogue', 'companions'}
LEADER = 'society::actor.study_caravanner'
INVITEE = 'society::actor.apprentice'


def validate_source(content: dict, original: dict) -> dict:
    """自有合同校验仅证明作者来源与旧状态未改，不能代替共享schema、安装或正式World验证。"""
    scenario = next(row for row in content['scenarios'] if row['id'] == 'scenario.qingchuan_following')
    previous = original['scenarios'][0]
    assert scenario['dynamic_world'] == previous['dynamic_world']
    for section in ('actors', 'wallets', 'inventories', 'relations', 'estates', 'garrisons', 'offer_ids'):
        assert scenario['initial'][section] == previous['initial'][section], section
    old_roles = {row['role_id']: row for row in previous['initial']['roles']}
    new_roles = {row['role_id']: row for row in scenario['initial']['roles']}
    assert all(new_roles[key] == value for key, value in old_roles.items())
    assert set(new_roles) - set(old_roles) == {'role.follow_caravanner'}
    assert content['scenarios'][0] == previous
    assert {LEADER, INVITEE}.issubset(scenario['enabled']['actors'])
    leader = next(row for row in scenario['initial']['actors'] if row['actor_id'] == LEADER)
    assert leader['place_id'] == 'society::place.courtyard'
    original_role = next(row for row in original['roles'] if row['id'] == 'role.companion')
    role = next(row for row in content['roles'] if row['id'] == 'role.follow_caravanner')
    assert role['companion_cost_per_tick'] == original_role['companion_cost_per_tick'] == 6
    assert role['initial_actor'] == LEADER and role['place_id'] == leader['place_id']
    assert CAPABILITIES <= set(content['required_capabilities'])
    assert scenario['party_invitation_profile'] == 'authored_node_v1'
    block = scenario['party_invitations']
    assert block['version'] == 1 and len(block['invitations']) == 1
    source = block['invitations'][0]
    assert source['leader_actor_id'] == LEADER and source['invitee_actor_id'] == INVITEE
    dialogue = next(row for row in content['dialogues'] if row['id'] == source['dialogue_id'])
    assert dialogue['context_role'] == source['role_id'] == role['id']
    node = next(row for row in dialogue['nodes'] if row['id'] == source['node_id'])
    assert node['kind'] == 'choice' and node['id'] == 'invitation'
    assert all('command' not in row for row in dialogue['nodes'])
    assert next(row for row in node['options'] if row['id'] == 'decline')['next'] == 'declined'
    assert LEADER in scenario['dynamic_world']['director']['subjects']
    return {'status': 'OWNED_AUTHOR_CONTRACT_VALID', 'formal_schema_verified': False,
            'runtime_verified': False, 'unchanged_initial_assets': True,
            'unchanged_director_policy': True, 'original_companion_cost_per_travel_tick': 6}


def generate(repo: Path, output: Path) -> dict:
    """新包保留原回应情景并新增随行情景；角色只邀请本人，后续行程仍由原Director实际决定。"""
    if output.resolve() == (repo / BASE_PACK).resolve() or (output / '.git').exists():
        raise ValueError('不能覆盖原回应包或仓库根')
    originals = {name: subprocess.check_output(['git', 'show', BASE + ':' + BASE_PACK + '/' + name], cwd=repo)
                 for name in ('content.json', 'pack.json')}
    original = json.loads(originals['content.json'])
    content = deepcopy(original)
    manifest = json.loads(originals['pack.json'])
    content['version'] = manifest['version'] = '1.2.0'
    for document in (content, manifest):
        assert CAPABILITIES <= set(document['required_capabilities'])
    content['texts'].update({
        'name.scenario.following': '青川同行相邀',
        'name.role.follow_caravanner': '行商的同行邀请',
        'line.follow.welcome': '我还要照应手头的差事。你若愿意，可以跟我同路；先听清楚，再由你自己决定。',
        'line.follow.invitation': '我愿带你同行。行程由我照手头事务安排；路上每半日六钱吃用，由我照应；停下来不收这笔路费。你愿意就当面应承，不愿也无妨。',
        'option.follow.decline': '这次先不同行。',
        'option.follow.leave': '我先告辞，再考虑一下。',
        'line.follow.declined': '好，我们各自照应手头的事。以后有机会，再当面商量。',
    })
    content['dialogues'].append({
        'id': 'dialog.follow_caravanner', 'context_role': 'role.follow_caravanner', 'entry': 'welcome',
        'nodes': [
            {'id': 'welcome', 'kind': 'say', 'speaker': '$provider', 'text_key': 'line.follow.welcome', 'next': 'invitation'},
            {'id': 'invitation', 'kind': 'choice', 'text_key': 'line.follow.invitation',
             'options': [
                 {'id': 'decline', 'text_key': 'option.follow.decline', 'conditions': [], 'next': 'declined'},
                 {'id': 'leave', 'text_key': 'option.follow.leave', 'conditions': [], 'next': 'end'},
             ]},
            {'id': 'declined', 'kind': 'say', 'speaker': '$provider', 'text_key': 'line.follow.declined', 'next': 'end'},
            {'id': 'end', 'kind': 'end'},
        ]})
    original_role = next(row for row in original['roles'] if row['id'] == 'role.companion')
    content['roles'].append({
        'id': 'role.follow_caravanner', 'name_key': 'name.role.follow_caravanner', 'organization_id': None,
        'initial_actor': LEADER, 'place_id': 'society::place.courtyard', 'services': ['information', 'join_party'],
        'companion_cost_per_tick': original_role['companion_cost_per_tick'],
        'companion_required_trust': original_role['companion_required_trust'],
        'dialogue_id': 'dialog.follow_caravanner',
    })
    invitation_block = {'version': 1, 'invitations': [{
        'id': 'invitation.caravanner', 'leader_actor_id': LEADER, 'invitee_actor_id': INVITEE,
        'dialogue_id': 'dialog.follow_caravanner', 'node_id': 'invitation', 'role_id': 'role.follow_caravanner',
    }]}
    scenario = deepcopy(original['scenarios'][0])
    scenario.update(id='scenario.qingchuan_following', seed_text='qingchuan-party-invitations-v1',
                    name_key='name.scenario.following', party_invitation_profile='authored_node_v1',
                    party_invitations=invitation_block)
    scenario['enabled']['roles'].append('role.follow_caravanner')
    scenario['enabled']['dialogues'].append('dialog.follow_caravanner')
    scenario['initial']['roles'].append({'role_id': 'role.follow_caravanner', 'actor_id': LEADER, 'enabled': True})
    content['scenarios'].append(scenario)
    result = validate_source(content, original)
    output.mkdir(parents=True, exist_ok=True)
    for name, value in (('content.json', content), ('pack.json', manifest)):
        (output / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    result.update(base_commit=BASE, scenario_id='contacts::scenario.qingchuan_following',
                  preserved_scenario_id='contacts::scenario.qingchuan_responses', pack_version='1.2.0',
                  original_source_sha256={name: hashlib.sha256(raw).hexdigest() for name, raw in originals.items()},
                  files={name: hashlib.sha256((output / name).read_bytes()).hexdigest() for name in originals})
    return result


def main() -> int:
    """只生成本域作者文件；正式schema/namespace/安装仍由下一公共候选统一处理。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(generate(args.repo.resolve(), args.output.resolve()), ensure_ascii=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
