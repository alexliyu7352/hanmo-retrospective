"""复用真实 Alpha 作者包生成两军测试定义；夹具不进入正式玩家内容。"""
from copy import deepcopy
import argparse
import json
from pathlib import Path

from tools.content.io import prepare_pack
from tools.content.military_links import SERVICES, TABLES

ROOT = Path(__file__).resolve().parents[1]


def military_pack() -> dict:
    """在原 Alpha 副本添加两套军事定义，不修改正式人物、内容包或游戏世界。"""
    pack = json.loads((ROOT / 'content/foundation/alpha_life.json').read_text())
    if 'military' not in pack['required_capabilities']: pack['required_capabilities'].append('military')
    for table in TABLES: pack[table] = []
    for actor in pack['actors']: actor.pop('military_plan', None)
    for role in pack['roles']: role['services'] = [service for service in role['services'] if service not in SERVICES] or ['information']
    name = 'name.military.fixture'; pack['texts'][name] = '军事作者测试'
    spear = next(item['id'] for item in pack['items'] if item.get('weapon', {}).get('category') == 'spear')
    for label, place in [('a', 'west.street'), ('b', 'east.entrance')]:
        prefix = 'military.' + label
        city = next(row['settlement_id'] for row in pack['places'] if row['id'] == place)
        organization = 'org.' + prefix; role_id = 'role.' + prefix
        for who in ('commander', 'provider'):
            actor = deepcopy(pack['actors'][0]); actor.update(id='actor.' + prefix + '.' + who, initial_place=place, initial_goods=[], combat={'weapon_id': '', 'learned': []}, name_key=name)
            for field in ('combat', 'social', 'road_conflict', 'travel_plan', 'road_dialogue_id'): actor.pop(field, None)
            pack['actors'].append(actor)
        pack['organizations'].append(dict(id=organization, name_key=name, kind='political', leader_actor='actor.' + prefix + '.provider'))
        pack['roles'].append(dict(id=role_id, name_key=name, organization_id=organization, initial_actor='actor.' + prefix + '.provider', place_id=place, services=['office_accept', 'office_resign', 'military_order', 'military_report']))
        food = 'item.' + prefix + '.food'
        pack['items'].append(dict(id=food, name_key=name, kind='commodity', base_price=2, weight=1 if label == 'a' else 2))
        owner = dict(kind='organization', id=organization)
        inventory = 'inventory.' + prefix; wallet = 'wallet.' + prefix
        pack['inventories'].append(dict(id=inventory, owner=owner, capacity=10000, goods=[dict(item_id=food, quantity=300), dict(item_id=spear, quantity=30)]))
        pack['wallets'].append(dict(id=wallet, owner=deepcopy(owner), initial_balance=200))
        goal = 'goal.' + prefix; board = 'field.' + prefix; troop = 'troop.' + prefix
        pack['military_objectives'].append(dict(id=goal, name_key=name, kind='repel', side=0, max_rounds=20, allow_withdrawal=True))
        pack['battlefields'].append(dict(id=board, name_key=name, objective_id=goal, cells=[dict(q=q, r=r, terrain='forest' if label == 'b' and q == r == 1 else 'plain') for q in range(4) for r in range(3)], spawns=[dict(side=0, position=[0, 1]), dict(side=1, position=[3, 1])], exits=[dict(side=0, position=[3, 0]), dict(side=1, position=[0, 0])], visual_tag='city'))
        pack['troop_types'].append(dict(id=troop, name_key=name, category='infantry' if label == 'a' else 'cavalry', movement=2 if label == 'a' else 4, minimum_range=1, maximum_range=1, attack=35, defense=10, accuracy=70, tactic_capacity=8, movement_costs=dict(plain=1, forest=2, hill=3, water=0, wall=0), tactics=[dict(id='tactic.rally', kind='rally', point_cost=3, power_bp=10000, accuracy_bonus=0, morale_effect=20, move_bonus=0, status_rounds=2)], battle_portrait_kind=pack['actors'][0]['portrait_kind']))
        camp = 'garrison.' + prefix; office = 'office.' + prefix; topology = 'topology.' + prefix
        pack['garrisons'].append(dict(id=camp, name_key=name, place_id=place, organization_id=organization, inventory_id=inventory, wallet_id=wallet, food_item_id=food, ration_per_person=1 if label == 'a' else 2, army_capacity=500, units=[dict(id='unit.' + prefix, troop_type_id=troop, commander_actor_id='actor.' + prefix + '.commander', initial_soldiers=8 if label == 'a' else 12, equipment=[dict(item_id=spear, quantity_per_person=1)])]))
        pack['offices'].append(dict(id=office, name_key=name, organization_id=organization, appointing_role_id=role_id, garrison_id=camp, command_scope='unit' if label == 'a' else 'army'))
        hold = 'stronghold.' + prefix
        pack['strongholds'].append(dict(id=hold, settlement_id=city, garrison_id=camp, topology_id=topology, public_inventory_ids=[inventory], public_wallet_ids=[wallet]))
        nodes = [dict(id=kind, kind=kind, max_hp=20 if kind in ('gate', 'wall', 'side') else 0, battlefield_id=board) for kind in ('approach', 'gate', 'wall', 'side', 'inner', 'supply')]
        edges = [dict(from_='approach', to='gate'), dict(from_='gate', to='inner', blocker_id='gate'), dict(from_='inner', to='wall'), dict(from_='approach', to='side'), dict(from_='side', to='supply', blocker_id='side'), dict(from_='supply', to='inner')]
        for edge in edges: edge['from'] = edge.pop('from_')
        rules = dict(hold_ticks=2, blockade_units=1, starvation_ticks=4, max_ticks=100, gate_damage=5, repair_amount=3, engine_bonus=2, engine_ticks=2, surrender_morale=20, wall_defense_bp=12000, engine_item_id=spear, engine_materials=[dict(item_id=food, quantity=2)], repair_materials=[dict(item_id=food, quantity=1)])
        pack['siege_topologies'].append(dict(id=topology, nodes=nodes, edges=edges, attacker_entry_id='approach', defender_entry_id='inner', inner_id='inner', supply_node_ids=['supply'], rules=rules))
        pack['military_orders'].append(dict(id='order.' + prefix, name_key=name, office_id=office, garrison_id=camp, issuer_role_id=role_id, report_role_id=role_id, target_stronghold_id=hold, deadline_ticks=200, objective=dict(kind='deliver_supply', item_id=food, quantity=20)))
        commander = next(actor for actor in pack['actors'] if actor['id'] == 'actor.' + prefix + '.commander')
        commander['military_plan'] = dict(office_id=office, order_id='order.' + prefix, unit_ids=['unit.' + prefix], food_quantity=30, money=10)
    private = dict(kind='actor', id=pack['actors'][0]['id'])
    pack['inventories'].append(dict(id='inventory.military.private', owner=private, capacity=1000, goods=[]))
    pack['wallets'].append(dict(id='wallet.military.private', owner=deepcopy(private), initial_balance=10))
    estate = dict(id='estate.military.private', name_key=name, place_id=pack['actors'][0]['initial_place'], owner_actor_id=private['id'], manager_actor_id=private['id'], resident_actor_ids=[private['id']], inventory_id='inventory.military.private', wallet_id='wallet.military.private', food_item_id='item.grain', ration_per_person=1, output_item_id='item.grain', output_per_worker=2, granary_capacity_per_level=100, palisade_defense_per_guard=2, initial_facilities=dict(farm=0, granary=0, palisade=0), initial_duties=[], construction=dict(max_level=3, build_ticks=2, money_cost=1, material_item_id='item.grain', material_cost=2, payee_wallet_id=pack['garrisons'][0]['wallet_id']))
    pack.setdefault('estates', []).append(estate)
    if 'estates' not in pack['required_capabilities']: pack['required_capabilities'].append('estates')
    return pack



