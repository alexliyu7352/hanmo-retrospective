"""Face3 只选择原计划专属告知根；结构配对不替代实际权源、消息与知情。"""


def check_face_to_face_contacts(pack, index):
    """沿 native Face3 明示选择；不自动补计划、报告、cohort 或通用已知事实。"""
    del index
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict) or 'face_to_face_contacts' not in dynamic:
            continue
        path = ('scenarios', number, 'dynamic_world', 'face_to_face_contacts')
        selection = dynamic['face_to_face_contacts']
        if (not isinstance(selection, dict) or set(selection) != {'version'}
                or type(selection.get('version')) is not int
                or selection['version'] != 3):
            issues.append(('FACE_TO_FACE_PHYSICAL3', path,
                           '必须明示 version:3；旧1/2存读不在此作者入口重解释'))
            continue
        clock = scenario.get('world_time')
        purchase = dynamic.get('household_purchases')
        retail = dynamic.get('household_retail_settlements')
        if (not isinstance(clock, dict)
                or clock.get('profile') != 'rational_world_complete_v2'
                or not isinstance(scenario.get('actor_foundation'), dict)
                or not isinstance(dynamic.get('organization'), dict)
                or not isinstance(purchase, dict)
                or type(purchase.get('version')) is not int
                or purchase['version'] not in (2, 3)
                or not isinstance(retail, dict)
                or type(retail.get('version')) is not int
                or retail['version'] != 1):
            issues.append(('FACE_TO_FACE_HOUSEHOLD_ORIGINAL_PAIR', path,
                           'Face3须同Complete人物、公权目录及原家庭采购2/3与零售1；实际Family来源仍由原native目录与同World全门核验'))
    return issues
