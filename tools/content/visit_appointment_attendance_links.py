"""只把显式书面政策接到原模板15；新目录不能创造NPC、承诺或另一端已读知识。"""
CAP='visit_appointment_attendance_v1'


def permits_private_reception_attendance(scene,policy):
    """仅新显式并集借原无组织Native准入；各真实目录内容仍交原完整checker。"""
    dynamic=scene.get('dynamic_world',{})
    selected=scene.get('visit_appointment_attendance',{})
    reception=scene.get('visit_reception_intents',{})
    original=dynamic.get('director',{}).get('personal_appointment_attendance',{})
    templates=set(policy.get('templates',[]))
    return (type(selected.get('version')) is int and selected['version']==1
            and bool(selected.get('invitation_policy_ids'))
            and scene.get('visit_appointment_profile')=='witnessed_visit_appointments_v1'
            and scene.get('personal_appointment_profile')=='witnessed_personal_meetings_v1'
            and scene.get('visit_reception_profile')=='authored_local_reception_v1'
            and type(reception.get('version')) is int and reception['version']==1 and bool(reception.get('intents'))
            and type(original.get('version')) is int and original['version']==1 and bool(original.get('policies'))
            # 固定Native的无组织模板15解码只在原named-membership分支支持。
            and dynamic.get('organization',{}).get('named_membership_enabled') is True
            and {'visit_reception','personal_appointment_attendance'}<=templates
            and templates<={'private_livelihood','maintain_commitment','await_adjustment','visit_reception','personal_appointment_attendance'})

def check_visit_appointment_attendance(pack,index):
    errors=[]
    for number,scene in enumerate(pack.get('scenarios',[])):
        block=scene.get('visit_appointment_attendance')
        if block is None:continue
        path=('scenarios',number,'visit_appointment_attendance')
        if (not isinstance(block,dict) or set(block)!={'version','invitation_policy_ids'}
                or type(block.get('version')) is not int or block['version']!=1
                or not isinstance(block.get('invitation_policy_ids'),list) or not 1<=len(block['invitation_policy_ids'])<=4096):
            errors.append(('VISIT_APPOINTMENT_ATTENDANCE_PROFILE',path,'须独立选择版本1和有限非空书面政策'));continue
        if (scene.get('visit_appointment_profile')!='witnessed_visit_appointments_v1'
                or not {CAP,'visit_appointments_v1','personal_appointment_attendance_v1','personal_appointments_v1','npc_society','native_dynamic_world'}<=set(pack.get('required_capabilities',[]))):
            errors.append(('VISIT_APPOINTMENT_ATTENDANCE_CAPABILITY',path,'须完整书面邀约与原模板15显式能力'))
        director=scene.get('dynamic_world',{}).get('director',{});old=director.get('personal_appointment_attendance',{})
        if old.get('version')!=1 or not isinstance(old.get('policies'),list):
            errors.append(('VISIT_APPOINTMENT_ATTENDANCE_ORIGINAL',path,'提前量必须出自原真实当面赴约目录'));continue
        invitations={p['id']:p for p in scene.get('visit_appointment_invitations',{}).get('policies',[]) if isinstance(p,dict) and 'id' in p}
        original={p['appointment_policy_id'] for p in old['policies'] if isinstance(p,dict) and isinstance(p.get('appointment_policy_id'),str)}
        seen=set()
        for offset,key in enumerate(block['invitation_policy_ids']):
            here=path+('invitation_policy_ids',offset)
            if not isinstance(key,str) or not 1<=len(key)<=256:
                errors.append(('VISIT_APPOINTMENT_ATTENDANCE_POLICY',here,'须具名原邀请政策'));continue
            if key in seen or key not in invitations:
                errors.append(('VISIT_APPOINTMENT_ATTENDANCE_SOURCE',here,'重复或不存在的邀请政策不能产生书面承诺'));continue
            seen.add(key);policy=invitations[key]
            if policy.get('inviter_commits_fixed_window') is not True or policy.get('appointment_policy_id') not in original:
                errors.append(('VISIT_APPOINTMENT_ATTENDANCE_ORIGINAL',here,'须原明确本人发邀承诺和同政策的原提前量'))
    return errors
