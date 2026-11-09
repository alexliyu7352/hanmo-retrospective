#!/usr/bin/env python3
"""固定首店基线的接册独立候选；只写新输出，公共构建登记交中央唯一写者。"""
import argparse, copy, hashlib, json, pathlib, subprocess, sys
BASE='fe0c16b55d4437ffb9fe3cb56e7682d04348e13a'
MODULE='political_shop_registry_successions'
ROOT='native/world/src/resident/'

def generate(repo, output):
    originals={}; texts={}
    def get(path):
        if path not in texts:
            originals[path]=subprocess.check_output(['git','-C',str(repo),'show',BASE+':'+path],text=True)
            texts[path]=originals[path]
        return texts[path]
    def edit(path,old,new,count=1):
        source=get(path)
        if source.count(old)!=count: raise RuntimeError(f'{path}: expected {count} anchors, got {source.count(old)}: {old[:90]}')
        texts[path]=source.replace(old,new)
    def after(path,anchor,addition): edit(path,anchor,anchor+addition)
    p='native/world/include/hanmo/resident/model_contract.hpp'
    after(p,'struct PoliticalShopLicensingState;', 'struct PoliticalShopRegistrySuccessionCatalog;struct PoliticalShopRegistrySuccessionState;')
    after(p,'    std::shared_ptr<const PoliticalShopLicensingCatalog> political_shop_licensing;', '\n    std::shared_ptr<const PoliticalShopRegistrySuccessionCatalog> political_shop_registry_successions;')
    p=ROOT+'model/common_model.hpp'
    after(p,'    std::shared_ptr<const PoliticalShopLicensingState> political_shop_licensing;', '\n    std::shared_ptr<const PoliticalShopRegistrySuccessionState> political_shop_registry_successions;')
    p=ROOT+'kernel/world_bundle.hpp'
    after(p,'#include "political_shop_licensing_integration.hpp"','\n#include "political_shop_registry_integration.hpp"')
    after(p,'    PoliticalShopLicensingState political_shop_licensing;bool political_shop_licensing_seed_complete=false;', '\n    PoliticalShopRegistrySuccessionState political_shop_registry_successions;bool political_shop_registry_successions_seed_complete=false;')
    after(p,'    bool political_shop_licensing_enabled()const{return bool(catalog->political_shop_licensing);}', '\n    bool political_shop_registry_successions_enabled()const{return bool(catalog->political_shop_registry_successions);}')
    p=ROOT+'kernel/world_bundle.cpp'
    after(p,'political_shop_licensing_seed_complete(bool(seed.political_shop_licensing)),', 'political_shop_registry_successions(seed.political_shop_registry_successions?*seed.political_shop_registry_successions:PoliticalShopRegistrySuccessionState{}),political_shop_registry_successions_seed_complete(bool(seed.political_shop_registry_successions)),')
    after(p,'political_shop_licensing_seed_complete(other.political_shop_licensing_seed_complete),', 'political_shop_registry_successions(other.political_shop_registry_successions),political_shop_registry_successions_seed_complete(other.political_shop_registry_successions_seed_complete),')
    edit(p,'    valid=validate_political_shop_licensing_world(*this);', '    valid=validate_political_shop_registry_world(*this);if(!valid.ok)return valid;\n    valid=validate_political_shop_licensing_world(*this);')
    after(p,'    for(const auto& row:w.political_shop_licensing.revocations.rows()){auto checked=legal_fact(row.first,"political_shop_permit_revoked");if(!checked.ok)return checked;}', '\n    for(const auto& row:w.political_shop_registry_successions.acceptances.rows()){const auto* h=w.common.state().facts.find(row.first);if(!w.political_shop_registry_successions_enabled()||!h)return common_failure("PoliticalShopRegistryPayloadRequired",row.first);auto checked=validate_political_shop_registry_fact_header(w.common,w.political_shop_registry_successions,*h);if(!checked.ok)return checked;checked=fact(row.first,political_shop_registry_accept_kind,political_shop_registry_succession_module);if(!checked.ok)return checked;}')
    p=ROOT+'kernel/command_types.hpp'
    after(p,'#include "resident/domains/politics/licensing/political_shop_licensing_profile.hpp"','\n#include "political_shop_registry_integration.hpp"')
    edit(p,'PrivateShopCommand,TaskShopOrderCommand>;', 'PrivateShopCommand,TaskShopOrderCommand,PoliticalAcceptShopRegistry>;')
    edit(p,'PrivateShopCommandQuote,TaskShopOrderQuote>;', 'PrivateShopCommandQuote,TaskShopOrderQuote,PoliticalShopRegistryAcceptanceQuote>;')
    p=ROOT+'kernel/dispatcher.cpp'
    after(p,'    if(political_shop_licensing_command_kind(kind)&&!w.political_shop_licensing_enabled())return common_failure("PoliticalShopLicensingProfileRequired");','\n    if(kind==political_shop_registry_accept_command&&!w.political_shop_registry_successions_enabled())return common_failure("PoliticalShopRegistrySuccessionProfileRequired");')
    anchor='    const auto* shop_law=std::get_if<PoliticalShopLicensingCommand>(&command.data);'
    edit(p,anchor,'    const auto* shop_registry=std::get_if<PoliticalAcceptShopRegistry>(&command.data);if((command.kind==political_shop_registry_accept_command)!=(shop_registry!=nullptr))return reject("InvalidPoliticalShopRegistryCommand");\n'+anchor)
    edit(p,'    if(shop_law){auto q=', '    if(shop_registry){auto q=prepare_political_shop_registry_world(w,id,*shop_registry);if(!q.ok)return {false,{},q.error};return ready(w,id,command,std::move(q.value));}\n    if(shop_law){auto q=')
    anchor='    if(const auto* c=std::get_if<PoliticalShopLicensingCommand>(&op.command.data))'
    edit(p,anchor,'    if(const auto* c=std::get_if<PoliticalAcceptShopRegistry>(&op.command.data)){if(op.command.kind!=political_shop_registry_accept_command||!std::holds_alternative<PoliticalShopRegistryAcceptanceQuote>(op.quote.data))return failed<ReceiptDetails>(common_failure("InvalidPoliticalShopRegistryCommand"));}else if(op.command.kind==political_shop_registry_accept_command)return failed<ReceiptDetails>(common_failure("InvalidPoliticalShopRegistryCommand"));\n'+anchor)
    anchor='    if(const auto* c=std::get_if<PoliticalShopLicensingCommand>(&command.data))'
    edit(p,anchor,'    if(const auto* c=std::get_if<PoliticalAcceptShopRegistry>(&command.data)){auto r=commit_political_shop_registry_world(w,id,*c,std::get<PoliticalShopRegistryAcceptanceQuote>(op.quote.data));if(!r.ok)return {false,{},r.error};return {true,ReceiptDetails{Done{},{}},{}};}\n'+anchor)
    p=ROOT+'kernel/phase_plan.cpp'
    after(p,'    auto shop_law=std::holds_alternative<PoliticalShopLicensingCommand>(command.data)?std::optional<PoliticalShopLicensingState>{w.political_shop_licensing}:std::nullopt;', '\n    auto registry=std::holds_alternative<PoliticalAcceptShopRegistry>(command.data)?std::optional<PoliticalShopRegistrySuccessionState>{w.political_shop_registry_successions}:std::nullopt;')
    after(p,'if(shop_law)w.political_shop_licensing=std::move(*shop_law);','if(registry)w.political_shop_registry_successions=std::move(*registry);')
    p=ROOT+'kernel/dynamic_integration.cpp'
    edit(p,'PoliticalShopLicensingState shop_licensing;', 'PoliticalShopLicensingState shop_licensing;PoliticalShopRegistrySuccessionState shop_registry;')
    after(p,'shop_licensing(w.political_shop_licensing),','shop_registry(w.political_shop_registry_successions),')
    after(p,'w.political_shop_licensing=std::move(shop_licensing);','w.political_shop_registry_successions=std::move(shop_registry);')
    p=ROOT+'kernel/live_view_queries.cpp'
    anchor='    if(query=="political_shop_licensing")'
    edit(p,anchor,'    if(query=="political_shop_registry_successions"){auto unique=validate_economic_payload_fields(args);if(!unique.ok)return {false,{},unique.error};boundary::fields(args,{"place_id","cursor"},{},"args");return query_political_shop_registry_world(world,boundary::get_string(args,"place_id","args"),boundary::get_int(args,"cursor","args"));}\n'+anchor)
    p=ROOT+'checkpoint/command_boundary.cpp'
    after(p,'    if(political_shop_licensing_command_kind(kind)){boundary::fields(value,{},{},"receipt/details");result.data=Done{};return result;}', '\n    if(kind==political_shop_registry_accept_command){boundary::fields(value,{},{},"receipt/details");result.data=Done{};return result;}')
    after(p,'Integer tag=x.data.index();c.field("variant",tag);','if(kind==political_shop_registry_accept_command&&(!(c.schema_features&political_shop_registry_succession_checkpoint_feature)||tag!=0))return c.reject("InvalidPoliticalShopRegistryReceipt");')
    after(p,'        if(political_shop_licensing_command_kind(r.kind)&&(!r.details||!std::holds_alternative<Done>(r.details->data)))failed("InvalidPoliticalShopLicensingReceipt","receipt/details",ErrorKind::rule);','\n        if(r.kind==political_shop_registry_accept_command&&(!r.details||!std::holds_alternative<Done>(r.details->data)))failed("InvalidPoliticalShopRegistryReceipt","receipt/details",ErrorKind::rule);')
    after(p,'    if(political_shop_licensing_command_kind(value.kind)&&!(c.schema_features&political_shop_licensing_checkpoint_feature))return c.reject("PoliticalShopLicensingProfileRequired");','\n    if(value.kind==political_shop_registry_accept_command&&!(c.schema_features&political_shop_registry_succession_checkpoint_feature))return c.reject("PoliticalShopRegistrySuccessionProfileRequired");')
    after(p,'c.field("has_details",has_details);','if(value.kind==political_shop_registry_accept_command&&!has_details)return c.reject("InvalidPoliticalShopRegistryReceipt");')
    after(p,'    if(political_shop_licensing_command_kind(kind)){auto decoded=decode_political_shop_licensing_command(kind,params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}', '\n    if(kind==political_shop_registry_accept_command){auto decoded=decode_political_accept_shop_registry(params);if(!decoded.ok)return {false,{},decoded.error};return {true,{kind,std::move(decoded.value)},{}};}')
    anchor='                else if constexpr(std::is_same_v<T,PoliticalShopLicensingQuote>)'
    edit(p,anchor,'                else if constexpr(std::is_same_v<T,PoliticalShopRegistryAcceptanceQuote>)return boundary::obj({{"ok",V{true}},{"duration",V{Integer{0}}},{"political_shop_registry_successions",encode_political_shop_registry_acceptance_quote(quote)}});\n'+anchor)
    p=ROOT+'storage/common_codec.cpp'
    edit(p,'std::string domain=(kind=="private_shop_opened"', 'std::string domain=kind=="political_shop_registry_accepted"?"political_shop_registry_successions":(kind=="private_shop_opened"')
    edit(p,'auto named=string(*declared,row.first);','auto named=string(*declared,row.first);if(kind=="political_shop_registry_accepted"&&named!="political_shop_registry_successions")fail("InvalidFactDomain",row.first);')
    edit(p,'if(!(named=="private_shops"','if(!(named=="political_shop_registry_successions"&&kind=="political_shop_registry_accepted")&&!(named=="private_shops"')
    # 三个目录生产者将独立新模块追加在原商业尾之后。
    p=ROOT+'kernel/native_initial_state.cpp'
    after(p,'#include "native_initial_state.hpp"','\n#include "resident/domains/politics/licensing/political_shop_registry_model.hpp"')
    after(p,'    if(const auto* profile=scenario.find("task_shop_order_profile")){if(string(*profile,"task_shop_order_profile")!="private_shop_orders_v1")fail("UnsupportedTaskShopOrderProfile","task_shop_order_profile");admitted.module_versions.emplace_back("task_shop_orders","1");}','\n    if(const auto* registry=dynamic.find("political_shop_registry_successions")){auto parsed=decode_political_shop_registry_catalog(*registry);if(!parsed.ok)return {false,{},parsed.error};if(!parsed.value.enabled||!dynamic.find("political_shop_licensing")||!dynamic.find("private_shops")||!dynamic.find("political_takeover"))fail("PoliticalShopRegistrySuccessionDependenciesRequired","dynamic_world/political_shop_registry_successions");admitted.module_versions.emplace_back("political_shop_registry_successions","1");}')
    # 剩余持久化/作者/UI注册由同工具的独立函数处理，旧模块字节无附加序列。
    registry_codec(get,edit,after)
    registry_author_ui(get,edit,after,texts)
    if output.exists():raise RuntimeError('output must be fresh')
    output.mkdir(parents=True);entries=[];patch=[]
    import difflib
    for p,s in texts.items():
        if s==originals[p]:continue
        target=output/'candidate'/p;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(s)
        blob=lambda v:hashlib.sha1(f'blob {len(v.encode())}\0'.encode()+v.encode()).hexdigest()
        entries.append({'path':p,'before_oid':blob(originals[p]),'after_oid':blob(s)})
        patch.extend(difflib.unified_diff(originals[p].splitlines(True),s.splitlines(True),fromfile='a/'+p,tofile='b/'+p))
    (output/'candidate.patch').write_text(''.join(patch));(output/'manifest.json').write_text(json.dumps({'base':BASE,'status':'SOURCE_ONLY','shared':entries,'build_registration':'Central sole writer appends exact new CPP/header/test identities; no prior denial applies'},indent=2)+'\n')
    return entries

