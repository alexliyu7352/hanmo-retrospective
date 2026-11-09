"""独立伤势来源1只做明示作者配对；不补伤势参数、人物或事实。"""


def check_character_injury_sources(pack, index):
    del index
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        foundation = scenario.get('actor_foundation')
        if isinstance(foundation, dict) and 'injury_capability_map' in foundation:
            mapping = foundation['injury_capability_map']
            map_path = ('scenarios', number, 'actor_foundation', 'injury_capability_map')
            # 只新增已定等级表作者profile；原生旧map1/2不由这里改写或迁移。
            if (not isinstance(mapping, dict)
                    or set(mapping) != {'version', 'profile'}
                    or type(mapping.get('version')) is not int
                    or mapping['version'] != 3
                    or mapping.get('profile') != 'original_injury_grade_effects_v1'):
                issues.append(('INJURY_GRADE_PROFILE3_SELECTION', map_path,
                               '本作者入口只认原伤损等级表version3/profile，不接受自填effects或摘要'))
            if ('original_battle_injury_rules' in foundation
                    or 'original_military_wound_rule' in foundation):
                issues.append(('INJURY_GRADE_PROFILE3_COMPILED_RULES', map_path,
                               '等级表由原冷编译器绑定两原战斗规则，不允许另填Applied/NoEffect或旧军伤绑定'))
            if 'character_injury_sources' not in scenario:
                issues.append(('INJURY_GRADE_PROFILE3_SOURCE1_REQUIRED', map_path,
                               '等级表须显式选择原伤势来源1；不能自动补选'))
        if 'character_injury_sources' not in scenario:
            continue
        path = ('scenarios', number, 'character_injury_sources')
        selected = scenario['character_injury_sources']
        if (not isinstance(selected, dict) or set(selected) != {'version'}
                or type(selected.get('version')) is not int
                or selected['version'] != 1):
            issues.append(('CHARACTER_INJURY_SOURCE1_SELECTION', path,
                           '必须明示独立 version:1；不得从Field或当前伤势自动推导'))
            continue
        lifecycle = scenario.get('personal_battle_lifecycle')
        if (not isinstance(lifecycle, dict)
                or set(lifecycle) != {'profile'}
                or lifecycle['profile'] != 'original_personal_battle_lifecycle_v1'):
            issues.append(('CHARACTER_INJURY_PERSONAL_LIFECYCLE1_REQUIRED', path,
                           '来源1须显式选择原personal_battle_lifecycle/profile；不能自动启用个人战斗生命周期'))
        capabilities = pack.get('required_capabilities')
        if (not isinstance(capabilities, list)
                or 'personal_combat' not in capabilities):
            issues.append(('CHARACTER_INJURY_PERSONAL_COMBAT_REQUIRED', path,
                           '来源1的真实PersonalCatalog必须由原personal_combat能力显式启用'))
        dynamic = scenario.get('dynamic_world')
        field = dynamic.get('field_battle') if isinstance(dynamic, dict) else None
        if (not isinstance(field, dict)
                or field.get('version') != 'field_battle/1'):
            issues.append(('CHARACTER_INJURY_FIELD1_REQUIRED', path,
                           '来源1必须显式选择原dynamic_world.field_battle/1，不能自动开Field或从当前伤势推导'))
        clock = scenario.get('world_time')
        permanent = scenario.get('growth_permanent_contributions')
        providers = clock.get('phase_providers') if isinstance(clock, dict) else None
        growth4 = isinstance(providers, list) and any(
            isinstance(row, dict) and row.get('provider') == 'growth'
            and row.get('profile_id') == 'growth_fixed_service_time2'
            and type(row.get('version')) is int and row['version'] == 4
            for row in providers)
        if (not isinstance(clock, dict)
                or clock.get('profile') != 'rational_world_complete_v2'
                or not isinstance(scenario.get('actor_foundation'), dict)
                or not isinstance(permanent, dict)
                or permanent.get('profile') != 'verified_work_growth_v1'
                or not growth4):
            issues.append(('CHARACTER_INJURY_ORIGINAL_COMPLETE_PAIR', path,
                           '来源1须配原Complete、人物底座、growth_fixed_service_time2/version4与verified_work_growth_v1；原生层再核真实Growth3/Condition8与完整来源'))
    return issues
