"""跨记录检查资产归属和交互来源，不执行运行期规则。"""


def required_capabilities(pack):
    """从受信描述选择器推导必需能力，不维护第二份能力或字段白名单。"""
    from .capability_contract import required_capabilities as from_contract
    return from_contract(pack)


def role_owner(role):
    """解析岗位授权主体；私人事务保留实际人物身份。"""
    if role['organization_id'] is not None:
        return {'kind': 'organization', 'id': role['organization_id']}
    return {'kind': 'actor', 'id': role['initial_actor']}


def check_links(pack, index):
    """引用存在后执行关联检查，返回可定位到作者字段的问题。"""
    from .companion_links import check_companions
    from .drill_links import check_drills
    from .dynamic_world_links import check_dynamic_world
    from .regional_life_links import regional_dialogue_bindings
    from .personal_links import check_personal
    from .personal_simultaneous_outcomes import check_personal_simultaneous_outcomes
    from .world_links import check_world
    from .military_links import check_military
    from .challenge_links import check_challenges
    from .siege_integration_links import check_siege_integration
    from .named_membership_links import check_named_membership
    from .politics_links import check_politics, check_political_fiscal, check_political_career
    from .politics_governance_links import check_politics_governance
    from .political_takeover_links import check_political_takeover
    from .political_military_appointment_links import check_political_military_appointments
    from .political_field_appointment_links import check_political_field_appointments
    from .political_shop_licensing_links import check_political_shop_licensing
    from .political_shop_licensing_result_links import check_political_shop_licensing_receipts
    from .political_governance_continuation_links import check_political_governance_continuations
    from .political_governance_invitation_links import check_governance_invitation_replies
    from .political_head_handover_links import check_political_head_handovers
    from .political_successor_round_links import check_political_successor_rounds
    from .political_term_handover_links import check_political_term_handovers
    from .political_office_reception_links import check_political_office_receptions
    from .political_office_resignation_links import check_political_office_resignations
    from .political_subordinate_appointment_links import check_political_subordinate_appointments
    from .political_dismissal_source_links import check_political_dismissal_sources
    from .political_office_resignation_response_links import check_political_office_resignation_responses
    from .political_governance_agency_links import check_political_governance_agencies
    from .political_governance_agency_intent_links import check_political_governance_agency_intents
    from .political_succession_links import check_political_successions
    from .political_governance_intents_links import check_political_governance_intents
    from .political_shop_registry_links import check_political_shop_registry_successions
    from .political_occupation_fiscal_links import check_political_occupation_fiscal
    from .occupation_fiscal_need_links import check_occupation_fiscal_needs
    from .fiscal_taxpayer_continuity_links import check_fiscal_taxpayer_continuity
    from .estate_employment_links import check_estate_employment
    from .private_shop_daily_rent_links import check_private_shop_daily_rent
    from .private_shop_command_results_links import check_private_shop_command_results
    from .private_shop_links import check_private_shops
    from .private_shop_liquidation_links import check_private_shop_liquidations
    from .task_shop_order_links import check_task_shop_orders
    from .supply_policy_links import check_economic_supply_policies
    from .political_supply_renewal_links import check_political_supply_renewals
    from .political_shop_registry_intent_links import check_political_shop_registry_intents
    from .treaty_passage_links import check_treaty_passage
    from .diplomacy_links import check_diplomacy
    from .public_relief_links import check_public_relief
    from .task_offer_response_links import check_task_offer_responses
    from .task_message_handoff_links import check_task_message_handoffs
    from .task_static_asset_links import check_task_static_assets
    from .task_extension_links import check_task_extensions
    from .task_message_return_links import check_task_message_returns
    from .task_personal_message_links import check_task_personal_messages
    from .task_personal_report_links import check_task_personal_reports
    from .task_personal_acceptance_links import check_task_personal_acceptances
    from .task_personal_cancellation_links import check_task_personal_cancellations
    from .personal_appointment_links import check_personal_appointments
    from .contact_inquiry_links import check_contact_inquiries
    from .visit_reply_links import check_visit_message_replies,check_visit_reply_intents
    from .visit_appointment_links import check_visit_appointments
    from .visit_message_receipt_links import check_visit_message_receipts
    from .visit_appointment_attendance_links import check_visit_appointment_attendance
    from .born_personal_dialogue_links import check_born_personal_dialogues,born_personal_dialogue_bindings
    from .known_personal_dialogue_links import check_known_personal_dialogues,known_personal_dialogue_bindings
    from .task_shop_order_contact_links import check_task_shop_order_contacts
    from .personal_appointment_attendance_links import check_personal_appointment_attendance
    from .party_recruitment_links import check_party_recruitment_consents
    from .visit_message_links import check_visit_messages
    from .visit_reception_links import check_visit_reception
    from .task_message_notice_links import check_task_message_notices
    from .task_authored_continuation_links import check_task_authored_continuation
    from .task_static_public_links import check_task_static_public_execution
    from .task_offer_revisit_links import check_task_offer_revisits
    from .task_personal_advance_links import check_task_personal_advances
    from .party_invitation_links import check_party_invitations
    from .production_management_links import check_production_management
    from .household_provision_links import check_household_provision
    from .fiscal_settlement_links import check_fiscal_settlement
    from .fiscal_need_links import check_fiscal_need_policies
    from .cohort_wage_need_links import check_cohort_wage_need_policies
    issues = check_companions(pack,index) + check_personal(pack,index) + check_personal_simultaneous_outcomes(pack,index) + check_world(pack,index) + check_military(pack,index) + check_challenges(pack,index) + check_drills(pack,index) + check_dynamic_world(pack,index) + check_siege_integration(pack,index) + check_named_membership(pack,index) + check_politics(pack,index) + check_political_fiscal(pack,index) + check_production_management(pack,index) + check_household_provision(pack,index) + check_fiscal_settlement(pack,index) + check_fiscal_need_policies(pack,index) + check_cohort_wage_need_policies(pack,index) + check_political_career(pack,index) + check_diplomacy(pack,index) + check_politics_governance(pack,index) + check_political_takeover(pack,index) + check_political_occupation_fiscal(pack,index) + check_treaty_passage(pack,index) + check_political_military_appointments(pack,index) + check_political_field_appointments(pack,index) + check_political_shop_licensing(pack,index) + check_political_shop_licensing_receipts(pack,index) + check_political_governance_continuations(pack,index) + check_governance_invitation_replies(pack,index) + check_political_head_handovers(pack,index) + check_political_term_handovers(pack,index) + check_political_office_receptions(pack,index) + check_political_office_resignations(pack,index) + check_political_subordinate_appointments(pack,index) + check_political_dismissal_sources(pack,index) + check_political_office_resignation_responses(pack,index) + check_political_governance_agencies(pack,index) + check_political_governance_agency_intents(pack,index) + check_political_successor_rounds(pack,index) + check_political_successions(pack,index) + check_political_governance_intents(pack,index) + check_political_shop_registry_successions(pack,index) + check_occupation_fiscal_needs(pack,index) + check_fiscal_taxpayer_continuity(pack,index) + check_estate_employment(pack,index) + check_private_shop_daily_rent(pack,index) + check_private_shop_command_results(pack,index) + check_private_shops(pack,index) + check_private_shop_liquidations(pack,index) + check_task_shop_orders(pack,index) + check_economic_supply_policies(pack,index) + check_political_supply_renewals(pack,index) + check_political_shop_registry_intents(pack,index) + check_public_relief(pack,index) + check_task_offer_responses(pack,index) + check_task_message_handoffs(pack,index) + check_task_static_assets(pack,index) + check_task_extensions(pack,index) + check_personal_appointments(pack,index) + check_contact_inquiries(pack,index) + check_visit_message_replies(pack,index) + check_visit_reply_intents(pack,index) + check_visit_appointments(pack,index) + check_visit_message_receipts(pack,index) + check_visit_appointment_attendance(pack,index) + check_born_personal_dialogues(pack,index) + check_known_personal_dialogues(pack,index) + check_task_shop_order_contacts(pack,index) + check_personal_appointment_attendance(pack,index) + check_party_recruitment_consents(pack,index) + check_visit_messages(pack,index) + check_visit_reception(pack,index) + check_task_message_notices(pack,index) + check_task_authored_continuation(pack,index) + check_task_static_public_execution(pack,index) + check_task_offer_revisits(pack,index) + check_task_personal_advances(pack,index) + check_party_invitations(pack,index) + check_task_message_returns(pack,index) + check_task_personal_messages(pack,index) + check_task_personal_reports(pack,index) + check_task_personal_acceptances(pack,index) + check_task_personal_cancellations(pack,index)
    for number, actor in enumerate(pack['actors']):
        if actor.get('social'):
            policy = actor['social']
            if policy['target_actor_id'] == actor['id'] or index['items'][policy['food_item_id']]['kind'] != 'commodity':
                issues.append(('SOCIAL_TARGET', ('actors', number, 'social'), '社会目标须为另一真实人物，口粮须为实际商品'))
    for number, inv in enumerate(pack['inventories']):
        seen = set()
        weight = 0
        for g in inv['goods']:
            if g['item_id'] in seen:
                issues.append(('DUPLICATE_GOODS', ('inventories', number, 'goods'), 'One initial quantity per item is required'))
            seen.add(g['item_id'])
            weight += g['quantity'] * index['items'][g['item_id']]['weight']
        if weight > inv['capacity']:
            issues.append(('INITIAL_CAPACITY', ('inventories', number, 'capacity'), 'Initial cargo exceeds capacity'))
    for number, role in enumerate(pack['roles']):
        if 'dialogue_id' in role and index['dialogues'][role['dialogue_id']]['context_role'] != role['id']:
            issues.append(('ROLE_DIALOGUE_CONTEXT', ('roles', number, 'dialogue_id'), '岗位显式对白必须使用同一岗位上下文'))
        for kind, terms in role.get('activities', {}).items():
            if kind not in role['services'] or index['wallets'][terms['wallet_id']]['owner'] != role_owner(role):
                issues.append(('SERVICE_AUTHORITY', ('roles', number, 'activities', kind), 'Service payer must belong to its declared provider'))
    for number, shop in enumerate(pack['shops']):
        wallet = index['wallets'][shop['wallet_id']]
        inventory = index['inventories'][shop['inventory_id']]
        if wallet['owner'] != inventory['owner']:
            issues.append(('OWNER_MISMATCH', ('shops', number), 'Shop wallet and inventory must have the same owner'))
        for role_id in shop['representative_roles']:
            role = index['roles'][role_id]
            if 'trade' not in role['services']:
                issues.append(('ROLE_SERVICE', ('shops', number, 'representative_roles'), 'Representative cannot trade: ' + role_id))
            if role['place_id'] != shop['place_id']:
                issues.append(('SHOP_ROLE', ('shops', number, 'representative_roles'), 'Representative is assigned to another shop place'))
            if role_owner(role) != wallet['owner'] or role_owner(role) != inventory['owner']:
                issues.append(('OWNER_MISMATCH', ('shops', number), 'Representative cannot dispose of this owner\'s goods'))
    for number, offer in enumerate(pack['offers']):
        path = ('offers', number)
        for field, service in [('accept_role', 'accept_offer'), ('report_role', 'report_offer')]:
            role = index['roles'][offer[field]]
            if service not in role['services']:
                issues.append(('ROLE_SERVICE', path + (field,), 'Required service is missing: ' + service))
            if role_owner(role) != offer['issuer']:
                issues.append(('OFFER_AUTHORITY', path + (field,), 'Representative does not act for the issuer'))
        if index['wallets'][offer['funding_wallet']]['owner'] != offer['issuer']:
            issues.append(('OFFER_AUTHORITY', path + ('funding_wallet',), 'Offer cannot use another entity\'s wallet'))
        goal = offer['goal']
        expected = {'procurement': 'deliver_item', 'message': 'deliver_message', 'escort': 'escort'}[offer['template']]
        if goal['kind'] != expected:
            issues.append(('GOAL_TEMPLATE', path + ('goal',), 'Goal type does not match the supported template'))
        if goal['kind'] == 'deliver_message':
            if index['items'][goal['message_id']]['kind'] != 'message':
                issues.append(('GOAL_ITEM', path + ('goal', 'message_id'), 'Message delivery needs a message item'))
            recipients = [r for r in pack['roles'] if r['initial_actor'] == goal['receiver_actor'] and 'deliver_message' in r['services']]
            if not recipients:
                issues.append(('ROLE_SERVICE', path + ('goal',), 'Recipient has no receive-message role'))
        elif goal['kind'] == 'deliver_item':
            if index['items'][goal['item_id']]['kind'] != 'commodity':
                issues.append(('GOAL_ITEM', path + ('goal', 'item_id'), 'Procurement needs a commodity'))
            recipient_role = index['roles'][goal['receiver_role']]
            if 'report_offer' not in recipient_role['services']:
                issues.append(('ROLE_SERVICE', path + ('goal',), 'Goods recipient has no receipt authority'))
            if role_owner(recipient_role) != offer['issuer']:
                issues.append(('OFFER_AUTHORITY', path + ('goal', 'receiver_role'),
                               'Goods recipient does not represent the issuing owner'))
        elif goal['kind'] == 'escort':
            target = index['actors'][goal['actor_id']]
            if target['initial_place'] != index['roles'][offer['accept_role']]['place_id']:
                issues.append(('ESCORT_PRESENCE', path + ('goal', 'actor_id'), '护卫对象须在受托地点等待，不能临时召出远方人物'))
            if index.get('personal_objectives', {}).get(goal['objective_id'], {}).get('kind') != 'escort':
                issues.append(('ESCORT_OBJECTIVE', path + ('goal', 'objective_id'), '护卫冲突必须使用保护对象目标'))
            if not any(r['initial_actor'] == goal['actor_id'] and 'join_party' in r['services'] for r in pack['roles']):
                issues.append(('ESCORT_COMPANION', path + ('goal', 'actor_id'), '护卫对象须有实际同行约定'))
        for source in offer['discovery_sources']:
            if source['kind'] == 'prop':
                prop = index['props'][source['id']]
                if offer['id'] not in prop['offer_ids']:
                    issues.append(('DISCOVERY_LINK', path + ('discovery_sources',), 'Notice must visibly reference this offer'))
                if offer['issuer']['kind'] == 'organization' and prop['publisher'] != offer['issuer']['id']:
                    issues.append(('DISCOVERY_LINK', path + ('discovery_sources',), 'Notice publisher does not match the public issuer'))
    for number, prop in enumerate(pack['props']):
        for offer_id in prop['offer_ids']:
            if {'kind':'prop', 'id':prop['id']} not in index['offers'][offer_id]['discovery_sources']:
                issues.append(('DISCOVERY_LINK', ('props', number, 'offer_ids'), 'Offer does not acknowledge its notice source'))
    actor_dialogues, need_dialogues = regional_dialogue_bindings(pack)
    actor_dialogues = set(actor_dialogues) | born_personal_dialogue_bindings(pack) | known_personal_dialogue_bindings(pack)
    for number, dialogue in enumerate(pack['dialogues']):
        if not dialogue['context_role']:
            if dialogue['id'] not in actor_dialogues or any(node['kind'] not in ('say', 'choice', 'end') for node in dialogue['nodes']):
                issues.append(('REGIONAL_LIFE_NARRATIVE', ('dialogues', number), '空岗位对白只能由已声明的当面人物近况进入，且不可含业务命令'))
            continue
        # 原型依赖可保留另一年代对白定义，但不能凭此激活其岗位授权。
        if 'active_entity_ids' in index and dialogue['context_role'] not in index['roles']:
            continue
        role = index['roles'][dialogue['context_role']]
        for n, node in enumerate(dialogue['nodes']):
            if node['kind'] != 'action':
                continue
            if node['command'] == {'kind': 'accept_offer'}:
                valid = any(topic['role_id'] == role['id'] for topic in need_dialogues.get(dialogue['id'], []))
                reason = '' if valid and 'accept_offer' in role['services'] else '无静态offer_id的接单只允许真实共同需求绑定的同岗位对白'
            else:
                reason = check_command(node['command'], role, index)
            if reason:
                issues.append(('DIALOGUE_AUTHORITY', ('dialogues', number, 'nodes', n, 'command'), reason))
    for number, event in enumerate(pack['events']):
        dialogue = index['dialogues'][event['dialogue_id']]
        role = index['roles'].get(dialogue['context_role'])
        if role is None or role['place_id'] != event['place_id']:
            issues.append(('EVENT_CONTEXT', ('events', number, 'place_id'), 'Event dialogue is assigned to another location'))
    for number, actor in enumerate(pack['actors']):
        for field in ('dialogue_id', 'road_dialogue_id'):
            dialogue = index['dialogues'].get(actor.get(field))
            if dialogue and not dialogue['context_role']:
                issues.append(('REGIONAL_LIFE_NARRATIVE_ENTRY', ('actors', number, field), '空岗位近况对白必须经显式actor_topics绑定，不能借旧人物或道路对白绕过来源'))
    pairs = set()
    for number, relation in enumerate(pack['relations']):
        pair = (relation['from_actor'], relation['to_actor'])
        if pair[0] == pair[1]:
            issues.append(('SELF_RELATION', ('relations', number), 'An interpersonal relation cannot target itself'))
        if pair in pairs:
            issues.append(('DUPLICATE_RELATION', ('relations', number), 'Duplicate directed relationship'))
        pairs.add(pair)
    return issues


