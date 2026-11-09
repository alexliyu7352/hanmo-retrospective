"""具名本人受领复用原Task唯一预约/发信writer；固定Git对象生成，两叶不覆中央。"""
from pathlib import Path
import subprocess,json,hashlib,argparse
BASE='0fd81b1cd20d45fa7fcfd57fe64533cdbe9f03dd'
def replace(s,x,y):
 assert s.count(x)==1,(x[:100],s.count(x));return s.replace(x,y)
def transform(path,old):
 if path.endswith('/task_model.hpp'):
  s=replace(old,'#include "personal_cancellations/task_personal_cancellation_state.hpp"','#include "personal_cancellations/task_personal_cancellation_state.hpp"\n#include "personal_acceptances/task_personal_acceptance_state.hpp"')
  s=replace(s,'struct TaskCatalog {','struct TaskPersonalAcceptanceCatalog;\nstruct TaskCatalog {')
  s=replace(s,'OrderedTable<TaskDefinition> templates; };','OrderedTable<TaskDefinition> templates;std::shared_ptr<const TaskPersonalAcceptanceCatalog> personal_acceptances; };')
  s=replace(s,'struct TaskPersonalReportServices;','struct TaskPersonalAcceptanceServices;struct TaskPersonalReportServices;')
  s=replace(s,'std::optional<TaskPersonalCancellationState> personal_cancellations;','std::optional<TaskPersonalCancellationState> personal_cancellations;std::optional<TaskPersonalAcceptanceState> personal_acceptances;')
  i=s.index('Status start_task_action(');j=s.index(';',i);part=s[i:j];s=s[:i]+replace(part,'const TaskMessageReturnServices* returns=nullptr)','const TaskMessageReturnServices* returns=nullptr,const TaskPersonalAcceptanceServices* personal_acceptances=nullptr)')+s[j:]
  return replace(s,'const TaskPersonalReportServices* personal_reports=nullptr);','const TaskPersonalReportServices* personal_reports=nullptr,const TaskPersonalAcceptanceServices* personal_acceptances=nullptr);')
 s='#include "personal_acceptances/task_personal_acceptance_services.hpp"\n'+old
 # 每个原preview均取同一immutable目录，不能让漏传UI/NPC参数绕选择。
 i=s.index('Outcome<TaskQuote> preview_task(');j=s.index('\nStatus start_task_action',i);part=s[i:j];needle='auto checked=validate_interaction(c,i,ic,access,actor,command.context';part=replace(part,needle,'auto acceptance_gate=task_personal_acceptance_original_preview_gate(catalog.personal_acceptances.get(),command);if(!acceptance_gate.ok)return {false,{},acceptance_gate.error};'+needle);s=s[:i]+part+s[j:]
 for name in ['Status start_task_action(','static Outcome<TaskEffect> finish_task_action_original_with_returns(','\nOutcome<TaskEffect> finish_task_action(']:
  i=s.index(name);j=s.index('{',i);signature=s[i:j];assert signature.endswith(')');s=s[:i]+signature[:-1]+',const TaskPersonalAcceptanceServices* personal_acceptances)'+s[j:]
 i=s.index('Status start_task_action(');j=s.index('{',i)+1;s=s[:j]+'\n    auto acceptance_gate=task_personal_acceptance_original_start_gate(c,s,catalog,catalog.personal_acceptances.get(),personal_acceptances,action,command,quote);if(!acceptance_gate.ok)return acceptance_gate;'+s[j:]
 needle='const bool personal_report_action=p->source_kind==std::optional<StableId>{task_personal_report_source_kind};'
 s=replace(s,needle,'const bool personal_acceptance_action=p->source_kind==std::optional<StableId>{task_personal_acceptance_source_kind};if(personal_acceptance_action&&!personal_acceptances)return {false,{},common_failure("TaskPersonalAcceptanceProfileRequired").error};'+needle)
 s=replace(s,'(personal_action||personal_report_action)?StableId{}:action.kind','(personal_action||personal_report_action||personal_acceptance_action)?StableId{}:action.kind')
 needle='    std::optional<TaskMessageReturnDeliveryPreparation> return_delivery;'
 s=replace(s,needle,'    if(personal_acceptance_action){auto prepared=prepare_task_personal_acceptance_completion(*personal_acceptances,action,finish_tick);if(!prepared.ok){if(!task_personal_acceptance_can_interrupt(prepared.error.original.code))return {false,{},prepared.error};auto released=c.apply_resources({{ReleaseActionResources{action.id}}, {}});if(!released.ok)return {false,{},released.error};t->status="cancelled";TaskEffect stopped;stopped.success=false;stopped.task_id=t->id;stopped.code=prepared.error.original.code;stopped.on_unavailable=d->on_unavailable;stopped.retry="original_person_available";return {true,std::move(stopped),{}};}}\n'+needle)
 needle='auto report_services=validate_task_personal_report_services('
 s=replace(s,needle,'auto acceptance_services=validate_task_personal_acceptance_services(c,s,catalog,s.personal_acceptances?&*s.personal_acceptances:nullptr,personal_acceptances);if(!acceptance_services.ok)return {false,{},acceptance_services.error};'+needle)
 s=replace(s,'const bool personal_report_action=personal_params&&','const bool personal_acceptance_action=personal_params&&personal_params->source_kind==std::optional<StableId>{task_personal_acceptance_source_kind};const bool personal_report_action=personal_params&&')
 s=replace(s,'finish_tick,affairs,execution,notices,returns,personal,personal_reports);if(!result.ok)return result;','finish_tick,affairs,execution,notices,returns,personal,personal_reports,personal_acceptances);if(!result.ok)return result;if(personal_acceptance_action){auto recorded=record_task_personal_acceptance_completion_in_candidate(c,*s.personal_acceptances,personal_acceptances->sources,action,finish_tick,result.value);if(!recorded.ok)return {false,{},recorded.error};}')
 i=s.index('static Status validate_task_records(');j=s.index('{',i)+1;s=s[:j]+'auto acceptance_coverage=validate_task_personal_acceptance_task_coverage(s,catalog.personal_acceptances.get(),s.personal_acceptances?&*s.personal_acceptances:nullptr);if(!acceptance_coverage.ok)return acceptance_coverage;'+s[j:]
 needle='auto& p=row.second;const bool personal_report_action='
 s=replace(s,needle,'auto& p=row.second;const bool personal_acceptance_action=p.source_kind==std::optional<StableId>{task_personal_acceptance_source_kind};if(personal_acceptance_action){if(!s.personal_acceptances)return common_failure("TaskPersonalAcceptanceProfileRequired");auto shaped=validate_task_personal_acceptance_action_shape(c,s,*s.personal_acceptances,*a,p);if(!shaped.ok)return shaped;}const bool personal_report_action=')
 s=replace(s,'*p.source_kind!=a->kind&&!personal_action&&!personal_report_action','*p.source_kind!=a->kind&&!personal_action&&!personal_report_action&&!personal_acceptance_action')
 return replace(s,'(!personal_action&&!personal_report_action&&!common.roles.find(p.context.role_id))','(!personal_action&&!personal_report_action&&!personal_acceptance_action&&!common.roles.find(p.context.role_id))')
def main():
 p=argparse.ArgumentParser();p.add_argument('--base',default=BASE);p.add_argument('--output',required=True,type=Path);a=p.parse_args();rows=[]
 for path in ('native/world/src/resident/domains/tasks/task_model.hpp','native/world/src/resident/domains/tasks/task_rules.cpp'):
  old=subprocess.check_output(['git','show',a.base+':'+path]).decode();new=transform(path,old);f=a.output/'candidate'/path;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(new);oid=lambda s:hashlib.sha1(b'blob '+str(len(s.encode())).encode()+b'\0'+s.encode()).hexdigest();rows.append({'path':path,'base_oid':oid(old),'result_oid':oid(new)})
 (a.output/'manifest.json').write_text(json.dumps({'base':a.base,'shared':rows,'status':'SOURCE_ONLY_NOT_REGISTERED'},indent=2)+'\n');print(a.output)
if __name__=='__main__':main()
