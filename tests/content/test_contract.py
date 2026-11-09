"""Tests of authoring data, not a Python implementation of game rules."""
import copy
import importlib
import json
from pathlib import Path
import sys
import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))


def api():
    """Load the real checker, with an explicit missing-feature assertion for the first RED run."""
    assert (ROOT / 'tools/content/validation.py').exists(), 'Content validator is not implemented'
    return importlib.import_module('tools.content.validation')


def load_fixture(name='two_stories'):
    """Read a fresh actual authored pack for each test; mutations never touch the source."""
    return json.loads((ROOT / 'content/foundation' / (name + '.json')).read_text(encoding='utf8'))


def codes(pack):
    """Collect stable error identifiers from the actual validation result."""
    return {i.code for i in api().validate_pack(pack, 'fixture.json').issues}


@pytest.mark.parametrize('name', ['procurement', 'two_stories'])
def test_authored_samples_are_statically_valid(name):
    """Deleting a required reference or adding an invalid node must make this fail."""
    report = api().validate_pack(load_fixture(name), name + '.json')
    assert report.ok, report.to_dict()
    assert report.to_dict()['status'] == 'STRUCTURE_VALID'
    assert report.to_dict()['runtime_verified'] is False


@pytest.mark.parametrize('path,value', [
    (('actors', 0, 'initial_place'), 'place.missing'),
    (('roles', 0, 'initial_actor'), 'actor.missing'),
    (('roles', 0, 'organization_id'), 'org.missing'),
    (('settlements', 0, 'entry_place'), 'place.missing'),
    (('routes', 0, 'origin'), 'city.missing'),
    (('shops', 0, 'wallet_id'), 'wallet.missing'),
    (('shops', 0, 'inventory_id'), 'inventory.missing'),
    (('offers', 0, 'accept_role'), 'role.missing'),
    (('offers', 0, 'funding_wallet'), 'wallet.missing'),
    (('offers', 1, 'goal', 'receiver_actor'), 'actor.missing'),
    (('events', 0, 'dialogue_id'), 'dialog.missing'),
    (('actors', 0, 'name_key'), 'text.missing'),
    (('dialogues', 0, 'entry'), 'missing'),
])
def test_dangling_reference_has_path(path, value):
    """A broken reference must not be silently accepted as an empty list."""
    p = load_fixture()
    target = p
    for key in path[:-1]:
        target = target[key]
    target[path[-1]] = value
    report = api().validate_pack(p, 'fixture.json')
    assert not report.ok
    assert any(i.code in {'REFERENCE', 'DIALOGUE_TARGET'} for i in report.issues)
    assert all(i.path.startswith('fixture.json#/') for i in report.issues)


@pytest.mark.parametrize('bad', [-1, 650.5, True, '650', 2147483648])
def test_budget_requires_bounded_integer(bad):
    """Malformed content must be rejected before relationships are traversed."""
    p = load_fixture(); p['offers'][0]['budget'] = bad
    assert 'SCHEMA' in codes(p)


def test_float_written_as_integer_is_rejected():
    """Even 650.0 is rejected to keep strict cross-runtime integer semantics explicit."""
    p = load_fixture(); p['offers'][0]['budget'] = 650.0
    assert 'SCHEMA' in codes(p)


@pytest.mark.parametrize('table', ['actors', 'places', 'offers', 'roles', 'dialogues'])
def test_duplicate_ids_rejected(table):
    """A second record cannot shadow the first record in an index."""
    p = load_fixture(); p[table].append(copy.deepcopy(p[table][0]))
    assert 'DUPLICATE_ID' in codes(p)


def test_new_npc_requires_no_fixed_actor_count():
    """Adding an ordinary original NPC must not require a kernel source edit."""
    p = load_fixture(); a = copy.deepcopy(p['actors'][0]); a.update(id='actor.guest', dialogue_id=None)
    p['actors'].append(a)
    assert api().validate_pack(p, 'fixture').ok


def test_display_name_change_keeps_identity():
    """Changing a name edits only localization, not references."""
    p = load_fixture(); p['texts'][p['actors'][1]['name_key']] = '另一称呼'
    assert api().validate_pack(p, 'fixture').ok


