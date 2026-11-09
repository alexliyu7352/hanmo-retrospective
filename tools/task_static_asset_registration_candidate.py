"""固定d3公共基准生成静态货物实交独立段；仅写隔离候选，中央独占共享实际修改。"""
import argparse, hashlib, json, subprocess
from pathlib import Path
BASE='d3f007c0409f0baee5a18f7f75ebc2ef870860d8'
INCLUDE='#include "resident/domains/tasks/static_assets/task_static_asset.hpp"\n'
PROFILE='original_task_assets_v1'
CAP='task_static_assets_v1'


def replace(source, old, new):
    """每个公共锚必须唯一且存在，拒绝把旧整叶盖上并行修改。"""
    if source.count(old)!=1:raise ValueError('nonunique anchor '+old[:160]+' count='+str(source.count(old)))
    return source.replace(old,new)


def after_line(source, anchor, extra):
    """只在完整具名语句后追加，模块marker与冷尾不共享错误的相似锚。"""
    lines=[line for line in source.splitlines() if anchor in line]
    if len(lines)!=1:raise ValueError('line '+anchor+' count='+str(len(lines)))
    return replace(source,lines[0],lines[0]+'\n'+extra)


def transform(path, source):
    """新段不改旧Task2 visitor；原资源成功后同候选附凭据，并保原成功/取消状态机。"""
    if path.endswith('/task_model.hpp'):
        source=replace(source,'#pragma once','#pragma once\n#include "static_assets/task_static_asset_state.hpp"')
        return replace(source,'std::optional<TaskMessageNoticeState> message_notices;', 'std::optional<TaskMessageNoticeState> message_notices;std::optional<TaskStaticAssetState> static_assets;')
    if path.endswith('/model_contract.hpp'):
        return replace(source,'bool task_message_handoffs_enabled=false;','bool task_static_assets_enabled=false;\n    bool task_message_handoffs_enabled=false;')
    if path.endswith('/task_rules.cpp'):
        source=INCLUDE+source
        source=replace(source,'    std::optional<TaskStaticPaymentPreparation> static_payment;', '    std::optional<TaskStaticAssetPreparation> static_assets;\n    if(s.static_assets&&action.kind=="report_offer"&&d->template_id=="procurement"){if(!ic.common)return {false,{},common_failure("MissingInteractionCatalog").error};const DynamicStepInput asset_phase{c.tick(),finish_tick};auto prepared=prepare_task_static_asset_transfer(c,s,catalog,*ic.common,*s.static_assets,t->id,"procurement_delivery",&action,finish_tick,&asset_phase);if(!prepared.ok)return {false,{},prepared.error};static_assets=std::move(prepared.value); }\n    std::optional<TaskStaticPaymentPreparation> static_payment;')
        source=after_line(source,'if(static_payment){auto recorded=record_task_static_payment_after_resources(c,*s.takeover_execution,*static_payment);if(!recorded.ok)return {false,{},recorded.error};}', '    if(static_assets){auto recorded=record_task_static_asset_transfer_after_resources(c,*s.static_assets,*static_assets);if(!recorded.ok)return {false,{},recorded.error};}')
        source=replace(source,'auto amount=c.wallet(t->budget_wallet_id)->balance;ResourceRecipe recipe;', 'std::optional<TaskStaticAssetPreparation> static_assets;if(s.static_assets){auto prepared=prepare_task_static_asset_transfer(c,s,catalog,common,*s.static_assets,id,"cancellation_return",nullptr,c.tick(),execution?execution->phase:nullptr);if(!prepared.ok)return {false,{},prepared.error};static_assets=std::move(prepared.value);}auto amount=c.wallet(t->budget_wallet_id)->balance;ResourceRecipe recipe;')
        return replace(source,'t->status=c.tick()>t->deadline_tick?"failed":"cancelled";', 'if(static_assets){auto recorded=record_task_static_asset_transfer_after_resources(c,*s.static_assets,*static_assets);if(!recorded.ok)return recorded;}t->status=c.tick()>t->deadline_tick?"failed":"cancelled";')
    if path.endswith('/world_codec.cpp'):
        source=INCLUDE+source
        source=replace(source,'if(family_marriage_enabled(catalog))domains.push_back({"family_marriage",1});return domains;', 'if(family_marriage_enabled(catalog))domains.push_back({"family_marriage",1});if(catalog.task_static_assets_enabled)domains.push_back({"task_static_assets",1});return domains;')
        source=replace(source,'{"task_message_notices",{"1"}},','{"task_message_notices",{"1"}}, {"task_static_assets",{"1"}},')
        source=after_line(source,'if(has("family_marriage")){if(!dynamic', '        if(has("task_static_assets")){if(!dynamic)fail("TaskStaticAssetProfileMismatch","checkpoint/module_versions");domains.push_back({"task_static_assets",1});}')
        source=after_line(source,'if(has("task_message_notices","1")!=', '            if(has("task_static_assets","1")!=(version("task_static_assets")==1)||(!dynamic&&version("task_static_assets")))return {false,{},common_failure("TaskStaticAssetProfileMismatch").error};')
        source=after_line(source,'result->task_message_handoff_version=handoffs.value;', '    auto static_assets=decode_task_static_asset_profile(scenario,strings(field(data,"required_capabilities","data"),"required_capabilities"));if(!static_assets.ok)return {false,{},static_assets.error};result->task_static_assets_enabled=static_assets.value;')
        source=after_line(source,'if(family_marriage_enabled(*result))id.module_versions', '    if(result->task_static_assets_enabled)id.module_versions.emplace_back("task_static_assets","1");')
        source=after_line(source,'else if(source.find("task_message_notices"))', '    if(catalog.task_static_assets_enabled){auto decoded=decode_task_static_assets(field(source,"task_static_assets","state"));if(!decoded.ok)return {false,{},decoded.error};auto extended=std::make_shared<TaskState>(*result.tasks);extended->static_assets=std::move(decoded.value);result.tasks=std::move(extended);}else if(source.find("task_static_assets"))fail("UnexpectedTaskStaticAssetDomain","task_static_assets");')
        source=replace(source,'||(w.catalog->task_message_notices&&w.tasks.message_notices&&header->domain=="tasks"', '||(w.catalog->task_static_assets_enabled&&w.tasks.static_assets&&header->domain=="tasks"&&header->kind==task_static_asset_fact_kind&&w.tasks.static_assets->receipts.find(row.first))||(w.catalog->task_message_notices&&w.tasks.message_notices&&header->domain=="tasks"')
        source=after_line(source,'set(output,"task_message_notices",','    if(w.catalog->task_static_assets_enabled){if(!w.tasks.static_assets)fail("MissingTaskStaticAssetDomain","task_static_assets");set(output,"task_static_assets",encode_task_static_assets(*w.tasks.static_assets));}')
        source=after_line(source,'r=section("family_marriage",', '    if(w.catalog->task_static_assets_enabled){if(!w.tasks.static_assets)return {false,{},common_failure("MissingTaskStaticAssetDomain").error};r=section("task_static_assets",[&](auto& c){return visit_task_static_assets_checkpoint(c,*w.tasks.static_assets);});if(!r.ok)return {false,{},r.error};}')
        return after_line(source,'else if(section.domain=="task_message_notices")', '        else if(section.domain=="task_static_assets"){tasks->static_assets.emplace();r=visit_task_static_assets_checkpoint(reader,*tasks->static_assets);}')
    if path.endswith('/native_initial_state.cpp'):
        source=INCLUDE+source
        source=after_line(source,'admitted.module_versions.emplace_back("family_marriage","1")', '    auto static_assets=decode_task_static_asset_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!static_assets.ok)return {false,{},static_assets.error};if(static_assets.value)admitted.module_versions.emplace_back("task_static_assets","1");')
        return replace(source,'if(mutable_catalog->task_message_notices)tasks->message_notices.emplace();','if(mutable_catalog->task_message_notices)tasks->message_notices.emplace();if(mutable_catalog->task_static_assets_enabled)tasks->static_assets.emplace();')
    if path.endswith('/world_bundle.cpp'):
        source=INCLUDE+source
        source=after_line(source,'if(w.tasks.message_notices)for(const auto& row:', '    if(w.tasks.static_assets)for(const auto& row:w.tasks.static_assets->receipts.rows()){auto r=fact(row.first,task_static_asset_fact_kind,"tasks");if(!r.ok)return r;}')
        return replace(source,'    auto inheritance=family_inheritance_access();', '    {auto layout=validate_task_static_asset_layout(common,catalog->task_static_assets_enabled,catalog->identity.module_versions,tasks.static_assets?&*tasks.static_assets:nullptr);if(!layout.ok)return layout;if(tasks.static_assets){if(!dynamic_enabled())return common_failure("TaskStaticAssetProfileMismatch");valid=validate_task_static_assets(common,tasks,*catalog->tasks,*catalog->common,*tasks.static_assets);if(!valid.ok)return valid;valid=validate_task_static_asset_cash_sources(common,tasks,*tasks.static_assets);if(!valid.ok)return valid;}}\n    auto inheritance=family_inheritance_access();')
    if path.endswith('/common_codec.cpp'):
        anchor='kind=="task_death_notice_bound"||'
        if source.count(anchor)!=2:raise ValueError('Common infer/explicit pair missing')
        return source.replace(anchor,'kind=="task_static_asset_transfer"||'+anchor)
    if path.endswith('/native_resident_backend.gd'):
        return after_line(source,'expected["module_versions"].append({"name":"family_marriage"', '\tif definition.has("task_static_asset_profile"): expected["module_versions"].append({"name":"task_static_assets","version":"1"})')
    if path.endswith('/content.schema.json'):
        data=json.loads(source);properties=data['$defs']['scenario']['properties'];assert 'task_static_asset_profile' not in properties;properties['task_static_asset_profile']={'type':'string','enum':[PROFILE]};data['properties']['required_capabilities']['items']['enum'].append(CAP);return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    if path.endswith('/rule_capabilities.json'):
        data=json.loads(source);assert not any(x['capability_id']==CAP for x in data['capabilities']);data['capabilities'].append({'capability_id':CAP,'rule_version':PROFILE,'module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.task_static_asset_profile','equals':PROFILE}],'supported_semantics':[],'test_ids':['test_interaction_tasks_panel','test_interaction_tasks_prepare'],'evidence':[]});return json.dumps(data,ensure_ascii=False,indent=2)+'\n'
    if path=='tools/content/links.py':
        source=after_line(source,'from .task_message_handoff_links import','    from .task_static_asset_links import check_task_static_assets')
        return replace(source,' + check_task_message_handoffs(pack,index)',' + check_task_message_handoffs(pack,index) + check_task_static_assets(pack,index)')
    raise ValueError(path)

