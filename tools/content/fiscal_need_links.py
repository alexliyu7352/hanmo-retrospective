"""财政Need只映射既有税源；本叶不改变税政、付款、家庭权限或原工资策略。"""


def check_fiscal_need_policies(pack, index):
    """显式新版本采用钱粮分源闭集；未选该能力的旧情景不产生任何变化。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict):
            continue
        affairs = dynamic.get('affairs')
        if not isinstance(affairs, dict) or 'fiscal_need_policies' not in affairs:
            continue
        block = affairs['fiscal_need_policies']
        root = ('scenarios', number, 'dynamic_world', 'affairs', 'fiscal_need_policies')

        def issue(code, path, message):
            """诊断定位原作者字段，不把坏引用降格成无欠款。"""
            issues.append((code, root + tuple(path), message))

        if not isinstance(block, dict) or type(block.get('version')) is not int or block.get('version') != 1:
            issue('FISCAL_NEED_VERSION', (), '财政需求须显式选择version=1')
            continue
        taxes = dynamic.get('fiscal_settlement')
        if not isinstance(taxes, dict) or type(taxes.get('version')) is not int or taxes.get('version') != 1:
            issue('FISCAL_NEED_SETTLEMENT', (), '财政需求只能引用已启用的唯一财政结算域')
            continue
        policies = block.get('policies')
        if set(block) != {'version', 'policies'} or not isinstance(policies, dict) or not 1 <= len(policies) <= 20000:
            issue('FISCAL_NEED_SHAPE', (), '财政需求策略须为1至20000项具名表，禁止额外金额/钱包字段')
            continue
        districts = taxes.get('districts', {})
        keys, semantic_keys = set(), set()
        old_ids = set()
        for field in ('need_policies', 'obligation_policies'):
            table = affairs.get(field)
            if isinstance(table, dict):
                old_ids.update(table)
        for name, policy in policies.items():
            path = ('policies', name)
            fields = {'id', 'district_id', 'source_id', 'unit', 'semantic_key'}
            if not isinstance(policy, dict) or set(policy) != fields or policy.get('id') != name or any(not isinstance(policy.get(key), str) or not policy[key] for key in fields):
                issue('FISCAL_NEED_POLICY', path, '策略只保匹配ID、辖区税源、单位与语义键')
                continue
            district = districts.get(policy['district_id']) if isinstance(districts, dict) else None
            sources = district.get('sources') if isinstance(district, dict) else None
            source = sources.get(policy['source_id']) if isinstance(sources, dict) else None
            if not isinstance(source, dict) or source.get('id') != policy['source_id']:
                issue('FISCAL_NEED_SOURCE', path, '需求必须引用同辖区已有税源，不能新增债务')
            if policy['unit'] not in ('cash', 'grain'):
                issue('FISCAL_NEED_UNIT', path + ('unit',), '仅支持钱款cash或原税粮grain；不能互换单位')
            key = (policy['district_id'], policy['source_id'], policy['unit'])
            if key in keys or policy['semantic_key'] in semantic_keys or name in old_ids:
                issue('FISCAL_NEED_DUPLICATE', path, '同一税源单位只能映射一个Need策略，ID不能冲撞原策略')
            keys.add(key)
            semantic_keys.add(policy['semantic_key'])
    return issues
