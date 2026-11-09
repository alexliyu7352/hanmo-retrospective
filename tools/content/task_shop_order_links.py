"""私铺店单只借本情景启用的原采购模板；发行、本人同意和钱货仍由运行时证明。"""

PROFILE = 'private_shop_orders_v1'
CAPABILITY = 'task_shop_orders_v1'


def check_task_shop_orders(pack, index):
    """严格配对店单独立能力与S5真实目录，不把安装并集或普通Offer当Task模板。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        root = ('scenarios', number)
        selected = 'task_shop_order_profile' in scenario
        dynamic = scenario.get('dynamic_world')
        shop = dynamic.get('private_shops') if isinstance(dynamic, dict) else None
        orders = shop.get('orders') if isinstance(shop, dict) else None
        policies = orders.get('policies') if isinstance(orders, dict) else None
        # 未选本能力的旧店铺可保留空订单目录；不能因别的情景声明能力而自动启用。
        if not selected and (orders is None or policies == {}):
            continue
        profile_path = root + ('task_shop_order_profile',)
        policy_path = root + ('dynamic_world', 'private_shops', 'orders', 'policies')
        if scenario.get('task_shop_order_profile') != PROFILE:
            issues.append(('TASK_SHOP_ORDER_PROFILE', profile_path,
                           '非空私铺订单须显式选择 private_shop_orders_v1'))
        required = {CAPABILITY, 'native_dynamic_world', 'multi_actor', 'tasks.procurement'}
        if not required <= set(pack.get('required_capabilities', [])):
            issues.append(('TASK_SHOP_ORDER_CAPABILITY', profile_path,
                           '店单须显式声明原生采购、人物及 task_shop_orders_v1 能力'))
        if (not isinstance(dynamic, dict) or dynamic.get('profile') != 'local_society_v1'
                or not isinstance(shop, dict)):
            issues.append(('TASK_SHOP_ORDER_NATIVE_PROFILE', profile_path,
                           '店单只能配对同一原生世界的真实 private_shops 目录'))
        if not isinstance(policies, dict) or not policies:
            issues.append(('TASK_SHOP_ORDER_POLICIES', policy_path,
                           '店单能力须配对非空、按ID索引的原私铺订单政策'))
            continue
        enabled = scenario.get('enabled', {}).get('task_templates', [])
        for policy_id, policy in policies.items():
            path = policy_path + (policy_id, 'task_template_id')
            template_id = policy.get('task_template_id') if isinstance(policy, dict) else None
            template = (index.get('task_templates', {}).get(template_id)
                        if isinstance(template_id, str) and template_id else None)
            if (not isinstance(template, dict) or template.get('id') != template_id
                    or template.get('template') != 'procurement' or template_id not in enabled):
                issues.append(('TASK_SHOP_ORDER_TEMPLATE', path,
                               '店单须引用本情景启用的真实 task_templates 采购模板'))
    return issues
