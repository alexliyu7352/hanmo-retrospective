"""静态核验宏观情景的产权、人口责任、权限和货源；不运行世界或制造初态事实。"""
from collections import defaultdict
from .political_shop_author_profile import political_shop_author_pair
from .political_takeover_links import political_takeover_grant_declares_assets
from .political_governance_continuation_links import governance_continuation_grant_declares_assets
from .political_occupation_fiscal_links import occupation_fiscal_grant_declares_assets
from .political_supply_links import political_supply_grant_declares_assets
from .director_registry_links import director_policies, check_director_registry
from .population_military_links import check_population_military, future_ration_authorizations
from .regional_life_links import check_regional_life
from .visit_appointment_attendance_links import permits_private_reception_attendance


def _overlap(left, right):
    """作者责任区间按闭区间解释，首尾同tick仍不能重复分配同一人口。"""
    return left['from_tick'] <= right['through_tick'] and right['from_tick'] <= left['through_tick']



def _initial_provision_owners(initial, enabled, index):
    """沿旧域真实初态领粮条件选所有者；初态没有伤势、占用、军团、俘虏或训练行动。"""
    actors = {row['actor_id']: row for row in initial.get('actors', [])}
    owners = defaultdict(set)

    def present(actor_id, place_id):
        """新局人物的存活、可用与地点取情景覆盖，不从原型或组织隶属猜领粮资格。"""
        actor = actors.get(actor_id)
        return actor is not None and actor['alive'] and actor['available'] and actor['place_id'] == place_id

    estates = {row['id']: row for row in initial.get('estates', [])}
    for estate_id in sorted(enabled.get('estates', [])):
        definition, estate = index.get('estates', {}).get(estate_id), estates.get(estate_id)
        if definition is None or estate is None:
            continue  # 缺少定义/初态由既有引用与snapshot门拒绝，不猜一份默认责任。
        manager = estate['manager_actor_id']
        duties = {actor_id: 'idle' for actor_id in estate['resident_actor_ids']}
        duties.update((row['actor_id'], row['duty']) for row in estate['initial_duties'])
        duties[manager] = 'idle'  # 与native/旧Factory初始化的经理专职覆盖相同。
        mouths = {manager} if manager and present(manager, definition['place_id']) else set()
        mouths.update(actor_id for actor_id in estate['resident_actor_ids']
                      if duties[actor_id] in ('labor', 'guard') and present(actor_id, definition['place_id']))
        for actor_id in mouths:
            owners[actor_id].add(('estate', definition['inventory_id'], estate_id))

    garrisons = {row['id']: row for row in initial.get('garrisons', [])}
    for garrison_id in sorted(enabled.get('garrisons', [])):
        definition, garrison = index.get('garrisons', {}).get(garrison_id), garrisons.get(garrison_id)
        if definition is None or garrison is None:
            continue
        # 具名初态只包含在营指挥官；匿名soldier IDs不等同actor，后续训练教官由runtime口径负责。
        for unit in garrison['units']:
            actor_id = unit['commander_actor_id']
            if present(actor_id, definition['place_id']):
                owners[actor_id].add(('military', definition['inventory_id'], garrison_id))
    return owners


