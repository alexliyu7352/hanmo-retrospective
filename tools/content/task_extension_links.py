"""私人延期只绑定本情景原委托人的真实说话节点；作者许可不预造运行期同意。"""
PROFILE = 'private_actor_deadline_v1'
CAPABILITY = 'task_extensions_v1'


def check_task_extensions(pack, index):
    """与原生同样限定Actor采购/送信、原report岗位、唯一政策和有限固定增量。"""
    errors = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'task_extensions' not in scenario:
            continue
        block = scenario['task_extensions']
        root = ('scenarios', number, 'task_extensions')
        if (not isinstance(block, dict) or set(block) != {'version', 'profile', 'policies'}
                or type(block.get('version')) is not int or block['version'] != 1
                or block.get('profile') != PROFILE or not isinstance(block.get('policies'), list)
                or not 1 <= len(block['policies']) <= 4096):
            errors.append(('TASK_EXTENSION_PROFILE', root, '须显式版本1、私人一次延期profile及有限非空政策'))
            continue
        required = {CAPABILITY, 'native_dynamic_world', 'multi_actor', 'dialogue'}
        if not required <= set(pack.get('required_capabilities', [])):
            errors.append(('TASK_EXTENSION_CAPABILITY', root, '须显式声明私人延期和原生真人会面能力'))
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict) or dynamic.get('profile') != 'local_society_v1':
            errors.append(('TASK_EXTENSION_NATIVE_PROFILE', root, '私人延期须沿原生任务与同候选永久来源'))
        enabled = scenario.get('enabled', {})
        seen, offers = set(), set()
        for offset, policy in enumerate(block['policies']):
            path = root + ('policies', offset)
            names = {'id', 'offer_id', 'dialogue_id', 'node_id'}
            if (not isinstance(policy, dict) or set(policy) != names | {'additional_ticks'}
                    or any(not isinstance(policy[k], str) or not policy[k] or len(policy[k]) > 256 for k in names)
                    or type(policy['additional_ticks']) is not int or not 1 <= policy['additional_ticks'] <= 720):
                errors.append(('TASK_EXTENSION_POLICY', path, '政策只含原委托/节点及1至720个半日固定增量'))
                continue
            if policy['id'] in seen or policy['offer_id'] in offers:
                errors.append(('TASK_EXTENSION_DUPLICATE_POLICY', path, '每个原委托至多一份延期政策'))
            seen.add(policy['id']); offers.add(policy['offer_id'])
            offer = index.get('offers', {}).get(policy['offer_id'], {})
            issuer = offer.get('issuer') if isinstance(offer, dict) else None
            if (not isinstance(issuer, dict) or set(issuer) != {'kind', 'id'}
                    or issuer.get('kind') != 'actor' or not isinstance(issuer.get('id'), str)
                    or issuer['id'] not in enabled.get('actors', [])
                    or offer.get('template') not in ('procurement', 'message')
                    or policy['offer_id'] not in enabled.get('offers', [])
                    or type(offer.get('deadline_ticks')) is not int or offer['deadline_ticks'] < 1):
                errors.append(('TASK_EXTENSION_PRIVATE_SOURCE', path, '只准本情景启用的原Actor采购/送信私约；公约动态护送均需后继来源'))
                continue
            if 'tasks.' + offer['template'] not in pack.get('required_capabilities', []):
                errors.append(('TASK_EXTENSION_CAPABILITY', path, '须显式声明原任务类型能力'))
            capabilities = set(pack.get('required_capabilities', []))
            takeover = dynamic.get('political_takeover', {}) if isinstance(dynamic, dict) else {}
            payment_history = (isinstance(takeover, dict) and type(takeover.get('task_execution_version')) is int
                               and takeover.get('task_execution_version') == 2
                               and 'task_static_public_execution_v2' in capabilities)
            report_history = ((offer['template'] == 'procurement'
                               and scenario.get('task_static_asset_profile') == 'original_task_assets_v1'
                               and 'task_static_assets_v1' in capabilities)
                              or (offer['template'] == 'message'
                                  and scenario.get('task_message_handoff_profile') in ('observed_letter_delivery_and_report_v2','known_personal_delivery_and_report_v3','known_personal_delivery_and_sender_report_v4')
                                  and 'task_message_reports_v2' in capabilities))
            if not payment_history and not report_history:
                errors.append(('TASK_EXTENSION_REPORT_HISTORY', path, '须启用该原任务的永久报告Action来源：采购实物1、私信返报2或静态付款2'))
            role_id = offer.get('report_role')
            role = index.get('roles', {}).get(role_id, {}) if isinstance(role_id, str) else {}
            dialogue = index.get('dialogues', {}).get(policy['dialogue_id'], {})
            if (role_id not in enabled.get('roles', []) or policy['dialogue_id'] not in enabled.get('dialogues', [])
                    or role.get('organization_id') is not None or role.get('initial_actor') != issuer['id']
                    or role.get('dialogue_id') != policy['dialogue_id'] or 'report_offer' not in role.get('services', [])
                    or dialogue.get('context_role') != role_id):
                errors.append(('TASK_EXTENSION_ORIGINAL_ISSUER', path, '须原委托人自己的私人接报岗位和同一对话，不能借现任公职代签'))
            nodes = [n for n in dialogue.get('nodes', []) if isinstance(n, dict) and n.get('id') == policy['node_id']]
            if (len(nodes) != 1 or nodes[0].get('kind') != 'say'
                    or nodes[0].get('speaker') not in ('$provider', issuer['id']) or 'command' in nodes[0]):
                errors.append(('TASK_EXTENSION_DECLARATION', path, '须原provider明确说话节点；选项、自动命令和旁人台词不算同意'))
    return errors
