#!/usr/bin/env python3
"""私人垫款的隔离注册候选；旧Task段不加字节，中央审合后才启用新ABI。"""
from pathlib import Path
import argparse, difflib, hashlib, json, subprocess

BASE = 'b97055ec922de482ad2e843a1983f88684897b46'
PREFIX = 'native/world/'
SHARED = [PREFIX+p for p in [
    'include/hanmo/resident/model_contract.hpp',
    'src/resident/domains/tasks/task_model.hpp',
    'src/resident/domains/tasks/task_dynamic_rules.cpp',
    'src/resident/domains/tasks/task_dynamic_validation.cpp',
    'src/resident/kernel/command_types.hpp',
    'src/resident/kernel/dispatcher.cpp',
    'src/resident/kernel/dynamic_integration.cpp',
    'src/resident/kernel/world_bundle.cpp',
    'src/resident/kernel/world_codec.cpp',
    'src/resident/kernel/native_initial_state.cpp',
    'src/resident/kernel/live_view_queries.cpp',
    'src/resident/checkpoint/command_boundary.cpp',
    'resident.cmake']]+['game/ui/native_resident_backend.gd','game/ui/main.gd',
    'game/ui/resident_page_requests.gd','game/ui/screen_composer.gd',
    'schemas/content.schema.json','tools/content/links.py']

def replace(text, old, new, count=1):
    if text.count(old) != count:
        raise ValueError(f'expected {count} anchors, got {text.count(old)}: {old[:100]}')
    return text.replace(old, new)

