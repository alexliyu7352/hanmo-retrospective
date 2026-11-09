"""身后清店的作者入口只声明真实双方与原送信路径，不预造委托、同意或死亡事实。"""


def check_private_shop_liquidations(pack, index):
    """旧情景不启用；新政策须原店主/执行人各自岗位的say，以及原收信人确为执行人。"""
    issues = []
    tables = {name: dict(index.get(name, {})) for name in ('actors', 'roles', 'dialogues', 'offers')}
    for name in tables:
        tables[name].update({row['id']: row for row in pack.get(name, [])
                             if isinstance(row, dict) and isinstance(row.get('id'), str)})
    caps = set(pack.get('required_capabilities', []))
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict) or 'private_shop_liquidations' not in dynamic:
            continue
        path = ('scenarios', number, 'dynamic_world', 'private_shop_liquidations')
        def bad(code, suffix, message):
            issues.append((code, path + tuple(suffix), message))
        required = {'private_shop_liquidations_v1', 'private_shops_v1', 'family_life_v1',
                    'task_death_notices_v1', 'task_message_handoffs_v1', 'native_dynamic_world',
                    'tasks.message', 'dialogue', 'multi_actor', 'task_shop_orders_v1'}
        if not required <= caps:
            bad('PRIVATE_SHOP_ESTATE_CAPABILITIES', (), '须显式清算、原私铺、生命与原实交死亡文书能力')
        if scenario.get('task_shop_order_profile') != 'private_shop_orders_v1':
            bad('PRIVATE_SHOP_ESTATE_ORDER_PAIR', (), '须沿真实店单原受领/预约与结算模块处理旧未交责任')
        shops = dynamic.get('private_shops')
        if (dynamic.get('profile') != 'local_society_v1' or not isinstance(shops, dict)
                or shops.get('enabled') is not True or type(shops.get('version')) is not int
                or shops['version'] != 1):
            bad('PRIVATE_SHOP_ESTATE_PAIR', (), '只能配对同一原生世界明确启用的1版私铺')
        life = scenario.get('family_life', {})
        notices = scenario.get('task_message_notices', {})
        if (not isinstance(life, dict) or life.get('profile') != 'verified_military_deaths_v1'
                or life.get('player_death') != 'await_explicit_continuation'
                or not isinstance(notices, dict) or notices.get('profile') != 'witnessed_death_letters_v1'
                or type(notices.get('version')) is not int or notices['version'] != 1
                or scenario.get('task_message_handoff_profile') not in
                    ('observed_letter_delivery_v1', 'observed_letter_delivery_and_report_v2')):
            bad('PRIVATE_SHOP_ESTATE_DEATH_SOURCE', (), '实际军中死亡、原文书绑定与本人收信必须同情景启用')
        config = dynamic['private_shop_liquidations']
        if (not isinstance(config, dict) or set(config) != {'version', 'policies'}
                or type(config.get('version')) is not int or config['version'] != 1
                or not isinstance(config.get('policies'), dict) or not config['policies']):
            bad('PRIVATE_SHOP_ESTATE_POLICIES', (), '仅接受按原ID索引的1版委托政策，不能作者写入运行授权')
            continue
        premises = shops.get('premises', {}) if isinstance(shops, dict) else {}
        enabled = scenario.get('enabled', {})
        fields = {'id', 'premises_id', 'proprietor_actor_id', 'executor_actor_id',
                  'proprietor_role_id', 'executor_role_id', 'proprietor_dialogue_id',
                  'proprietor_appointment_node_id', 'executor_dialogue_id', 'executor_acceptance_node_id'}
        for key, policy in config['policies'].items():
            suffix = ('policies', key)
            if (not isinstance(policy, dict) or set(policy) != fields or policy.get('id') != key
                    or any(not isinstance(value, str) or not value for value in policy.values())
                    or not isinstance(premises, dict) or policy.get('premises_id') not in premises):
                bad('PRIVATE_SHOP_ESTATE_POLICY_SOURCE', suffix, '政策字段、原ID与真实铺位须完整匹配')
                continue
            owner, executor = policy['proprietor_actor_id'], policy['executor_actor_id']
            if (owner == executor or any(actor not in tables['actors'] or actor not in enabled.get('actors', [])
                                         for actor in (owner, executor))):
                bad('PRIVATE_SHOP_ESTATE_PARTIES', suffix, '本人和执行人须为两个本情景原真实人物')
            place = premises[policy['premises_id']].get('place_id')
            for party, actor, node_field in [('proprietor', owner, 'proprietor_appointment_node_id'),
                                            ('executor', executor, 'executor_acceptance_node_id')]:
                role_id, dialogue_id = policy[party + '_role_id'], policy[party + '_dialogue_id']
                role, dialogue = tables['roles'].get(role_id, {}), tables['dialogues'].get(dialogue_id, {})
                nodes = [n for n in dialogue.get('nodes', []) if n.get('id') == policy[node_field]]
                if (role_id not in enabled.get('roles', []) or dialogue_id not in enabled.get('dialogues', [])
                        or role.get('initial_actor') != actor or role.get('place_id') != place
                        or role.get('dialogue_id') != dialogue_id or len(nodes) != 1
                        or nodes[0].get('kind') != 'say' or nodes[0].get('speaker') not in ('$provider', actor)):
                    bad('PRIVATE_SHOP_ESTATE_PERSONAL_SAY', suffix + (party + '_role_id',),
                        '必须是该本人原岗位、铺位和明确say，不能借他人同馆柜台或玩家发言')
            reachable = False
            bindings = notices.get('bindings', []) if isinstance(notices, dict) else []
            if not isinstance(bindings, list):
                bindings = []
            for binding in bindings:
                if not isinstance(binding, dict) or binding.get('decedent_actor_id') != owner:
                    continue
                offer_id = binding.get('offer_id'); offer = tables['offers'].get(offer_id, {})
                issuer = offer.get('issuer', {})
                if (offer_id in enabled.get('offers', []) and offer.get('template') == 'message'
                        and offer.get('goal', {}).get('receiver_actor') == executor
                        and issuer.get('kind') == 'actor' and issuer.get('id') not in (owner, executor)
                        and issuer.get('id') in enabled.get('actors', [])):
                    reachable = True
            if not reachable:
                bad('PRIVATE_SHOP_ESTATE_NOTICE_ROUTE', suffix,
                    '须已有真实亲历者可将本店主死亡文书送达原执行人的启用信件，不能默认远方知情')
    return issues