# 独立冷段/精确Common头，不给旧politics位隐式授权。
def registry_codec(get,edit,after):
    p=ROOT+'kernel/world_codec.cpp'
    after(p,'#include "resident/domains/politics/licensing/political_shop_licensing_profile.hpp"','\n#include "resident/domains/politics/licensing/political_shop_registry_profile.hpp"\n#include "resident/domains/politics/licensing/political_shop_registry_rules.hpp"')
    after(p,'if(catalog.task_shop_orders_enabled)domains.push_back({"task_shop_orders",1});','if(catalog.political_shop_registry_successions)domains.push_back({"political_shop_registry_successions",1});')
    after(p,'{"task_shop_orders",{"1"}}',' ,{"political_shop_registry_successions",{"1"}}')
    after(p,'        for(const auto* name:{"growth_market_history","political_shop_licensing","private_shops","task_shop_orders"})if(has(name))domains.push_back({name,1});','\n        if(has("political_shop_registry_successions")){if(!dynamic||!has("political_shop_licensing")||!has("private_shops")||!has("political_takeover"))fail("PoliticalShopRegistrySuccessionDependenciesRequired","checkpoint/module_versions");domains.push_back({"political_shop_registry_successions",1});}')
    anchor='        const auto* law=dynamic->find("political_shop_licensing");'
    after(p,anchor,'\n        if(const auto* registry=dynamic->find("political_shop_registry_successions")){if(!law||!dynamic->find("private_shops")||!result->political_takeover||std::find(caps.begin(),caps.end(),"political_shop_registry_successions_v1")==caps.end())fail("PoliticalShopRegistrySuccessionDependenciesRequired","dynamic_world/political_shop_registry_successions");auto parsed=decode_political_shop_registry_catalog(*registry);if(!parsed.ok)return {false,{},parsed.error};result->political_shop_registry_successions=std::make_shared<const PoliticalShopRegistrySuccessionCatalog>(std::move(parsed.value));}')
    after(p,'    if(result->task_shop_orders_enabled)id.module_versions.emplace_back("task_shop_orders","1");','\n    if(result->political_shop_registry_successions)id.module_versions.emplace_back("political_shop_registry_successions","1");')
    anchor='    if(catalog.political_shop_licensing){auto parsed=decode_political_shop_licensing(field(source,"political_shop_licensing","state"));if(!parsed.ok)return {false,{},parsed.error};result.political_shop_licensing=std::make_shared<const PoliticalShopLicensingState>(std::move(parsed.value));}else if(source.find("political_shop_licensing"))fail("UnexpectedShopMarketDomain","political_shop_licensing");'
    after(p,anchor,'\n    if(catalog.political_shop_registry_successions){auto parsed=decode_political_shop_registry_successions(field(source,"political_shop_registry_successions","state"));if(!parsed.ok)return {false,{},parsed.error};result.political_shop_registry_successions=std::make_shared<const PoliticalShopRegistrySuccessionState>(std::move(parsed.value));}else if(source.find("political_shop_registry_successions"))fail("UnexpectedPoliticalShopRegistrySuccessionState","political_shop_registry_successions");')
    # 原JSON额外字段拒绝门之外，接册唯一头同样只能有kind/domain。
    anchor='std::set<StableId> legal_ids;'
    edit(p,anchor,'        std::set<StableId> registry_ids;for(const auto& row:object(field(source,"facts","state"),"facts")){const auto kind=get_string(row.second,"kind",row.first);const auto* named=row.second.find("domain");if(kind!=political_shop_registry_accept_kind&&(!named||string(*named,row.first)!=political_shop_registry_succession_module))continue;if(!catalog.political_shop_registry_successions||!result.political_shop_registry_successions)fail("PoliticalShopRegistrySuccessionProfileRequired",row.first);fields(row.second,{"kind","domain"},{},row.first);if(!registry_ids.insert(row.first).second)fail("PoliticalShopRegistryPayloadRequired",row.first);const auto* h=legal_common.state().facts.find(row.first);if(!h)fail("PoliticalShopRegistryPayloadRequired",row.first);auto checked=validate_political_shop_registry_fact_header(legal_common,*result.political_shop_registry_successions,*h);if(!checked.ok)return {false,{},checked.error};}if(result.political_shop_registry_successions&&registry_ids.size()!=result.political_shop_registry_successions->acceptances.size())fail("PoliticalShopRegistryPayloadRequired","political_shop_registry_successions");\n'+anchor)
    edit(p,'    std::size_t legal_exported=0;','    std::size_t legal_exported=0,registry_exported=0;')
    anchor='        const auto* legal_header=w.common.state().facts.find(row.first);'
    after(p,anchor,'\n        if(legal_header&&(legal_header->kind==political_shop_registry_accept_kind||legal_header->domain==political_shop_registry_succession_module)){if(!w.political_shop_registry_successions_enabled()||payload)fail("PoliticalShopRegistryPayloadRequired",row.first);auto checked=validate_political_shop_registry_fact_header(w.common,w.political_shop_registry_successions,*legal_header);if(!checked.ok)return {false,{},checked.error};set(row.second,"domain",BoundaryValue{legal_header->domain});++registry_exported;continue;}')
    anchor='    {const auto& state=w.political_shop_licensing;if(legal_exported!='
    edit(p,anchor,'    if(registry_exported!=w.political_shop_registry_successions.acceptances.size())fail("PoliticalShopRegistryPayloadRequired","political_shop_registry_successions");\n'+anchor)
    after(p,'    if(w.political_shop_licensing_enabled())set(output,"political_shop_licensing",encode_political_shop_licensing(w.political_shop_licensing));','\n    if(w.political_shop_registry_successions_enabled())set(output,"political_shop_registry_successions",encode_political_shop_registry_successions(w.political_shop_registry_successions));')
    after(p,'writer.schema_features|=private_shop_checkpoint_feature;','if(w.political_shop_registry_successions_enabled())writer.schema_features|=political_shop_registry_succession_checkpoint_feature;')
    after(p,'if(enabled)political_features|=private_shop_checkpoint_feature;','if(std::find(modules.begin(),modules.end(),std::make_pair(StableId{"political_shop_registry_successions"},StableId{"1"}))!=modules.end())political_features|=political_shop_registry_succession_checkpoint_feature;')
    after(p,'reader.schema_features|=private_shop_checkpoint_feature;','if(catalog.political_shop_registry_successions)reader.schema_features|=political_shop_registry_succession_checkpoint_feature;')
    anchor='    if(w.task_shop_orders_enabled()){if(!w.tasks.shop_orders)return {false,{},common_failure("MissingTaskShopOrderDomain").error};r=section("task_shop_orders",[&](auto& c){return visit_task_shop_orders_checkpoint(c,*w.tasks.shop_orders);});if(!r.ok)return {false,{},r.error};}'
    after(p,anchor,'\n    if(w.political_shop_registry_successions_enabled()){r=section("political_shop_registry_successions",[&](auto& c){return visit_political_shop_registry_successions_checkpoint(c,w.political_shop_registry_successions);});if(!r.ok)return {false,{},r.error};}')
    anchor='        else if(section.domain=="political_shop_licensing"){auto state=std::make_shared<PoliticalShopLicensingState>();r=visit_political_shop_licensing_checkpoint(reader,*state);seed.political_shop_licensing=std::move(state);}'
    after(p,anchor,'\n        else if(section.domain=="political_shop_registry_successions"){auto state=std::make_shared<PoliticalShopRegistrySuccessionState>();r=visit_political_shop_registry_successions_checkpoint(reader,*state);seed.political_shop_registry_successions=std::move(state);}')

