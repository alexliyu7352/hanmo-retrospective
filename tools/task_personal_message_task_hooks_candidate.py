"""原Task唯一writer的显式私人接信接点；共享候选固定Git基准，不覆盖中央。"""
from pathlib import Path
import subprocess,json,hashlib
import argparse
parser=argparse.ArgumentParser(description="生成Q01原Task两叶独立候选；不改主工作树")
parser.add_argument('--base',default='a7e37936e1718d5673d0d7fa1c78ffd09a8ae679')
parser.add_argument('--output',required=True,type=Path)
args=parser.parse_args();wt=Path(__file__).resolve().parents[1];base=args.base;out=args.output;candidate=out/'candidate';candidate.mkdir(parents=True,exist_ok=True)

def blob(b):return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
def read(p):return subprocess.check_output(['git','show',base+':'+p],cwd=wt).decode()
def replace(s,a,b):
 assert s.count(a)==1,(a[:100],s.count(a));return s.replace(a,b)
rows=[]
def put(p,old,new):
 dest=candidate/p;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(new);rows.append({'path':p,'base_oid':blob(old.encode()),'result_oid':blob(new.encode())})
p='native/world/src/resident/domains/tasks/task_model.hpp';old=read(p);s=replace(old,'#include "message_returns/task_message_return_state.hpp"','#include "message_returns/task_message_return_state.hpp"\n#include "personal_messages/task_personal_message_state.hpp"');s=replace(s,'struct TaskMessageReturnServices;','struct TaskPersonalMessageServices;struct TaskMessageReturnServices;');s=replace(s,'std::optional<TaskMessageReturnState> message_returns;','std::optional<TaskMessageReturnState> message_returns;std::optional<TaskPersonalMessageState> personal_messages;');needle='Outcome<TaskEffect> finish_task_action(';a=s.index(needle);b=s.index('\n',a);decl=s[a:b];s=s[:a]+decl[:-2]+',const TaskPersonalMessageServices* personal=nullptr);'+s[b:];put(p,old,s)
p='native/world/src/resident/domains/tasks/task_rules.cpp';old=read(p);s='#include "personal_messages/task_personal_message_services.hpp"\n'+old
for name in ['static Outcome<TaskEffect> finish_task_action_original_with_returns(','\nOutcome<TaskEffect> finish_task_action(']:
 a=s.index(name);b=s.index('{',a);signature=s[a:b];assert signature.endswith(')');s=s[:a]+signature[:-1]+',const TaskPersonalMessageServices* personal)'+s[b:]
needle='auto checked=validate_interaction(c,interaction,ic,access,action.actor_id,p->context,action.kind,action.id);';s=replace(s,needle,'const bool personal_action=p->source_kind==std::optional<StableId>{task_personal_message_source_kind};if(personal_action&&!personal)return {false,{},common_failure("TaskPersonalMessageProfileRequired").error};auto checked=validate_interaction(c,interaction,ic,access,action.actor_id,p->context,personal_action?StableId{}:action.kind,action.id);')
needle='auto prepared=prepare_task_message_handoff(c,s,catalog,*ic.common,*s.message_handoffs,action,finish_tick,&message_phase);if(!prepared.ok)return {false,{},prepared.error};'
s=replace(s,needle,'auto prepared=personal_action?prepare_task_personal_message_handoff(*personal,action,finish_tick):prepare_task_message_handoff(c,s,catalog,*ic.common,*s.message_handoffs,action,finish_tick,&message_phase);if(!prepared.ok){if(!personal_action||!task_personal_message_can_interrupt(prepared.error.original.code))return {false,{},prepared.error};auto released=c.apply_resources({{ReleaseActionResources{action.id}}, {}});if(!released.ok)return {false,{},released.error};TaskEffect stopped;stopped.success=false;stopped.task_id=t->id;stopped.code=prepared.error.original.code;stopped.on_unavailable=d->on_unavailable;stopped.retry="original_person_available";return {true,std::move(stopped),{}};}')
s=replace(s,'s.message_handoffs->version==2&&action.kind=="report_offer"','(s.message_handoffs->version==2||s.message_handoffs->version==3)&&action.kind=="report_offer"')
needle='auto result=finish_task_action_original_with_returns(c,s,social,catalog,interaction,ic,access,action,finish_tick,affairs,execution,notices,returns);if(!result.ok||result.value.success||!returns)return result;'
s=replace(s,needle,'auto personal_services=validate_task_personal_message_services(c,s,catalog,s.personal_messages?&*s.personal_messages:nullptr,personal);if(!personal_services.ok)return {false,{},personal_services.error};const auto* personal_params=s.actions.find(action.id);const bool personal_action=personal_params&&personal_params->source_kind==std::optional<StableId>{task_personal_message_source_kind};auto result=finish_task_action_original_with_returns(c,s,social,catalog,interaction,ic,access,action,finish_tick,affairs,execution,notices,returns,personal);if(!result.ok)return result;if(personal_action){auto recorded=record_task_personal_message_completion_in_candidate(c,*s.personal_messages,personal->sources,action,finish_tick,result.value);if(!recorded.ok)return {false,{},recorded.error};}if(result.value.success||!returns)return result;')
needle='auto& p=row.second;if(p.source_kind&&*p.source_kind!=a->kind)return common_failure("InvalidTaskAction");'
s=replace(s,needle,'auto& p=row.second;const bool personal_action=p.source_kind==std::optional<StableId>{task_personal_message_source_kind};if(personal_action){if(!s.personal_messages)return common_failure("TaskPersonalMessageProfileRequired");auto shaped=validate_task_personal_message_action_shape(c,s,*s.personal_messages,*a,p);if(!shaped.ok)return shaped;}if(p.source_kind&&*p.source_kind!=a->kind&&!personal_action)return common_failure("InvalidTaskAction");')
s=replace(s,'||!common.roles.find(p.context.role_id)||','||(!personal_action&&!common.roles.find(p.context.role_id))||');put(p,old,s)
(out/'manifest.json').write_text(json.dumps({'base':base,'shared':rows,'status':'SOURCE_ONLY_TASK_HOOKS_NOT_REGISTERED'},indent=2));print(out, len(rows))