def prepare_world_fixture(folder: Path) -> Path:
    """经正式作者管线生成跨城军需夹具；取消自主抢任，使玩家正常受任并获得明确接收授权。"""
    pack = military_pack()
    for actor in pack['actors']:
        actor.pop('military_plan', None)
    order = next(row for row in pack['military_orders'] if row['id'] == 'order.military.a')
    order['target_stronghold_id'] = 'stronghold.military.b'
    order['objective']['recipient_organization_id'] = 'org.military.b'
    folder.mkdir(parents=True, exist_ok=True)
    source = folder / 'military-world.json'
    source.write_text(json.dumps(pack, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    output = folder / 'military-world.hmcatalog.json'
    prepare_pack(source, output)
    return output


def prepare_lifecycle_fixture(folder: Path) -> Path:
    """正式作者变体支持第二军制的占城与真实守军；足量作者军粮避免把短链变为饥荒测试。"""
    pack = military_pack()
    for actor in pack['actors']:
        actor.pop('military_plan', None)
    order = next(row for row in pack['military_orders'] if row['id'] == 'order.military.a')
    order['target_stronghold_id'] = 'stronghold.military.b'
    order['objective'] = dict(kind='occupy')
    for label in ('a', 'b'):
        inventory = next(row for row in pack['inventories'] if row['id'] == 'inventory.military.' + label)
        next(row for row in inventory['goods'] if row['item_id'] == 'item.military.' + label + '.food')['quantity'] = 1200
        next(row for row in pack['garrisons'] if row['id'] == 'garrison.military.' + label)['army_capacity'] = 2000
    folder.mkdir(parents=True, exist_ok=True)
    source = folder / 'military-lifecycle.json'
    source.write_text(json.dumps(pack, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    output = folder / 'military-lifecycle.hmcatalog.json'
    prepare_pack(source, output)
    return output


def main() -> int:
    """供发布门禁单独准备测试目录，坏作者数据沿正式管线失败，不加载 pytest。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--lifecycle', action='store_true', help='准备独立第二军事占城/撤军全链作者变体')
    args = parser.parse_args()
    print(prepare_lifecycle_fixture(args.folder) if args.lifecycle else prepare_world_fixture(args.folder))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
