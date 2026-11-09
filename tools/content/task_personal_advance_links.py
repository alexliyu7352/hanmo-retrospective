"""私人垫款只来自本情景明确授权的原采购模板；没有新增全局能力或默认借款。"""
def check_task_personal_advances(pack, index):
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        block = dynamic.get('task_personal_advances')
        if block is None:
            continue
        path = ('scenarios', number, 'dynamic_world', 'task_personal_advances')
        if dynamic.get('profile') != 'local_society_v1' or not {'native_dynamic_world', 'tasks.procurement', 'economy', 'multi_actor'} <= set(pack.get('required_capabilities', [])):
            issues.append(('TASK_ADVANCE_CAPABILITY', path, '垫款须由已有原生采购、经济及人物能力共同支持'))
        if not isinstance(block, dict) or set(block) != {'enabled', 'version', 'policies'} or block.get('enabled') is not True or type(block.get('version')) is not int or block['version'] != 1 or not isinstance(block.get('policies'), list):
            issues.append(('TASK_ADVANCE_PROFILE', path, '情景须明确启用version1垫款政策'))
            continue
        ids, templates = set(), set()
        for offset, row in enumerate(block['policies']):
            source = path + ('policies', offset)
            if not isinstance(row, dict) or set(row) != {'id', 'version', 'task_template_id', 'maximum_cash', 'repayment_policy'}:
                issues.append(('TASK_ADVANCE_POLICY', source, '授权须具名模板、累计金额上限和先还本金政策'))
                continue
            identity, template = row['id'], row['task_template_id']
            if identity in ids or template in templates:
                issues.append(('TASK_ADVANCE_DUPLICATE', source, '同一模板只能有一份明确垫款授权'))
            ids.add(identity); templates.add(template)
            definition = index.get('task_templates', {}).get(template, {})
            if template not in scenario.get('enabled', {}).get('task_templates', []) or definition.get('template') != 'procurement':
                issues.append(('TASK_ADVANCE_TEMPLATE', source, '授权只指向本情景启用的真实采购模板'))
            if type(row['version']) is not int or row['version'] != 1 or type(row['maximum_cash']) is not int or not 1 <= row['maximum_cash'] <= 2147483647 or row['repayment_policy'] != 'authorized_advance_first_v1':
                issues.append(('TASK_ADVANCE_TERMS', source, '政策须明确正整数上限，清算先还本人实际垫款'))
    return issues
