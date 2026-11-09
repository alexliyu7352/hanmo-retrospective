"""回复作者只声明原接待点两个受理节点；不生成答复、信件、预约或控制者切换。"""
PROFILE='witnessed_visit_replies_v1'
CAPABILITY='visit_message_replies_v1'
AUTHORED_PROFILE='witnessed_authored_visit_replies_v2'
def check_visit_message_replies(pack,index):
    """原拜帖目录由旧门完整核，新目录只允许精确同原政策/岗位/本人say的返交。"""
    errors=[]
    for number,s in enumerate(pack.get('scenarios',[])):
        profile,catalog=s.get('visit_reply_profile'),s.get('visit_message_replies');root=('scenarios',number,'visit_message_replies')
        if profile is None and catalog is None:continue
        if profile not in (PROFILE,AUTHORED_PROFILE) or not isinstance(catalog,dict) or set(catalog)!={'version','policies'} or type(catalog.get('version')) is not int or catalog['version']!=(2 if profile==AUTHORED_PROFILE else 1) or not isinstance(catalog.get('policies'),list) or not 1<=len(catalog['policies'])<=4096:
            errors.append(('VISIT_REPLY_PROFILE',root,'须显式回复1与非空有限政策'));continue
        if not {CAPABILITY,'visit_messages_v1','dialogue','multi_actor','native_dynamic_world'}<=set(pack.get('required_capabilities',[])):errors.append(('VISIT_REPLY_CAPABILITY',root,'须显式原拜帖及回复能力'))
        originals=s.get('visit_messages')
        if s.get('visit_message_profile')!='witnessed_reception_v1' or not isinstance(originals,dict) or originals.get('version')!=1 or not isinstance(originals.get('policies'),list):errors.append(('VISIT_REPLY_ORIGINAL_PROFILE',root,'必须原托留/本人领读目录配对'));continue
        if not isinstance(s.get('dynamic_world'),dict) or s['dynamic_world'].get('profile')!='local_society_v1':errors.append(('VISIT_REPLY_NATIVE',root,'只在原生同候选世界办理'))
        source={p.get('id'):p for p in originals['policies'] if isinstance(p,dict)};ids=set();linked=set()
        for offset,p in enumerate(catalog['policies']):
            path=root+('policies',offset)
            if not isinstance(p,dict) or set(p)!={'id','message_policy_id','reply_node_id','collect_node_id'} or any(not isinstance(v,str) or not 1<=len(v)<=256 for v in p.values()):errors.append(('VISIT_REPLY_POLICY',path,'只声明原政策和两个有限节点'));continue
            if p['id'] in ids or p['message_policy_id'] in linked:errors.append(('VISIT_REPLY_DUPLICATE',path,'同一原拜帖政策只准一组回复入口'))
            ids.add(p['id']);linked.add(p['message_policy_id']);original=source.get(p['message_policy_id'])
            if not original:errors.append(('VISIT_REPLY_SOURCE',path,'必须精确引用本场景原拜帖政策'));continue
            role=index.get('roles',{}).get(original.get('role_id'),{});dialogue=index.get('dialogues',{}).get(original.get('dialogue_id'),{})
            if role.get('initial_actor')!=original.get('custodian_actor_id') or role.get('dialogue_id')!=original.get('dialogue_id') or dialogue.get('context_role')!=original.get('role_id'):errors.append(('VISIT_REPLY_CUSTODIAN',path,'原保管人身份、岗位和对白必须一致'))
            if p['reply_node_id']==p['collect_node_id']:errors.append(('VISIT_REPLY_NODE',path,'本人托回与原发信人领取须分别声明'))
            for field in ['reply_node_id','collect_node_id']:
                nodes=[n for n in dialogue.get('nodes',[]) if isinstance(n,dict) and n.get('id')==p[field]]
                if len(nodes)!=1 or nodes[0].get('kind')!='say' or nodes[0].get('speaker') not in ['$provider',original.get('custodian_actor_id')] or 'command' in nodes[0]:errors.append(('VISIT_REPLY_DECLARATION',path+(field,),'只能接待者实际say受理，不能替本人自动发信'))
    return errors

