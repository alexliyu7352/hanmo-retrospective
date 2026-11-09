"""新先天学习政策只随所选原生剧本与真实课程成组启用。"""


def check_family_innate_learning(pack, index):
    """不改原课程或未知禀赋；未选入剧本的新课程不污染该旧剧本身份。"""
    issues = []
    raw = index.get('roles', {})
    roles = dict(raw) if isinstance(raw, dict) else {r['id']: r for r in raw if isinstance(r, dict) and 'id' in r}
    roles.update({r['id']: r for r in pack.get('roles', []) if isinstance(r, dict) and 'id' in r})
    caps = set(pack.get('required_capabilities', []))
    new_roles = {key for key, role in roles.items() if any(course.get('settlement_policy') == 'innate_per_lesson_v1' for course in role.get('skill_courses', []))}
    for number, scenario in enumerate(pack.get('scenarios', [])):
        profile = scenario.get('family_innate_learning')
        active = new_roles & set(scenario.get('enabled', {}).get('roles', []))
        path = ('scenarios', number, 'family_innate_learning')
        if profile is None:
            if active: issues.append(('INNATE_LEARNING_REQUIRED', path, '选定新课程须显式启用原生先天学习'))
            continue
        if not isinstance(profile, dict) or set(profile) != {'version', 'profile'} or type(profile.get('version')) is not int or profile['version'] != 1 or profile.get('profile') != 'lineage_per_lesson_v1':
            issues.append(('INNATE_LEARNING_FIELDS', path, '只接受版本1与明确逐课政策，不能写入XP或出生来源')); continue
        if not {'family_innate_learning_v1', 'cultivation', 'family_birth_v1', 'native_dynamic_world'} <= caps or not isinstance(scenario.get('dynamic_world'), dict) or not isinstance(scenario.get('family_birth'), dict) or not isinstance(scenario.get('family_lineage'), dict) or scenario.get('skill_profile') != 'full_sixteen_v1':
            issues.append(('INNATE_LEARNING_DEPENDENCIES', path, '需要同剧本原生出生谱系、16技艺与原修业能力'))
        if not active: issues.append(('INNATE_LEARNING_COURSE', path, '须至少选入一门显式先天逐课课程'))
    return issues
