"""战法目录只有明确初态/教学/实践来源，不用技能等级或军种模板隐式授予。"""
def check_military_tactic_learning(pack: dict, index: dict) -> list:
    """只校验所选情景真实pair和原表引用；实际掌握仍由运行永久账证明。"""
    issues=[]
    rules={'rally':(15,10000,10,0),'guard':(15,10000,0,0),'suppress':(20,8000,0,0),'flank':(20,12500,0,0),'reform':(15,10000,0,0),'march':(15,10000,0,2),'feint':(20,10000,0,0),'support':(20,10000,0,0)}
    for i,scenario in enumerate(pack.get('scenarios',[])):
        selected=scenario.get('military_tactic_learning');lessons=scenario.get('dynamic_world',{}).get('growth_tactic_lessons')
        if selected is None and lessons is None:continue
        path=('scenarios',i,'military_tactic_learning')
        if (not isinstance(selected,dict) or selected.get('version')!=1 or not isinstance(lessons,dict) or lessons.get('version')!=1
                or scenario.get('military_command_resources')!={'profile':'entry_tactics_recovery_05_v1'}
                or not {'military_tactic_learning_v1','growth_tactic_lessons_v1','military_command_resources_v1'}<=set(pack.get('required_capabilities',[]))):
            issues.append(('MILITARY_TACTIC_LEARNING_PAIR',path,'须完整指挥资源及教学1明确成对选择'));continue
        definitions={}
        for definition in selected.get('definitions',[]):
            id=definition['id']
            if id in definitions:issues.append(('MILITARY_TACTIC_DEFINITION_DUPLICATE',path,id))
            definitions[id]=definition
            for table,refs in [('actors',definition.get('teacher_actor_ids',[])),('drill_challenges',definition.get('practice_challenge_ids',[])),('texts',[definition.get('name_key'),definition.get('description_key')])]:
                for ref in refs:
                    if ref not in index.get(table,{}):issues.append(('MILITARY_TACTIC_REFERENCE',path,str(ref)))
        grant_ids=set();persons=set()
        for grant in selected.get('initial_grants',[]):
            pair=(grant.get('actor_id'),grant.get('tactic_id'))
            if grant.get('id') in grant_ids or pair in persons or pair[0] not in index.get('actors',{}) or pair[1] not in definitions:issues.append(('MILITARY_TACTIC_INITIAL_GRANT',path,str(pair)))
            grant_ids.add(grant.get('id'));persons.add(pair)
        # 正常剧本先投影明确初态；旧目录camp不能替代本情景已改兵种的真实单位。
        projected = None
        if 'initial' in scenario:
            projected = {}
            for n, row in enumerate(scenario['initial'].get('garrisons', [])):
                camp_id = row.get('id')
                if camp_id in projected:
                    issues.append(('MILITARY_TACTIC_INITIAL_GARRISON', ('scenarios', i, 'initial', 'garrisons', n), str(camp_id)))
                projected[camp_id] = row
        for camp in scenario.get('enabled',{}).get('garrisons',[]):
            source = projected.get(camp) if projected is not None else index.get('garrisons',{}).get(camp)
            if not isinstance(source, dict) or not isinstance(source.get('units'), list):
                issues.append(('MILITARY_TACTIC_INITIAL_GARRISON', path, str(camp)))
                continue
            for unit in source['units']:
                troop=index.get('troop_types',{}).get(unit.get('troop_type_id'))
                if not isinstance(troop, dict):
                    issues.append(('MILITARY_TACTIC_TROOP_REQUIRED', path, str(unit.get('troop_type_id'))))
                    continue
                for tactic in troop.get('tactics',[]):
                    definition=definitions.get(tactic.get('learned_tactic_id'));kind=tactic.get('kind');expected=rules.get(kind)
                    if not definition or definition.get('kind')!=kind or expected is None or tuple(tactic.get(x) for x in ['point_cost','power_bp','morale_effect','move_bonus'])!=expected or tactic.get('accuracy_bonus')!=0 or tactic.get('status_rounds')!=1:issues.append(('MILITARY_TACTIC_BINDING',path,str(tactic.get('id'))))
    return issues
