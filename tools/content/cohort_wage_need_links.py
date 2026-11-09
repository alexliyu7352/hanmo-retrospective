"""匿名工资需求只映射原机构对群体的工资义务，不新增工资或家庭权限。"""


def check_cohort_wage_need_policies(pack, index):
    """新版本严格闭集，主体保持cohort且原工资/财政开关互相独立。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict):
            continue
        affairs = dynamic.get('affairs')
        if not isinstance(affairs, dict) or 'cohort_wage_need_policies' not in affairs:
            continue
        block = affairs['cohort_wage_need_policies']
        root = ('scenarios', number, 'dynamic_world', 'affairs', 'cohort_wage_need_policies')

        def issue(code, path, message):
            """错误定位原作者字段，不能当作未启用或没有工资。"""
            issues.append((code, root + tuple(path), message))

        if not isinstance(block, dict) or type(block.get('version')) is not int or block.get('version') != 1:
            issue('COHORT_WAGE_NEED_VERSION', (), '匿名工资需求须显式version=1')
            continue
        policies = block.get('policies')
        if set(block) != {'version', 'policies'} or not isinstance(policies, dict) or not 1 <= len(policies) <= 20000:
            issue('COHORT_WAGE_NEED_SHAPE', (), '策略须1至20000项具名表，不接受金额、钱包或受益人覆盖字段')
            continue
        economy = dynamic.get('local_economy', {})
        if not isinstance(economy, dict):
            issue('COHORT_WAGE_NEED_SOURCE', (), '匿名工资需求必须引用真实地方经济目录')
            continue
        obligations = economy.get('obligations', {})
        cohorts = economy.get('cohorts', {})
        if not isinstance(obligations, dict) or not isinstance(cohorts, dict):
            issue('COHORT_WAGE_NEED_SOURCE', (), '原义务与群体须为具名表')
            continue
        old_ids = set()
        for name in ('need_policies', 'obligation_policies'):
            table = affairs.get(name)
            if isinstance(table, dict):
                old_ids.update(table)
        fiscal = affairs.get('fiscal_need_policies')
        if isinstance(fiscal, dict) and isinstance(fiscal.get('policies'), dict):
            old_ids.update(fiscal['policies'])
        sources, semantics = set(), set()
        for name, policy in policies.items():
            path = ('policies', name)
            fields = {'id', 'obligation_id', 'organization_id', 'semantic_key'}
            if not isinstance(policy, dict) or set(policy) != fields or policy.get('id') != name or any(not isinstance(policy.get(key), str) or not policy[key] for key in fields):
                issue('COHORT_WAGE_NEED_POLICY', path, '策略只允许ID、原义务、付款机构及语义键')
                continue
            obligation = obligations.get(policy['obligation_id'])
            principal = obligation.get('principal', {}) if isinstance(obligation, dict) else {}
            beneficiary = obligation.get('beneficiary', {}) if isinstance(obligation, dict) else {}
            if not isinstance(obligation, dict) or obligation.get('id') != policy['obligation_id'] or obligation.get('kind') != 'wage' or principal != {'kind': 'organization', 'id': policy['organization_id']} or not isinstance(beneficiary, dict) or beneficiary.get('kind') != 'cohort' or beneficiary.get('id') not in cohorts:
                issue('COHORT_WAGE_NEED_SOURCE', path, '须引用现有organization→cohort工资，不可拿具名Actor或口粮替代')
            if name in old_ids or policy['obligation_id'] in sources or policy['semantic_key'] in semantics:
                issue('COHORT_WAGE_NEED_DUPLICATE', path, '同一工资义务只能映射一次，策略ID不可冲撞其他Need')
            sources.add(policy['obligation_id'])
            semantics.add(policy['semantic_key'])
    return issues