def _ration_due_in_binding(obligation, binding):
    """只核作者整数日程是否相交，不计算粮耗、产量或任何账目结果。"""
    if not obligation['active'] or obligation['kind'] != 'ration' or obligation['beneficiary'] != binding['subject']:
        return False
    period = obligation['period_ticks']
    if period < 1:
        return False  # 类型/正数边界仍由Schema和义务门负责。
    due = obligation['next_due_tick']
    if due < binding['from_tick']:
        due += ((binding['from_tick'] - due + period - 1) // period) * period
    return due <= binding['through_tick']


def check_dynamic_world(pack, index):
    """只检查显式macro情景；每条诊断保留原作者表、记录与字段路径。"""
    from .face_to_face_contacts_links import check_face_to_face_contacts
    from .character_injury_sources_links import check_character_injury_sources
    from .field_battle_parameters_links import check_field_battle_parameters
    from .political_relations_links import check_political_relations
    from .movement_group_member_handoffs_links import check_movement_group_member_handoffs
    from .appearance_choices import check_appearance_choices
    from .regional_outcome_links import check_regional_outcomes
    issues = check_movement_group_member_handoffs(pack, index) + check_political_relations(pack, index) + check_field_battle_parameters(pack, index) + check_character_injury_sources(pack, index) + check_face_to_face_contacts(pack, index) + check_population_military(pack, index) + check_regional_life(pack, index) + check_appearance_choices(pack, index) + check_regional_outcomes(pack, index)
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world')
        for actor_id in scenario.get('enabled', {}).get('actors', index.get('actors', {}).keys()):
            policy = index.get('actors', {}).get(actor_id, {}).get('road_conflict', {}).get('cargo_policy')
            if policy is None:
                continue
            path = ('scenarios', number, 'enabled', 'actors')
            if dynamic is None or dynamic.get('profile') != 'local_society_v1':
                issues.append(('CARGO_POLICY_PROFILE', path, '道路货物处置只能在明确启用的原生动态世界中使用：' + actor_id))
            if not {'personal_combat', 'road_encounters', 'native_dynamic_world'} <= set(pack.get('required_capabilities', [])):
                issues.append(('CARGO_POLICY_CAPABILITIES', path, '道路货物处置须显式声明个人战、真实道路相遇和原生动态世界能力'))
            if policy['task_template_id'] not in scenario.get('enabled', {}).get('task_templates', []):
                issues.append(('CARGO_POLICY_ENABLED_TEMPLATE', path, '道路货物政策必须引用本情景启用的采购模板'))
        if dynamic is None:
            continue
        root = ('scenarios', number, 'dynamic_world')

        def issue(code, path, message):
            """将本情景问题投射到正式编译器既有字段诊断，不补齐缺失作者决策。"""
            issues.append((code, root + tuple(path), message))

        if dynamic['profile'] != 'local_society_v1' or dynamic['founder_policy'] not in ('fixed_actor', 'fixed_actor_appearance_v1', 'fixed_actor_appearance_composition_v1', 'selected_existing_actor_v1', 'selected_existing_actor_roster_v1', 'created_character_v1'):
            issue('DYNAMIC_PROFILE', (), '首批宏观情景必须明确使用固定作者人物的原生profile')
        # 两个显式选人版本的覆盖、成年与外貌门由check_appearance_choices统一核验，旧单人合同保持。
        if dynamic['founder_policy'] not in ('selected_existing_actor_v1', 'selected_existing_actor_roster_v1', 'created_character_v1') and scenario.get('playable_actor_ids') != [scenario['player_actor']]:
            issue('DYNAMIC_FOUNDER', ('founder_policy',), '固定人物策略只允许剧本指定的一名player_actor')
        initial = scenario.get('initial', {})
        enabled = scenario.get('enabled', {})
        actors = {row['actor_id']: row for row in initial.get('actors', [])}
        wallets = {row['id']: row for row in initial.get('wallets', [])}
        inventories = {row['id']: row for row in initial.get('inventories', [])}
        roles = {row['role_id']: row for row in initial.get('roles', [])}
        economy, organization = dynamic['local_economy'], dynamic['organization']
        from .household_provision_links import household_ration_authorizations
        private_rations = household_ration_authorizations(scenario, index)
        future_rations = future_ration_authorizations(dynamic, index)
        cohorts = economy['cohorts']
        organizations = organization['organizations']
        for domain_name in ('local_economy', 'organization'):
            for table, rows in dynamic[domain_name].items():
                if domain_name == 'local_economy' and table in ('production_management', 'household_provisioning'):
                    continue
                if not isinstance(rows, dict):
                    continue
                for key, row in rows.items():
                    id_field = 'source_key' if table == 'supply_sources' else 'unit_id' if table == 'production_bindings' else 'id'
                    if row.get(id_field) != key:
                        issue('DYNAMIC_RECORD_ID', (domain_name, table, key, id_field), '字典身份键必须等于记录中的唯一ID')
        if set(economy['named_actors_excluded']) != set(actors):
            issue('DYNAMIC_POPULATION_EXCLUSION', ('local_economy', 'named_actors_excluded'), '具名人物必须逐项排除于匿名cohort人口计数')
        for key, household in economy['households'].items():
            owner = {'kind': 'household', 'id': key}
            if wallets.get(household['wallet_id'], {}).get('owner') != owner or inventories.get(household['inventory_id'], {}).get('owner') != owner:
                issue('DYNAMIC_HOUSEHOLD_OWNER', ('local_economy', 'households', key), '家庭的两个实际容器须归同一家庭且在本情景显式初始化')
        for key, cohort in cohorts.items():
            household = economy['households'].get(cohort['household_group_id'])
            if household is None or cohort['private_wallet_id'] != household['wallet_id'] or cohort['private_inventory_id'] != household['inventory_id']:
                issue('DYNAMIC_COHORT_ASSETS', ('local_economy', 'cohorts', key), '群体只能引用其真实家庭容器，拆分不能复制私产')
            if cohort['labor_capacity'] > cohort['headcount'] or cohort['site_id'] not in enabled.get('places', []):
                issue('DYNAMIC_COHORT_CAPACITY', ('local_economy', 'cohorts', key), '有效劳力不能超过人数，所在地必须启用')
            if cohort['status'] == 'serving' and not (dynamic.get('population_military', {}).get('enabled') and any(source['serving_cohort_id'] == key for source in dynamic['population_military']['sources'].values())):
                issue('DYNAMIC_UNSUPPORTED_SERVING', ('local_economy', 'cohorts', key, 'status'), '首批未接匿名军队adapter，不能直接创建服役群体')
        allocations = defaultdict(list)
        for key, allocation in economy['labor_allocations'].items():
            if allocation['cohort_id'] not in cohorts or allocation['from_tick'] > allocation['through_tick'] or not allocation['commitment_id']:
                issue('DYNAMIC_LABOR_SOURCE', ('local_economy', 'labor_allocations', key), '劳动分配须引用有效群体及合法责任区间')
                continue
            allocations[allocation['cohort_id']].append(allocation)
        for cohort_id, rows in allocations.items():
            boundaries = {row['from_tick'] for row in rows} | {row['through_tick'] for row in rows}
            for tick in boundaries:
                if sum(row['workers'] for row in rows if row['from_tick'] <= tick <= row['through_tick']) > cohorts[cohort_id]['labor_capacity']:
                    issue('DYNAMIC_LABOR_DUPLICATE', ('local_economy', 'labor_allocations'), '同一tick的劳力承诺超过群体可用劳力：' + cohort_id)
                    break
        for key, unit in economy['production_units'].items():
            recipe = economy['recipes'].get(unit['recipe_id'])
            if recipe is None or len(unit['input_remainders']) != len(recipe['inputs']) or len(unit['output_remainders']) != len(recipe['outputs']):
                issue('DYNAMIC_RECIPE', ('local_economy', 'production_units', key), '生产配方及有理余数维度必须完整对应')
            if inventories.get(unit['output_inventory_id'], {}).get('owner') != unit['owner']:
                issue('DYNAMIC_PRODUCTION_OWNER', ('local_economy', 'production_units', key, 'output_inventory_id'), '生产输出必须进入作者声明所有者的真实库存')
            for allocation_id in unit['labor_allocation_ids']:
                allocation = economy['labor_allocations'].get(allocation_id)
                if allocation is None or allocation['activity_or_production_id'] != key:
                    issue('DYNAMIC_PRODUCTION_LABOR', ('local_economy', 'production_units', key, 'labor_allocation_ids'), '生产劳力必须指向本单位，不能重复借另一活动的分配')
            if economy['production_bindings'].get(key, {}).get('policy_owner') != 'macro':
                issue('DYNAMIC_PRODUCTION_BINDING', ('local_economy', 'production_bindings', key), '本批生产必须明确macro单一执行者，不能同时由庄园/军队重复结算')
        provisions = defaultdict(list)
        initial_owners = _initial_provision_owners(initial, enabled, index)
        for key, binding in economy['provision_bindings'].items():
            subject = (binding['subject']['kind'], binding['subject']['id'])
            if binding['from_tick'] > binding['through_tick']:
                issue('DYNAMIC_PROVISION_RANGE', ('local_economy', 'provision_bindings', key), '供养责任区间必须非空')
            for before in provisions[subject]:
                if _overlap(before, binding):
                    issue('DYNAMIC_PROVISION_DUPLICATE', ('local_economy', 'provision_bindings', key), '同一主体同一tick只能由一个领域承担供养')
            provisions[subject].append(binding)
            obligation = economy['obligations'].get(binding['obligation_id'])
            if binding['policy_owner'] == 'macro' and (obligation is None or obligation['kind'] != 'ration' or obligation['beneficiary'] != binding['subject'] or obligation['source_wallet_or_inventory'] != binding['payer_inventory']):
                issue('DYNAMIC_PROVISION_OBLIGATION', ('local_economy', 'provision_bindings', key), '宏观供养绑定必须指向同一受益人、粮库及口粮义务')
            legacy = initial_owners.get(binding['subject']['id'], set()) if binding['subject']['kind'] == 'actor' else set()
            due = any(_ration_due_in_binding(row, binding) for row in economy['obligations'].values())
            if legacy and due:
                descriptions = '、'.join(owner + ':' + source for owner, _inventory, source in sorted(legacy))
                issue('DYNAMIC_PROVISION_OWNER_CONFLICT', ('local_economy', 'provision_bindings', key, 'policy_owner'),
                      '同一具名人物已有初态供养者（' + descriptions + '），不能同时保留此区间内的宏观领粮义务')
            population_source = dynamic.get('population_military', {}).get('sources', {}).get(binding.get('military_source_id', ''), {})
            adapter_binding = (binding['policy_owner'] == 'military' and binding['subject']['kind'] == 'cohort'
                               and binding['subject']['id'] in [population_source.get(field) for field in ('serving_cohort_id', 'wounded_cohort_id', 'captive_cohort_id')]
                               and not binding['payer_inventory'] and not binding['obligation_id'])
            if binding['policy_owner'] != 'macro' and not adapter_binding:
                selected = {row for row in legacy if row[0] == binding['policy_owner'] and row[1] == binding['payer_inventory']}
                if len(selected) != 1 or selected != legacy:
                    issue('DYNAMIC_PROVISION_LEGACY_OWNER', ('local_economy', 'provision_bindings', key, 'payer_inventory'),
                          '旧领域绑定须唯一匹配实际初态领粮者与其库存；仅修改标签不能转移供养责任')
        for key, obligation in economy['obligations'].items():
            issuer = organization['institutional_issuers'].get(obligation['authority_ref'])
            source = obligation['source_wallet_or_inventory']
            source_owner = (inventories if obligation['kind'] == 'ration' else wallets).get(source, {}).get('owner')
            future_owner = (key, source, obligation['principal']['id']) in future_rations
            if key not in private_rations and (issuer is None or obligation['principal'] != {'kind': 'organization', 'id': issuer['organization_id']} or key not in issuer['obligation_ids'] or source not in issuer['source_container_ids'] or source_owner != obligation['principal'] and not future_owner):
                issue('DYNAMIC_OBLIGATION_AUTHORITY', ('local_economy', 'obligations', key, 'authority_ref'), '机构支出须有匹配组织、义务、实际容器的登记授权')
            if obligation['rate_profile'] not in economy['rates']:
                issue('DYNAMIC_RATE', ('local_economy', 'obligations', key, 'rate_profile'), '义务金额/口粮率必须有明确整数配置')
            beneficiary = obligation['beneficiary']
            if obligation['kind'] == 'ration':
                if obligation['destination']:
                    issue('DYNAMIC_RATION_DESTINATION', ('local_economy', 'obligations', key, 'destination'), '口粮从真实源库存消费，不再制造一份收粮库存')
            else:
                target = wallets.get(obligation['destination'])
                expected = beneficiary
                if beneficiary['kind'] == 'cohort':
                    cohort = cohorts.get(beneficiary['id'])
                    expected = {'kind': 'household', 'id': cohort['household_group_id']} if cohort else None
                if target is None or target['owner'] != expected:
                    issue('DYNAMIC_BENEFICIARY', ('local_economy', 'obligations', key, 'destination'), '工资必须支付给真实人物或群体家庭的唯一钱包')
        for key, org in organizations.items():
            if key not in enabled.get('organizations', []) or org['headquarters_site'] not in enabled.get('places', []):
                issue('DYNAMIC_ORGANIZATION', ('organization', 'organizations', key), '组织与总部必须是此剧本启用的实体')
            for parent_field in ('administrative_parent', 'branch_parent'):
                visited, current = {key}, org[parent_field]
                while current is not None:
                    if current in visited or current not in organizations:
                        issue('DYNAMIC_ORGANIZATION_CYCLE', ('organization', 'organizations', key, parent_field), '组织从属关系不得循环或引用不存在的组织')
                        break
                    visited.add(current)
                    current = organizations[current][parent_field]
        for key, office in organization['offices'].items():
            if office['organization_id'] not in organizations or office['holder_actor_id'] is not None and office['holder_actor_id'] not in actors:
                issue('DYNAMIC_OFFICE_HOLDER', ('organization', 'offices', key), '职务必须属于本情景组织，由真实活动人物持有')
            for grant_id in office['grant_ids']:
                if organization['grants'].get(grant_id, {}).get('office_id') != key:
                    issue('DYNAMIC_OFFICE_GRANT', ('organization', 'offices', key, 'grant_ids'), '职务只能取得明确指向本职务的授权')
        for key, grant in organization['grants'].items():
            office = organization['offices'].get(grant['office_id'])
            expected = {'kind': 'organization', 'id': office['organization_id']} if office else None
            if office is None or (not political_takeover_grant_declares_assets(scenario, index, grant) and not occupation_fiscal_grant_declares_assets(scenario,index,grant) and not political_supply_grant_declares_assets(scenario,index,grant) and not governance_continuation_grant_declares_assets(scenario,index,grant) and (any(wallets.get(value, {}).get('owner') != expected for value in grant['wallet_ids']) or any(inventories.get(value, {}).get('owner') != expected for value in grant['inventory_ids']))):
                issue('DYNAMIC_GRANT_ASSETS', ('organization', 'grants', key), '职务授权只能列入本组织真实钱包和库存')
            allowed = {'recruit_population', 'approve_org_activity', 'claim_need'}
            if economy.get('enabled') is True:
                allowed.add('inspect_local_economy')
                management = economy.get('production_management')
                if isinstance(management, dict) and type(management.get('version')) is int and management['version'] == 1:
                    allowed.add('configure_local_production')
            if dynamic.get('population_military', {}).get('enabled'):
                allowed.update(('mobilize_reserve', 'release_reserve'))
            if organization.get('named_membership_enabled'):
                allowed.update(('organization_offer_membership', 'contract_executor'))
            if dynamic.get('politics', {}).get('enabled'):
                allowed.update(('political_appoint', 'political_dismiss'))
            diplomacy = dynamic.get('diplomacy', {})
            if diplomacy.get('enabled') is True and type(diplomacy.get('version')) is int and diplomacy['version'] in (1, 2):
                allowed.update({'diplomatic_propose_passage', 'diplomatic_commission_envoy', 'diplomatic_receive_passage', 'diplomatic_reply_passage'})
                if diplomacy['version'] == 2:
                    allowed.add('diplomatic_receive_envoy_report')
            takeover = dynamic.get('political_takeover', {})
            if takeover.get('enabled') is True and type(takeover.get('version')) is int and takeover['version'] == 1:
                allowed.add('political_assume_public_responsibilities')
            if dynamic.get('political_occupation_fiscal', {}).get('enabled'):
                allowed.add('political_set_occupation_tax')
            supply = dynamic.get('economic_supply_policies', {})
            if type(supply.get('version')) is int and supply['version'] == 1:
                allowed.add('political_issue_supply_policy')
            continued = dynamic.get('political_governance_continuations', {})
            if type(continued.get('version')) is int and continued['version'] == 1:
                allowed.add('political_continue_governance')
            invitation_replies = dynamic.get('political_governance_invitation_replies', {})
            if type(invitation_replies.get('version')) is int and invitation_replies['version'] == 1:
                allowed.add('political_withdraw_governance_invitation')
            successions = dynamic.get('political_head_handovers', {})
            if type(successions.get('version')) is int and successions['version'] == 1:
                allowed.update({'political_offer_head_handover', 'political_withdraw_head_handover'})
            successions = dynamic.get('political_term_handovers', {})
            if type(successions.get('version')) is int and successions['version'] == 1:
                allowed.update({'political_offer_term_handover', 'political_withdraw_term_handover'})
            successions = dynamic.get('political_office_receptions', {})
            if type(successions.get('version')) is int and successions['version'] == 1:
                allowed.update({'political_assume_office_reception'})
            resignations = dynamic.get('political_office_resignations', {})
            if type(resignations.get('version')) is int and resignations['version'] in (1,2):
                allowed.update({'political_answer_office_resignation'})
            subordinate = dynamic.get('political_subordinate_appointments', {})
            if type(subordinate.get('version')) is int and subordinate['version'] == 1:
                allowed.update({'political_offer_subordinate_appointment','political_answer_subordinate_appointment','political_withdraw_subordinate_appointment','political_arrive_subordinate_appointment'})
            agencies = dynamic.get('political_governance_agencies', {})
            if type(agencies.get('version')) is int and agencies['version'] in (1, 2):
                allowed.update({'political_offer_governance_agency', 'political_revoke_governance_agency'})
            successions = dynamic.get('political_successor_rounds', {})
            if type(successions.get('version')) is int and successions['version'] == 1:
                allowed.update({'political_nominate_next_successor', 'political_withdraw_next_successor'})
            successions = dynamic.get('political_successions', {})
            if type(successions.get('version')) is int and successions['version'] == 1:
                allowed.update({'political_nominate_successor', 'political_withdraw_successor'})
            shop_law = dynamic.get('political_shop_licensing', {})
            registry = dynamic.get('political_shop_registry_successions', {})
            if registry.get('enabled') is True and type(registry.get('version')) is int and registry['version'] == 1 and shop_law.get('version') == 1 and takeover.get('enabled') is True and takeover.get('version') == 1:
                allowed.add('political_accept_shop_registry')
            private_shops = dynamic.get('private_shops', {})
            if political_shop_author_pair(shop_law, private_shops):
                allowed.update(['political_issue_shop_permit', 'political_record_shop_clearance',
                                'political_register_wanted', 'political_release_wanted',
                                'political_revoke_shop_permit'])
            if dynamic.get('treaty_passage', {}).get('enabled'):
                allowed.add('diplomatic_ratify_passage')
            if dynamic.get('political_military_appointments', {}).get('enabled'):
                allowed.add('political_assign_military_officer')
            if dynamic.get('political_field_appointments', {}).get('enabled'):
                allowed.update(['political_assign_field_unit','political_assign_field_army'])
            if dynamic.get('political_fiscal', {}).get('enabled'):
                allowed.add('political_set_tax')
            if dynamic.get('politics_governance', {}).get('enabled') is True and type(dynamic.get('public_relief', {}).get('version')) is int and dynamic['public_relief']['version'] == 1:
                allowed.add('political_commission_governance')
            if set(grant['allowed_command_kinds']) - allowed:
                issue('DYNAMIC_GRANT_COMMAND', ('organization', 'grants', key, 'allowed_command_kinds'), '作者不能借任意命令名扩大首批机构权限')
            if grant['valid_until'] is not None and grant['valid_until'] < grant['valid_from']:
                issue('DYNAMIC_GRANT_RANGE', ('organization', 'grants', key), '授权期限不能早于生效时刻')
        for key, issuer in organization['institutional_issuers'].items():
            expected = {'kind': 'organization', 'id': issuer['organization_id']}
            for container in issuer['source_container_ids']:
                future_only = bool(issuer['obligation_ids']) and all(
                    (identifier, container, issuer['organization_id']) in future_rations
                    for identifier in issuer['obligation_ids'])
                if wallets.get(container, inventories.get(container, {})).get('owner') != expected and not future_only:
                    issue('DYNAMIC_ISSUER_ASSETS', ('organization', 'institutional_issuers', key, 'source_container_ids'), '机构义务签发者不能支配不存在或他人的容器')
        for key, duty in organization['duties'].items():
            role = index['roles'].get(duty['role_id']) if duty['role_id'] else None
            if duty['actor_id'] not in actors or duty['organization_id'] not in organizations or duty['policy_id'] not in organization['member_policies']:
                issue('DYNAMIC_DUTY_SOURCE', ('organization', 'duties', key), '职责须有实际人物、组织及成员政策')
            if role and (roles.get(duty['role_id'], {}).get('actor_id') != duty['actor_id'] or role['organization_id'] != duty['organization_id']):
                issue('DYNAMIC_DUTY_ROLE', ('organization', 'duties', key, 'role_id'), '职责不能借用另一人物或组织的现任岗位')
        for key, terms in organization['activity_terms'].items():
            if terms['kind'] == 'population_training' and (not terms['consenting_cohort_ids'] or terms['training_capacity'] < 1):
                issue('DYNAMIC_RECRUIT_CONSENT', ('organization', 'activity_terms', key), '训练须声明已同意群体及真实容量')
            if any(cohort_id not in cohorts for cohort_id in terms['consenting_cohort_ids']):
                issue('DYNAMIC_RECRUIT_COHORT', ('organization', 'activity_terms', key, 'consenting_cohort_ids'), '训练同意范围只能引用本情景人口')
        for key, policy in dynamic['affairs']['need_policies'].items():
            path = ('affairs', 'need_policies', key)
            source = economy['supply_sources'].get(policy['source_key'])
            if source is None or any(source[field] != policy[field] for field in ('organization_id', 'site_id', 'inventory_id', 'item_id')):
                issue('DYNAMIC_NEED_SOURCE', path, '需求必须观察同一个有名称的真实库存来源')
            template = index.get('task_templates', {}).get(policy['template_id'])
            if template is None or template['template'] != 'procurement' or policy['template_id'] not in enabled.get('task_templates', []):
                issue('DYNAMIC_NEED_TEMPLATE', path + ('template_id',), '首批共享需求须引用已启用的采购模板')
            for field in ('funding_wallet_id', 'reward_wallet_id'):
                if wallets.get(policy[field], {}).get('owner') != {'kind': 'organization', 'id': policy['organization_id']}:
                    issue('DYNAMIC_NEED_FUNDING', path + (field,), '采购预算和报酬须出自需求组织的真实钱包')
            for field, service in (('accept_role', 'accept_offer'), ('report_role', 'report_offer')):
                role = index['roles'].get(policy[field], {})
                if role.get('organization_id') != policy['organization_id'] or service not in role.get('services', []) or policy[field] not in roles:
                    issue('DYNAMIC_NEED_ROLE', path + (field,), '接单/验收须由此组织当前合法服务岗位办理')
            if not _reachable_supply(policy, enabled, inventories, economy, index):
                issue('DYNAMIC_NEED_NO_SOURCE', path + ('item_id',), '此情景没有可经已启用道路到达的实际可售货源')
        for code, path, message in check_director_registry(scenario):
            issue(code, path, message)
        for key, policy in dynamic['affairs'].get('obligation_policies', {}).items():
            path = ('affairs', 'obligation_policies', key)
            source = economy['obligations'].get(policy['obligation_id'])
            if (dynamic['affairs'].get('obligation_needs_version') != 1 or policy['id'] != key or
                    source is None or source['kind'] not in ('wage', 'contract_payment') or
                    source['principal'] != {'kind': 'organization', 'id': policy['organization_id']} or
                    source['beneficiary']['kind'] != 'actor' or source['beneficiary']['id'] not in actors):
                issue('DYNAMIC_OBLIGATION_NEED_SOURCE', path, '欠款需求须引用同组织对真人的原工资/合同欠据，不是第二份钱包或采购需求')
        registered = 'registry_version' in dynamic['director']
        for actor_id, policy in director_policies(dynamic).items():
            policy_path = ('director', 'policies' if registered else 'subjects', actor_id)
            private_only = permits_private_reception_attendance(scenario,policy) or (scenario.get('personal_appointment_profile') == 'witnessed_personal_meetings_v1' and 'personal_appointment_attendance' in policy['templates'] and set(policy['templates']) <= {'private_livelihood','maintain_commitment','await_adjustment','personal_appointment_attendance'}) or (scenario.get('visit_reception_profile') == 'authored_local_reception_v1' and 'visit_reception' in policy['templates'] and set(policy['templates']) <= {'private_livelihood','maintain_commitment','await_adjustment','visit_reception'}) or policy['templates'] == ['private_livelihood'] or (dynamic.get('politics', {}).get('enabled') and set(policy['templates']) <= {'private_livelihood', 'political_career'} and 'political_career' in policy['templates'])
            observation_only = registered and set(policy['templates']) <= {'maintain_commitment', 'await_adjustment'}
            organization_id = policy.get('organization_id')
            if (not registered and actor_id not in actors or organization_id is not None and organization_id not in organizations
                    or organization_id is None and not private_only and not observation_only):
                issue('DYNAMIC_DIRECTOR_SUBJECT', policy_path, '有限决策须绑定本剧本真实人物；只有纯private_livelihood模板可不指定组织')
            for source_key in policy.get('report_source_keys', []):
                source = economy['supply_sources'].get(source_key)
                if source is None or not source.get('public_report', False):
                    issue('DYNAMIC_REPORT_SOURCE', policy_path + ('report_source_keys',), '例行异地报告只能订阅明确公开的真实来源，不能订阅私人库存')
    return issues


def _reachable_supply(policy, enabled, inventories, economy, index):
    """有向作者道路只检查连通，不替运行旅行计算距离、价格或风险结果。"""
    target = index['places'][policy['site_id']]['settlement_id']
    edges = defaultdict(set)
    for route_id in enabled.get('routes', []):
        route = index['routes'][route_id]
        edges[route['origin']].add(route['destination'])
    reached, queue = {target}, [target]
    while queue:
        for adjacent in edges[queue.pop()] - reached:
            reached.add(adjacent)
            queue.append(adjacent)
    for shop_id in enabled.get('shops', []):
        shop = index['shops'][shop_id]
        supplier = index['places'][shop['place_id']]['settlement_id']
        if supplier not in reached:
            continue
        returned, pending = {supplier}, [supplier]
        while pending:
            for adjacent in edges[pending.pop()] - returned:
                returned.add(adjacent)
                pending.append(adjacent)
        if target not in returned:
            continue
        inv = inventories.get(shop['inventory_id'], {})
        available = any(good['item_id'] == policy['item_id'] and good['quantity'] > 0 for good in inv.get('goods', []))
        produced = any(unit['output_inventory_id'] == shop['inventory_id'] and any(output['item_id'] == policy['item_id'] and output['numerator'] > 0 for output in economy['recipes'][unit['recipe_id']]['outputs']) for unit in economy['production_units'].values() if unit['recipe_id'] in economy['recipes'])
        if available or produced:
            return True
    return False
