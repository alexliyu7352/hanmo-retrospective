"""条约只声明真实辖区、专项认可和独立入境条件；目录不预制双签或远方知情。"""

def check_treaty_passage(pack,index):
    issues=[]
    for n,scenario in enumerate(pack.get('scenarios',[])):
        world=scenario.get('dynamic_world',{});config=world.get('treaty_passage');root=('scenarios',n,'dynamic_world','treaty_passage')
        def error(code,path,message):issues.append((code,root+tuple(path),message))
        if config is None:
            def uses(condition):
                return condition.get('op')=='treaty_personal_passage' or any(uses(child) for child in condition.get('terms',[]))
            condition_used=any(uses(c) for place in scenario.get('enabled',{}).get('places',[]) for c in index.get('places',{}).get(place,{}).get('admission',[]))
            director=world.get('director',{});policy_used=any('treaty_ratification' in p.get('templates',[]) for p in director.get('policies',director.get('subjects',{})).values())
            if condition_used or policy_used:error('TREATY_PASSAGE_PROFILE',(),'新准入条件与NPC认可必须有显式双签配置')
            continue
        if not isinstance(config,dict) or config.get('version')!=1 or type(config.get('version')) is not int or config.get('enabled') is not True or 'treaty_passage_v1' not in pack.get('required_capabilities',[]) or not world.get('diplomacy',{}).get('enabled') or not world.get('politics',{}).get('enabled'):
            error('TREATY_PASSAGE_PROFILE',(),'双签通行须在真实政治和外交之上显式启用');continue
        org=world.get('organization',{});offices=org.get('offices',{});grants=org.get('grants',{});jurisdictions=world.get('politics',{}).get('jurisdictions',{});terms=world['diplomacy'].get('passage_terms',{});provisions=config.get('provisions',{});mandates=set()
        for key,provision in provisions.items():
            term=terms.get(key,{});bindings=provision.get('places',[]);selected=[b.get('place_id') for b in bindings]
            if not term or provision.get('id')!=key or provision.get('terms_id')!=key or selected!=term.get('place_ids') or len(set(selected))!=len(selected):error('TREATY_PASSAGE_TERMS',(key,),'须完整绑定原条款的具名场所')
            for binding in bindings:
                if binding.get('place_id') not in index.get('places',{}) or binding.get('jurisdiction_id') not in jurisdictions:error('TREATY_PASSAGE_JURISDICTION',(key,),'须引用真实场所与辖区')
            for office in provision.get('npc_ratifier_office_ids',[]):
                if offices.get(office,{}).get('organization_id') not in [term.get('grantor_organization_id'),term.get('beneficiary_organization_id')]:error('TREATY_PASSAGE_NPC',(key,),'NPC认可只可由条约一方的真实公职作出')
                mandates.add(office)
            for policy in world['diplomacy'].get('mission_policies',{}).values():
                if key not in policy.get('passage_terms_ids',[]):continue
                for organization,site,scope in [(term.get('grantor_organization_id'),policy.get('target_site_id'),set(selected)),(term.get('beneficiary_organization_id'),policy.get('issuer_site_id'),set())]:
                    authorized=False
                    for office in offices.values():
                        if office.get('organization_id')!=organization:continue
                        if organization==term.get('grantor_organization_id') and office.get('id')!=policy.get('target_office_id'):continue
                        for grant_id in office.get('grant_ids',[]):
                            grant=grants.get(grant_id,{});sites={s.get('id') for s in grant.get('scopes',[]) if s.get('kind')=='site'}
                            if grant.get('office_id')==office.get('id') and 'diplomatic_ratify_passage' in grant.get('allowed_command_kinds',[]) and not grant.get('wallet_ids') and not grant.get('inventory_ids') and {site}.union(scope)<=sites:authorized=True
                    if not authorized:error('TREATY_PASSAGE_GRANT',(key,),'双方需独立专项认可Grant；授方还须覆盖原条款全部场所')
        director=world.get('director',{});policies=director.get('policies',director.get('subjects',{}))
        for key,policy in policies.items():
            if 'treaty_ratification' in policy.get('templates',[]) and (director.get('registry_version')!=1 or not any(offices.get(o,{}).get('organization_id')==policy.get('organization_id') for o in mandates)):error('TREATY_PASSAGE_DIRECTOR',(key,),'原公平调度的签署模板须有同组织明确公职认可')
        def visit(condition,place):
            if condition.get('op')=='treaty_personal_passage':
                provision=provisions.get(condition.get('terms_id'),{})
                if condition.get('place_id')!=place or not any(p.get('place_id')==place for p in provision.get('places',[])):error('TREATY_PASSAGE_ADMISSION',(place,),'入境条件须绑定本场所与本配置真实条款')
            for child in condition.get('terms',[]):visit(child,place)
        for place in scenario.get('enabled',{}).get('places',[]):
            for condition in index.get('places',{}).get(place,{}).get('admission',[]):visit(condition,place)
    return issues