def registry_author_ui(get,edit,after,texts):
    p='game/ui/native_resident_backend.gd'
    after(p,'\tif definition.get("task_shop_order_profile","")=="private_shop_orders_v1": expected["module_versions"].append({"name":"task_shop_orders","version":"1"})','\n\tif definition.get("dynamic_world",{}).has("political_shop_registry_successions"): expected["module_versions"].append({"name":"political_shop_registry_successions","version":"1"})')
    after(p,'func political_shop_licensing_enabled() -> bool:', ' return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_shop_licensing")\nfunc political_shop_registry_successions_enabled() -> bool:')
    # 上一步保原方法主体并使新方法读正确开关。
    edit(p,'func political_shop_registry_successions_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_shop_licensing")','func political_shop_registry_successions_enabled() -> bool: return _catalog.get("data",{}).get("scenarios",{}).get(_scenario_id,{}).get("dynamic_world",{}).has("political_shop_registry_successions")\nfunc political_shop_registry_successions(place_id: String, cursor: int=0) -> Dictionary:\n\treturn demand_view("political_shop_registry_successions",{"place_id":place_id,"cursor":cursor})')
    p='game/ui/resident_page_requests.gd'
    after(p,'const PoliticalShopLicensingPanel=preload("res://ui/political_shop_licensing_panel.gd")','\nconst PoliticalShopRegistryPanel=preload("res://ui/political_shop_registry_panel.gd")')
    after(p,'\t\tif not PoliticalShopLicensingPanel.prepare(host,str(location.get("place_id",""))): return false','\n\t\tif not PoliticalShopRegistryPanel.prepare(host,str(location.get("place_id",""))): return false')
    p='game/ui/screen_composer.gd'
    after(p,'const PoliticalShopLicensingPanel=preload("res://ui/political_shop_licensing_panel.gd")','\nconst PoliticalShopRegistryPanel=preload("res://ui/political_shop_registry_panel.gd")')
    anchor='\t\tPoliticalShopLicensingPanel.render(m,_sidebar_section(m,stage,"PoliticalShopLicensingArea","私铺许可"),page,current_place)'
    after(p,anchor,'\n\tif not current_place.is_empty() and kernel.has_method("political_shop_registry_successions_enabled") and kernel.political_shop_registry_successions_enabled():\n\t\tPoliticalShopRegistryPanel.render(m,_sidebar_section(m,stage,"PoliticalShopRegistryArea","司法接册"),kernel.political_shop_registry_successions(current_place,int(m.get_meta("shop_registry_cursor",0))),current_place)')
    p='tools/content/links.py'
    after(p,'    from .political_shop_licensing_links import check_political_shop_licensing','\n    from .political_shop_registry_links import check_political_shop_registry_successions')
    after(p,'check_political_shop_licensing(pack,index)',' + check_political_shop_registry_successions(pack,index)')
    p='tools/content/dynamic_world_links.py'
    after(p,"            shop_law = dynamic.get('political_shop_licensing', {})","\n            registry = dynamic.get('political_shop_registry_successions', {})\n            if registry.get('enabled') is True and type(registry.get('version')) is int and registry['version'] == 1 and shop_law.get('version') == 1 and takeover.get('enabled') is True and takeover.get('version') == 1:\n                allowed.add('political_accept_shop_registry')")
    p='schemas/content.schema.json';schema=json.loads(get(p));from content.political_shop_registry_schema import with_political_shop_registry_schema
    texts[p]=json.dumps(with_political_shop_registry_schema(schema),ensure_ascii=False,indent=2)+'\n'
    p='game/data/rule_capabilities.json';caps=json.loads(get(p));table=caps['capabilities'];template=copy.deepcopy(next(v for v in table if v['capability_id']=='political_shop_licensing_v1'));template.update(capability_id='political_shop_registry_successions_v1',rule_version='political_shop_registry_successions_v1',schema_selectors=[{'path':'scenarios.*.dynamic_world.political_shop_registry_successions.version','equals':1}],test_ids=['test_political_shop_registry_panel'],evidence=[]);table.append(template);texts[p]=json.dumps(caps,ensure_ascii=False,indent=2)+'\n'
    p='tools/verify.py';edit(p,"TESTS = [", "TESTS = ['test_political_shop_registry_panel', ")

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=pathlib.Path,required=True);parser.add_argument('--output',type=pathlib.Path,required=True);args=parser.parse_args();print(json.dumps({'shared_count':len(generate(args.repo,args.output))}))
