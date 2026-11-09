"""静态货物能力只由当前作者显式选定；安装并集不替旧完成任务生成缺失历史。"""


def check_task_static_assets(pack, _index):
    """真实原生Task使用独立实交段；无Task2原支付时本段不授继产历史免责。"""
    issues = []
    for index, scenario in enumerate(pack.get('scenarios', [])):
        if 'task_static_asset_profile' not in scenario:
            continue
        path = ('scenarios', index, 'task_static_asset_profile')
        if scenario['task_static_asset_profile'] != 'original_task_assets_v1':
            issues.append(('TASK_STATIC_ASSET_PROFILE', path, '仅支持原静态任务实交凭据 original_task_assets_v1'))
        for capability in ('task_static_assets_v1', 'native_dynamic_world', 'multi_actor'):
            if capability not in pack.get('required_capabilities', []):
                issues.append(('TASK_STATIC_ASSET_CAPABILITY', path, '静态货物实交缺少显式能力：' + capability))
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict) or dynamic.get('profile') != 'local_society_v1':
            issues.append(('TASK_STATIC_ASSET_NATIVE_PROFILE', path, '实交凭据须原生同候选任务规则'))
    return issues
