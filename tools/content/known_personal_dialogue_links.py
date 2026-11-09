"""眼前真实本人模板明确两端出处；不读取血缘或预置认可。"""
CAPABILITY='known_personal_dialogues_v1'

def known_personal_dialogue_bindings(pack):
    """仅供原空岗位对白入口分类；完整资格仍必须通过下方检查。"""
    result=set()
    for scene in pack.get('scenarios',[]):
        dynamic=scene.get('dynamic_world',{})
        block=dynamic.get('known_personal_dialogues') if isinstance(dynamic,dict) else None
        if isinstance(block,dict) and isinstance(block.get('templates'),list):
            result.update(t['dialogue_id'] for t in block['templates'] if isinstance(t,dict) and isinstance(t.get('dialogue_id'),str))
    return result

def check_known_personal_dialogues(pack,index):
    errors=[]
    for number,scene in enumerate(pack.get('scenarios',[])):
        dynamic=scene.get('dynamic_world',{})
        block=dynamic.get('known_personal_dialogues') if isinstance(dynamic,dict) else None
        if block is None:continue
        root=('scenarios',number,'dynamic_world','known_personal_dialogues')
        def error(code,text,path=root):errors.append((code,path,text))
        if dynamic.get('profile')!='local_society_v1' or not isinstance(block,dict) or set(block)!={'version','templates'} or type(block.get('version')) is not int or block['version']!=1 or not isinstance(block.get('templates'),list) or not 1<=len(block['templates'])<=256:
            error('KNOWN_PERSONAL_DIALOGUE_PROFILE','须显式本人模板1与有限非空目录');continue
        if not {CAPABILITY,'dialogue','multi_actor','native_dynamic_world'}<=set(pack.get('required_capabilities',[])):error('KNOWN_PERSONAL_DIALOGUE_CAPABILITY','须明确原生出生与本人对白能力')
        if not isinstance(scene.get('family_life'),dict):error('KNOWN_PERSONAL_DIALOGUE_LIFE_PROFILE','须原生命来源');continue
        arrangements=scene.get('family_birth',{}).get('arrangements',{})
        prototypes={r.get('terms',{}).get('child_prototype_id') for r in arrangements.values() if isinstance(r,dict) and isinstance(r.get('terms'),dict)} if isinstance(arrangements,dict) else set()
        enabled=scene.get('enabled',{});ids=set();dialogues=set()
        for offset,t in enumerate(block['templates']):
            path=root+('templates',offset)
            if not isinstance(t,dict) or set(t)!={'id','listener','provider','dialogue_id'} or any(not isinstance(t.get(k),str) or not 1<=len(t[k])<=256 for k in ('id','dialogue_id')):error('KNOWN_PERSONAL_DIALOGUE_TEMPLATE','明确模板、对白及双方原出处',path);continue
            if t['id'] in ids or t['dialogue_id'] in dialogues:error('KNOWN_PERSONAL_DIALOGUE_DUPLICATE','模板及对白唯一',path)
            ids.add(t['id']);dialogues.add(t['dialogue_id'])
            for endpoint in ('listener','provider'):
                choice=t.get(endpoint)
                if not isinstance(choice,dict) or set(choice)!={'kind','source_id'} or choice.get('kind') not in ('installed_actor','born_prototype') or not isinstance(choice.get('source_id'),str) or not 1<=len(choice['source_id'])<=256:error('KNOWN_PERSONAL_DIALOGUE_SELECTOR','明确Installed人物或原Birth prototype',path+(endpoint,));continue
                source=choice['source_id']
                if choice['kind']=='installed_actor':
                    if source not in index.get('actors',{}) or source not in enabled.get('actors',[]):error('KNOWN_PERSONAL_DIALOGUE_INSTALLED','必须原已安装人物',path+(endpoint,))
                else:
                    if not all(isinstance(scene.get(k),dict) for k in ('family_birth','family_lineage')) or 'family_birth_v1' not in pack.get('required_capabilities',[]) or source not in prototypes or source not in index.get('actors',{}) or source in enabled.get('actors',[]):error('KNOWN_PERSONAL_DIALOGUE_BIRTH','须原未安装出生原型与本场景真实出生条款',path+(endpoint,))
            if t['listener']==t['provider'] and isinstance(t['listener'],dict) and t['listener'].get('kind')=='installed_actor':error('KNOWN_PERSONAL_DIALOGUE_SELF','本人不能与自己交谈',path)
            d=index.get('dialogues',{}).get(t['dialogue_id'])
            if not isinstance(d,dict) or d.get('context_role')!='' or t['dialogue_id'] not in enabled.get('dialogues',[]) or not isinstance(d.get('nodes'),list) or not 1<=len(d['nodes'])<=128:error('KNOWN_PERSONAL_DIALOGUE_DEFINITION','须原已启用无岗位有界对白',path);continue
            nodes={};valid=True
            for node in d['nodes']:
                if not isinstance(node,dict) or not isinstance(node.get('id'),str) or not node['id'] or node['id'] in nodes:valid=False;break
                nodes[node['id']]=node
                if set(node)&{'command','condition','if_true','if_false','on_success','on_failure'}:valid=False;break
                if node.get('kind')=='say':
                    if node.get('speaker') not in ('$provider','$player') or not node.get('next') or node.get('options'):valid=False;break
                elif node.get('kind')=='choice':
                    opts=node.get('options');seen=set()
                    if node.get('speaker') or node.get('next') or not isinstance(opts,list) or not 1<=len(opts)<=16:valid=False;break
                    for o in opts:
                        if not isinstance(o,dict) or not o.get('id') or o['id'] in seen or not o.get('next') or o.get('conditions'):valid=False;break
                        seen.add(o['id'])
                    if not valid:break
                elif node.get('kind')!='end' or node.get('speaker') or node.get('next') or node.get('options'):valid=False;break
            if not valid:error('KNOWN_PERSONAL_DIALOGUE_NODE','只准实际provider/player说话与明确选择，不准自动命令或固定原型speaker',path);continue
            entry=nodes.get(d.get('entry'),{})
            if entry.get('kind')!='say' or entry.get('speaker')!='$provider':error('KNOWN_PERSONAL_DIALOGUE_ENTRY','实际本人必须由provider say开口',path);continue
            pending=[d['entry']];reached=set()
            while pending:
                key=pending.pop()
                if key in reached:continue
                if key not in nodes:valid=False;break
                reached.add(key);n=nodes[key]
                if n.get('next'):pending.append(n['next'])
                pending.extend(o['next'] for o in n.get('options',[]))
            if not valid or reached!=set(nodes):error('KNOWN_PERSONAL_DIALOGUE_REACHABILITY','业务节点必须从实际入口可达',path)
    return errors
