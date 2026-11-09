"""私人遗产作者约束：原条款公开、原死亡通知与本人受领分别启用，不允许种子代替来源。"""
PROFILE = {'profile': 'disclosed_personal_bequest_v1'}


def check_family_inheritance(pack, index):
    """登记依赖与精确策略；真实资格及责任由原生同候选门检查，作者不能直接授予遗产。"""
    del index
    issues = []
    required = {'family', 'family_life_v1', 'family_bequest_v1', 'family_inheritance_v1',
                'task_death_notices_v1', 'task_message_handoffs_v1', 'tasks.message',
                'native_dynamic_world', 'dialogue', 'multi_actor'}
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'family_inheritance' not in scenario:
            continue
        path = ('scenarios', number, 'family_inheritance')
        if scenario['family_inheritance'] != PROFILE:
            issues.append(('FAMILY_INHERITANCE_PROFILE', path, '只支持已公开私人遗赠的本人受领与分次交割'))
        if scenario.get('family_bequest') != {'profile': 'revocable_personal_bequest_v1'}:
            issues.append(('FAMILY_INHERITANCE_BEQUEST', path, '须使用独立原遗赠声明策略'))
        if not isinstance(scenario.get('family_life'), dict) or scenario['family_life'].get('profile') != 'verified_military_deaths_v1':
            issues.append(('FAMILY_INHERITANCE_LIFE', path, '须启用原具名死亡来源'))
        notice = scenario.get('task_message_notices')
        if not isinstance(notice, dict) or notice.get('version') != 1 or notice.get('profile') != 'witnessed_death_letters_v1':
            issues.append(('FAMILY_INHERITANCE_NOTICE', path, '须由原真实送信及亲历者来源授予死亡知情'))
        if not isinstance(scenario.get('dynamic_world'), dict) or scenario['dynamic_world'].get('profile') != 'local_society_v1':
            issues.append(('FAMILY_INHERITANCE_WORLD', path, '须在同一原生动态世界办理'))
        missing = required - set(pack.get('required_capabilities', []))
        if missing:
            issues.append(('FAMILY_INHERITANCE_CAPABILITY', path, '缺少明确能力：' + ', '.join(sorted(missing))))
    return issues