def transform(path, text):
    """每处追加均有精确锚点；其它owner的新差量由中央三方合并，不覆盖活动源。"""
    if path.endswith('model_contract.hpp'):
        text=replace(text,'struct TaskCatalog;', 'struct TaskCatalog;struct TaskPersonalAdvanceCatalog;')
        text=replace(text,'    std::shared_ptr<const TaskCatalog> tasks;', '    std::shared_ptr<const TaskCatalog> tasks;\n    std::shared_ptr<const TaskPersonalAdvanceCatalog> task_personal_advances;')
    elif path.endswith('task_model.hpp'):
        text=replace(text,'#include "task_offer_response.hpp"','#include "task_offer_response.hpp"\n#include "task_personal_advance.hpp"')
        text=replace(text,'std::optional<TaskOfferResponseState> offer_responses;','std::optional<TaskOfferResponseState> offer_responses;std::optional<TaskPersonalAdvanceState> personal_advances;')
    elif path.endswith('task_dynamic_rules.cpp'):
        text=replace(text,'#include "task_model.hpp"','#include "task_model.hpp"\n#include "task_personal_advance_rules.hpp"')
        anchor='if(!contract->execution_stopped)return common_success();const auto& terms=contract->frozen_terms;'
        gate='''if(!contract->execution_stopped)return common_success();
    // 真实本金尚未归还时，保留原货物与清算责任；本口不得先发报酬或退走托管金。
    if(contract->frozen_terms.personal_advance_allowed){
        if(!state.personal_advances)return common_failure("MissingTaskAdvanceDomain",id);
        auto advance=read_task_personal_advance_summary(common,state,*state.personal_advances,id);if(!advance.ok)return {false,{},advance.error};
        if(advance.value.paid!=advance.value.repaid){contract->status=ContractStatus::settling;contract->restitution_refs=cargo_locations(common,*contract);task->status="settling";return common_success();}
    }
    const auto& terms=contract->frozen_terms;'''
        text=replace(text,anchor,gate)
    elif path.endswith('task_dynamic_validation.cpp'):
        text=replace(text,'#include "task_model.hpp"','#include "task_model.hpp"\n#include "task_personal_advance_rules.hpp"')
        anchor='    for(const auto& row:state.offer_instances.rows())'
        text=replace(text,anchor,'    if(state.personal_advances){auto valid=validate_task_personal_advance_state(common,state,*state.personal_advances);if(!valid.ok)return valid;}\n'+anchor)
        text=replace(text,'||terms.personal_advance_allowed||','||(terms.personal_advance_allowed&&!state.personal_advances)||')
        anchor='const auto remaining_reward=terms.reward_total-contract.paid_reward-contract.refunded_reward;'
        text=replace(text,anchor,anchor+'Integer outstanding_advance=0;if(terms.personal_advance_allowed){auto advance=read_task_personal_advance_summary(common,state,*state.personal_advances,contract.id);if(!advance.ok)return {false,{},advance.error};outstanding_advance=advance.value.paid-advance.value.repaid;}')
        text=replace(text,'wallet->balance-remaining_reward>terms.purchase_budget_cap-contract.refunded_budget','(wallet->balance-remaining_reward>terms.purchase_budget_cap-contract.refunded_budget&&wallet->balance-remaining_reward-(terms.purchase_budget_cap-contract.refunded_budget)>outstanding_advance)')
    elif path.endswith('command_types.hpp'):
        text=replace(text,'#include "resident/domains/tasks/task_model.hpp"','#include "resident/domains/tasks/task_model.hpp"\n#include "resident/domains/tasks/task_personal_advance_command.hpp"')
        text=replace(text,'ConfigureHouseholdProvisionCommand>;','ConfigureHouseholdProvisionCommand,TaskPersonalAdvanceCommand>;')
        text=replace(text,'HouseholdProvisionConfigurationQuote>;','HouseholdProvisionConfigurationQuote,TaskPersonalAdvanceQuote>;')
    elif path.endswith('dispatcher.cpp'):
        text=replace(text,'#include "world_bundle.hpp"','#include "world_bundle.hpp"\n#include "task_advance_integration.hpp"')
        anchor='    if(kind=="decline_runtime_offer"'
        text=replace(text,anchor,'    if(kind=="task_personal_advance"&&!w.catalog->task_personal_advances)return common_failure("TaskAdvanceProfileRequired",{},ErrorKind::unsupported_capability);\n'+anchor)
        anchor='    if(const auto* party=std::get_if<PartyParticipationCommand>(&command.data))'
        text=replace(text,anchor,'    if(const auto* advance=std::get_if<TaskPersonalAdvanceCommand>(&command.data)){auto q=preview_task_advance_world(w,id,*advance);if(!q.ok)return {false,{},q.error};return ready(w,id,command,std::move(q.value));}\n'+anchor)
        anchor='    if(const auto* party=std::get_if<PartyParticipationCommand>(&op.command.data))'
        text=replace(text,anchor,'    if(const auto* advance=std::get_if<TaskPersonalAdvanceCommand>(&op.command.data)){const auto* q=std::get_if<TaskPersonalAdvanceQuote>(&op.quote.data);if(!q)return failed<ReceiptDetails>(common_failure("InvalidTaskAdvanceQuote"));auto r=apply_task_advance_world(w,id,*advance,*q);if(!r.ok)return failed<ReceiptDetails>(r);return {true,ReceiptDetails{Done{},{}},{}};}\n'+anchor)
        anchor='    if(!status.ok)return failed<ActionStarted>(status);return {true,{action.id},{}};'
        text=replace(text,anchor,'    if(!status.ok)return failed<ActionStarted>(status);status=authorize_task_advance_after_accept(w,action.id);if(!status.ok)return failed<ActionStarted>(status);return {true,{action.id},{}};')
    elif path.endswith('dynamic_integration.cpp'):
        text=replace(text,'#include "dynamic_integration.hpp"','#include "dynamic_integration.hpp"\n#include "task_advance_integration.hpp"')
        anchor='    auto contracts=reconcile_task_contracts(w.common,w.tasks,w.affairs,tick);if(!contracts.ok)return contracts;'
        text=replace(text,anchor,anchor+'\n    auto advances=settle_task_advances_after_clock(w);if(!advances.ok)return advances;')
    elif path.endswith('world_bundle.cpp'):
        text=replace(text,'#include "world_bundle.hpp"','#include "world_bundle.hpp"\n#include "resident/domains/tasks/task_personal_advance_profile.hpp"\n#include "resident/domains/tasks/task_personal_advance_rules.hpp"')
        anchor='        for(const auto& row:w.tasks.transfers.rows())'
        text=replace(text,anchor,'        if(w.tasks.personal_advances){for(const auto& row:w.tasks.personal_advances->payments.rows()){auto r=fact(row.first,task_advance_fact_kind,"tasks");if(!r.ok)return r;}for(const auto& row:w.tasks.personal_advances->repayments.rows()){auto r=fact(row.first,task_advance_repayment_fact_kind,"tasks");if(!r.ok)return r;}}\n'+anchor)
        anchor='    valid=validate_task_offer_response_version_history(common,catalog->task_offer_response_version);'
        text=replace(text,anchor,'    valid=validate_task_personal_advance_layout(bool(catalog->task_personal_advances),catalog->identity.module_versions,tasks.personal_advances?&*tasks.personal_advances:nullptr);if(!valid.ok)return valid;\n    valid=validate_task_personal_advance_history(common,bool(catalog->task_personal_advances));if(!valid.ok)return valid;\n    if(tasks.personal_advances){valid=validate_task_personal_advance_catalog_links(*catalog->task_personal_advances,*catalog->tasks,*tasks.personal_advances);if(!valid.ok)return valid;}\n'+anchor)
    elif path.endswith('native_initial_state.cpp'):
        text=replace(text,'#include "native_initial_state.hpp"','#include "native_initial_state.hpp"\n#include "resident/domains/tasks/task_personal_advance_profile.hpp"')
        anchor='    auto invitations=decode_party_invitation_profile(scenario,'
        text=replace(text,anchor,'    auto advances=decode_task_personal_advance_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!advances.ok)return {false,{},advances.error};if(advances.value)admitted.module_versions.emplace_back("task_personal_advances","1");\n'+anchor)
        text=replace(text,'seed.tasks=tasks;', 'if(mutable_catalog->task_personal_advances)tasks->personal_advances.emplace();seed.tasks=tasks;')
    elif path.endswith('world_codec.cpp'):
        text=replace(text,'#include "world_codec.hpp"','#include "world_codec.hpp"\n#include "resident/domains/tasks/task_personal_advance_profile.hpp"')
        text=replace(text,'if(catalog.task_offer_responses)domains.push_back({"task_offer_responses",catalog.task_offer_response_version});','if(catalog.task_offer_responses)domains.push_back({"task_offer_responses",catalog.task_offer_response_version});if(catalog.task_personal_advances)domains.push_back({"task_personal_advances",1});')
        text=replace(text,'{"task_offer_responses",{"1","2"}},','{"task_offer_responses",{"1","2"}}, {"task_personal_advances",{"1"}},')
        anchor='        if (has("task_offer_responses")) domains.push_back({"task_offer_responses",modules.at("task_offer_responses")=="2"?2:1});'
        text=replace(text,anchor,anchor+'\n        if (has("task_personal_advances")) domains.push_back({"task_personal_advances",1});')
        anchor='    auto a=decode_common_catalog(data);'
        text=replace(text,anchor,'    auto advances=decode_task_personal_advance_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!advances.ok)return {false,{},advances.error};if(advances.value)result->task_personal_advances=std::make_shared<const TaskPersonalAdvanceCatalog>(std::move(*advances.value));\n'+anchor)
        anchor='result->tasks=std::make_shared<const TaskCatalog>(std::move(tasks.value));'
        text=replace(text,anchor,anchor+'if(result->task_personal_advances){auto valid=validate_task_personal_advance_catalog(*result->task_personal_advances,*result->tasks);if(!valid.ok)return {false,{},valid.error};}')
        anchor='    if(result->task_offer_responses)id.module_versions.emplace_back("task_offer_responses",std::to_string(result->task_offer_response_version));'
        text=replace(text,anchor,anchor+'\n    if(result->task_personal_advances)id.module_versions.emplace_back("task_personal_advances","1");')
        # 新段显式存在；旧Task/动态Task的字段顺序与字节完全保持原样。
        anchor='    if(catalog.task_offer_responses){'
        text=replace(text,anchor,'    if(catalog.task_personal_advances){const auto* input=source.find("task_personal_advances");if(!input)fail("MissingTaskAdvanceDomain","task_personal_advances");auto advances=decode_task_personal_advance_state(*input);if(!advances.ok)return {false,{},advances.error};auto extended=std::make_shared<TaskState>(*result.tasks);extended->personal_advances=std::move(advances.value);result.tasks=std::move(extended);}else if(source.find("task_personal_advances"))fail("UnexpectedTaskAdvanceDomain","task_personal_advances");\n'+anchor)
        anchor='    if(w.catalog->task_offer_responses){if(!w.tasks.offer_responses)fail('
        text=replace(text,anchor,'    if(w.catalog->task_personal_advances){if(!w.tasks.personal_advances)fail("MissingTaskAdvanceDomain","task_personal_advances");set(output,"task_personal_advances",encode_task_personal_advance_state(*w.tasks.personal_advances));}\n'+anchor)
        anchor='r=section("task_offer_responses",[&](auto& c){return visit_task_offer_response_checkpoint(c,*w.tasks.offer_responses);});if(!r.ok)return {false,{},r.error};}'
        text=replace(text,anchor,anchor+'\n    if(w.catalog->task_personal_advances){if(!w.tasks.personal_advances)return {false,{},common_failure("MissingTaskAdvanceDomain").error};r=section("task_personal_advances",[&](auto& c){return visit_task_personal_advance_checkpoint(c,*w.tasks.personal_advances);});if(!r.ok)return {false,{},r.error};}')
        anchor='        else if(section.domain=="task_offer_responses")'
        text=replace(text,anchor,'        else if(section.domain=="task_personal_advances"){tasks->personal_advances.emplace();r=visit_task_personal_advance_checkpoint(reader,*tasks->personal_advances);}\n'+anchor)
    elif path.endswith('live_view_queries.cpp'):
        text=replace(text,'#include "runtime_task_response_integration.hpp"','#include "runtime_task_response_integration.hpp"\n#include "task_advance_integration.hpp"')
        anchor='    if(query=="runtime_interaction_tasks")'
        text=replace(text,anchor,'    if(query=="task_personal_advances"){boundary::fields(args,{}, {"cursor"},"args");return query_task_advances_world(world,args.find("cursor")?boundary::get_int(args,"cursor","args"):0);}\n'+anchor)
    elif path.endswith('command_boundary.cpp'):
        anchor='    if(kind=="accept_offer"||kind=="report_offer"'
        text=replace(text,anchor,'    if(kind=="task_personal_advance"){auto decoded=decode_task_personal_advance_command(params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}\n'+anchor)
        anchor='    if(party_participation_command_kind(kind)){boundary::fields(value,{},{},"receipt/details");'
        text=replace(text,anchor,'    if(kind=="task_personal_advance"){boundary::fields(value,{},{},"receipt/details");result.data=Done{};return result;}\n'+anchor)
        text=replace(text,'Integer tag=x.data.index();c.field("variant",tag);','Integer tag=x.data.index();c.field("variant",tag);if(kind=="task_personal_advance"&&tag!=0)return c.reject("InvalidTaskAdvanceReceipt");')
        anchor='        if(party_participation_command_kind(r.kind)'
        text=replace(text,anchor,'        if(r.kind=="task_personal_advance"&&(!r.details||!std::holds_alternative<Done>(r.details->data)))failed("InvalidTaskAdvanceReceipt","receipt/details",ErrorKind::rule);\n'+anchor)
        text=replace(text,'bool has_details=static_cast<bool>(value.details);c.field("has_details",has_details);','bool has_details=static_cast<bool>(value.details);c.field("has_details",has_details);if(value.kind=="task_personal_advance"&&!has_details)return c.reject("MissingTaskAdvanceReceipt");')
        anchor='                else if constexpr(std::is_same_v<T,PartyParticipationQuote>)'
        text=replace(text,anchor,'                else if constexpr(std::is_same_v<T,TaskPersonalAdvanceQuote>)return boundary::obj({{"ok",V{true}},{"duration",V{Integer{0}}},{"task_personal_advance",encode_task_personal_advance_quote(quote)}});\n'+anchor)
    elif path.endswith('resident.cmake'):
        cpp=[p for p in owned_paths() if p.startswith(PREFIX) and p.endswith('.cpp')]
        headers=[p for p in owned_paths() if p.startswith(PREFIX) and p.endswith('.hpp')]
        text=replace(text,'    src/resident/domains/tasks/task_offer_response.cpp\n','    src/resident/domains/tasks/task_offer_response.cpp\n'+''.join('    '+p[len(PREFIX):]+'\n' for p in cpp),2)
        text=replace(text,'    src/resident/domains/tasks/task_offer_response.hpp\n','    src/resident/domains/tasks/task_offer_response.hpp\n'+''.join('    '+p[len(PREFIX):]+'\n' for p in headers))
    elif path.endswith('native_resident_backend.gd'):
        anchor='\tif definition.get("task_offer_response_profile","") in ["known_offers_v1","known_offers_v2"]: expected["module_versions"].append({"name":"task_offer_responses","version":"2" if definition.task_offer_response_profile=="known_offers_v2" else "1"})'
        text=replace(text,anchor,anchor+'\n\tif definition.get("dynamic_world",{}).has("task_personal_advances"): expected["module_versions"].append({"name":"task_personal_advances","version":"1"})')
    elif path.endswith('main.gd'):
        text=replace(text,'"decline_offer","decline_runtime_offer",','"decline_offer","decline_runtime_offer","task_personal_advance",',2)
    elif path.endswith('resident_page_requests.gd'):
        anchor='const PartyParticipationPanel=preload("res://ui/party_participation_panel.gd")'
        text=replace(text,anchor,anchor+'\nconst TaskAdvancePanel=preload("res://ui/task_personal_advance_panel.gd")')
        text=replace(text,'\tif not PartyParticipationPanel.prepare(host): return false','\tif not PartyParticipationPanel.prepare(host): return false\n\tif not TaskAdvancePanel.prepare(host): return false')
    elif path.endswith('screen_composer.gd'):
        anchor='const PartyParticipationPanel=preload("res://ui/party_participation_panel.gd")'
        text=replace(text,anchor,anchor+'\nconst TaskAdvancePanel=preload("res://ui/task_personal_advance_panel.gd")')
        anchor='PartyParticipationPanel.build(m,_sidebar_section(m,stage,"PartyParticipationArea","同行"))'
        lines=text.splitlines(True);changed=0
        for index,line in enumerate(lines):
            if anchor in line:
                prefix=line[:len(line)-len(line.lstrip())];lines[index]=line+prefix+'if m._resident_mode and TaskAdvancePanel.visible_here(m): TaskAdvancePanel.build(m,_sidebar_section(m,stage,"TaskAdvanceArea","受托经费"))\n';changed+=1
        if changed!=2: raise ValueError('scene/road sidebar anchors changed')
        text=''.join(lines)
    elif path.endswith('links.py'):
        anchor='    from .task_offer_revisit_links import check_task_offer_revisits'
        text=replace(text,anchor,anchor+'\n    from .task_personal_advance_links import check_task_personal_advances')
        text=replace(text,'+ check_task_offer_revisits(pack,index)', '+ check_task_offer_revisits(pack,index) + check_task_personal_advances(pack,index)')
    elif path.endswith('content.schema.json'):
        value=json.loads(text);defs=value['$defs'];defs['dynamic_world']['properties']['task_personal_advances']={'$ref':'#/$defs/task_personal_advances'}
        ident={'type':'string','pattern':r'^(?:[a-z][a-z0-9_-]{0,31}::)?[a-z][a-z0-9_.-]{0,79}(?![\s\S])','x-hanmo-global-definition':'scenarios[*].dynamic_world.task_personal_advances.policies'}
        defs['task_personal_advance_policy']={'type':'object','additionalProperties':False,'required':['id','version','task_template_id','maximum_cash','repayment_policy'],'properties':{
            'id':ident,'version':{'const':1},'task_template_id':{'$ref':'#/$defs/reference_task_templates'},'maximum_cash':{'type':'integer','minimum':1,'maximum':2147483647},'repayment_policy':{'const':'authorized_advance_first_v1'}}}
        defs['task_personal_advances']={'type':'object','additionalProperties':False,'required':['enabled','version','policies'],'properties':{'enabled':{'const':True},'version':{'const':1},'policies':{'type':'array','items':{'$ref':'#/$defs/task_personal_advance_policy'},'maxItems':128}}}
        text=json.dumps(value,ensure_ascii=False,indent=2)+'\n'
    else:
        raise ValueError(path)
    return text