def test_absent_npc_definition_is_not_a_phantom_duplicate():
    """An assigned worker may start away; runtime must handle absence, not duplicate the actor."""
    p = load_fixture(); p['actors'][1]['initial_place'] = 'west.street'
    assert api().validate_pack(p, 'fixture').ok


def test_checker_does_not_mutate_content():
    """Validation must not silently rewrite IDs or initialize runtime state."""
    p = load_fixture(); before = copy.deepcopy(p)
    api().validate_pack(p, 'fixture')
    assert p == before


@pytest.mark.parametrize('mutation, expected', [
 ('no_trade','ROLE_SERVICE'),('wrong_shop_place','SHOP_ROLE'),('wrong_wallet_owner','OWNER_MISMATCH'),
 ('wrong_inventory_owner','OWNER_MISMATCH'),('unfunded_offer','OFFER_AUTHORITY'),
 ('wrong_accept_role','ROLE_SERVICE'),('wrong_template','GOAL_TEMPLATE'),('wrong_message_item','GOAL_ITEM'),
 ('missing_notice_link','DISCOVERY_LINK'),('personal_public_mismatch','OFFER_AUTHORITY'),
 ('duplicate_goods','DUPLICATE_GOODS'),('overfull','INITIAL_CAPACITY'),
 ('unknown_flag','REFERENCE'),('wrong_event_place','EVENT_CONTEXT'),
 ('undeclared_capability','CAPABILITY_DECLARATION'),('same_relation','SELF_RELATION'),
 ('parent_cycle','PLACE_PARENT'),('cross_city_passage','CROSS_SETTLEMENT_PASSAGE'),
 ('stranded_place','PLACE_RETURN'),('unreachable_place','PLACE_REACHABILITY'),
 ('one_way_world','WORLD_RETURN'),('self_route','SELF_ROUTE'),
])
def test_semantic_content_errors(mutation, expected):
    """Each mutation violates one contract that plain JSON Schema cannot establish."""
    p = load_fixture()
    if mutation == 'no_trade': p['roles'][1]['services'] = ['information']
    elif mutation == 'wrong_shop_place': p['roles'][1]['place_id'] = 'west.office'
    elif mutation == 'wrong_wallet_owner': p['wallets'][2]['owner']['id'] = 'org.office'
    elif mutation == 'wrong_inventory_owner': p['inventories'][0]['owner']['id'] = 'org.office'
    elif mutation == 'unfunded_offer': p['offers'][0]['funding_wallet'] = 'wallet.player'
    elif mutation == 'wrong_accept_role': p['offers'][0]['accept_role'] = 'role.clerk'
    elif mutation == 'wrong_template': p['offers'][0]['template'] = 'message'
    elif mutation == 'wrong_message_item': p['offers'][1]['goal']['message_id'] = 'item.grain'
    elif mutation == 'missing_notice_link': p['props'][0]['offer_ids'] = []
    elif mutation == 'personal_public_mismatch': p['offers'][1]['report_role'] = 'role.office'
    elif mutation == 'duplicate_goods': p['inventories'][0]['goods'] *= 2
    elif mutation == 'overfull': p['inventories'][0]['capacity'] = 20
    elif mutation == 'unknown_flag': p['places'][0]['admission'] = [{'op':'flag','flag':'flag.missing','equals':True}]
    elif mutation == 'wrong_event_place': p['events'][0]['place_id'] = 'west.office'
    elif mutation == 'undeclared_capability': p['required_capabilities'].remove('tasks.message')
    elif mutation == 'same_relation': p['relations'][0]['to_actor'] = p['relations'][0]['from_actor']
    elif mutation == 'parent_cycle': p['places'][0]['parent_place'] = 'west.office'
    elif mutation == 'cross_city_passage': p['places'][1]['passages'] = ['east.home']
    elif mutation == 'stranded_place': p['places'][1]['passages'] = ['west.office']
    elif mutation == 'unreachable_place': p['places'][0]['passages'].remove('west.grain')
    elif mutation == 'one_way_world': p['routes'].pop()
    elif mutation == 'self_route': p['routes'][0]['destination'] = p['routes'][0]['origin']
    assert expected in codes(p), codes(p)


def test_dialogue_cannot_grant_money_directly():
    """Only registered domain requests are allowed, not wallet-writing script strings."""
    p = load_fixture(); next(n for n in p['dialogues'][0]['nodes'] if n['kind']=='action')['command'] = {'kind':'add_money','amount':650}
    assert 'SCHEMA' in codes(p)


