"""固定共同Git源接入模板15；独立目录身份，不扩旧Director/Interaction visitor。"""
import argparse,difflib,hashlib,json,subprocess
from pathlib import Path
from task_extension_registration_candidate import replace,after
BASE='9a34cf3aad20d7c994d9a6e4f96847215bb9d794'
ROOT='native/world/src/resident/domains/interaction/appointments/'
INCLUDE='#include "resident/domains/interaction/appointments/personal_appointment_attendance_world.hpp"\n'
SHARED=['native/world/src/resident/domains/director/'+n for n in ['director_model.hpp','director_codec.cpp','director_validation.cpp','director_rules.cpp']]+['native/world/src/resident/kernel/'+n for n in ['dynamic_integration.cpp','native_initial_state.cpp','world_codec.cpp','world_bundle.cpp']]+['native/world/resident.cmake','game/ui/native_resident_backend.gd','schemas/content.schema.json','game/data/rule_capabilities.json','tools/content/links.py','tools/content/dynamic_world_links.py','tools/verify.py']

def transform(path,s):
    """锚点唯一才生成；中央后继模板14三方保留，不整页覆回旧源。"""
    if path.endswith('/director_model.hpp'):
        s=replace(s,'struct AffairsState;','struct AffairsState;\nstruct PersonalAppointmentAttendanceCatalog;struct PersonalAppointmentAttendanceRead;')
        s=replace(s,'visit_reception=13 };','visit_reception=13, personal_appointment_attendance=15 };')
        s=replace(s,'struct DirectorCatalog {','struct DirectorCatalog {\n    std::shared_ptr<const PersonalAppointmentAttendanceCatalog> personal_appointment_attendance;')
        s=replace(s,'const VisitReceptionRead* visit_reception=nullptr);','const VisitReceptionRead* visit_reception=nullptr,const PersonalAppointmentAttendanceRead* appointment_attendance=nullptr);')
        return replace(s,'std::shared_ptr<const VisitReceptionCatalog> visit_reception=nullptr);','std::shared_ptr<const VisitReceptionCatalog> visit_reception=nullptr,bool personal_appointment_attendance_enabled=false);')
    if path.endswith('/director_codec.cpp'):
        s=INCLUDE+s
        s=replace(s,'DirectorTemplate read_template(const StableId& name){','DirectorTemplate read_template(const StableId& name){if(name=="personal_appointment_attendance")return DirectorTemplate::personal_appointment_attendance;')
        s=replace(s,'std::shared_ptr<const VisitReceptionCatalog> reception){try','std::shared_ptr<const VisitReceptionCatalog> reception,bool attendance_enabled){try')
        anchor='"diplomatic_response_version","authored_task_continuation","political_supply_renewals"'
        if s.count(anchor)!=2:raise ValueError('legacy and registry optional slots required')
        s=s.replace(anchor,anchor+',"personal_appointment_attendance"')
        s=after(s,'if(auto* renewal=value.find("political_supply_renewals"))legacy.emplace_back','    if(auto* attendance=value.find("personal_appointment_attendance"))legacy.emplace_back("personal_appointment_attendance",*attendance);')
        s=replace(s,'military,supply,reception);if(!parsed.ok)','military,supply,reception,attendance_enabled);if(!parsed.ok)')
        s=replace(s,'DirectorCatalog c;c.visit_reception=std::move(reception);','DirectorCatalog c;c.visit_reception=std::move(reception);if(attendance_enabled!=(value.find("personal_appointment_attendance")!=nullptr))fail("AppointmentAttendanceProfileMismatch","director");if(attendance_enabled){auto decoded=decode_personal_appointment_attendance_catalog(*value.find("personal_appointment_attendance"));if(!decoded.ok)return {false,{},decoded.error};c.personal_appointment_attendance=std::make_shared<const PersonalAppointmentAttendanceCatalog>(std::move(decoded.value));}')
        s=replace(s,'auto t=read_template(name);','auto t=read_template(name);if(t==DirectorTemplate::personal_appointment_attendance&&!c.personal_appointment_attendance)fail("AppointmentAttendanceProfileRequired",row.first);')
        return replace(s,'&&t!=DirectorTemplate::visit_reception;','&&t!=DirectorTemplate::visit_reception&&t!=DirectorTemplate::personal_appointment_attendance;')
    if path.endswith('/director_validation.cpp'):
        s=replace(s,'(t==DirectorTemplate::visit_reception?', '(t==DirectorTemplate::personal_appointment_attendance?!catalog.personal_appointment_attendance:t==DirectorTemplate::visit_reception?')
        s=replace(s,'&&t!=DirectorTemplate::visit_reception;','&&t!=DirectorTemplate::visit_reception&&t!=DirectorTemplate::personal_appointment_attendance;')
        return replace(s,'&&!(catalog.visit_reception&&p.template_id=="visit_reception"))','&&!(catalog.visit_reception&&p.template_id=="visit_reception")&&!(catalog.personal_appointment_attendance&&p.template_id=="personal_appointment_attendance"))')
    if path.endswith('/director_rules.cpp'):
        s=INCLUDE+s;s=replace(s,'const VisitReceptionRead* visit_reception){','const VisitReceptionRead* visit_reception,const PersonalAppointmentAttendanceRead* appointment_attendance){')
        call='    // 原Task责任先行，赴约只消费本人原已答允来源。\n    if(catalog.personal_appointment_attendance&&enabled(*policy,DirectorTemplate::personal_appointment_attendance)){if(!appointment_attendance)return {false,{},common_failure("AppointmentAttendanceAccessMissing",view.actor_id,ErrorKind::internal).error};auto attendance=propose_personal_appointment_attendance(*appointment_attendance,*catalog.personal_appointment_attendance,travel,view,p);if(!attendance.ok)return {false,{},attendance.error};if(attendance.value)return {true,{std::move(*attendance.value)},{}};}\n'
        return replace(s,'    // 撤准入后只履行已冻结责任，',call+'    // 撤准入后只履行已冻结责任，')
    if path.endswith('/dynamic_integration.cpp'):
        s=INCLUDE+s;s=replace(s,'return reconcile_director_registry(w.common,w.director,*w.catalog->director,authority,w.tasks,w.services,diplomatic_conversation_response(w));','auto result=reconcile_director_registry(w.common,w.director,*w.catalog->director,authority,w.tasks,w.services,diplomatic_conversation_response(w));if(!result.ok)return result;return wake_personal_appointment_attendance_world(w);')
        s=after(s,'std::unique_ptr<VisitReceptionWorldAccess> reception;', '        std::optional<PersonalAppointmentAttendanceRead> appointment_attendance;if(w.catalog->director->personal_appointment_attendance){if(!w.catalog->personal_appointments||!w.interaction.personal_appointments)return common_failure("AppointmentAttendanceProfileMismatch");appointment_attendance.emplace(PersonalAppointmentAttendanceRead{w.common,w.interaction,*w.catalog->interaction,*w.catalog->personal_appointments,*w.interaction.personal_appointments});}')
        return replace(s,'reception?&reception->context():nullptr);if(!proposed.ok)','reception?&reception->context():nullptr,appointment_attendance?&*appointment_attendance:nullptr);if(!proposed.ok)')
    if path.endswith('/native_initial_state.cpp'):
        s=INCLUDE+s;before='    auto attendance=decode_personal_appointment_attendance_profile(scenario,strings(field(reception_data,"required_capabilities","data"),"required_capabilities"));if(!attendance.ok)return {false,{},attendance.error};if(attendance.value){auto checked=validate_personal_appointment_attendance_author_source(scenario,reception_data);if(!checked.ok)return {false,{},checked.error};}\n'
        s=replace(s,'    auto response_director=decode_director_catalog(',before+'    auto response_director=decode_director_catalog(')
        s=replace(s,'},reception_catalog);if(!response_director.ok)','},reception_catalog,attendance.value);if(!response_director.ok)')
        return after(s,'if(dynamic->find("personal_injury_recovery"))admitted.module_versions.emplace_back','    if(attendance.value)admitted.module_versions.emplace_back("personal_appointment_attendance","1");')
    if path.endswith('/world_codec.cpp'):
        s=INCLUDE+s;s=replace(s,'{"personal_appointments",{"1"}}','{"personal_appointments",{"1"}}, {"personal_appointment_attendance",{"1"}}')
        s=after(s,'if(has("personal_appointments")){','        if(has("personal_appointment_attendance")&&(!dynamic||!has("personal_appointments")))fail("AppointmentAttendanceProfileMismatch","checkpoint/module_versions");')
        s=after(s,'if(has("personal_appointments","1")!=','            if(has("personal_appointment_attendance","1")&&(!dynamic||!has("personal_appointments","1")))return {false,{},common_failure("AppointmentAttendanceProfileMismatch").error};')
        s=after(s,'auto appointments=decode_personal_appointment_profile(scenario,','    auto attendance=decode_personal_appointment_attendance_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!attendance.ok)return {false,{},attendance.error};')
        s=replace(s,'},reception_catalog);if(!director.ok)','},reception_catalog,attendance.value);if(!director.ok)')
        s=replace(s,'director.value.exclude_selected_player=native_existing_actor_policy','if(director.value.personal_appointment_attendance){if(!result->personal_appointments)fail("AppointmentAttendanceProfileMismatch","director");auto checked=validate_personal_appointment_attendance_catalog(*director.value.personal_appointment_attendance,*result->personal_appointments);if(!checked.ok)return {false,{},checked.error};}director.value.exclude_selected_player=native_existing_actor_policy')
        return after(s,'if(result->personal_recovery)id.module_versions.emplace_back','    if(result->director&&result->director->personal_appointment_attendance)id.module_versions.emplace_back("personal_appointment_attendance","1");')
    if path.endswith('/world_bundle.cpp'):
        return after(INCLUDE+s,'valid=validate_personal_appointment_world(*this);','    valid=validate_personal_appointment_attendance_world(*this);if(!valid.ok)return valid;')
    if path.endswith('/resident.cmake'):
        anchor='    src/resident/domains/interaction/appointments/personal_appointment_progress.cpp'
        if s.count(anchor)!=2:raise ValueError('two actual native lists required')
        s=s.replace(anchor,anchor+''.join('\n    '+ROOT.replace('native/world/','')+p+'.cpp' for p in ['personal_appointment_attendance','personal_appointment_attendance_catalog','personal_appointment_attendance_world']))
        anchor='    src/resident/domains/interaction/appointments/personal_appointment_progress.hpp';s=replace(s,anchor,anchor+''.join('\n    '+ROOT.replace('native/world/','')+p+'.hpp' for p in ['personal_appointment_attendance','personal_appointment_attendance_world']))
        return s+'\n# 原已受领约定的有界NPC赴约源门。\nadd_executable(test_resident_personal_appointment_attendance tests/resident/personal_appointment_attendance_test.cpp)\ntarget_link_libraries(test_resident_personal_appointment_attendance PRIVATE hanmo_resident)\ntarget_include_directories(test_resident_personal_appointment_attendance PRIVATE src)\ntarget_compile_options(test_resident_personal_appointment_attendance PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_personal_appointment_attendance COMMAND test_resident_personal_appointment_attendance)\n'
    if path.endswith('/native_resident_backend.gd'):
        return after(s,'definition.get("dynamic_world",{}).has("personal_injury_recovery")','\tif definition.get("dynamic_world",{}).get("director",{}).has("personal_appointment_attendance"): expected["module_versions"].append({"name":"personal_appointment_attendance","version":"1"})')
    if path.endswith('/content.schema.json'):
        d=json.loads(s);value={'type':'object','additionalProperties':False,'required':['version','policies'],'properties':{'version':{'type':'integer','enum':[1]},'policies':{'type':'array','minItems':1,'maxItems':4096,'items':{'type':'object','additionalProperties':False,'required':['appointment_policy_id','depart_before_ticks'],'properties':{'appointment_policy_id':{'type':'string','minLength':1,'maxLength':256},'depart_before_ticks':{'type':'integer','minimum':0,'maximum':720}}}}}}
        for name in ['dynamic_director','dynamic_director_legacy','dynamic_director_registry']:d['$defs'][name]['properties']['personal_appointment_attendance']=value
        templates=d['$defs']['dynamic_director_subject']['properties']['templates']['items']['enum'];assert 'personal_appointment_attendance' not in templates;templates.append('personal_appointment_attendance');d['properties']['required_capabilities']['items']['enum'].append('personal_appointment_attendance_v1');return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        d=json.loads(s);d['capabilities'].append({'capability_id':'personal_appointment_attendance_v1','rule_version':'accepted_personal_appointment_attendance_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.director.personal_appointment_attendance.version','equals':1}],'supported_semantics':[],'test_ids':['test_personal_appointment_attendance_catalog'],'evidence':[]});return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path=='tools/content/links.py':
        s=after(s,'from .personal_appointment_links import','    from .personal_appointment_attendance_links import check_personal_appointment_attendance')
        return replace(s,' + check_personal_appointments(pack,index)',' + check_personal_appointments(pack,index) + check_personal_appointment_attendance(pack,index)')
    if path=='tools/content/dynamic_world_links.py':
        return replace(s,'private_only = ',"private_only = (scenario.get('personal_appointment_profile') == 'witnessed_personal_meetings_v1' and 'personal_appointment_attendance' in policy['templates'] and set(policy['templates']) <= {'private_livelihood','maintain_commitment','await_adjustment','personal_appointment_attendance'}) or ")
    if path=='tools/verify.py':
        return replace(s,'EXCLUDED_TESTS = {',"EXCLUDED_TESTS = {\n    'test_personal_appointment_attendance_catalog': '须正规1.1作者索引和匹配schema；仅核目录与三producer身份，不裸跑或替代真实NPC赴约。',")
    raise ValueError(path)

def main():
    """先算完全部锚点再写候选，保精确Git原件与候选OID。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.output.exists():raise ValueError('fresh output required')
    oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest();results={};rows=[];patch=[]
    for path in SHARED:
        old=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);new=transform(path,old.decode()).encode();results[path]=new;rows.append({'path':path,'base_oid':oid(old),'result_oid':oid(new)});patch.extend(difflib.unified_diff(old.decode().splitlines(True),new.decode().splitlines(True),fromfile='a/'+path,tofile='b/'+path))
    for path,data in results.items():target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    (a.output/'candidate.patch').write_text(''.join(patch));(a.output/'manifest.json').write_text(json.dumps({'base':a.base,'shared':rows,'status':'SOURCE_ONLY','module':'personal_appointment_attendance/1','template':15,'old_visitors_unchanged':['Director','Interaction','ReceiptData'],'whole_world':'NOT_RUN'},ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
