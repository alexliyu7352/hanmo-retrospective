#!/usr/bin/env python3
"""沿固定78生成安民续办显式尾与真实提交接线；只写隔离候选，中央三方合流。"""
import argparse,difflib,hashlib,json,subprocess
from pathlib import Path
BASE='78f88ce96217a26cb71ef9380f56327a464a6ad1'
MODULE='political_governance_continuations'
HEADER='resident/domains/politics/governance_continuation/political_governance_continuation.hpp'
DECL='resident/domains/politics/governance_continuation/political_governance_continuation_declarations.hpp'
WORLD='political_governance_continuation_world.hpp'

def candidate(repo,output):
    """每处旧锚唯一，保原尾序与旧字段；缺锚即停，不猜中央新正文。"""
    if output.exists():raise ValueError('fresh output required')
    rows=[];patch=[]
    def change(path,fn):
        before=subprocess.check_output(['git','-C',str(repo),'show',BASE+':'+path],text=True);text=before
        def one(old,new):
            nonlocal text
            if text.count(old)!=1:raise ValueError(f'{path}: expected one {old[:130]!r}, got {text.count(old)}')
            text=text.replace(old,new)
        def line(token,new):
            matches=[v for v in text.splitlines(True) if token in v]
            if len(matches)!=1:raise ValueError(f'{path}: line {token!r}: {len(matches)}')
            one(matches[0],matches[0]+new+'\n')
        fn(one,line)
        p=output/'candidate'/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
        oid=lambda s:subprocess.check_output(['git','hash-object','--stdin'],input=s.encode()).decode().strip()
        rows.append({'path':path,'before_oid':oid(before),'after_oid':oid(text),'sha256':hashlib.sha256(text.encode()).hexdigest()})
        patch.extend(difflib.unified_diff(before.splitlines(True),text.splitlines(True),fromfile='a/'+path,tofile='b/'+path))
    def model(one,line):
        one('struct PoliticsGovernanceState;struct PoliticsGovernanceCatalog;','struct PoliticsGovernanceState;struct PoliticsGovernanceCatalog;struct PoliticalGovernanceContinuationState;')
        line('bool political_shop_licensing_receipts=false;','    bool political_governance_continuations=false;')
    change('native/world/include/hanmo/resident/model_contract.hpp',model)
    change('native/world/src/resident/model/common_model.hpp',lambda one,line:line('std::shared_ptr<const PoliticalShopLicensingReceiptState> political_shop_licensing_receipts;','    std::shared_ptr<const PoliticalGovernanceContinuationState> political_governance_continuations;'))
    def bundle_header(one,line):
        one('#pragma once','#pragma once\n#include "'+HEADER+'"')
        line('std::optional<PoliticalShopLicensingReceiptState> political_shop_licensing_receipts;','    std::optional<PoliticalGovernanceContinuationState> political_governance_continuations;')
        one('const StableId& licensing_source_id={});','const StableId& licensing_source_id={},const StableId& governance_continuation_id={});')
    change('native/world/src/resident/kernel/world_bundle.hpp',bundle_header)
    def bundle(one,line):
        one('#include "political_shop_registry_director_integration.hpp"','#include "political_shop_registry_director_integration.hpp"\n#include "'+WORLD+'"\n#include "'+DECL+'"')
        one('political_shop_licensing_receipts(seed.political_shop_licensing_receipts?std::optional<PoliticalShopLicensingReceiptState>{*seed.political_shop_licensing_receipts}:std::nullopt),','political_shop_licensing_receipts(seed.political_shop_licensing_receipts?std::optional<PoliticalShopLicensingReceiptState>{*seed.political_shop_licensing_receipts}:std::nullopt),political_governance_continuations(seed.political_governance_continuations?std::optional<PoliticalGovernanceContinuationState>{*seed.political_governance_continuations}:std::nullopt),')
        one('political_shop_licensing_receipts(other.political_shop_licensing_receipts),','political_shop_licensing_receipts(other.political_shop_licensing_receipts),political_governance_continuations(other.political_governance_continuations),')
        line('for(const auto& row:w.political_shop_registry_successions.acceptances.rows())','    if(w.political_governance_continuations)for(const auto& row:w.political_governance_continuations->continuations.rows()){const auto* h=w.common.state().facts.find(row.first);if(!w.catalog->political_governance_continuations||!h)return common_failure("GovernanceContinuationFactMismatch",row.first);auto checked=validate_political_governance_continuation_fact_header(w.common,*w.political_governance_continuations,*h);if(!checked.ok)return checked;checked=fact(row.first,political_governance_continue_kind,political_governance_continuation_module);if(!checked.ok)return checked;}')
        one('||(economic_supply_policies_enabled()&&political_supply_grant_declares_resource','||(catalog->political_governance_continuations&&governance_continuation_grant_declares_resource(*catalog->common,*catalog->organization,*catalog->politics,*catalog->military,*catalog->political_takeover,true,grant.id,resource))||(economic_supply_policies_enabled()&&political_supply_grant_declares_resource')
        line('if(private_shops_enabled()&&takeover_access){','    // 原军事、治理工作与付款全史先验后，再核精确续办及永久玩家结果。\n    valid=validate_political_governance_continuation_world(*this);if(!valid.ok)return valid;')
    change('native/world/src/resident/kernel/world_bundle.cpp',bundle)
    def types(one,line):
        one('#pragma once','#pragma once\n#include "'+WORLD+'"')
        one('PoliticalAcceptShopRegistry,PersonalAppointmentCommand>;','PoliticalAcceptShopRegistry,PersonalAppointmentCommand,PoliticalContinueGovernance>;')
        one('PoliticalShopRegistryAcceptanceQuote,PersonalAppointmentQuote>;','PoliticalShopRegistryAcceptanceQuote,PersonalAppointmentQuote,PoliticalGovernanceContinuationQuote>;')
    change('native/world/src/resident/kernel/command_types.hpp',types)
    def dispatcher(one,line):
        line('if(kind==political_shop_registry_accept_command&&!w.political_shop_registry_successions_enabled())','    if(kind==political_governance_continue_command&&!w.catalog->political_governance_continuations)return common_failure("GovernanceContinuationProfileRequired");')
        line('const auto* shop_registry=std::get_if<PoliticalAcceptShopRegistry>(&command.data);','    const auto* governance_continuation=std::get_if<PoliticalContinueGovernance>(&command.data);if((command.kind==political_governance_continue_command)!=(governance_continuation!=nullptr))return reject("InvalidGovernanceContinuationCommand");')
        line('if(shop_registry){auto q=prepare_political_shop_registry_world','    if(governance_continuation){auto q=prepare_political_governance_continuation_world(w,id,*governance_continuation);if(!q.ok)return {false,{},q.error};return ready(w,id,command,std::move(q.value));}')
        line('if(const auto* c=std::get_if<PoliticalAcceptShopRegistry>(&op.command.data))','    if(const auto* c=std::get_if<PoliticalContinueGovernance>(&op.command.data)){if(op.command.kind!=political_governance_continue_command||!std::holds_alternative<PoliticalGovernanceContinuationQuote>(op.quote.data))return failed<ReceiptDetails>(common_failure("InvalidGovernanceContinuationCommand"));}else if(op.command.kind==political_governance_continue_command)return failed<ReceiptDetails>(common_failure("InvalidGovernanceContinuationCommand"));')
        line('if(const auto* c=std::get_if<PoliticalAcceptShopRegistry>(&command.data))','    if(const auto* c=std::get_if<PoliticalContinueGovernance>(&command.data)){auto r=commit_political_governance_continuation_world(w,id,*c,std::get<PoliticalGovernanceContinuationQuote>(op.quote.data));if(!r.ok)return {false,{},r.error};return {true,ReceiptDetails{Done{},{}},{}};}')
    change('native/world/src/resident/kernel/dispatcher.cpp',dispatcher)
    def command(one,line):
        line('if(kind==political_shop_registry_accept_command){boundary::fields(value','    if(kind==political_governance_continue_command){boundary::fields(value,{},{},"receipt/details");result.data=Done{};return result;}')
        one('Integer tag=x.data.index();c.field("variant",tag);','Integer tag=x.data.index();c.field("variant",tag);if(kind==political_governance_continue_command&&(!(c.schema_features&political_governance_continuation_feature)||tag!=0))return c.reject("InvalidGovernanceContinuationReceipt");')
        line('if(r.kind==political_shop_registry_accept_command&&','        if(r.kind==political_governance_continue_command&&(!r.details||!std::holds_alternative<Done>(r.details->data)))failed("InvalidGovernanceContinuationReceipt","receipt/details",ErrorKind::rule);')
        line('if(value.kind==political_shop_registry_accept_command&&!(c.schema_features','    if(value.kind==political_governance_continue_command&&!(c.schema_features&political_governance_continuation_feature))return c.reject("GovernanceContinuationProfileRequired");')
        one('c.field("has_details",has_details);','c.field("has_details",has_details);if(value.kind==political_governance_continue_command&&!has_details)return c.reject("InvalidGovernanceContinuationReceipt");')
        line('if(kind==political_shop_registry_accept_command){auto decoded=','    if(kind==political_governance_continue_command){auto decoded=decode_political_continue_governance_command(params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}')
        line('else if constexpr(std::is_same_v<T,PoliticalShopRegistryAcceptanceQuote>)','                else if constexpr(std::is_same_v<T,PoliticalGovernanceContinuationQuote>)return boundary::obj({{"ok",V{true}},{"duration",V{Integer{0}}},{"political_governance_continuations",encode_political_governance_continuation_quote(quote)}});')
    change('native/world/src/resident/checkpoint/command_boundary.cpp',command)
    def resident(one,line):
        one('auto appointment_sources=personal_appointment_new_result_world(', 'StableId governance_continuation_id;if(committed_kind==political_governance_continue_command){if(!current.political_governance_continuations||!candidate->political_governance_continuations)return {false,{},common_failure("GovernanceContinuationProfileRequired").error};auto source=political_governance_continuation_new_player_source(*current.political_governance_continuations,*candidate->political_governance_continuations,command.actor_id,candidate->common.tick());if(!source.ok)return {false,{},source.error};governance_continuation_id=std::move(source.value);}auto appointment_sources=personal_appointment_new_result_world(')
        one('nullptr,licensing_source_id);','nullptr,licensing_source_id,governance_continuation_id);')
        one('const StableId& licensing_source_id){','const StableId& licensing_source_id,const StableId& governance_continuation_id){')
        line('if(!licensing_source_id.empty()||','    // 本次唯一新续办在真实Common结果已分配并入缓存后绑定，裁剪后不猜更早同tick来源。\n    if(!governance_continuation_id.empty()||kind==political_governance_continue_command){if(kind!=political_governance_continue_command||!w.catalog->political_governance_continuations||!w.political_governance_continuations||governance_continuation_id.empty())return common_failure("GovernanceContinuationNewPlayerSourceRequired");auto bound=bind_political_governance_continuation_result(w.common,*w.political_governance_continuations,governance_continuation_id,actor,sequence,*receipt,[](const CommitResult& r){return r.details&&std::holds_alternative<Done>(r.details->data);});if(!bound.ok)return bound;}')
    change('native/world/src/resident/kernel/resident_world.cpp',resident)
    def governance(one,line):
        one('#include "governance_task_integration.hpp"','#include "governance_task_integration.hpp"\n#include "'+WORLD+'"')
        # 两个原调用各持同一个takeover，在其生命周期内同步包一层真实新续办权源。
        old='    return step_governance_task_action(w.common,w.tasks,w.governance_tasks,authority,action_id,phase.to_tick,freeze,release,takeover.value->tasks());'
        new='    std::unique_ptr<PoliticalGovernanceContinuationWorldAccess> continuation;const GovernanceAuthorityRead* effective=&authority;\n    if(w.catalog->political_governance_continuations){if(!takeover.value->responsibilities())return {false,{},common_failure("GovernanceContinuationTakeoverUnavailable").error};auto read=PoliticalGovernanceContinuationWorldAccess::create(w,*takeover.value->responsibilities(),&phase);if(!read.ok)return {false,{},read.error};continuation=std::move(read.value);effective=&continuation->authority();}\n    return step_governance_task_action(w.common,w.tasks,w.governance_tasks,*effective,action_id,phase.to_tick,freeze,release,takeover.value->tasks());'
        one(old,new)
        line('PublicReliefRead receipts(w.common,w.public_relief);','    std::unique_ptr<PoliticalGovernanceContinuationWorldAccess> continuation;const GovernanceAuthorityRead* effective=&authority;\n    if(w.catalog->political_governance_continuations){if(!takeover.value->responsibilities())return common_failure("GovernanceContinuationTakeoverUnavailable");auto read=PoliticalGovernanceContinuationWorldAccess::create(w,*takeover.value->responsibilities());if(!read.ok)return {false,{},read.error};continuation=std::move(read.value);effective=&continuation->authority();}')
        one('w.public_relief,tasks,authority,source.acceptance.task_id,','w.public_relief,tasks,*effective,source.acceptance.task_id,')
    change('native/world/src/resident/kernel/governance_task_integration.cpp',governance)
    def codec(one,line):
        one('#include "resident/domains/politics/licensing/political_shop_registry_profile.hpp"','#include "resident/domains/politics/licensing/political_shop_registry_profile.hpp"\n#include "'+HEADER+'"')
        one('if(catalog.political_shop_licensing_receipts)domains.push_back({"political_shop_licensing_receipts",1});return domains;','if(catalog.political_shop_licensing_receipts)domains.push_back({"political_shop_licensing_receipts",1});if(catalog.political_governance_continuations)domains.push_back({"political_governance_continuations",1});return domains;')
        one('{"political_shop_licensing_receipts",{"1"}}','{"political_shop_licensing_receipts",{"1"}}, {"political_governance_continuations",{"1"}}')
        line('if(has("political_shop_licensing_receipts")){','        auto continued_layout=validate_political_governance_continuation_modules(identity.catalog.module_versions,has("political_governance_continuations"));if(!continued_layout.ok)return {false,{},continued_layout.error};if(has("political_governance_continuations")){if(!dynamic)fail("GovernanceContinuationProfileRequired","checkpoint/module_versions");domains.push_back({"political_governance_continuations",1});}')
        line('auto legal_receipts=decode_political_shop_licensing_receipt_profile(scenario,caps);','        auto continued=decode_political_governance_continuation_profile(scenario,caps);if(!continued.ok)return {false,{},continued.error};result->political_governance_continuations=continued.value;')
        line('if(result->political_shop_licensing_receipts)id.module_versions','    if(result->political_governance_continuations)id.module_versions.emplace_back("political_governance_continuations","1");')
        line('if(catalog.political_shop_licensing_receipts){auto parsed=','    if(catalog.political_governance_continuations){auto parsed=decode_political_governance_continuations(field(source,"political_governance_continuations","state"));if(!parsed.ok)return {false,{},parsed.error};result.political_governance_continuations=std::make_shared<const PoliticalGovernanceContinuationState>(std::move(parsed.value));}else if(source.find("political_governance_continuations"))fail("UnexpectedGovernanceContinuations","political_governance_continuations");')
        line('CommonStore legal_common(result.common,catalog.common);','        std::set<StableId> continued_ids;for(const auto& row:object(field(source,"facts","state"),"facts")){const auto kind=get_string(row.second,"kind",row.first);const auto* named=row.second.find("domain");if(kind!=political_governance_continue_kind&&(!named||string(*named,row.first)!=political_governance_continuation_module))continue;if(!catalog.political_governance_continuations||!result.political_governance_continuations)fail("GovernanceContinuationProfileRequired",row.first);fields(row.second,{"kind","domain"},{},row.first);if(!continued_ids.insert(row.first).second)fail("GovernanceContinuationFactMismatch",row.first);const auto* h=legal_common.state().facts.find(row.first);if(!h)fail("GovernanceContinuationFactMismatch",row.first);auto checked=validate_political_governance_continuation_fact_header(legal_common,*result.political_governance_continuations,*h);if(!checked.ok)return {false,{},checked.error};}if(result.political_governance_continuations&&continued_ids.size()!=result.political_governance_continuations->continuations.size())fail("GovernanceContinuationFactMismatch","political_governance_continuations");')
        line('if(legal_header&&(legal_header->kind==political_shop_registry_accept_kind','        if(legal_header&&(legal_header->kind==political_governance_continue_kind||legal_header->domain==political_governance_continuation_module)){if(!w.catalog->political_governance_continuations||!w.political_governance_continuations||payload)fail("GovernanceContinuationFactMismatch",row.first);auto checked=validate_political_governance_continuation_fact_header(w.common,*w.political_governance_continuations,*legal_header);if(!checked.ok)return {false,{},checked.error};set(row.second,"domain",BoundaryValue{legal_header->domain});continue;}')
        line('if(w.catalog->political_shop_licensing_receipts){if(!w.political_shop_licensing_receipts)fail(','    if(w.catalog->political_governance_continuations){if(!w.political_governance_continuations)fail("GovernanceContinuationProfileRequired","state");set(output,"political_governance_continuations",encode_political_governance_continuations(*w.political_governance_continuations));}')
        one('CodecVisitor writer(s.payload);','CodecVisitor writer(s.payload);if(w.catalog->political_governance_continuations)writer.schema_features|=political_governance_continuation_feature;')
        line('if(w.catalog->political_shop_licensing_receipts){if(!w.political_shop_licensing_receipts)return','    if(w.catalog->political_governance_continuations){if(!w.political_governance_continuations)return {false,{},common_failure("GovernanceContinuationProfileRequired").error};r=section("political_governance_continuations",[&](auto& c){return visit_political_governance_continuations_checkpoint(c,*w.political_governance_continuations);});if(!r.ok)return {false,{},r.error};}')
        one('std::uint32_t political_features=0;','std::uint32_t political_features=0;{const auto& modules=decoded.value.identity.catalog.module_versions;const bool enabled=std::find(modules.begin(),modules.end(),std::make_pair(StableId{political_governance_continuation_module},StableId{"1"}))!=modules.end();auto checked=validate_political_governance_continuation_modules(modules,enabled);if(!checked.ok)return {false,{},checked.error};if(enabled)political_features|=political_governance_continuation_feature;}')
        one('CodecVisitor reader(section.payload);if(catalog.private_shops','CodecVisitor reader(section.payload);if(catalog.political_governance_continuations)reader.schema_features|=political_governance_continuation_feature;if(catalog.private_shops')
        line('else if(section.domain=="political_shop_licensing_receipts")','        else if(section.domain=="political_governance_continuations"){auto state=std::make_shared<PoliticalGovernanceContinuationState>();r=visit_political_governance_continuations_checkpoint(reader,*state);seed.political_governance_continuations=std::move(state);}')
    change('native/world/src/resident/kernel/world_codec.cpp',codec)
    def initial(one,line):
        one('#include "resident/domains/politics/licensing/political_shop_licensing_result_bindings.hpp"','#include "resident/domains/politics/licensing/political_shop_licensing_result_bindings.hpp"\n#include "'+HEADER+'"')
        line('auto legal_receipts=decode_political_shop_licensing_receipt_profile(scenario,','    auto continued=decode_political_governance_continuation_profile(scenario,strings(field(field(catalog,"data","catalog"),"required_capabilities","data"),"required_capabilities"));if(!continued.ok)return {false,{},continued.error};if(continued.value)admitted.module_versions.emplace_back("political_governance_continuations","1");')
        line('if(mutable_catalog->public_relief)seed.public_relief=','    if(mutable_catalog->political_governance_continuations)seed.political_governance_continuations=std::make_shared<const PoliticalGovernanceContinuationState>();')
    change('native/world/src/resident/kernel/native_initial_state.cpp',initial)
    def query(one,line):
        one('#include "live_view_queries.hpp"','#include "live_view_queries.hpp"\n#include "'+WORLD+'"')
        line('if(query=="political_shop_registry_successions")','    if(query=="political_governance_continuations"){auto unique=validate_economic_payload_fields(args);if(!unique.ok)return {false,{},unique.error};boundary::fields(args,{"place_id","cursor"},{},"args");return query_political_governance_continuation_world(world,boundary::get_string(args,"place_id","args"),boundary::get_int(args,"cursor","args"));}')
    change('native/world/src/resident/kernel/live_view_queries.cpp',query)
    def backend(one,line):
        line('if definition.get("dynamic_world",{}).has("political_shop_licensing_receipts"):','\tif definition.get("dynamic_world",{}).has("political_governance_continuations"): expected["module_versions"].append({"name":"political_governance_continuations","version":"1"})')
        line('func political_shop_registry_successions_enabled()','## 原生完整profile控制有限本地续办查询。\nfunc political_governance_continuations_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_governance_continuations")\nfunc political_governance_continuations(place_id: String, cursor: int=0) -> Dictionary:\n\treturn demand_view("political_governance_continuations",{"place_id":place_id,"cursor":cursor})')
    change('game/ui/native_resident_backend.gd',backend)
    def requests(one,line):
        line('const PoliticalShopRegistryPanel=preload(','const GovernanceContinuationPanel=preload("res://ui/political_governance_continuation_panel.gd")')
        line('if not PoliticalShopRegistryPanel.prepare(host,','\tif not str(location.get("place_id","")).is_empty():\n\t\tif not GovernanceContinuationPanel.prepare(host,str(location.get("place_id",""))): return false')
    change('game/ui/resident_page_requests.gd',requests)
    def screen(one,line):
        line('const PoliticalShopRegistryPanel=preload(','const GovernanceContinuationPanel=preload("res://ui/political_governance_continuation_panel.gd")')
        line('PoliticalShopRegistryPanel.render(m,','\tif not current_place.is_empty() and kernel.has_method("political_governance_continuations_enabled") and kernel.political_governance_continuations_enabled():\n\t\tGovernanceContinuationPanel.render(m,_sidebar_section(m,stage,"GovernanceContinuationArea","安民责任续办"),kernel.political_governance_continuations(current_place,int(m.get_meta("governance_continuation_cursor",0))),current_place)')
    change('game/ui/screen_composer.gd',screen)
    def links(one,line):
        line('from .political_shop_licensing_result_links import','    from .political_governance_continuation_links import check_political_governance_continuations')
        one('check_political_shop_licensing_receipts(pack,index)','check_political_shop_licensing_receipts(pack,index) + check_political_governance_continuations(pack,index)')
    change('tools/content/links.py',links)
    def grant(one,line):
        line('from .political_takeover_links import political_takeover_grant_declares_assets','from .political_governance_continuation_links import governance_continuation_grant_declares_assets')
        one('not political_supply_grant_declares_assets(scenario,index,grant) and','not political_supply_grant_declares_assets(scenario,index,grant) and not governance_continuation_grant_declares_assets(scenario,index,grant) and')
        line("allowed.add('political_issue_supply_policy')","            continued = dynamic.get('political_governance_continuations', {})\n            if type(continued.get('version')) is int and continued['version'] == 1:\n                allowed.add('political_continue_governance')")
    change('tools/content/dynamic_world_links.py',grant)
    change('tools/verify.py',lambda one,line:one('TESTS = [',"TESTS = ['test_political_governance_continuation_panel', "))
    for path in ['schemas/content.schema.json','game/data/rule_capabilities.json']:
        before=subprocess.check_output(['git','-C',str(repo),'show',BASE+':'+path],text=True);value=json.loads(before)
        if path.startswith('schemas'):
            from content.political_governance_continuation_links import with_governance_continuation_schema
            value=with_governance_continuation_schema(value)
        else:value['capabilities'].append({'capability_id':'political_governance_continuations_v1','rule_version':'political_governance_continuations_v1','module_id':'native_dynamic_profile','support_status':'implemented/unverified','schema_selectors':[{'path':'scenarios.*.dynamic_world.political_governance_continuations.version','equals':1}],'supported_semantics':[],'test_ids':['test_political_governance_continuation_panel'],'evidence':[]})
        after=json.dumps(value,ensure_ascii=False,indent=2)+'\n';change(path,lambda one,line,b=before,a=after:one(b,a))
    (output/'candidate.patch').write_text(''.join(patch));(output/'manifest.json').write_text(json.dumps({'base':BASE,'status':'SOURCE_ONLY_UNREGISTERED','shared':rows,'cmake':'central sole writer registers domain/kernel headers and CPP, native test, author and finite Godot panel test; no denial','profile':MODULE+'/1; exact Done feature16777216; optional state and final tail; no NPC issuance producer','runtime':'NOT_RUN'},indent=2)+'\n')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();candidate(a.repo,a.output)
