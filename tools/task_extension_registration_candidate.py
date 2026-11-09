"""从固定de95生成私人延期完整共享候选；所有实际中央文件仍由中央串行合流。"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
BASE='de95de3aa71b9a0c1b2f57b5e25bb650b629f265'
ROOT='native/world/src/resident/domains/tasks/extensions/'
INCLUDE='#include "resident/domains/tasks/extensions/task_extension_world.hpp"\n'
PROFILE='private_actor_deadline_v1'
CAP='task_extensions_v1'
SHARED=['native/world/src/resident/domains/tasks/task_model.hpp','native/world/include/hanmo/resident/model_contract.hpp',
'native/world/src/resident/kernel/command_types.hpp','native/world/src/resident/kernel/dispatcher.cpp',
'native/world/src/resident/checkpoint/command_boundary.cpp','native/world/src/resident/kernel/phase_plan.cpp',
'native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/native_initial_state.cpp',
'native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/storage/common_codec.cpp',
'native/world/src/resident/kernel/live_view_queries.cpp','game/ui/native_resident_backend.gd',
'game/ui/resident_page_requests.gd','game/ui/screen_composer.gd','schemas/content.schema.json',
'game/data/rule_capabilities.json','tools/content/links.py','tools/verify.py']


def replace(s, old, new):
    """必须精确唯一锚点，失败保原候选，不猜测新公共语义。"""
    if s.count(old)!=1:raise ValueError((old[:160],s.count(old)))
    return s.replace(old,new)


def after(s, anchor, extra):
    """完整具名行后追加，三冷尾顺序按同一新尾排列。"""
    lines=[x for x in s.splitlines() if anchor in x]
    if len(lines)!=1:raise ValueError((anchor,len(lines)))
    return replace(s,lines[0],lines[0]+'\n'+extra)


def transform(path,s):
    """只新显式段、同意命令和有限页；原Task1/2与既有Plan拒门不改。"""
    if path.endswith('/task_model.hpp'):
        s=replace(s,'#pragma once','#pragma once\n#include "extensions/task_extension_state.hpp"')
        return replace(s,'std::optional<TaskStaticAssetState> static_assets;','std::optional<TaskStaticAssetState> static_assets;std::optional<TaskExtensionState> deadline_extensions;')
    if path.endswith('/model_contract.hpp'):
        s=replace(s,'struct TaskMessageNoticeCatalog;','struct TaskMessageNoticeCatalog;struct TaskExtensionCatalog;')
        return replace(s,'bool task_static_assets_enabled=false;','bool task_static_assets_enabled=false;\n    std::shared_ptr<const TaskExtensionCatalog> task_extensions;')
    if path.endswith('/command_types.hpp'):
        s=INCLUDE+s
        s=replace(s,'VisitMessageCommand,ChildCareReliefCommand>;','VisitMessageCommand,ChildCareReliefCommand,TaskExtensionCommand>;')
        return replace(s,'VisitMessageQuote,ChildCareReliefCommandQuote>;','VisitMessageQuote,ChildCareReliefCommandQuote,TaskExtensionQuote>;')
    if path.endswith('/dispatcher.cpp'):
        s=INCLUDE+s
        s=replace(s,'Prepared prepare_command(const WorldBundle& w,const StableId& id,const CommandPayload& command){','Prepared prepare_command(const WorldBundle& w,const StableId& id,const CommandPayload& command){\n    const auto* extension=std::get_if<TaskExtensionCommand>(&command.data);if((command.kind==task_extension_command_kind)!=(extension!=nullptr))return reject("InvalidTaskExtensionCommand");')
        s=after(s,'auto guard=command_common_guard(w,id,command.kind);','    if(extension){auto quote=prepare_task_extension_world(w,id,*extension);if(!quote.ok)return {false,{},quote.error};return ready(w,id,command,std::move(quote.value));}')
        s=after(s,'const auto* visit=std::get_if<VisitMessageCommand>(&op.command.data);','    const auto* extension=std::get_if<TaskExtensionCommand>(&op.command.data);if((op.command.kind==task_extension_command_kind)!=(extension!=nullptr)||(extension&&!std::holds_alternative<TaskExtensionQuote>(op.quote.data)))return failed<ReceiptDetails>(common_failure("InvalidTaskExtensionCommand"));')
        return after(s,'if(visit){auto result=commit_visit_message_world','    if(extension){auto result=commit_task_extension_world(w,id,*extension,std::get<TaskExtensionQuote>(op.quote.data));if(!result.ok)return {false,{},result.error};return {true,ReceiptDetails{std::move(result.value),{}},{}};}')
    if path.endswith('/command_boundary.cpp'):
        s=INCLUDE+s
        s=after(s,'if(visit_message_command_kind(kind)){auto decoded=','    if(kind==task_extension_command_kind){auto decoded=decode_task_extension_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}')
        s=after(s,'else if constexpr(std::is_same_v<T,VisitMessageQuote>)return','                else if constexpr(std::is_same_v<T,TaskExtensionQuote>)return encode_task_extension_quote(quote);')
        s=replace(s,'else if(visit_message_command_kind(kind)){boundary::fields','else if(visit_message_command_kind(kind)||kind==task_extension_command_kind){boundary::fields')
        s=replace(s,'if(visit_message_command_kind(kind)&&','if((visit_message_command_kind(kind)||kind==task_extension_command_kind)&&')
        return replace(s,'if(visit_message_command_kind(r.kind)){const auto* effect=','if(visit_message_command_kind(r.kind)||r.kind==task_extension_command_kind){const auto* effect=')
    if path.endswith('/phase_plan.cpp'):
        return replace(s,'const bool task_write=takeover_command||','const bool task_write=std::holds_alternative<TaskExtensionCommand>(command.data)||takeover_command||')
    if path.endswith('/native_initial_state.cpp'):
        s=INCLUDE+s
        s=after(s,'admitted.module_versions.emplace_back("family_care_relief","1");','    auto task_extensions=decode_task_extension_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!task_extensions.ok)return {false,{},task_extensions.error};if(task_extensions.value)admitted.module_versions.emplace_back("task_extensions","1");')
        return replace(s,'if(mutable_catalog->task_static_assets_enabled)tasks->static_assets.emplace();','if(mutable_catalog->task_static_assets_enabled)tasks->static_assets.emplace();if(mutable_catalog->task_extensions)tasks->deadline_extensions.emplace();')
    if path.endswith('/world_codec.cpp'):
        s=INCLUDE+s
        s=replace(s,'if(family_care_relief_enabled(catalog))domains.push_back({"family_care_relief",1});return domains;','if(family_care_relief_enabled(catalog))domains.push_back({"family_care_relief",1});if(catalog.task_extensions)domains.push_back({"task_extensions",1});return domains;')
        s=replace(s,'{"family_care_relief",{"1"}}','{"family_care_relief",{"1"}}, {"task_extensions",{"1"}}')
        s=after(s,'if(has("family_care_relief")){if(!dynamic','        if(has("task_extensions")){if(!dynamic)fail("TaskExtensionProfileMismatch","checkpoint/module_versions");domains.push_back({"task_extensions",1});}')
        s=after(s,'if(has("family_care_relief","1")!=','            if(has("task_extensions","1")!=(version("task_extensions")==1)||(!dynamic&&version("task_extensions")))return {false,{},common_failure("TaskExtensionProfileMismatch").error};')
        s=after(s,'result->tasks=std::make_shared<const TaskCatalog>', '    auto task_extensions=decode_task_extension_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!task_extensions.ok)return {false,{},task_extensions.error};if(task_extensions.value){auto decoded=decode_task_extension_catalog(field(scenario,"task_extensions","scenario"),*result->tasks,*result->interaction);if(!decoded.ok)return {false,{},decoded.error};result->task_extensions=std::make_shared<const TaskExtensionCatalog>(std::move(decoded.value));}')
        s=after(s,'if(family_care_relief_enabled(*result))id.module_versions','    if(result->task_extensions)id.module_versions.emplace_back("task_extensions","1");')
        s=after(s,'else if(source.find("task_static_assets"))','    if(catalog.task_extensions){auto decoded=decode_task_extensions(field(source,"task_extensions","state"));if(!decoded.ok)return {false,{},decoded.error};auto extended=std::make_shared<TaskState>(*result.tasks);extended->deadline_extensions=std::move(decoded.value);result.tasks=std::move(extended);}else if(source.find("task_extensions"))fail("UnexpectedTaskExtensionDomain","task_extensions");')
        s=replace(s,'||(w.catalog->task_static_assets_enabled&&w.tasks.static_assets&&header->domain=="tasks"','||(w.catalog->task_extensions&&w.tasks.deadline_extensions&&header->domain==task_extension_module&&header->kind==task_extension_fact_kind&&w.tasks.deadline_extensions->facts.find(row.first))||(w.catalog->task_static_assets_enabled&&w.tasks.static_assets&&header->domain=="tasks"')
        s=after(s,'set(output,"task_static_assets",','    if(w.catalog->task_extensions){if(!w.tasks.deadline_extensions)fail("MissingTaskExtensionDomain","task_extensions");set(output,"task_extensions",encode_task_extensions(*w.tasks.deadline_extensions));}')
        s=after(s,'r=section("family_care_relief",','    if(w.catalog->task_extensions){if(!w.tasks.deadline_extensions)return {false,{},common_failure("MissingTaskExtensionDomain").error};r=section("task_extensions",[&](auto& c){return visit_task_extensions_checkpoint(c,*w.tasks.deadline_extensions);});if(!r.ok)return {false,{},r.error};}')
        return after(s,'else if(section.domain=="task_static_assets")','        else if(section.domain=="task_extensions"){tasks->deadline_extensions.emplace();r=visit_task_extensions_checkpoint(reader,*tasks->deadline_extensions);}')
    if path.endswith('/world_bundle.cpp'):
        s=INCLUDE+s
        s=after(s,'if(w.tasks.static_assets)for(const auto& row:','    if(w.tasks.deadline_extensions)for(const auto& row:w.tasks.deadline_extensions->facts.rows()){auto r=fact(row.first,task_extension_fact_kind,task_extension_module);if(!r.ok)return r;}')
        return after(s,'{auto layout=validate_task_static_asset_layout(common,','    valid=validate_task_extension_world(*this);if(!valid.ok)return valid;')
    if path.endswith('/common_codec.cpp'):
        s=replace(s,'std::string domain=kind=="visit_reception_claimed"','if(kind=="task_deadline_extended")fields(row.second,{"kind"},{"domain"},row.first);std::string domain=kind=="task_deadline_extended"?"task_extensions":kind=="visit_reception_claimed"')
        s=replace(s,'if(kind=="visit_reception_claimed"&&named!=','if(kind=="task_deadline_extended"&&named!="task_extensions")fail("InvalidFactDomain",row.first);if(kind=="visit_reception_claimed"&&named!=')
        return replace(s,'if(!(named=="visit_reception_claims"','if(!(named=="task_extensions"&&kind=="task_deadline_extended")&&!(named=="visit_reception_claims"')
    if path.endswith('/live_view_queries.cpp'):
        return after(INCLUDE+s,'if(query=="visit_messages"){','    if(query=="task_extensions"){boundary::fields(args,{},{"cursor"},"args");return query_task_extension_world(world,args.find("cursor")?boundary::get_int(args,"cursor","args"):0);}')
    if path.endswith('/native_resident_backend.gd'):
        return after(s,'expected["module_versions"].append({"name":"family_care_relief"','\tif definition.has("task_extensions"): expected["module_versions"].append({"name":"task_extensions","version":"1"})')
    if path.endswith('/resident_page_requests.gd'):
        s=after(s,'const TaskStaticAssetPanel=','const TaskExtensionPanel=preload("res://ui/task_extension_panel.gd")')
        return after(s,'if not TaskStaticAssetPanel.prepare(host):','\tif not TaskExtensionPanel.prepare(host): return false')
    if path.endswith('/screen_composer.gd'):
        s=after(s,'const TaskStaticAssetPanel=','const TaskExtensionPanel=preload("res://ui/task_extension_panel.gd")')
        lines=[x for x in s.splitlines() if 'TaskStaticAssetPanel.visible_here(m)' in x]
        if len(lines)!=2:raise ValueError('two original sidebar locations')
        for line in lines:s=replace(s,'\n'+line+'\n','\n'+line+'\n'+line[:len(line)-len(line.lstrip())]+'if m._resident_mode and TaskExtensionPanel.visible_here(m): TaskExtensionPanel.build(m,_sidebar_section(m,stage,"TaskExtensionArea","本人私人委托延期"))\n')
        return s
    if path.endswith('/content.schema.json'):
        d=json.loads(s);p=d['$defs']['scenario']['properties'];assert 'task_extensions' not in p
        fields={'id':{'type':'string','minLength':1,'maxLength':256},'offer_id':{'$ref':'#/$defs/reference_offers'},'dialogue_id':{'$ref':'#/$defs/reference_dialogues'},'node_id':{'type':'string','minLength':1,'maxLength':256},'additional_ticks':{'type':'integer','minimum':1,'maximum':720}}
        p['task_extensions']={'type':'object','additionalProperties':False,'required':['version','profile','policies'],'properties':{'version':{'type':'integer','enum':[1]},'profile':{'type':'string','enum':[PROFILE]},'policies':{'type':'array','minItems':1,'maxItems':4096,'items':{'type':'object','additionalProperties':False,'required':list(fields),'properties':fields}}}}
        d['properties']['required_capabilities']['items']['enum'].append(CAP);return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        d=json.loads(s);assert not any(x['capability_id']==CAP for x in d['capabilities']);d['capabilities'].append({'capability_id':CAP,'rule_version':PROFILE,'module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.task_extensions.profile','equals':PROFILE}],'supported_semantics':[],'test_ids':['test_task_extension_panel_guard'],'evidence':[]});return json.dumps(d,ensure_ascii=False,indent=2)+'\n'
    if path=='tools/content/links.py':
        s=after(s,'from .task_static_asset_links import','    from .task_extension_links import check_task_extensions')
        return replace(s,' + check_task_static_assets(pack,index)',' + check_task_static_assets(pack,index) + check_task_extensions(pack,index)')
    if path=='tools/verify.py':return replace(s,"'test_task_static_asset_panel_guard',","'test_task_static_asset_panel_guard', 'test_task_extension_panel_guard',")
    raise ValueError(path)


def cmake_candidate(repo, base, output):
    """独立追加真实CMake单叶；清单声明不能代实际编译源/身份头/test目标登记。"""
    import difflib
    path='native/world/resident.cmake'
    before=subprocess.check_output(['git','-C',str(repo),'show',base+':'+path]);source=before.decode()
    cpp=['src/resident/domains/tasks/extensions/task_extension_'+name+'.cpp' for name in ['catalog','codec','rules','validation','boundary','view','world']]
    headers=['src/resident/domains/tasks/extensions/task_extension'+name+'.hpp' for name in ['', '_state','_boundary','_view','_world']]
    anchor='    src/resident/domains/tasks/static_assets/task_static_asset_view.cpp'
    if source.count(anchor)!=2:raise ValueError('expected production and identity CPP lists')
    source=source.replace(anchor,anchor+'\n'+'\n'.join('    '+x for x in cpp))
    anchor='    src/resident/domains/tasks/static_assets/task_static_asset_view.hpp'
    source=replace(source,anchor,anchor+'\n'+'\n'.join('    '+x for x in headers))
    source+='\n# 私人一次延期沿原真实受领/Talk/交付，坏档与钱货不变门；运行按风险另排。\nadd_executable(test_resident_task_extension tests/resident/task_extension_test.cpp)\ntarget_link_libraries(test_resident_task_extension PRIVATE hanmo_resident)\ntarget_include_directories(test_resident_task_extension PRIVATE src)\ntarget_compile_options(test_resident_task_extension PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)\nadd_test(NAME resident_task_extension COMMAND test_resident_task_extension)\n'
    for item in cpp:
        if source.count(item)!=2:raise ValueError('wrong CPP registration count '+item)
    for item in headers:
        if source.count(item)!=1:raise ValueError('wrong header registration count '+item)
    data=source.encode();target=output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    (output/'candidate.patch').write_text(''.join(difflib.unified_diff(before.decode().splitlines(True),source.splitlines(True),fromfile='a/'+path,tofile='b/'+path)))
    (output/'manifest.json').write_text(json.dumps(dict(base=base,status='SOURCE_ONLY_CMAKE_NOT_CONFIGURED',shared=[dict(path=path,base_oid=oid(before),result_oid=oid(data))],cpp_occurrences=2,header_occurrences=1,test_target='test_resident_task_extension',test_name='resident_task_extension',external_publication='central/root only; original public-file restriction retained'),ensure_ascii=False,indent=2)+'\n')


def main():
    """锚点全部闭合才落候选，记录真实基准OID与结果OID；不执行构建或覆原中央。"""
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--base',default=BASE);p.add_argument('--output',type=Path,required=True);p.add_argument('--cmake-only',action='store_true');a=p.parse_args()
    if a.output.exists():raise ValueError('fresh output required')
    if a.cmake_only:
        cmake_candidate(a.repo,a.base,a.output);return
    rows=[];result={};oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
    for path in SHARED:
        before=subprocess.check_output(['git','-C',str(a.repo),'show',a.base+':'+path]);data=transform(path,before.decode()).encode();result[path]=data;rows.append(dict(path=path,base_oid=oid(before),result_oid=oid(data)))
    for path,data in result.items():target=a.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    (a.output/'manifest.json').write_text(json.dumps(dict(base=a.base,shared=rows,status='SOURCE_ONLY_PENDING_SYNTAX',module='task_extensions/1',profile=PROFILE,capability=CAP,old_visitors_unchanged=['Task1','Task2','Interaction','ReceiptData'],build_cpp=[ROOT+'task_extension_'+x+'.cpp' for x in ['catalog','codec','rules','validation','boundary','view','world']],build_headers=[ROOT+'task_extension'+x+'.hpp' for x in ['','_state','_boundary','_view','_world']],tests=['native/world/tests/resident/task_extension_test.cpp','game/tests/test_task_extension_panel_guard.gd'],cold_order='after complete original Task and payment/assets/message history; before family inheritance and private shop eligibility'),ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':main()
