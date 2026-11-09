"""当面探听只绑定原私人说话者和明确对象；不把目录或当前位置当作已获知的消息。"""
PROFILE='witnessed_personal_inquiries_v1'
CAPABILITY='contact_inquiries_v1'

def check_contact_inquiries(pack,index):
    """与原生相同的一期会面/本人来源/有限透露语义；原亲见目录另由各自作者门完整校验。"""
    errors=[]
    for number,scenario in enumerate(pack.get('scenarios',[])):
        profile,catalog=scenario.get('contact_inquiry_profile'),scenario.get('contact_inquiries')
        if profile is None and catalog is None:continue
        root=('scenarios',number,'contact_inquiries')
        if profile!=PROFILE or not isinstance(catalog,dict) or set(catalog)!={'version','policies'} or type(catalog.get('version')) is not int or catalog['version']!=1 or not isinstance(catalog.get('policies'),list) or not 1<=len(catalog['policies'])<=4096:
            errors.append(('CONTACT_INQUIRY_PROFILE',root,'须明确探听版本1和有限非空政策'));continue
        if not {CAPABILITY,'dialogue','multi_actor','native_dynamic_world'}<=set(pack.get('required_capabilities',[])):errors.append(('CONTACT_INQUIRY_CAPABILITY',root,'须本包显式声明原生当面探听能力'))
        if not isinstance(scenario.get('dynamic_world'),dict) or scenario['dynamic_world'].get('profile')!='local_society_v1':errors.append(('CONTACT_INQUIRY_NATIVE',root,'须沿原生同候选世界'))
        if scenario.get('personal_appointment_profile')!='witnessed_personal_meetings_v1' and scenario.get('task_message_handoff_profile') not in {'observed_letter_delivery_v1','observed_letter_delivery_and_report_v2','known_personal_delivery_and_report_v3','known_personal_delivery_and_sender_report_v4'}:errors.append(('CONTACT_INQUIRY_SOURCE',root,'只支持原私人会面或原信件实交的本人亲见，不猜世界位置'))
        enabled=scenario.get('enabled',{});seen=set()
        for offset,p in enumerate(catalog['policies']):
            path=root+('policies',offset);strings={'id','provider_actor_id','subject_actor_id','role_id','dialogue_id','node_id','disclosure'}
            if not isinstance(p,dict) or set(p)!=strings|{'duration_ticks'} or any(not isinstance(p[k],str) or not 1<=len(p[k])<=256 for k in strings) or type(p['duration_ticks']) is not int or p['duration_ticks']!=1 or p['disclosure'] not in {'share_last_seen','decline'}:
                errors.append(('CONTACT_INQUIRY_POLICY',path,'只准一个半日、明确分享原亲见或拒答'));continue
            if p['id'] in seen:errors.append(('CONTACT_INQUIRY_DUPLICATE',path,'政策身份须唯一'))
            seen.add(p['id']);role=index.get('roles',{}).get(p['role_id'],{});dialogue=index.get('dialogues',{}).get(p['dialogue_id'],{})
            if p['provider_actor_id']==p['subject_actor_id'] or p['provider_actor_id'] not in enabled.get('actors',[]) or p['subject_actor_id'] not in enabled.get('actors',[]) or p['role_id'] not in enabled.get('roles',[]) or p['dialogue_id'] not in enabled.get('dialogues',[]) or role.get('organization_id') is not None or role.get('initial_actor')!=p['provider_actor_id'] or role.get('dialogue_id')!=p['dialogue_id'] or role.get('place_id') not in enabled.get('places',[]) or dialogue.get('context_role')!=p['role_id']:
                errors.append(('CONTACT_INQUIRY_ORIGINAL_PERSON',path,'须原私人岗位本人及真实已声明对象/地点/对白'))
            nodes=[n for n in dialogue.get('nodes',[]) if isinstance(n,dict) and n.get('id')==p['node_id']]
            if len(nodes)!=1 or nodes[0].get('kind')!='say' or nodes[0].get('speaker') not in ('$provider',p['provider_actor_id']) or 'command' in nodes[0]:errors.append(('CONTACT_INQUIRY_DECLARATION',path,'只能当前本人say表达可谈的话题，不能自动发答案'))
    return errors