def test_unimplemented_battle_command_is_rejected():
    """A future battle is not allowed to masquerade as a successful action node."""
    p = load_fixture(); next(n for n in p['dialogues'][0]['nodes'] if n['kind']=='action')['command'] = {'kind':'battle_win'}
    assert 'SCHEMA' in codes(p)


def test_dialogue_action_requires_appropriate_person():
    """A trader cannot accept public procurement simply because the node exists."""
    p = load_fixture(); d = p['dialogues'][1]
    next(n for n in d['nodes'] if n['kind']=='action')['command'] = {'kind':'accept_offer','offer_id':'offer.grain'}
    assert 'DIALOGUE_AUTHORITY' in codes(p)


def test_shop_context_cannot_serve_another_shop():
    """Opening trade must use this representative's shop, not a global market screen."""
    p = load_fixture(); p['dialogues'][1]['context_role'] = 'role.office'
    assert 'DIALOGUE_AUTHORITY' in codes(p)


def test_missing_dialogue_next_is_reported():
    """Node links resolve within the containing dialogue only."""
    p = load_fixture(); p['dialogues'][0]['nodes'][1]['next'] = 'missing'
    assert 'DIALOGUE_TARGET' in codes(p)


def test_duplicate_dialogue_node_is_reported():
    """Local node IDs may repeat across dialogues but not within one dialogue."""
    p = load_fixture(); p['dialogues'][0]['nodes'].append(copy.deepcopy(p['dialogues'][0]['nodes'][0]))
    assert 'DUPLICATE_NODE' in codes(p)


def test_unreachable_dialogue_node_is_reported():
    """Unconnected authoring debris must be diagnosed, not shipped invisibly."""
    p = load_fixture(); p['dialogues'][0]['nodes'].append({'id':'unused','kind':'end'})
    assert 'DIALOGUE_UNREACHABLE' in codes(p)


def test_dialogue_without_end_is_reported():
    """A menu with no route back to the scene must fail static validation."""
    p = load_fixture(); d=p['dialogues'][1]; d['nodes'][-1]={'id':'end','kind':'say','speaker':'$provider','text_key':'common.leave','next':'hello'}
    assert 'DIALOGUE_NO_END' in codes(p)


def test_autonomous_branch_loop_is_reported():
    """Branch cycles cannot spin the interpreter before returning player control."""
    p = load_fixture(); p['dialogues'][0]['nodes'][0]['if_true'] = 'entry'
    assert 'AUTOMATIC_CYCLE' in codes(p)


def test_choice_needs_unconditional_exit():
    """All-false condition sets need a guaranteed player-controlled escape."""
    p = load_fixture(); c=next(n for n in p['dialogues'][0]['nodes'] if n['kind']=='choice')
    for o in c['options']: o['conditions']=[{'op':'flag','flag':'office.introduced','equals':True}]
    assert 'CHOICE_ESCAPE' in codes(p)


def test_error_count_and_order_are_stable():
    """Editor diagnostics must not jump around with dictionary iteration order."""
    p = load_fixture(); p['actors'][0]['initial_place']='bad';p['roles'][0]['place_id']='bad'
    a=api().validate_pack(p,'x').to_dict();b=api().validate_pack(p,'x').to_dict()
    assert a == b


def test_schema_is_self_contained_and_offline():
    """No schema resolution may download a remote content definition."""
    s=json.loads((ROOT/'schemas/content.schema.json').read_text())
    from jsonschema import Draft202012Validator
    Draft202012Validator.check_schema(s)
    def walk(v):
        """Traverse schema nodes and reject external references."""
        if isinstance(v,dict):
            if '$ref' in v: assert v['$ref'].startswith('#/')
            for value in v.values(): walk(value)
        elif isinstance(v,list):
            for value in v: walk(value)
    walk(s)


def test_zero_time_discovery_and_branch_cycle_is_reported():
    """A loop mixing branch and zero-time discovery must not evade the automatic-cycle check."""
    p = load_fixture()
    d = p['dialogues'][0]
    d['nodes'][0]['if_true'] = 'discover'
    next(n for n in d['nodes'] if n['id'] == 'discover')['on_success'] = 'entry'
    assert 'AUTOMATIC_CYCLE' in codes(p)


