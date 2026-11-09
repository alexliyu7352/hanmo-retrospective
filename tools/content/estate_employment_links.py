"""私人雇用只绑定原庄园、明确分数工资和双方真实会面，不作者预制合同或付款。"""
from collections import Counter

CAPABILITY = 'estate_employment_v1'
FIELDS = {'id', 'estate_id', 'job', 'rate_profile', 'offer_dialogue_id', 'offer_node_id',
          'worker_dialogue_id', 'worker_node_id', 'service_ticks', 'offer_valid_ticks',
          'provider_offer_dialogue_id', 'provider_offer_node_id',
          'provider_reply_dialogue_id', 'provider_reply_node_id', 'provider_reply_decision'}


def check_estate_employment(pack, index):
    """正式作者诊断与原生相同闭集；本金、未赚退款及本人决定一律留待实际规则。"""
    issues = []
    tables = {}
    for name in ('estates', 'actors', 'dialogues', 'wallets', 'inventories'):
        supplied = index.get(name, {})
        tables[name] = dict(supplied) if isinstance(supplied, dict) else {r['id']: r for r in supplied}
        tables[name].update({r['id']: r for r in pack.get(name, []) if isinstance(r, dict) and isinstance(r.get('id'), str)})
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        if 'estate_employment' not in world:
            continue
        path = ('scenarios', number, 'dynamic_world', 'estate_employment')
        def issue(code, detail):
            """保留原情景字段定位，不把未知作者语义改成默认工作许可。"""
            issues.append((code, path, detail))
        config = world['estate_employment']
        if not isinstance(config, dict) or set(config) != {'version', 'policies'} or type(config.get('version')) is not int or config['version'] != 1:
            issue('EMPLOYMENT_PROFILE', '雇用须显式选择1版政策表，不接受种子合同、预付或未知字段')
            continue
        if world.get('profile') != 'local_society_v1' or not {'native_dynamic_world', 'estates', 'economy', 'dialogue', 'multi_actor', CAPABILITY} <= set(pack.get('required_capabilities', [])):
            issue('EMPLOYMENT_CAPABILITIES', '雇用依赖真实庄园、工资原账、人物和会面能力')
        policies = config['policies']
        if not isinstance(policies, dict) or not 1 <= len(policies) <= 4096:
            issue('EMPLOYMENT_POLICIES', '政策须为非空有限表')
            continue
        initial = scenario.get('initial', {})
        wallets = dict(tables['wallets']);wallets.update({r['id']: r for r in initial.get('wallets', [])})
        inventories = dict(tables['inventories']);inventories.update({r['id']: r for r in initial.get('inventories', [])})
        actors = {r['actor_id']: r for r in initial.get('actors', [])}
        rates = world.get('local_economy', {}).get('rates', {})
        node_counts = Counter()
        for key, policy in policies.items():
            if not isinstance(policy, dict) or set(policy) != FIELDS:
                issue('EMPLOYMENT_POLICY_FIELDS', '每项须有完整双向来源字段，未启用的provider声明须显式空值')
                continue
            strings = FIELDS - {'service_ticks', 'offer_valid_ticks'}
            if any(not isinstance(policy[k], str) or len(policy[k]) > 256 for k in strings) or policy['id'] != key or not key:
                issue('EMPLOYMENT_POLICY_ID', '政策ID和引用须明确且有界')
                continue
            if policy['job'] not in {'labor', 'guard', 'manager'} or any(type(policy[k]) is not int or not 1 <= policy[k] <= 720 for k in ('service_ticks', 'offer_valid_ticks')):
                issue('EMPLOYMENT_TERMS', '只支持实际耕作、守备、管事及1至720半日期限')
            rate = rates.get(policy['rate_profile'], {})
            if (not isinstance(rate, dict) or rate.get('per_capita') is not False
                    or type(rate.get('numerator')) is not int or type(rate.get('denominator')) is not int
                    or rate.get('numerator', 0) <= 0 or rate.get('denominator', 0) <= 0
                    or (type(policy['service_ticks']) is int and rate.get('numerator', 0) * policy['service_ticks'] > 9223372036854775807)):
                issue('EMPLOYMENT_RATE', '工资须为正的本人分数率，不得复用按人口应付或溢出预约')
            estate = tables['estates'].get(policy['estate_id'])
            if not estate or policy['estate_id'] not in scenario.get('enabled', {}).get('estates', []):
                issue('EMPLOYMENT_ESTATE', '政策须引用本情景真正启用的原庄园')
            else:
                owner = {'kind': 'actor', 'id': estate['owner_actor_id']}
                if wallets.get(estate['wallet_id'], {}).get('owner') != owner or inventories.get(estate['inventory_id'], {}).get('owner') != owner:
                    issue('EMPLOYMENT_PRIVATE_ASSETS', '工资只来自该原雇主庄园独立私钱包，不能借公库或第三人资产')
                if any(r.get('wallet_id') == estate['wallet_id'] or r.get('inventory_id') == estate['inventory_id'] for r in actors.values()):
                    issue('EMPLOYMENT_SEPARATE_ASSETS', '庄园钱货须与本人随身容器分开')
            for prefix in ('offer', 'worker', 'provider_offer', 'provider_reply'):
                dialogue, node_id = policy[prefix + '_dialogue_id'], policy[prefix + '_node_id']
                provider = prefix.startswith('provider_')
                if provider and not dialogue and not node_id:
                    if prefix == 'provider_reply' and policy['provider_reply_decision']:
                        issue('EMPLOYMENT_PROVIDER_DECISION', '答复语义须有完整provider原节点')
                    continue
                raw = tables['dialogues'].get(dialogue)
                nodes = [n for n in raw.get('nodes', []) if n.get('id') == node_id] if raw else []
                if len(nodes) != 1 or nodes[0].get('kind') not in ({'say'} if provider else {'say', 'choice'}):
                    issue('EMPLOYMENT_NODE', '每一来源须命中唯一原会面节点，provider声明只认本人say')
                    continue
                if provider and (not nodes[0].get('speaker') or nodes[0]['speaker'] == '$player' or (nodes[0]['speaker'] != '$provider' and nodes[0]['speaker'] not in tables['actors'])):
                    issue('EMPLOYMENT_PROVIDER_SPEAKER', '雇主的点击不能代替provider本人发言')
                if prefix == 'provider_reply' and policy['provider_reply_decision'] not in {'accept', 'decline'}:
                    issue('EMPLOYMENT_PROVIDER_DECISION', 'provider只能明确接受或拒绝该条款')
                if prefix in {'offer', 'provider_offer'}:
                    node_counts[(prefix, dialogue, node_id)] += 1
        if any(n > 50 for n in node_counts.values()):
            issue('EMPLOYMENT_NODE_LIMIT', '同一会面节点最多50项政策，页面不得丢失未列出的条款')
    return issues
