"""固定原Task唯一writer：回书原件与报酬同recipe，独立来源同保存点；不直接编辑共享工区。"""
import argparse, difflib, hashlib, json, subprocess
from pathlib import Path
from task_extension_registration_candidate import replace
BASE='a52488b202a1e23af6c23bb964083449cacbf765'
MODEL='native/world/src/resident/domains/tasks/task_model.hpp'
RULES='native/world/src/resident/domains/tasks/task_rules.cpp'

def transform(path,s):
 if path==MODEL:
  s=replace(s,'#include "extensions/task_extension_state.hpp"','#include "extensions/task_extension_state.hpp"\n#include "message_returns/task_message_return_state.hpp"')
  s=replace(s,'struct TaskMessageNoticeServices;','struct TaskMessageReturnServices;struct TaskMessageNoticeServices;')
  s=replace(s,'std::optional<TaskExtensionState> deadline_extensions;','std::optional<TaskExtensionState> deadline_extensions;\n    // 独立returns/1保存；旧Task1/2 visitor不添加空字段。\n    std::optional<TaskMessageReturnState> message_returns;')
  for name in ['preview_task','start_task_action','finish_task_action']:
   line=next(line for line in s.splitlines() if (name+'(') in line)
   s=replace(s,line,line[:-2]+',const TaskMessageReturnServices* returns=nullptr);')
  return s
 assert path==RULES
 s='#include "message_returns/task_message_return_services.hpp"\n'+s
 for name in ['preview_task','start_task_action','finish_task_action']:
  line=next(line for line in s.splitlines() if (name+'(') in line)
  s=replace(s,line,line[:-2]+',const TaskMessageReturnServices* returns){')
 # 原热门顺序及动态出口不改；仅在选中profile时防漏同候选服务。
 gate='auto return_services=validate_task_message_return_services(c,s,catalog,s.message_returns?&*s.message_returns:nullptr,returns);if(!return_services.ok)return {false,{},return_services.error};'
 preview='Outcome<TaskQuote> preview_task('
 start='Status start_task_action('
 finish='Outcome<TaskEffect> finish_task_action('
 line=next(line for line in s.splitlines() if line.startswith(preview));s=replace(s,line,line+'\n    '+gate)
 line=next(line for line in s.splitlines() if line.startswith(start));s=replace(s,line,line+'\n    auto return_services=validate_task_message_return_services(c,s,catalog,s.message_returns?&*s.message_returns:nullptr,returns);if(!return_services.ok)return return_services;')
 s=replace(s,'return {true,{task_handover_ticks,context,t->id,reward},{}};','TaskQuote quote{task_handover_ticks,context,t->id,reward};if(returns){auto checked_return=preview_task_message_return_report(returns->sources,i,access,actor,command,quote);if(!checked_return.ok)return {false,{},checked_return.error};}return {true,std::move(quote),{}};')
 anchor='    auto status=c.apply_resources(recipe);if(!status.ok)return status;s.actions.insert(action.id,std::move(params));return common_success();'
 replacement='''    std::optional<TaskMessageReturnReportPreparation> return_start;
    if(returns){auto prepared=prepare_task_message_return_report_start(returns->sources,returns->interaction,returns->interaction_access,action,command,quote);if(!prepared.ok)return {false,{},prepared.error};return_start=std::move(prepared.value);if(return_start)recipe.operations.insert(recipe.operations.end(),return_start->reservation.operations.begin(),return_start->reservation.operations.end());}
    auto status=c.apply_resources(recipe);if(!status.ok)return status;s.actions.insert(action.id,std::move(params));
    if(return_start){status=record_task_message_return_report_start_after_resources(c,*s.message_returns,returns->sources,*return_start);if(!status.ok)return status;}return common_success();'''
 s=replace(s,anchor,replacement)
 # 原所有failure分支先保持其TaskEffect；外层薄包装只记录已实际发生的失败，统一release仍原phase。
 line=next(line for line in s.splitlines() if line.startswith(finish));s=replace(s,line,line.replace('finish_task_action(','static finish_task_action_PLACEHOLDER('))
 s=s.replace('Outcome<TaskEffect> static finish_task_action_PLACEHOLDER(','static Outcome<TaskEffect> finish_task_action_original_with_returns(')
 insertion='''    std::optional<TaskMessageReturnDeliveryPreparation> return_delivery;
    if(returns){auto prepared=prepare_task_message_return_delivery(returns->sources,interaction,access,action,finish_tick);if(!prepared.ok){if(!task_message_return_report_can_interrupt(prepared.error.original.code))return {false,{},prepared.error};auto released=c.apply_resources({{ReleaseActionResources{action.id}}, {}});if(!released.ok)return {false,{},released.error};TaskEffect stopped;stopped.success=false;stopped.task_id=t->id;stopped.code=prepared.error.original.code;stopped.on_unavailable=d->on_unavailable;stopped.retry="original_person_available";return {true,std::move(stopped),{}};}return_delivery=std::move(prepared.value);}
'''
 s=replace(s,'    std::optional<TaskMessageHandoffPreparation> message_handoff;',insertion+'    std::optional<TaskMessageHandoffPreparation> message_handoff;')
 s=replace(s,'    ResourceRecipe recipe;auto owner=task_owner(t->id);auto bag=c.actor(action.actor_id)->inventory_id;','    ResourceRecipe recipe;if(return_delivery)recipe.operations=return_delivery->transfer.operations;auto owner=task_owner(t->id);auto bag=c.actor(action.actor_id)->inventory_id;')
 s=replace(s,'}TaskEffect effect;effect.task_id=t->id;return {true,std::move(effect),{}};','}if(return_delivery){auto recorded=record_task_message_return_delivery_after_report(c,*s.message_returns,returns->sources,*return_delivery);if(!recorded.ok)return {false,{},recorded.error};}TaskEffect effect;effect.task_id=t->id;return {true,std::move(effect),{}};')
 wrapper=line+'\n    '+gate+'''
    auto result=finish_task_action_original_with_returns(c,s,social,catalog,interaction,ic,access,action,finish_tick,affairs,execution,notices,returns);if(!result.ok||result.value.success||!returns)return result;
    auto recorded=record_task_message_return_report_interruption(c,*s.message_returns,returns->sources,action,finish_tick,result.value);if(!recorded.ok)return {false,{},recorded.error};return result;
}
'''
 s=replace(s,'Status cancel_task(CommonStore& c,',wrapper+'Status cancel_task(CommonStore& c,')
 return s

def main():
 p=argparse.ArgumentParser();p.add_argument('--base',default=BASE);p.add_argument('--output',required=True);a=p.parse_args();out=Path(a.output);out.mkdir(parents=True,exist_ok=True);rows=[];diff=[]
 for path in [MODEL,RULES]:
  before=subprocess.check_output(['git','show',a.base+':'+path]).decode();after=transform(path,before);dest=out/'candidate'/path;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(after)
  oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
  rows.append({'path':path,'base_oid':oid(before.encode()),'result_oid':oid(after.encode())});diff.extend(difflib.unified_diff(before.splitlines(True),after.splitlines(True),fromfile='a/'+path,tofile='b/'+path))
 (out/'candidate.patch').write_text(''.join(diff));(out/'manifest.json').write_text(json.dumps({'base':a.base,'status':'PARTIAL_SOURCE_ONLY_UNREGISTERED','shared':rows},indent=2)+'\n')
if __name__=='__main__':main()
