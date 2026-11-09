"""固定3604完整约期/Plan后继；仅隔离候选，不写任何中央公共叶。"""
import argparse,difflib,hashlib,json,subprocess
from pathlib import Path
from task_extension_registration_candidate import replace,after
BASE='3604cc9b195234d9e60d5e0a5ee12d014962acf2'
ROOT='native/world/src/resident/domains/interaction/visit_appointments/'
INCLUDE='#include "resident/domains/interaction/visit_appointments/visit_appointment_attendance_world.hpp"\n'
SHARED=['native/world/include/hanmo/resident/model_contract.hpp','native/world/src/resident/domains/interaction/appointments/personal_appointment_attendance.hpp','native/world/src/resident/domains/interaction/appointments/personal_appointment_attendance.cpp','native/world/src/resident/domains/interaction/appointments/personal_appointment_attendance_world.cpp','native/world/src/resident/kernel/dynamic_integration.cpp','native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/native_initial_state.cpp','game/ui/native_resident_backend.gd','schemas/content.schema.json','game/data/rule_capabilities.json','tools/content/links.py','tools/content/dynamic_world_links.py','tools/verify.py','native/world/resident.cmake']

def transform(path,s,repo):
    if path.endswith('/model_contract.hpp'):
        s=replace(s,'struct VisitAppointmentIntentCatalog;','struct VisitAppointmentIntentCatalog;struct VisitAppointmentAttendanceCatalog;')
        return after(s,'std::shared_ptr<const VisitAppointmentIntentCatalog> visit_appointment_intents;','    std::shared_ptr<const VisitAppointmentAttendanceCatalog> visit_appointment_attendance;')
    if path.endswith('/personal_appointment_attendance.hpp'):
        s=replace(s,'namespace hanmo::resident {','namespace hanmo::resident {\nstruct VisitAppointmentAttendanceRead;struct VisitAppointmentAttendanceCatalog;')
        return after(s,'    const PersonalAppointmentState& state;','    const VisitAppointmentAttendanceRead* written_visits=nullptr;\n    const VisitAppointmentAttendanceCatalog* written_catalog=nullptr;')
    if path.endswith('/personal_appointment_attendance.cpp'):
        s=INCLUDE+s
        return replace(s,'    if(!selected)return {true,std::nullopt,{}};','''    // 新旧承诺均用本人原冻结开窗排序；任何另一端未知收取/答受都不参与选择。
    if(read.written_visits||read.written_catalog){if(!read.written_visits||!read.written_catalog)return {false,{},common_failure("VisitAppointmentAttendanceAccessMissing").error};auto written=propose_visit_appointment_attendance(*read.written_visits,*read.written_catalog,travel,view,original);if(!written.ok)return written;if(written.value){const auto* promise=read.written_visits->state.offered.find(written.value->transition->goal_ref);if(!promise)return {false,{},common_failure("VisitAppointmentAttendanceSourceRequired").error};if(!selected||promise->terms.earliest_tick<selected->earliest_tick)return written;}}
    if(!selected)return {true,std::nullopt,{}};''')
    if path.endswith('/personal_appointment_attendance_world.cpp'):
        s=INCLUDE+s
        return replace(s,'for(const auto& row:plans->rows()){auto valid=validate_personal_appointment_attendance_plan','for(const auto& row:plans->rows()){if(w.catalog->visit_appointment_attendance&&visit_appointment_attendance_goal(row.second.goal_ref))continue;auto valid=validate_personal_appointment_attendance_plan')
    if path.endswith('/dynamic_integration.cpp'):
        s=INCLUDE+s
        s=replace(s,'return wake_personal_appointment_attendance_world(w);','auto attendance=wake_personal_appointment_attendance_world(w);if(!attendance.ok)return attendance;return wake_visit_appointment_attendance_world(w);')
        s=replace(s,'std::optional<PersonalAppointmentAttendanceRead> appointment_attendance;', 'auto written_attendance=visit_appointment_attendance_read_world(w);if(!written_attendance.ok)return {false,{},written_attendance.error};std::optional<PersonalAppointmentAttendanceRead> appointment_attendance;')
        return replace(s,'*w.catalog->personal_appointments,*w.interaction.personal_appointments});','*w.catalog->personal_appointments,*w.interaction.personal_appointments,written_attendance.value?&*written_attendance.value:nullptr,w.catalog->visit_appointment_attendance.get()});')
    if path.endswith('/world_bundle.cpp'):
        return after(INCLUDE+s,'valid=validate_visit_appointment_world(*this);','    valid=validate_visit_appointment_attendance_world(*this);if(!valid.ok)return valid;')
    if path.endswith('/world_codec.cpp'):
        s=INCLUDE+s
        s=replace(s,'{"visit_appointment_intents",{"1"}}','{"visit_appointment_intents",{"1"}}, {"visit_appointment_attendance",{"1"}}')
        s=after(s,'if(has("personal_appointment_attendance")','        if(has("visit_appointment_attendance")&&(!dynamic||!has("visit_appointment_invitations")||!has("visit_appointment_meetings")||!has("personal_appointment_attendance")))fail("VisitAppointmentAttendanceProfileMismatch","checkpoint/module_versions");')
        s=after(s,'if(has("personal_appointment_attendance","1")','            if(has("visit_appointment_attendance","1")!=(version("visit_appointment_attendance")==1)||(version("visit_appointment_attendance")&&(!dynamic||!has("visit_appointment_invitations","1")||!has("visit_appointment_meetings","1")||!has("personal_appointment_attendance","1"))))return {false,{},common_failure("VisitAppointmentAttendanceProfileMismatch").error};')
        s=after(s,'auto attendance=decode_personal_appointment_attendance_profile(scenario,','    auto written_attendance=decode_visit_appointment_attendance_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!written_attendance.ok)return {false,{},written_attendance.error};')
        s=after(s,'if(const auto* config=scenario.find("visit_appointment_intents"))','''        if(written_attendance.value){if(!result->director->personal_appointment_attendance||!result->visit_appointment_invitations||!result->personal_appointments)return {false,{},common_failure("VisitAppointmentAttendanceProfileRequired").error};auto parsed=decode_visit_appointment_attendance_catalog(field(scenario,"visit_appointment_attendance","scenario"));if(!parsed.ok)return {false,{},parsed.error};auto valid=validate_visit_appointment_attendance_catalog(parsed.value,*result->visit_appointment_invitations,*result->director->personal_appointment_attendance,*result->personal_appointments);if(!valid.ok)return {false,{},valid.error};result->visit_appointment_attendance=std::make_shared<const VisitAppointmentAttendanceCatalog>(std::move(parsed.value));}''')
        return after(s,'if(result->visit_appointment_invitations){id.module_versions.emplace_back','    if(result->visit_appointment_attendance)id.module_versions.emplace_back("visit_appointment_attendance","1");')
    if path.endswith('/native_initial_state.cpp'):
        return after(INCLUDE+s,'auto visit_appointments=decode_visit_appointment_profile(scenario,','    auto written_attendance=decode_visit_appointment_attendance_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!written_attendance.ok)return {false,{},written_attendance.error};if(written_attendance.value)admitted.module_versions.emplace_back("visit_appointment_attendance","1");')
    if path.endswith('/native_resident_backend.gd'):
        return after(s,'if definition.has("visit_appointment_intents"):','\tif definition.has("visit_appointment_attendance"): expected["module_versions"].append({"name":"visit_appointment_attendance","version":"1"})')
    if path.endswith('/content.schema.json'):
        d=json.loads(s);p=d['$defs']['scenario']['properties'];assert 'visit_appointment_attendance' not in p;p['visit_appointment_attendance']={'type':'object','additionalProperties':False,'required':['version','invitation_policy_ids'],'properties':{'version':{'type':'integer','enum':[1]},'invitation_policy_ids':{'type':'array','minItems':1,'maxItems':4096,'uniqueItems':True,'items':{'type':'string','minLength':1,'maxLength':256}}}};d['properties']['required_capabilities']['items']['enum'].append('visit_appointment_attendance_v1');return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        d=json.loads(s);assert not any(c['capability_id']=='visit_appointment_attendance_v1' for c in d['capabilities']);d['capabilities'].append({'capability_id':'visit_appointment_attendance_v1','rule_version':'own_written_visit_promise_attendance_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.visit_appointment_attendance.version','equals':1}],'supported_semantics':[],'test_ids':['test_visit_appointment_attendance_catalog'],'evidence':[]});return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/dynamic_world_links.py'):
        s=after(s,'from .regional_life_links import check_regional_life','from .visit_appointment_attendance_links import permits_private_reception_attendance')
        return replace(s,'            private_only = ',"            private_only = permits_private_reception_attendance(scenario,policy) or ")
    if path.endswith('/links.py'):
        s=after(s,'from .visit_appointment_links import check_visit_appointments','    from .visit_appointment_attendance_links import check_visit_appointment_attendance');return replace(s,'+ check_visit_appointments(pack,index) +','+ check_visit_appointments(pack,index) + check_visit_appointment_attendance(pack,index) +')
    if path.endswith('/verify.py'):
        return after(s,"'test_personal_appointment_attendance_catalog':", "    'test_visit_appointment_attendance_catalog': '须正规书面赴约1.1作者索引和匹配schema；只核原Snapshot与三producer身份，不冒实际NPC赴约。',")
    if path.endswith('/resident.cmake'):
        for suffix in ['visit_appointment_attendance.cpp','visit_appointment_attendance_catalog.cpp','visit_appointment_attendance_world.cpp']:
            anchor='    src/resident/domains/interaction/visit_appointments/visit_appointment_world.cpp';assert s.count(anchor)==2;s=s.replace(anchor,anchor+'\n    src/resident/domains/interaction/visit_appointments/'+suffix)
        anchor='    src/resident/domains/interaction/visit_appointments/visit_appointment_world.hpp'
        s=replace(s,anchor,anchor+'\n    src/resident/domains/interaction/visit_appointments/visit_appointment_attendance.hpp\n    src/resident/domains/interaction/visit_appointments/visit_appointment_attendance_world.hpp')
        # Keep the original test registration style and original library target intact.
        lines=[l for l in s.splitlines() if 'visit_appointment_authored_test.cpp' in l];assert len(lines)==1,lines
        return replace(s,lines[0],lines[0]+'\n'+lines[0].replace('visit_appointment_authored','visit_appointment_attendance'))
    raise ValueError(path)

