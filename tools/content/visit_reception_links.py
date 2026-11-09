"""拜帖NPC意向只声明本人在当地的有限问讯，未收到信息不能触发远方寻人。"""


def check_visit_reception(pack, index):
    """核原message政策、同一演员、有限say/choice路径和显式Director模板。"""
    errors=[]
    for number,scene in enumerate(pack.get('scenarios',[])):
        root=('scenarios',number,'visit_reception_intents');profile=scene.get('visit_reception_profile');block=scene.get('visit_reception_intents');director=scene.get('dynamic_world',{}).get('director',{});policies=director.get('policies',director.get('subjects',{}))
        selected=any('visit_reception' in p.get('templates',[]) for p in policies.values())
        if profile is None and block is None and not selected:continue
        if profile!='authored_local_reception_v1' or not isinstance(block,dict) or set(block)!={'version','intents'} or type(block.get('version')) is not int or block['version']!=1 or not isinstance(block.get('intents'),list) or not 1<=len(block['intents'])<=4096:
            errors.append(('VISIT_RECEPTION_PROFILE',root,'须显式启用版本1的有限本人来访政策'));continue
        if scene.get('visit_message_profile')!='witnessed_reception_v1' or not {'visit_reception_v1','visit_messages_v1','native_dynamic_world','multi_actor','dialogue'}<=set(pack.get('required_capabilities',[])):
            errors.append(('VISIT_RECEPTION_CAPABILITY',root,'须同场景启用真实拜帖与本地来访能力'))
        sources={p['id']:p for p in scene.get('visit_messages',{}).get('policies',[])};ids=set();actors=set()
        for offset,row in enumerate(block['intents']):
            path=root+('intents',offset)
            if not isinstance(row,dict) or set(row)!={'id','actor_id','message_policy_id','purpose','cooldown_ticks','route'} or any(not isinstance(row[k],str) or not row[k] for k in ('id','actor_id','message_policy_id','purpose')) or type(row.get('cooldown_ticks')) is not int or not 1<=row['cooldown_ticks']<=120 or not isinstance(row['route'],list) or len(row['route'])>16:
                errors.append(('VISIT_RECEPTION_INTENT',path,'只准具名本人、原消息和有限冷却路径'));continue
            source=sources.get(row['message_policy_id']);purpose=row['purpose'];actor=row['actor_id']
            if row['id'] in ids or actor in actors or purpose not in ('deposit','receive') or not source or actor!=source.get('sender_actor_id' if purpose=='deposit' else 'recipient_actor_id') or actor not in scene.get('enabled',{}).get('actors',[]):
                errors.append(('VISIT_RECEPTION_ACTOR',path,'每个本人仅一份明确来访意图，不能代另一人领读'));continue
            ids.add(row['id']);actors.add(actor)
            if not any('visit_reception' in p.get('templates',[]) and ('registry_version' in director or key==actor) for key,p in policies.items()):errors.append(('VISIT_RECEPTION_DIRECTOR',path,'须给该本人启用原预算内的来访模板'))
            dialogue=index.get('dialogues',{}).get(source['dialogue_id'],{});nodes={n['id']:n for n in dialogue.get('nodes',[])};current=dialogue.get('entry');target=source['deposit_node_id' if purpose=='deposit' else 'receive_node_id'];seen=set();valid=True
            for step in row['route']:
                if not isinstance(step,dict) or not {'node_id'}<=set(step)<={'node_id','option_id'} or current==target or step.get('node_id')!=current or current in seen:valid=False;break
                seen.add(current);node=nodes.get(current,{})
                if node.get('kind')=='say' and 'option_id' not in step:current=node.get('next')
                elif node.get('kind')=='choice' and isinstance(step.get('option_id'),str):
                    choices=[o for o in node.get('options',[]) if o['id']==step['option_id']]
                    if len(choices)!=1:valid=False;break
                    current=choices[0].get('next')
                else:valid=False;break
            if not valid or current!=target or target in seen:errors.append(('VISIT_RECEPTION_ROUTE',path,'原入口至本人声明只走具名say/choice，无自动行动或循环'))
    return errors