PATHS=['native/world/src/resident/domains/tasks/task_model.hpp','native/world/src/resident/domains/tasks/task_rules.cpp','native/world/include/hanmo/resident/model_contract.hpp','native/world/src/resident/kernel/world_codec.cpp','native/world/src/resident/kernel/native_initial_state.cpp','native/world/src/resident/kernel/world_bundle.cpp','native/world/src/resident/storage/common_codec.cpp','game/ui/native_resident_backend.gd','schemas/content.schema.json','game/data/rule_capabilities.json','tools/content/links.py']


def main():
    """所有锚点先闭合再写候选；保原GitOID，构建登记作为白名单交中央。"""
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);parser.add_argument('--base',default=BASE);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    if args.output.exists():raise ValueError('fresh output required')
    results={};rows=[]
    for path in PATHS:
        before=subprocess.check_output(['git','-C',str(args.repo),'show',args.base+':'+path]);after=transform(path,before.decode()).encode();results[path]=after;oid=lambda b:hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest();rows.append({'path':path,'base_oid':oid(before),'result_oid':oid(after)})
    for path,data in results.items():target=args.output/'candidate'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    (args.output/'manifest.json').write_text(json.dumps({'base':args.base,'shared':rows,'module':'task_static_assets/1','profile':PROFILE,'capability':CAP,'old_task_and_task2_visitor_unchanged':True,'status':'SOURCE_ONLY_PENDING_COMPILE','build_sources':['native/world/src/resident/domains/tasks/static_assets/task_static_asset_'+suffix+'.cpp' for suffix in ['rules','codec','validation','cash_sources']],'build_headers':['native/world/src/resident/domains/tasks/static_assets/'+name for name in ['task_static_asset.hpp','task_static_asset_state.hpp']]},ensure_ascii=False,indent=2))
if __name__=='__main__':main()
