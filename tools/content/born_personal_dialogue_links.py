"""真实出生本人模板只绑定原 prototype 与有界 provider 对白，不写未来人物或预置同意。"""
CAPABILITY='born_personal_dialogues_v1'

def born_personal_dialogue_bindings(pack):
    """仅供原空岗位对白入口分类；完整资格仍必须通过下方检查。"""
    result=set()
    for scene in pack.get('scenarios',[]):
        dynamic=scene.get('dynamic_world',{})
        block=dynamic.get('born_personal_dialogues') if isinstance(dynamic,dict) else None
        if isinstance(block,dict) and isinstance(block.get('templates'),list):
            result.update(t['dialogue_id'] for t in block['templates'] if isinstance(t,dict) and isinstance(t.get('dialogue_id'),str))
    return result

def check_born_personal_dialogues(pack,index):
    errors=[]
    for number,scene in enumerate(pack.get('scenarios',[])):
        dynamic=scene.get('dynamic_world',{})
        block=dynamic.get('born_personal_dialogues') if isinstance(dynamic,dict) else None
        if block is None:continue
        root=('scenarios',number,'dynamic_world','born_personal_dialogues')
        def error(code,text,path=root):errors.append((code,path,text))
        if dynamic.get('profile')!='local_society_v1' or not isinstance(block,dict) or set(block)!={'version','templates'} or type(block.get('version')) is not int or block['version']!=1 or not isinstance(block.get('templates'),list) or not 1<=len(block['templates'])<=256:
            error('BORN_PERSONAL_DIALOGUE_PROFILE','须显式本人模板1与有限非空目录');continue
        if not {CAPABILITY,'family_birth_v1','dialogue','multi_actor','native_dynamic_world'}<=set(pack.get('required_capabilities',[])):error('BORN_PERSONAL_DIALOGUE_CAPABILITY','须明确原生出生与本人对白能力')
        if not all(isinstance(scene.get(key),dict) for key in ('family_birth','family_lineage','family_life')):error('BORN_PERSONAL_DIALOGUE_BIRTH_PROFILE','须配对原出生、亲缘与生命来源');continue
        arrangements=scene['family_birth'].get('arrangements',{})
        prototypes={r.get('terms',{}).get('child_prototype_id') for r in arrangements.values() if isinstance(r,dict) and isinstance(r.get('terms'),dict)} if isinstance(arrangements,dict) else set()
        enabled=scene.get('enabled',{});ids=set();dialogues=set()
        for offset,t in enumerate(block['templates']):
            path=root+('templates',offset)
            if not isinstance(t,dict) or set(t)!={'id','prototype_id','dialogue_id'} or any(not isinstance(v,str) or not 1<=len(v)<=256 for v in t.values()):error('BORN_PERSONAL_DIALOGUE_TEMPLATE','只声明template/prototype/dialogue三个身份',path);continue
            if t['id'] in ids or t['dialogue_id'] in dialogues:error('BORN_PERSONAL_DIALOGUE_DUPLICATE','模板及其对白唯一',path)
            ids.add(t['id']);dialogues.add(t['dialogue_id'])
            if t['prototype_id'] not in prototypes or t['prototype_id'] not in index.get('actors',{}):error('BORN_PERSONAL_DIALOGUE_PROTOTYPE','须本场景原实际出生条款的prototype定义，不需把原型启用为活人',path)
            d=index.get('dialogues',{}).get(t['dialogue_id'])
            if not isinstance(d,dict) or d.get('context_role')!='' or t['dialogue_id'] not in enabled.get('dialogues',[]) or not isinstance(d.get('nodes'),list) or not 1<=len(d['nodes'])<=128:error('BORN_PERSONAL_DIALOGUE_DEFINITION','须原已启用无岗位有界对白',path);continue
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
            if not valid:error('BORN_PERSONAL_DIALOGUE_NODE','只准实际provider/player说话与明确选择，不准自动命令或固定原型speaker',path);continue
            entry=nodes.get(d.get('entry'),{})
            if entry.get('kind')!='say' or entry.get('speaker')!='$provider':error('BORN_PERSONAL_DIALOGUE_ENTRY','实际本人必须由provider say开口',path);continue
            pending=[d['entry']];reached=set()
            while pending:
                key=pending.pop()
                if key in reached:continue
                if key not in nodes:valid=False;break
                reached.add(key);n=nodes[key]
                if n.get('next'):pending.append(n['next'])
                pending.extend(o['next'] for o in n.get('options',[]))
            if not valid or reached!=set(nodes):error('BORN_PERSONAL_DIALOGUE_REACHABILITY','业务节点必须从实际入口可达',path)
    return errors
