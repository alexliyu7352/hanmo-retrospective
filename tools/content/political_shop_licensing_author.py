"""从已存在作者人物/法院/铺位生成独立声明片段；调用者显式合并后再走正规compose。"""
from copy import deepcopy

ISSUE_COMMANDS = ('political_issue_shop_permit', 'political_record_shop_clearance')
ADMIN_COMMANDS = ('political_register_wanted', 'political_release_wanted', 'political_revoke_shop_permit')


def build_political_shop_court_declaration(
        scenario, index, *, court_id, jurisdiction_id, office_id, counter_actor_id,
        role_id, request_node_id, premises_ids, grant_id, complete_registry,
        initial_wanted, warrant_reason_codes, permit_ticks=20,
        administrative_commands=(), valid_from=0, valid_until=None):
    """只提供明确原岗的初态片段，不创建人物/对白，不签发Common司法事实或许可。"""
    def require(condition, code):
        if not condition:
            raise ValueError(code)

    def named(value):
        return isinstance(value, str) and 0 < len(value) <= 4096

    for value in (court_id, jurisdiction_id, office_id, counter_actor_id, role_id, request_node_id, grant_id):
        require(named(value), 'PoliticalShopAuthorIdentityRequired')
    require(complete_registry is True and isinstance(initial_wanted, list), 'PoliticalShopExplicitCompleteRegistryRequired')
    require(type(permit_ticks) is int and 1 <= permit_ticks <= 4096, 'PoliticalShopAuthorDeadlineRequired')
    require(type(valid_from) is int and 0 <= valid_from <= 2147483647, 'PoliticalShopAuthorGrantWindowRequired')
    require(valid_until is None or type(valid_until) is int and valid_from <= valid_until <= 2147483647,
            'PoliticalShopAuthorGrantWindowRequired')
    require(isinstance(premises_ids, list) and 1 <= len(premises_ids) <= 4096
            and all(named(v) for v in premises_ids) and len(set(premises_ids)) == len(premises_ids),
            'PoliticalShopAuthorPremisesRequired')
    require(isinstance(warrant_reason_codes, list) and 1 <= len(warrant_reason_codes) <= 256
            and all(named(v) for v in warrant_reason_codes) and len(set(warrant_reason_codes)) == len(warrant_reason_codes),
            'PoliticalShopAuthorReasonsRequired')
    require(isinstance(administrative_commands, (list, tuple))
            and all(v in ADMIN_COMMANDS for v in administrative_commands)
            and len(set(administrative_commands)) == len(administrative_commands),
            'PoliticalShopAuthorAdministrativeScopeRequired')
    dynamic = scenario.get('dynamic_world', {})
    organization, politics, shops = (dynamic.get(name, {}) for name in ('organization', 'politics', 'private_shops'))
    require(dynamic.get('profile') == 'local_society_v1' and organization.get('enabled') is True
            and politics.get('enabled') is True and shops.get('enabled') is True
            and type(shops.get('version')) is int and shops['version'] == 1, 'PoliticalShopAuthorProfileRequired')
    jurisdiction = politics.get('jurisdictions', {}).get(jurisdiction_id)
    office = organization.get('offices', {}).get(office_id)
    role = index.get('roles', {}).get(role_id)
    require(isinstance(jurisdiction, dict) and isinstance(office, dict) and isinstance(role, dict),
            'PoliticalShopOriginalCounterRequired')
    org, seat_id = jurisdiction.get('political_organization_id'), jurisdiction.get('seat_place_id')
    seat = index.get('places', {}).get(seat_id, {})
    enabled = scenario.get('enabled', {})
    require(jurisdiction.get('revision', 0) == 0 and jurisdiction.get('controlling_organization_id') == org
            and office.get('organization_id') == org and office.get('holder_actor_id') == counter_actor_id
            and office.get('status') == 'active' and office.get('id') == office_id
            and role.get('organization_id') == org and role.get('place_id') == seat_id
            and role.get('initial_actor') == counter_actor_id and named(seat.get('settlement_id'))
            and counter_actor_id in index.get('actors', {}) and counter_actor_id in enabled.get('actors', [])
            and role_id in enabled.get('roles', []) and seat_id in enabled.get('places', []),
            'PoliticalShopOriginalCounterMismatch')
    dialogue_id = role.get('dialogue_id')
    dialogue = index.get('dialogues', {}).get(dialogue_id, {})
    nodes = [row for row in dialogue.get('nodes', []) if row.get('id') == request_node_id]
    require(dialogue_id in enabled.get('dialogues', []) and dialogue.get('context_role') == role_id
            and dialogue.get('entry') == request_node_id and len(nodes) == 1
            and nodes[0].get('kind') == 'say' and nodes[0].get('speaker') == '$provider',
            'PoliticalShopExistingProviderEntryRequired')
    originals = shops.get('premises', {})
    require(isinstance(originals, dict) and 1 <= len(originals) <= 4096
            and all(named(key) and isinstance(record, dict) and record.get('id') == key
                    for key, record in originals.items()), 'PoliticalShopAuthorPremisesRequired')
    for identifier in premises_ids:
        premise = originals.get(identifier)
        require(isinstance(premise, dict) and premise.get('id') == identifier, 'PoliticalShopOriginalPremisesRequired')
        place = index.get('places', {}).get(premise.get('place_id'), {})
        require(place.get('settlement_id') == seat['settlement_id']
                and premise.get('place_id') in enabled.get('places', [])
                and premise.get('lessor_actor_id') in enabled.get('actors', [])
                and premise.get('lessor_actor_id') in index.get('actors', {}),
                'PoliticalShopOriginalPremisesMismatch')
    require(len(initial_wanted) <= 4096, 'PoliticalShopAuthorRegistryLimit')
    known = set()
    for row in initial_wanted:
        require(isinstance(row, dict) and set(row) == {'actor_id', 'reason_code'}
                and named(row.get('actor_id')) and row['actor_id'] in index.get('actors', {})
                and row['actor_id'] in enabled.get('actors', []) and row['actor_id'] not in known
                and row.get('reason_code') in warrant_reason_codes, 'PoliticalShopOriginalWantedSeedRequired')
        known.add(row['actor_id'])
    court = {'id': court_id, 'jurisdiction_id': jurisdiction_id, 'office_id': office_id,
             'role_id': role_id, 'dialogue_id': dialogue_id, 'request_node_id': request_node_id,
             'premises_ids': deepcopy(premises_ids), 'warrant_reason_codes': deepcopy(warrant_reason_codes),
             'initial_wanted': deepcopy(initial_wanted), 'complete_registry': True, 'permit_ticks': permit_ticks}
    commands = list(ISSUE_COMMANDS) + list(administrative_commands)
    grant = {'id': grant_id, 'office_id': office_id, 'allowed_command_kinds': commands,
             'scopes': [{'kind': 'site', 'id': seat_id}], 'wallet_ids': [], 'inventory_ids': [],
             'amount_limit': 0, 'valid_from': valid_from, 'valid_until': valid_until,
             'may_delegate': False, 'continuity_policy_id': 'stop_when_vacant'}
    old_court = dynamic.get('political_shop_licensing', {}).get('courts', {}).get(court_id)
    old_grant = organization.get('grants', {}).get(grant_id)
    require(old_court is None or old_court == court, 'PoliticalShopAuthorCourtConflict')
    require(old_grant is None or old_grant == grant, 'PoliticalShopAuthorGrantConflict')
    grants = list(office.get('grant_ids', []))
    require(all(named(value) for value in grants) and len(grants) == len(set(grants)), 'PoliticalShopOriginalOfficeGrantMismatch')
    if grant_id not in grants:
        grants.append(grant_id)
    return {'court': court, 'grant': grant, 'office_id': office_id, 'office_grant_ids': grants,
            'required_capabilities': ['political_shop_licensing_v1', 'private_shops_v1'],
            'required_political_command_names': commands}