def owned_paths():
    root=Path(__file__).resolve().parents[1]
    return sorted([str(p.relative_to(root)) for pattern in ['native/world/src/resident/domains/tasks/task_personal_advance*.hpp','native/world/src/resident/domains/tasks/task_personal_advance*.cpp','native/world/src/resident/kernel/task_advance_integration.*'] for p in root.glob(pattern)]+[
        'game/ui/task_personal_advance_panel.gd','tools/content/task_personal_advance_links.py','tools/task_advance_content.py',
        'content/packs/authored/task_contacts_1_5/pack.json','content/packs/authored/task_contacts_1_5/content.json'])

def blob(data):
    return hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--base',default=BASE);parser.add_argument('--output',required=True);args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];out=Path(args.output).resolve();out.mkdir(parents=True,exist_ok=True);shared=[];patch=[]
    for path in SHARED:
        old=subprocess.check_output(['git','show',args.base+':'+path],cwd=root).decode();new=transform(path,old)
        for area,text in [('base',old),('candidate',new)]:
            dest=out/area/path;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(text)
        shared.append({'path':path,'base_git_oid':blob(old.encode()),'candidate_git_oid':blob(new.encode())})
        patch.extend(difflib.unified_diff(old.splitlines(True),new.splitlines(True),'a/'+path,'b/'+path))
    owned=[]
    for path in owned_paths():
        data=(root/path).read_bytes();dest=out/'owned'/path;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data);owned.append({'path':path,'git_oid':blob(data)})
    (out/'registration.patch').write_text(''.join(patch));(out/'manifest.json').write_text(json.dumps({'base':args.base,'status':'WIP full registration candidate; not active in production','shared':shared,'owned':owned},indent=2)+'\n')
    print(json.dumps({'shared':len(shared),'owned':len(owned),'manifest':str(out/'manifest.json')}))
if __name__=='__main__':
    main()