def check_visit_reply_intents(pack,index):
    """显式v2只准原已领读者固定回复；两段实际路线不含自动业务，不创造NPC新承诺。"""
    errors=[]
    for number,scene in enumerate(pack.get('scenarios',[])):
        block=scene.get('visit_reply_intents');profile=scene.get('visit_reply_profile');root=('scenarios',number,'visit_reply_intents')
        if block is None and profile!=AUTHORED_PROFILE:continue
        def error(code,text,path=root):errors.append((code,path,text))
        if profile!=AUTHORED_PROFILE or not isinstance(block,dict) or set(block)!={'version','intents'} or type(block.get('version')) is not int or block['version']!=1 or not isinstance(block.get('intents'),list) or not 1<=len(block['intents'])<=4096:
            error('VISIT_REPLY_INTENT_PROFILE','须明确回复2及独立有限意向1');continue
        if not {'visit_message_replies_v1','visit_message_replies_v2','visit_reception_v1'}<=set(pack.get('required_capabilities',[])):error('VISIT_REPLY_INTENT_CAPABILITY','须原拜帖、认领及回复能力完整成组')
        reception=scene.get('visit_reception_intents',{});replies=scene.get('visit_message_replies',{});messages=scene.get('visit_messages',{})
        if scene.get('visit_reception_profile')!='authored_local_reception_v1' or reception.get('version')!=1 or replies.get('version')!=2:error('VISIT_REPLY_INTENT_PAIR','须真实原接待目录和回复2配对');continue
        find=lambda rows,key:next((v for v in rows if isinstance(v,dict) and v.get('id')==key),None)
        ids=set();original_ids=set()
        for offset,row in enumerate(block['intents']):
            path=root+('intents',offset)
            if not isinstance(row,dict) or set(row)!={'id','reception_intent_id','reply_policy_id','response','route'} or any(not isinstance(row[k],str) or not 1<=len(row[k])<=256 for k in ('id','reception_intent_id','reply_policy_id','response')) or not isinstance(row['route'],list) or not 1<=len(row['route'])<=16:
                error('VISIT_REPLY_INTENT','只准具名原来源和有限实际节点',path);continue
            original=find(reception.get('intents',[]),row['reception_intent_id']);reply=find(replies.get('policies',[]),row['reply_policy_id']);message=find(messages.get('policies',[]),reply.get('message_policy_id')) if reply else None
            if row['id'] in ids or row['reception_intent_id'] in original_ids or row['response'] not in ('declined','contact_in_person') or not original or original.get('purpose')!='receive' or not message or original.get('message_policy_id')!=message.get('id') or original.get('actor_id')!=message.get('recipient_actor_id'):
                error('VISIT_REPLY_INTENT_SOURCE','只准原收帖本人且不能另授预约',path);continue
            ids.add(row['id']);original_ids.add(row['reception_intent_id']);dialogue=index.get('dialogues',{}).get(message.get('dialogue_id'),{});nodes={n.get('id'):n for n in dialogue.get('nodes',[]) if isinstance(n,dict)}
            if any(step.get('node_id') not in nodes or 'command' in nodes[step['node_id']] for step in original.get('route',[])):error('VISIT_REPLY_INTENT_ORIGINAL_ROUTE','原接待路线不能自动执行业务',path)
            current=message.get('receive_node_id');target=reply.get('reply_node_id');seen=set();valid=True
            for step in row['route']:
                if not isinstance(step,dict) or not {'node_id'}<=set(step)<={'node_id','option_id'} or current==target or step.get('node_id')!=current or current in seen:valid=False;break
                seen.add(current);node=nodes.get(current,{})
                if 'command' in node:valid=False;break
                if node.get('kind')=='say' and not step.get('option_id'):current=node.get('next')
                elif node.get('kind')=='choice' and isinstance(step.get('option_id'),str):
                    options=[o for o in node.get('options',[]) if o.get('id')==step['option_id']]
                    if len(options)!=1:valid=False;break
                    current=options[0].get('next')
                else:valid=False;break
            if not valid or current!=target or current in seen:error('VISIT_REPLY_INTENT_ROUTE','须从原receive到原reply的无环有限路径',path)
    return errors
