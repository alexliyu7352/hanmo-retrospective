"""驻营调任只绑定真实营地、公职与独立专项Grant；作者不能预写签受或交接事实。"""
def check_political_military_appointments(pack,index):
    issues=[]
    for number,scenario in enumerate(pack.get('scenarios',[])):
        dynamic=scenario.get('dynamic_world',{});config=dynamic.get('political_military_appointments');military=scenario.get('military_officer_assignments');director=dynamic.get('director',{});policies=director.get('policies',director.get('subjects',{}));root=('scenarios',number,'dynamic_world','political_military_appointments')
        def error(code,path,text):issues.append((code,root+tuple(path),text))
        requested=any('military_appointment' in p.get('templates',[]) for p in policies.values())
        if config is None:
            if military or requested:error('POLITICAL_MILITARY_PAIR',(),'军事调任与政治签受须明确同启')
            continue
        if not isinstance(config,dict) or config.get('enabled') is not True or type(config.get('version')) is not int or config.get('version')!=1 or military!={'profile':'political_appointment_v1'} or not dynamic.get('politics',{}).get('enabled') or not {'political_military_appointments_v1','military_officer_assignments_v1'}<=set(pack.get('required_capabilities',[])):
            error('POLITICAL_MILITARY_PAIR',(),'仅允许明确Military6与政治签受1配对');continue
        org=dynamic.get('organization',{});offices=org.get('offices',{});grants=org.get('grants',{});enabled=set(scenario.get('enabled',{}).get('garrisons',[]));organizations=set()
        for key,policy in config.get('policies',{}).items():
            g=index.get('garrisons',{}).get(policy.get('garrison_id'),{});organizations.add(policy.get('organization_id'))
            if key!=policy.get('id') or policy.get('garrison_id') not in enabled or g.get('organization_id')!=policy.get('organization_id') or g.get('place_id')!=policy.get('site_id'):
                error('POLITICAL_MILITARY_SCOPE',(key,),'调任须绑定本剧本同组织真实营地与场所')
            authorized=False
            for office in offices.values():
                if office.get('organization_id')!=policy.get('organization_id'):continue
                for gid in office.get('grant_ids',[]):
                    grant=grants.get(gid,{})
                    if grant.get('office_id')==office.get('id') and 'political_assign_military_officer' in grant.get('allowed_command_kinds',[]) and not grant.get('wallet_ids') and not grant.get('inventory_ids') and grant.get('amount_limit',0)>=1 and {'kind':'site','id':policy.get('site_id')} in grant.get('scopes',[]):authorized=True
            if not authorized:error('POLITICAL_MILITARY_GRANT',(key,),'须有真实公职的驻营调任专项Grant，不能借公账权限')
        for key,policy in policies.items():
            if 'military_appointment' in policy.get('templates',[]) and (director.get('registry_version')!=1 or policy.get('organization_id') not in organizations):error('POLITICAL_MILITARY_DIRECTOR',(key,),'NPC仅在原公平动态登记下接受同组织具名调任')
    return issues
