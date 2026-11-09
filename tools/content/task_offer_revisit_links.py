"""回访条件只引用本人实际婉拒；作者不能借会面前筛选读取另一个人物的记忆。"""


def _revisit_terms(condition, path):
    """沿已定 all/any 语法找具名条件，普通作者值不当作可执行条件。"""
    if not isinstance(condition, dict):
        return
    if condition.get('op') == 'offer_declined_before':
        yield path, condition
    if condition.get('op') in ('all', 'any'):
        for number, child in enumerate(condition.get('terms', [])):
            yield from _revisit_terms(child, path + ('terms', number))


def check_task_offer_revisits(pack, index):
    """逐所选情景检查启用profile与真实offer引用，未启用情景不继承别包回应能力。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        enabled = scenario.get('enabled', {}).get('dialogues')
        for offset, dialogue in enumerate(pack.get('dialogues', [])):
            if enabled is not None and dialogue['id'] not in enabled:
                continue
            for node_number, node in enumerate(dialogue.get('nodes', [])):
                path = ('dialogues', offset, 'nodes', node_number)
                conditions = []
                if node.get('kind') == 'branch':
                    conditions.extend(_revisit_terms(node.get('condition'), path + ('condition',)))
                if node.get('kind') == 'choice':
                    for option_number, option in enumerate(node.get('options', [])):
                        for condition_number, condition in enumerate(option.get('conditions', [])):
                            conditions.extend(_revisit_terms(condition, path + ('options', option_number, 'conditions', condition_number)))
                for source, condition in conditions:
                    if scenario.get('task_offer_response_profile') not in ('known_offers_v1', 'known_offers_v2'):
                        issues.append(('OFFER_REVISIT_PROFILE', source, '回访对白须由当前情景显式启用本人回应历史'))
                    if condition.get('offer_id') not in index.get('offers', {}):
                        issues.append(('OFFER_REVISIT_SOURCE', source, '回访只引用本情景实际定义的委托'))
    # Schema也限制场景；此语义门保留明确诊断，不能借嵌套条件移出会面。
    for table, field in (('places', 'admission'), ('events', 'conditions')):
        for number, record in enumerate(pack.get(table, [])):
            for offset, condition in enumerate(record.get(field, [])):
                for source, _ in _revisit_terms(condition, (table, number, field, offset)):
                    issues.append(('OFFER_REVISIT_REQUIRES_CONVERSATION', source, '回访须在已建立会面的对白分支或选择中读取'))
    return issues
