"""分项私人遗赠作者入口；本人的声明不自动产生死后知情、受领或公权。"""
PROFILE = {'profile': 'revocable_personal_bequest_v1'}


def check_family_bequest(pack, index):
    """只允许原动态家庭与生命来源配对；未显式启用的旧情景保持旧语义。"""
    del index
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'family_bequest' not in scenario:
            continue
        path = ('scenarios', number, 'family_bequest')
        if scenario['family_bequest'] != PROFILE:
            issues.append(('FAMILY_BEQUEST_PROFILE', path, '只支持生前可撤回的分项私产遗赠，不支持自动移交全部财产'))
        if not isinstance(scenario.get('dynamic_world'), dict) or not isinstance(scenario.get('family_life'), dict):
            issues.append(('FAMILY_BEQUEST_LIFE', path, '遗赠须配原生动态世界与真实死亡来源'))
        if 'family' not in pack.get('required_capabilities', []):
            issues.append(('FAMILY_BEQUEST_RELATION', path, '须启用原家庭关系，不能从姓氏推定受益资格'))
        if 'family_bequest_v1' not in pack.get('required_capabilities', []):
            issues.append(('FAMILY_BEQUEST_CAPABILITY', path, '须明确声明 family_bequest_v1'))
    return issues
