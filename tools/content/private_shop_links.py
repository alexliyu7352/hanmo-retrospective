"""私人首店准入核作者原租约/价格/说话人，实际技能、客户、通缉和财货仍由同World证明。"""

from .political_shop_author_profile import political_shop_author_pair

def check_private_shops(pack, index):
    """只读原作者，不默认无人通缉、默认同意、今天余额或运行生成的顾客来源。"""
    issues = []
    tables = {k: dict(index.get(k, {})) for k in ('actors', 'roles', 'places', 'shops', 'dialogues', 'wallets', 'items')}
    for kind in tables:
        tables[kind].update({r['id']: r for r in pack.get(kind, []) if isinstance(r, dict) and 'id' in r})
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        original_selectors = ('private_shop_operation_sources', 'private_shop_operation_capabilities')
        if ('private_shops' not in dynamic and 'growth_market_history' not in dynamic
                and not any(key in scenario for key in original_selectors)):
            continue
        path = ('scenarios', number, 'dynamic_world')
        def bad(code, field, message):
            issues.append((code, path + tuple(field), message))
        # /2、/3 必须有原完整源与能力；不能借旧日经营率或只抬持久版本。
        shop = dynamic.get('private_shops')
        shop_version = shop.get('version') if isinstance(shop, dict) else None
        exact_shop = type(shop_version) is int and shop_version in (2, 3)
        source_selection = scenario.get('private_shop_operation_sources')
        capability_selection = scenario.get('private_shop_operation_capabilities')
        selected_originals = any(key in scenario for key in original_selectors)
        if exact_shop:
            if (source_selection != {'profile': 'original_private_shop_operation_sources_v1'}
                    or capability_selection != {'profile': 'purpose_capability_v1'}
                    or scenario.get('world_time', {}).get('profile') != 'rational_world_complete_v2'
                    or not isinstance(scenario.get('actor_foundation'), dict)
                    or type(dynamic.get('household_purchases', {}).get('version')) is not int
                    or dynamic['household_purchases']['version'] not in ((2,) if shop_version == 2 else (2, 3))
                    or dynamic.get('household_retail_settlements', {}).get('version') != 1):
                bad('PRIVATE_SHOP_ORIGINAL_PAIR', ('private_shops',),
                    'Shop2/3须同Complete人物、原源/能力及真实家庭采购零售，不准旧日率分支')
        elif selected_originals:
            bad('PRIVATE_SHOP_ORIGINAL_PAIR', ('private_shops',),
                '原营业来源与能力只允许明确Shop2或3，不向旧1或未选目录扩张')
        history = dynamic.get('growth_market_history')
        selected = history.get('shop_ids') if isinstance(history, dict) else None
        if (not isinstance(history, dict) or set(history) != {'version', 'shop_ids'}
                or type(history.get('version')) is not int or history['version'] not in (1, 2)
                or not isinstance(selected, list) or not selected or any(not isinstance(x, str) or not x for x in selected) or len(set(selected)) != len(selected)):
            bad('SHOP_MARKET_HISTORY', ('growth_market_history',), '市场必须显式选择原1或2版商店历史，不能作者填余额或历史回执')
            continue
        # History2 is the actual Complete selector, never a relabelled legacy history.
        if history['version'] == 2 and (
                scenario.get('world_time', {}).get('profile') != 'rational_world_complete_v2'
                or not isinstance(scenario.get('actor_foundation'), dict)
                or scenario.get('selected_resource_operations') != {
                    'profile': 'original_selected_resource_operations_v1'}):
            bad('SHOP_MARKET_HISTORY2_PAIR', ('growth_market_history',),
                '2版市场须同目录 Complete、原人物和显式资源操作选择')
        caps = set(pack.get('required_capabilities', []))
        if dynamic.get('profile') != 'local_society_v1' or 'growth_market_history_v1' not in caps:
            bad('SHOP_MARKET_PROFILE', ('growth_market_history',), '保护市场须原生世界与独立历史能力')
        if not dynamic.get('regional_life'):
            bad('PRIVATE_SHOP_REGIONAL_LIFE', ('growth_market_history',), '实际原市场报价须已启用有限地域生活')
        enabled = scenario.get('enabled', {})
        market_inventories = set()
        for shop_id in selected:
            shop = tables['shops'].get(shop_id)
            if not shop or shop_id not in enabled.get('shops', []):
                bad('SHOP_ORIGINAL_MARKET', ('growth_market_history', 'shop_ids'), '所选市场必须是本情景启用的原商店')
                continue
            inventory = shop.get('inventory_id')
            if not inventory or inventory in market_inventories:
                bad('SHOP_MARKET_INVENTORY', ('growth_market_history', 'shop_ids'), '同一原库存不可被两个选中商店重挂')
            market_inventories.add(inventory)
        config = dynamic.get('private_shops')
        if config is None:
            continue
        if (not isinstance(config, dict) or set(config) != {'enabled', 'version', 'premises', 'staff', 'orders'}
                or config.get('enabled') is not True or type(config.get('version')) is not int or config['version'] not in (1, 2, 3)):
            bad('PRIVATE_SHOP_PROFILE', ('private_shops',), '首店须完整且显式1、2或3版目录，不能作者填运行状态')
            continue
        if not {'private_shops_v1', 'political_shop_licensing_v1', 'growth_market_history_v1', 'native_dynamic_world', 'economy', 'dialogue', 'multi_actor'} <= caps or not dynamic.get('political_shop_licensing'):
            bad('PRIVATE_SHOP_PAIR', ('private_shops',), '首店须完整配对法院许可、原市场历史与本人会面')
        if not political_shop_author_pair(dynamic.get('political_shop_licensing'), config):
            bad('PRIVATE_SHOP_LAW_PAIR', ('private_shops',), '旧Shop1/2须法律1，Shop3须明确法律2原目录')
        if config['version'] == 3 and history['version'] != 2:
            bad('PRIVATE_SHOP_HISTORY3_PAIR', ('growth_market_history',), 'Shop3只允许真实History2')
        premises = config.get('premises')
        if not isinstance(premises, dict) or not premises:
            bad('PRIVATE_SHOP_PREMISES', ('private_shops', 'premises'), '须有原人物私人铺位')
            continue
        rates = dynamic.get('local_economy', {}).get('rates', {})
        wallets = dict(tables['wallets']); wallets.update({r['id']: r for r in scenario.get('initial', {}).get('wallets', [])})
        def say(dialogue_id, node_id, actor):
            dialogue = tables['dialogues'].get(dialogue_id, {})
            found = [n for n in dialogue.get('nodes', []) if n.get('id') == node_id]
            return (dialogue_id in enabled.get('dialogues', []) and len(found) == 1
                    and found[0].get('kind') == 'say' and found[0].get('speaker') in ('$provider', actor))
        # 原对白以 context_role 绑定岗位；Role 无 dialogue_id，不能补造该作者字段。
        for key, premise in premises.items():
            p = ('private_shops', 'premises', key)
            if not isinstance(premise, dict) or premise.get('id') != key:
                bad('PRIVATE_SHOP_PREMISES_ID', p, '铺位表键须等于原ID'); continue
            actor = premise.get('lessor_actor_id'); place = premise.get('place_id')
            role = tables['roles'].get(premise.get('lessor_role_id'), {})
            shop = tables['shops'].get(premise.get('market_shop_id'), {})
            rate = rates.get(premise.get('rent_rate_profile'), {})
            if (actor not in tables['actors'] or actor not in enabled.get('actors', []) or place not in enabled.get('places', [])
                    or role.get('initial_actor') != actor or role.get('place_id') != place or role.get('organization_id') is not None
                    or tables['dialogues'].get(premise.get('agreement_dialogue_id'), {}).get('context_role') != premise.get('lessor_role_id')
                    or shop.get('place_id') != place or premise.get('market_shop_id') not in selected):
                bad('PRIVATE_SHOP_LESSOR_SOURCE', p, '出租人、原私人岗位、铺位和所选市场须在同一真实地点')
            if wallets.get(premise.get('lessor_wallet_id'), {}).get('owner') != {'kind': 'actor', 'id': actor}:
                bad('PRIVATE_SHOP_PRIVATE_WALLET', p, '租金只付原出租人明确私人钱包')
            if not say(premise.get('agreement_dialogue_id'), premise.get('agreement_node_id'), actor):
                bad('PRIVATE_SHOP_LEASE_SAY', p, '租约须原出租人明确say来源')
            if any(type(premise.get(k)) is not int or premise[k] != v for k, v in [('opening_fee', 900), ('initial_capital', 300), ('inventory_capacity', 200), ('daily_rent', 2)]) or rate.get('per_capita') is not False or rate.get('numerator') != 2 or rate.get('denominator') != 1:
                bad('PRIVATE_SHOP_ORIGINAL_COSTS', p, '首店沿原900开设、300本金、200库存、日租2的本人LE率')
        for label in ('staff', 'orders'):
            block = config.get(label)
            policies = block.get('policies') if isinstance(block, dict) else None
            if not isinstance(policies, dict):
                bad('PRIVATE_SHOP_POLICY_TABLE', ('private_shops', label), '必须显式有限政策表'); continue
            for key, policy in policies.items():
                p = ('private_shops', label, 'policies', key)
                if not isinstance(policy, dict) or policy.get('id') != key or policy.get('premises_id') not in premises:
                    bad('PRIVATE_SHOP_POLICY_SOURCE', p, '政策ID和原铺位引用必须相符'); continue
                if label == 'staff':
                    worker = policy.get('worker_actor_id'); rate = rates.get(policy.get('wage_rate_profile'), {})
                    if worker not in enabled.get('actors', []) or rate.get('per_capita') is not False or rate.get('numerator') != 3 or rate.get('denominator') != 2:
                        bad('PRIVATE_SHOP_STAFF_SOURCE', p, '店员须原真人与每半日3/2的钱工资率')
                    if not say(policy.get('worker_dialogue_id'), policy.get('worker_accept_node_id'), worker):
                        bad('PRIVATE_SHOP_STAFF_SAY', p, '受雇者须本人明确接受say，雇主点击不能代签')
                else:
                    client = policy.get('client_actor_id'); role = tables['roles'].get(policy.get('client_role_id'), {})
                    if client not in enabled.get('actors', []) or role.get('initial_actor') != client or role.get('organization_id') is not None or tables['dialogues'].get(policy.get('dialogue_id'), {}).get('context_role') != policy.get('client_role_id') or not say(policy.get('dialogue_id'), policy.get('node_id'), client):
                        bad('PRIVATE_SHOP_CLIENT_SAY', p, '客户订单须本人私人岗位的原say授权')
    return issues