def check_command(command, role, index):
    """只校验定义中的授权关系；当前在场和资源由运行核心复核。"""
    kind = command['kind']
    if kind == 'decline_offer':
        from .task_offer_response_links import check_task_offer_response_command
        return check_task_offer_response_command(command, role, index)
    if kind in ('accept_offer', 'report_offer'):
        offer = index['offers'][command['offer_id']]
        field = 'accept_role' if kind == 'accept_offer' else 'report_role'
        if role['id'] != offer[field] or kind not in role['services']:
            return 'This dialogue role cannot ' + kind
    elif kind == 'deliver_message':
        goal = index['offers'][command['offer_id']]['goal']
        if goal['kind'] != 'deliver_message' or goal['receiver_actor'] != role['initial_actor'] or kind not in role['services']:
            return 'Only the defined message recipient may receive this message'
    elif kind == 'open_trade':
        shop = index['shops'][command['shop_id']]
        if role['id'] not in shop['representative_roles'] or 'trade' not in role['services']:
            return 'This role is not an authorized representative of the shop'
    elif kind == 'join_party':
        if 'join_party' not in role['services']: return '当前人物没有声明同行约定'
    elif kind == 'duel_start':
        if kind not in role['services'] or not role.get('sparring'):
            return '当前人物没有声明切磋约定'
    elif kind == 'learn_skill':
        if kind not in role['services'] or not any(lesson['skill_id'] == command['skill_id'] for lesson in role.get('lessons', [])):
            return '当前人物没有声明此功法的授课资格'
    elif kind == 'service':
        if command['activity'] not in role['services'] or command['activity'] not in role.get('activities', {}):
            return 'This role has not declared this service or its actual terms'
    elif kind == 'move_local':
        if command['place_id'] not in index['places'][role['place_id']]['passages']:
            return 'Local movement cannot teleport to a disconnected place'
    elif kind == 'discover_offer':
        offer = index['offers'][command['offer_id']]
        if {'kind':'actor','id':role['initial_actor']} not in offer['discovery_sources']:
            return 'This speaker is not a declared source of the offer'
    return ''