def test_procurement_receiver_must_represent_the_issuer():
    """An unrelated private errand giver cannot acquire authority to receive public goods."""
    p = load_fixture()
    p['offers'][0]['goal']['receiver_role'] = 'role.sender'
    assert 'OFFER_AUTHORITY' in codes(p)


def _dynamic_provision_fixture(owner='estate', duty='labor'):
    """只在正式作者副本加一名跨域领粮者；不更改实际内容或模拟世界结算。"""
    pack = json.loads((ROOT / 'content/packs/authored/local_society_1_0/content.json').read_text())
    pack['scenarios'] = [pack['scenarios'][0]]
    scenario = pack['scenarios'][0]
    initial, enabled = scenario['initial'], scenario['enabled']
    economy = scenario['dynamic_world']['local_economy']
    actor_id, binding_id = 'actor.city_steward', 'provision.cross_domain'
    obligation = copy.deepcopy(economy['obligations']['obligation.city_ration'])
    obligation.update(id='obligation.cross_domain', beneficiary={'kind': 'actor', 'id': actor_id})
    economy['obligations'][obligation['id']] = obligation
    scenario['dynamic_world']['organization']['institutional_issuers']['issuer.city']['obligation_ids'].append(obligation['id'])
    economy['provision_bindings'][binding_id] = {
        'id': binding_id, 'subject': {'kind': 'actor', 'id': actor_id}, 'from_tick': 0,
        'through_tick': 2147483647, 'payer_inventory': 'inventory.city',
        'obligation_id': obligation['id'], 'policy_owner': 'macro'}
    if owner == 'estate':
        definition = {'id': 'estate.cross_domain', 'place_id': 'place.courtyard',
                      'inventory_id': 'inventory.cross_domain'}
        pack['estates'].append(definition)
        enabled['estates'] = [definition['id']]
        initial['estates'] = [{
            'id': definition['id'], 'owner_actor_id': 'actor.tutor',
            'manager_actor_id': actor_id if duty == 'manager' else 'actor.tutor',
            'resident_actor_ids': [actor_id, 'actor.tutor'],
            'initial_facilities': {'farm': 1, 'granary': 0, 'palisade': 0},
            'initial_duties': [{'actor_id': actor_id, 'duty': 'idle' if duty == 'manager' else duty}]}]
    else:
        definition = {'id': 'garrison.cross_domain', 'place_id': 'place.courtyard',
                      'inventory_id': 'inventory.cross_domain'}
        pack['garrisons'].append(definition)
        enabled['garrisons'] = [definition['id']]
        initial['garrisons'] = [{
            'id': definition['id'], 'organization_id': 'org.city',
            'units': [{'id': 'unit.cross_domain', 'commander_actor_id': actor_id}]}]
    return pack, binding_id


def _dynamic_provision_issues(pack):
    """直接调用受影响的实际跨域作者门；不带跑历史完整作者矩阵。"""
    from tools.content.dynamic_world_links import check_dynamic_world
    from tools.content.validation import TABLES
    index = {table: {row['id']: row for row in pack.get(table, [])} for table in TABLES}
    index.update(texts=pack['texts'], flags=pack['flags'])
    return check_dynamic_world(pack, index)


@pytest.mark.parametrize('duty', ['manager', 'labor', 'guard'])
def test_dynamic_provision_rejects_actual_estate_mouths(duty):
    """在场经理、劳动者及守卫都由原庄园供养，不能同时从宏观公库再领粮。"""
    pack, binding_id = _dynamic_provision_fixture(duty=duty)
    issues = _dynamic_provision_issues(pack)
    assert any(code == 'DYNAMIC_PROVISION_OWNER_CONFLICT' and path[-2:] == (binding_id, 'policy_owner')
               for code, path, _message in issues)


