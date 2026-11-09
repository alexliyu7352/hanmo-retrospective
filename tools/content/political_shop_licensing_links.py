"""私铺许可只覆盖显式完整司法册与原柜台声明，不从缺少通缉数据推断全境清白。"""

from .political_shop_author_profile import political_shop_author_catalog, political_shop_author_pair

CAPABILITY = 'political_shop_licensing_v1'
COURT_FIELDS = {'id', 'jurisdiction_id', 'office_id', 'role_id', 'dialogue_id',
                'request_node_id', 'premises_ids', 'warrant_reason_codes',
                'initial_wanted', 'complete_registry', 'permit_ticks'}


def _ids(value, maximum):
    """作者名单要求有界且唯一，布尔和空串不能充作具名来源。"""
    return (isinstance(value, list) and 1 <= len(value) <= maximum
            and all(isinstance(item, str) and 0 < len(item) <= 4096 for item in value)
            and len(set(value)) == len(value))


def check_political_shop_licensing(pack, index):
    """正规作者同时核法律/私铺pair、同城管辖、原Office专项Grant和真实provider节点。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        config = dynamic.get('political_shop_licensing')
        shops = dynamic.get('private_shops')
        if config is None and shops is None:
            continue
        root = ('scenarios', number, 'dynamic_world', 'political_shop_licensing')

        def issue(code, key, text):
            """错误只回指原声明，不补造监管机构、初册或许可。"""
            issues.append((code, root + tuple(key), text))

        profile = config
        config = political_shop_author_catalog(profile)
        if (config is None or not political_shop_author_pair(profile, shops)
                or not isinstance(shops.get('premises'), dict) or not 1 <= len(shops['premises']) <= 4096
                or any(not isinstance(key, str) or not key or not isinstance(record, dict) or record.get('id') != key
                       for key, record in shops['premises'].items())):
            issue('POLITICAL_SHOP_PAIR', (), '法律1与私铺1/2或法律2与私铺3须明确同启，并含有限法院及铺位目录')
            continue
        if profile['version'] == 2:
            root += ('catalog',)
        organization = dynamic.get('organization', {})
        politics = dynamic.get('politics', {})
        if (dynamic.get('profile') != 'local_society_v1' or organization.get('enabled') is not True
                or politics.get('enabled') is not True
                or not {CAPABILITY, 'private_shops_v1'} <= set(pack.get('required_capabilities', []))):
            issue('POLITICAL_SHOP_DEPENDENCIES', (), '须保原政治/组织目录与两项显式能力')
        premises = shops['premises']
        offices, grants = organization.get('offices', {}), organization.get('grants', {})
        jurisdictions = politics.get('jurisdictions', {})
        enabled = scenario.get('enabled', {})
        seen_jurisdictions, seen_premises = set(), set()
        for key, court in config['courts'].items():
            if (not isinstance(court, dict) or set(court) != COURT_FIELDS
                    or any(not isinstance(court.get(field), str) or not court[field] or len(court[field]) > 4096
                           for field in ('id', 'jurisdiction_id', 'office_id', 'role_id', 'dialogue_id', 'request_node_id'))
                    or court['id'] != key or court['complete_registry'] is not True
                    or type(court['permit_ticks']) is not int or not 1 <= court['permit_ticks'] <= 4096
                    or not _ids(court['premises_ids'], 4096) or not _ids(court['warrant_reason_codes'], 256)
                    or not isinstance(court['initial_wanted'], list) or len(court['initial_wanted']) > 4096):
                issue('POLITICAL_SHOP_COURT', (key,), '法院必须显式完整初册、期限、原因和逐铺准入名单')
                continue
            j = jurisdictions.get(court['jurisdiction_id'], {})
            office = offices.get(court['office_id'], {})
            role = index.get('roles', {}).get(court['role_id'], {})
            seat_id = j.get('seat_place_id')
            seat = index.get('places', {}).get(seat_id, {})
            org = j.get('political_organization_id')
            if (not j or j.get('revision', 0) != 0 or j.get('controlling_organization_id') != org
                    or court['jurisdiction_id'] in seen_jurisdictions
                    or not office or office.get('organization_id') != org
                    or not role or role.get('organization_id') != org or role.get('place_id') != seat_id
                    or not seat.get('settlement_id')
                    or seat_id not in enabled.get('places', []) or court['role_id'] not in enabled.get('roles', [])
                    or court['dialogue_id'] not in enabled.get('dialogues', [])):
                issue('POLITICAL_SHOP_JURISDICTION', (key,), '原司法公职、柜台与完整初册须属于同一真实辖区')
            seen_jurisdictions.add(court['jurisdiction_id'])
            dialogue = index.get('dialogues', {}).get(court['dialogue_id'], {})
            nodes = [n for n in dialogue.get('nodes', []) if n.get('id') == court['request_node_id']]
            if (dialogue.get('context_role') != court['role_id'] or len(nodes) != 1
                    or nodes[0].get('kind') != 'say' or nodes[0].get('speaker') != '$provider'):
                issue('POLITICAL_SHOP_DECLARATION', (key,), '查核仅由原柜台provider具名say节点办理')
            for premises_id in court['premises_ids']:
                p = premises.get(premises_id, {})
                place = index.get('places', {}).get(p.get('place_id'), {})
                if (not p or premises_id in seen_premises or not place
                        or place.get('settlement_id') != seat.get('settlement_id')
                        or p.get('place_id') not in enabled.get('places', [])
                        or p.get('lessor_actor_id') not in enabled.get('actors', [])
                        or p.get('lessor_actor_id') not in index.get('actors', {})):
                    issue('POLITICAL_SHOP_PREMISES', (key, 'premises_ids'), '铺位必须逐一列明并在原辖区真实同城范围内')
                seen_premises.add(premises_id)
            seeds = set()
            for seed in court['initial_wanted']:
                if (not isinstance(seed, dict) or set(seed) != {'actor_id', 'reason_code'}
                        or not isinstance(seed.get('actor_id'), str) or not isinstance(seed.get('reason_code'), str)
                        or seed['actor_id'] not in index.get('actors', {})
                        or seed['actor_id'] not in enabled.get('actors', []) or seed['actor_id'] in seeds
                        or seed['reason_code'] not in court['warrant_reason_codes']):
                    issue('POLITICAL_SHOP_INITIAL_REGISTRY', (key, 'initial_wanted'), '原初册逐本人和具名原因登记，不接受运行期事实')
                if isinstance(seed, dict) and isinstance(seed.get('actor_id'), str):
                    seeds.add(seed['actor_id'])
            valid_grant = False
            for grant_id in office.get('grant_ids', []):
                g = grants.get(grant_id, {})
                if (g.get('id') == grant_id and g.get('office_id') == court['office_id']
                        and {'political_issue_shop_permit', 'political_record_shop_clearance'} <= set(g.get('allowed_command_kinds', []))
                        and not g.get('wallet_ids') and not g.get('inventory_ids')
                        and type(g.get('amount_limit', 0)) is int and g.get('amount_limit', 0) >= 0
                        and {'kind': 'site', 'id': seat_id} in g.get('scopes', [])):
                    valid_grant = True
            if not valid_grant:
                issue('POLITICAL_SHOP_GRANT', (key,), '同一原Office专项Grant须同时授清白查核与具名铺位签证')
        if seen_premises != set(premises):
            issue('POLITICAL_SHOP_PREMISES_COVERAGE', (), '每个可办理租铺的铺位必须恰归一个明确法院，不允许遗漏管辖入口')
    return issues
