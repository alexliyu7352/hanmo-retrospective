"""首店正式作者关联的最小坏输入门，不用运行资金夹具冒正常Installed。"""
from copy import deepcopy
from tools.content.private_shop_links import check_private_shops
from tools.content.private_shop_schema import with_private_shop_schema


def test_private_shop_author():
    """原地点、租金、本人say和双pair逐项拒伪造。"""
    index = {'actors': {'landlord': {'id': 'landlord'}}, 'roles': {'lease': {'id': 'lease', 'initial_actor': 'landlord', 'organization_id': None, 'place_id': 'market', 'dialogue_id': 'agreement'}}, 'places': {'market': {'id': 'market'}}, 'shops': {'original': {'id': 'original', 'place_id': 'market', 'inventory_id': 'stock'}}, 'dialogues': {'agreement': {'id': 'agreement', 'nodes': [{'id': 'yes', 'kind': 'say', 'speaker': '$provider'}]}}, 'wallets': {'landlord_cash': {'id': 'landlord_cash', 'owner': {'kind': 'actor', 'id': 'landlord'}}}, 'items': {}}
    premise = {'id': 'premise', 'place_id': 'market', 'lessor_actor_id': 'landlord', 'lessor_wallet_id': 'landlord_cash', 'lessor_role_id': 'lease', 'agreement_dialogue_id': 'agreement', 'agreement_node_id': 'yes', 'market_shop_id': 'original', 'opening_fee': 900, 'initial_capital': 300, 'inventory_capacity': 200, 'daily_rent': 2, 'rent_rate_profile': 'rent'}
    dynamic = {'profile': 'local_society_v1', 'regional_life': {'enabled': True}, 'growth_market_history': {'version': 1, 'shop_ids': ['original']}, 'political_shop_licensing': {'version': 1, 'courts': {}}, 'local_economy': {'rates': {'rent': {'numerator': 2, 'denominator': 1, 'per_capita': False}}}, 'private_shops': {'enabled': True, 'version': 1, 'premises': {'premise': premise}, 'staff': {'policies': {}}, 'orders': {'policies': {}}}}
    pack = {'required_capabilities': ['private_shops_v1', 'political_shop_licensing_v1', 'growth_market_history_v1', 'native_dynamic_world', 'economy', 'dialogue', 'multi_actor'], 'scenarios': [{'enabled': {'actors': ['landlord'], 'places': ['market'], 'shops': ['original'], 'dialogues': ['agreement']}, 'dynamic_world': dynamic}]}
    assert not check_private_shops(pack, index)
    for field, value in [('daily_rent', 1), ('lessor_wallet_id', 'unknown'), ('agreement_node_id', 'unknown'), ('market_shop_id', 'unknown')]:
        bad = deepcopy(pack);bad['scenarios'][0]['dynamic_world']['private_shops']['premises']['premise'][field] = value
        assert check_private_shops(bad, index)
    bad = deepcopy(pack);del bad['scenarios'][0]['dynamic_world']['political_shop_licensing'];assert check_private_shops(bad, index)
    bad = deepcopy(pack);bad['scenarios'][0]['dynamic_world']['growth_market_history']['shop_ids'] *= 2;assert check_private_shops(bad, index)
    bad = deepcopy(index);bad['dialogues']['agreement']['nodes'][0]['speaker'] = '$player';assert check_private_shops(pack, bad)
    bad = deepcopy(pack);bad['scenarios'][0]['dynamic_world']['growth_market_history']['shop_ids'] = [{}];assert check_private_shops(bad,index)
    bad = deepcopy(pack);del bad['scenarios'][0]['dynamic_world']['regional_life'];del bad['scenarios'][0]['dynamic_world']['private_shops'];assert check_private_shops(bad,index)
    assert not check_private_shops({'scenarios': []}, {})
    schema = {'$defs': {'scenario': {'properties': {}}, 'dynamic_world': {'properties': {}}}}
    out = with_private_shop_schema(schema);assert 'private_shops' not in schema['$defs']['dynamic_world']['properties'];assert with_private_shop_schema(out) == out
    print('private shop author 13 checks passed')

if __name__ == '__main__':
    test_private_shop_author()