@pytest.mark.parametrize('change', ['idle', 'away', 'dead', 'unavailable', 'disabled_estate'])
def test_dynamic_provision_excludes_nonfeeding_initial_estate_residents(change):
    """不把闲居、异地、不可用或未启用庄园的居民误算作原阶段实际领粮人。"""
    pack, _binding_id = _dynamic_provision_fixture(duty='idle' if change == 'idle' else 'labor')
    scenario = pack['scenarios'][0]
    actor = next(row for row in scenario['initial']['actors'] if row['actor_id'] == 'actor.city_steward')
    if change == 'away': actor['place_id'] = 'place.school'
    elif change == 'dead': actor['alive'] = False
    elif change == 'unavailable': actor['available'] = False
    elif change == 'disabled_estate': scenario['enabled']['estates'] = []
    assert not any(code == 'DYNAMIC_PROVISION_OWNER_CONFLICT' for code, _path, _message in _dynamic_provision_issues(pack))


@pytest.mark.parametrize('owner', ['estate', 'military'])
def test_dynamic_provision_accepts_matching_legacy_owner_without_macro_ration(owner):
    """真实旧领域继续领粮时允许明确选择它，但同一宏观责任必须未激活。"""
    pack, binding_id = _dynamic_provision_fixture(owner=owner)
    economy = pack['scenarios'][0]['dynamic_world']['local_economy']
    economy['obligations']['obligation.cross_domain']['active'] = False
    economy['provision_bindings'][binding_id].update(policy_owner=owner, payer_inventory='inventory.cross_domain')
    assert not any(code.startswith('DYNAMIC_PROVISION_') for code, _path, _message in _dynamic_provision_issues(pack))


@pytest.mark.parametrize('owner', ['estate', 'military'])
def test_dynamic_provision_legacy_label_does_not_suppress_active_macro_ration(owner):
    """仅修改路由标签不能隐藏仍会到期的宏观领粮义务或改造旧领域账目。"""
    pack, binding_id = _dynamic_provision_fixture(owner=owner)
    economy = pack['scenarios'][0]['dynamic_world']['local_economy']
    economy['provision_bindings'][binding_id].update(policy_owner=owner, payer_inventory='inventory.cross_domain')
    assert any(code == 'DYNAMIC_PROVISION_OWNER_CONFLICT' for code, _path, _message in _dynamic_provision_issues(pack))


def test_dynamic_provision_rejects_wrong_legacy_inventory():
    """归属旧领域仍须指向其真实供养库存，不能把其他公库标成庄园来源。"""
    pack, binding_id = _dynamic_provision_fixture()
    economy = pack['scenarios'][0]['dynamic_world']['local_economy']
    economy['obligations']['obligation.cross_domain']['active'] = False
    economy['provision_bindings'][binding_id]['policy_owner'] = 'estate'
    assert any(code == 'DYNAMIC_PROVISION_LEGACY_OWNER' for code, _path, _message in _dynamic_provision_issues(pack))


def test_dynamic_provision_rejects_initial_garrison_commander_overlap():
    """初始驻军指挥官按实际在场、存活与可用条件进入军粮名单。"""
    pack, _binding_id = _dynamic_provision_fixture(owner='military')
    assert any(code == 'DYNAMIC_PROVISION_OWNER_CONFLICT' for code, _path, _message in _dynamic_provision_issues(pack))


def test_dynamic_provision_uses_snapshot_units_and_skips_absent_commander():
    """驻军以情景初态编成选择指挥官，原型旧人选和异地人物不触发重复供养。"""
    pack, _binding_id = _dynamic_provision_fixture(owner='military')
    pack['garrisons'][-1]['units'] = [{'commander_actor_id': 'actor.tutor'}]
    assert any(code == 'DYNAMIC_PROVISION_OWNER_CONFLICT' for code, _path, _message in _dynamic_provision_issues(pack))
    actor = next(row for row in pack['scenarios'][0]['initial']['actors'] if row['actor_id'] == 'actor.city_steward')
    actor['place_id'] = 'place.school'
    assert not any(code == 'DYNAMIC_PROVISION_OWNER_CONFLICT' for code, _path, _message in _dynamic_provision_issues(pack))


def test_dynamic_provision_binding_without_due_ration_does_not_double_consume():
    """责任区间内没有一次到期领粮时，不伪造重复消费来拒绝作者配置。"""
    pack, binding_id = _dynamic_provision_fixture()
    economy = pack['scenarios'][0]['dynamic_world']['local_economy']
    economy['provision_bindings'][binding_id].update(from_tick=5, through_tick=7)
    assert not any(code == 'DYNAMIC_PROVISION_OWNER_CONFLICT' for code, _path, _message in _dynamic_provision_issues(pack))
