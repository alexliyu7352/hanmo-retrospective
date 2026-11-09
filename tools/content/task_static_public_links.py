"""静态公账v2须由选定情景显式启用；能力并集不替旧Task1自动升级。"""
CAPABILITY = 'task_static_public_execution_v2'


def check_task_static_public_execution(pack, _index):
    """只核本域版本与能力契约，政治据点/权源的原作者校验继续独立生效。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        if not isinstance(dynamic, dict) or 'political_takeover' not in dynamic:
            continue
        config = dynamic['political_takeover']
        if not isinstance(config, dict):
            continue  # 原政治schema负责非对象错误，不能在这里转为合法缺省。
        version = config.get('task_execution_version', 1)
        path = ('scenarios', number, 'dynamic_world', 'political_takeover', 'task_execution_version')
        if type(version) is not int or version not in (1, 2):
            issues.append(('TASK_STATIC_PUBLIC_VERSION', path, '任务履责段只支持整数1或2；缺省保持旧1'))
        elif version == 2:
            if CAPABILITY not in pack.get('required_capabilities', []):
                issues.append(('TASK_STATIC_PUBLIC_CAPABILITY', path, '静态任务公账履责须显式声明task_static_public_execution_v2'))
            if config.get('enabled') is not True or config.get('version') != 1 or dynamic.get('profile') != 'local_society_v1':
                issues.append(('TASK_STATIC_PUBLIC_PROFILE', path, 'Task2须与原政治1、经济履责1共同开启'))
    return issues