def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.output.exists():raise ValueError('fresh isolated output required')
    a.output.mkdir(parents=True);rows=[];patch=[]
    def oid(b):return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    for path in SHARED:
        old=subprocess.check_output(['git','show',BASE+':'+path],cwd=a.repo);new=transform(path,old.decode(),a.repo).encode();assert new!=old,path;out=a.output/'candidate'/path;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(new);rows.append({'path':path,'base_oid':oid(old),'result_oid':oid(new),'bytes':len(new)});patch+=difflib.unified_diff(old.decode().splitlines(True),new.decode().splitlines(True),fromfile='a/'+path,tofile='b/'+path)
    (a.output/'candidate.patch').write_text(''.join(patch));(a.output/'manifest.json').write_text(json.dumps({'base':BASE,'status':'SOURCE_CANDIDATE','shared':rows,'boundary':['catalog marker only, no second persisted promises or changes to old visitor','same original template15 and budgets; old/new own commitments sorted by original window','all new source and meeting/result history before derived plan verification','existing Task responsibility and actual Action/meeting retain precedence'],'deferred':['behavior','World','GUI']},ensure_ascii=False,indent=2)+'\n');print(json.dumps({'shared':len(rows),'manifest':str(a.output/'manifest.json')}))
if __name__=='__main__':main()
